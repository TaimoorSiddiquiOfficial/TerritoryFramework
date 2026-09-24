#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryStateTask.h"
#include "UnrealFramework/NarrativePlayerController.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

/**
 * Authored floors stage defenders and drive per-floor story objectives.
 *
 * The whole feature is derived: a floor owns no stored state of its own. The guard posts
 * standing on a floor own the live counts, and BuildFloorSnapshots() regroups them. These
 * tests hold that property, because the tempting alternative - a saved floor record - would
 * be a second authority over the same guards and would need save migration.
 */
class FTFTerritoryFloorTestAccess
{
public:
	/** Attach a post the way the Definition synchronizer does, without a level load. */
	static void AttachPost(ATerritoryVolume& Territory, ATerritoryGuardSpawnPoint* Post)
	{
		Territory.GuardSpawnPoints.Add(Post);
	}

	/** Provision the saved reserve pool, which ownership normally does before deploying. */
	static void ProvisionReserves(ATerritoryGuardSpawnPoint& Post)
	{
		Post.InitializeReserves();
	}

	/** Set the saved reserve pool directly, standing in for casualties and deployments. */
	static void SetReserves(ATerritoryGuardSpawnPoint& Post, int32 Count)
	{
		Post.CurrentReserveCount = Count;
	}

	/** Publish a garrison read model, standing in for a deployment or a casualty. */
	static void SetGarrison(ATerritoryVolume& Territory, const FTerritoryGarrisonSnapshot& Snapshot)
	{
		Territory.GarrisonSnapshot = Snapshot;
	}

	/**
	 * Conclude a fight the way the defender death path and the abandoned-reserve path do, so
	 * a test can reach the announcement without spawning a Narrative NPC.
	 */
	static void ConcludeFight(ATerritoryVolume& Territory,
		const TArray<FTerritoryFloorSnapshot>& FloorsBeforeLoss,
		const FTerritoryTransitionContext& TransitionContext = FTerritoryTransitionContext())
	{
		Territory.DispatchClearedFloors(FloorsBeforeLoss, TransitionContext);
	}

	/** Announce a deployment the way TrySpawnSingleGuard does once the guard is configured. */
	static void AnnounceSpawn(ATerritoryVolume& Territory, AActor* Guard,
		ATerritoryGuardSpawnPoint* SpawnPoint)
	{
		Territory.AnnounceDefenderSpawned(Guard, SpawnPoint);
	}
};

namespace TerritoryFloorTest
{
	/** The Place tag the sibling task tests use, so the fixture matches real content. */
	FGameplayTag TestTag()
	{
		return FGameplayTag::RequestGameplayTag(
			TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	}

	/** One authored post row on a floor, carrying the stable identities a real asset has. */
	FTerritoryGuardPostTemplate MakePost(const TCHAR* PostID, int32 FloorIndex,
		int32 ReserveSlots = 1)
	{
		FTerritoryGuardPostTemplate Post;
		Post.GuardPostID = FName(PostID);
		Post.StableGuardPostGUID = FGuid::NewGuid();
		Post.FloorIndex = FloorIndex;
		Post.ReserveSlots = ReserveSlots;
		return Post;
	}

	FTerritoryFloorTemplate MakeFloor(int32 FloorIndex, int32 DesiredGuards)
	{
		FTerritoryFloorTemplate Floor;
		Floor.FloorIndex = FloorIndex;
		Floor.DisplayName = FText::FromString(
			FString::Printf(TEXT("Floor %d"), FloorIndex));
		Floor.DesiredGuards = DesiredGuards;
		return Floor;
	}

	/** Find a floor entry by index, or null when the snapshot has none. */
	const FTerritoryFloorSnapshot* FindFloor(
		const FTerritoryGarrisonSnapshot& Snapshot, int32 FloorIndex)
	{
		return Snapshot.Floors.FindByPredicate(
			[FloorIndex](const FTerritoryFloorSnapshot& Entry)
			{
				return Entry.FloorIndex == FloorIndex;
			});
	}

	/**
	 * A Place whose floors are authored but whose post actors are not created yet, so each
	 * test decides which posts to stand up and in what order.
	 */
	struct FFloorFixture
	{
		UWorld* World = nullptr;
		ATerritoryProperty* Place = nullptr;
		UTerritoryPlaceDefinition* Definition = nullptr;
		TMap<FName, ATerritoryGuardSpawnPoint*> Posts;

		bool IsValid() const { return World && Place && Definition; }

		void TearDown()
		{
			if (World) World->DestroyWorld(false);
			World = nullptr;
			Place = nullptr;
			Definition = nullptr;
			Posts.Reset();
		}

		/** Stand up the actor for one authored post row and bind it the way the builder does. */
		ATerritoryGuardSpawnPoint* StandUp(const TCHAR* PostID)
		{
			if (!IsValid()) return nullptr;
			ATerritoryGuardSpawnPoint* Post =
				NewObject<ATerritoryGuardSpawnPoint>(World->PersistentLevel);
			if (!Post) return nullptr;
			Post->SetDefinitionBinding(Definition, FName(PostID));
			if (!Post->ApplyTerritoryDefinition()) return nullptr;
			FTFTerritoryFloorTestAccess::AttachPost(*Place, Post);
			Posts.Add(FName(PostID), Post);
			return Post;
		}
	};

	/** Build the world and the Place for a floor fixture. */
	bool BuildFixture(FFloorFixture& Out, FAutomationTestBase& Test)
	{
		Out.World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!Test.TestNotNull(TEXT("Floor staging world exists"), Out.World)) return false;

		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		Out.Place = Out.World->SpawnActor<ATerritoryProperty>(
			ATerritoryProperty::StaticClass(), FTransform::Identity, SpawnParams);
		if (!Test.TestNotNull(TEXT("Floor staging Place exists"), Out.Place)) return false;

		Out.Definition = NewObject<UTerritoryPlaceDefinition>();
		Out.Definition->TerritoryTag = TestTag();
		Out.Definition->DisplayName = FText::FromString(TEXT("Blacksmith"));
		Out.Definition->StableTerritoryGUID = FGuid::NewGuid();
		Out.Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
		return Test.TestTrue(TEXT("Floor fixture tag exists"), TestTag().IsValid());
	}

#if WITH_EDITOR
	/** The repo's own validation-context form, so these tests exercise the real call shape. */
	FDataValidationContext MakeContext(const TArray<FAssetData>& Associated)
	{
		return FDataValidationContext(false, EDataValidationUsecase::Script, Associated);
	}

	bool HasIssueContaining(const FDataValidationContext& Context, const FString& Fragment)
	{
		return Context.GetIssues().ContainsByPredicate(
			[&Fragment](const FDataValidationContext::FIssue& Issue)
			{
				return Issue.Message.ToString().Contains(Fragment);
			});
	}
#endif
}

/**
 * Grouping and physical capacity.
 *
 * The load-bearing claim is that a floor's capacity comes from the post actors standing on
 * it, never from the authored quota. One post holds exactly one active guard, so data cannot
 * conjure a defender the level does not provide.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorStaging,
	"TerritoryFramework.Guards.Floors.AuthoredFloorStagingAndPhysicalCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorStaging::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorTest;
	FFloorFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }

	// Ground holds one post. Floor 2 holds three. Floor 5 holds none but asks for seven,
	// which is the impossible authoring case the capacity rule must refuse to honour.
	Fixture.Definition->Floors = {
		MakeFloor(0, 0),
		MakeFloor(2, 2),
		MakeFloor(5, 7) };
	Fixture.Definition->GuardPosts = {
		MakePost(TEXT("Ground_A"), 0),
		MakePost(TEXT("Upper_A"), 2),
		MakePost(TEXT("Upper_B"), 2),
		MakePost(TEXT("Upper_C"), 2) };

	TestTrue(TEXT("Floor fixture Definition applies to its Place"),
		Fixture.Definition->ApplyToTerritory(Fixture.Place));

	ATerritoryGuardSpawnPoint* Ground = Fixture.StandUp(TEXT("Ground_A"));
	ATerritoryGuardSpawnPoint* UpperA = Fixture.StandUp(TEXT("Upper_A"));
	ATerritoryGuardSpawnPoint* UpperB = Fixture.StandUp(TEXT("Upper_B"));
	ATerritoryGuardSpawnPoint* UpperC = Fixture.StandUp(TEXT("Upper_C"));
	if (!TestNotNull(TEXT("Ground post bound"), Ground)
		|| !TestNotNull(TEXT("Floor 2 first post bound"), UpperA)
		|| !TestNotNull(TEXT("Floor 2 second post bound"), UpperB)
		|| !TestNotNull(TEXT("Floor 2 third post bound"), UpperC))
	{
		Fixture.TearDown();
		return false;
	}

	TestEqual(TEXT("A post copies its authored floor from the Definition row"),
		UpperA->GetFloorIndex(), 2);
	TestEqual(TEXT("A ground post reports floor zero"), Ground->GetFloorIndex(), 0);

	Fixture.Place->RefreshGarrisonSnapshot();
	const FTerritoryGarrisonSnapshot Snapshot = Fixture.Place->GetGarrisonSnapshot();

	TestEqual(TEXT("One snapshot entry exists per declared floor row"),
		Snapshot.Floors.Num(), 3);
	TestEqual(TEXT("The whole Place still reports every physical slot"),
		Snapshot.MaximumGuards, 4);

	const FTerritoryFloorSnapshot* GroundEntry = FindFloor(Snapshot, 0);
	const FTerritoryFloorSnapshot* UpperEntry = FindFloor(Snapshot, 2);
	const FTerritoryFloorSnapshot* EmptyEntry = FindFloor(Snapshot, 5);
	if (!TestNotNull(TEXT("Ground entry exists"), GroundEntry)
		|| !TestNotNull(TEXT("Floor 2 entry exists"), UpperEntry)
		|| !TestNotNull(TEXT("Floor 5 entry exists"), EmptyEntry))
	{
		Fixture.TearDown();
		return false;
	}

	// Capacity is physical, and the quota never raises it.
	TestEqual(TEXT("Ground capacity follows its single post"), GroundEntry->MaximumGuards, 1);
	TestEqual(TEXT("Floor 2 capacity follows its three posts"), UpperEntry->MaximumGuards, 3);
	TestEqual(TEXT("A floor with no post holds no capacity, whatever it asks for"),
		EmptyEntry->MaximumGuards, 0);
	TestEqual(TEXT("An impossible quota is still reported rather than clamped away"),
		EmptyEntry->DesiredGuards, 7);

	// A zero quota means "every post on this floor", so a floor need not restate its count.
	TestEqual(TEXT("A zero quota reports the floor's own post count"),
		GroundEntry->DesiredGuards, 1);
	TestEqual(TEXT("An authored quota is honoured verbatim"),
		UpperEntry->DesiredGuards, 2);

	// Nothing has spawned, so every floor reads empty but still holds its reserves.
	TestEqual(TEXT("No floor reports a living guard before deployment"),
		GroundEntry->ActiveGuards + UpperEntry->ActiveGuards + EmptyEntry->ActiveGuards, 0);
	TestEqual(TEXT("A post that was never provisioned reports no reserve"),
		GroundEntry->ReserveGuards, 0);

	// Provision each post the way ownership does, then re-read: the totals must split by
	// floor rather than pooling into whichever floor happened to be read first.
	FTFTerritoryFloorTestAccess::ProvisionReserves(*Ground);
	FTFTerritoryFloorTestAccess::ProvisionReserves(*UpperA);
	FTFTerritoryFloorTestAccess::ProvisionReserves(*UpperB);
	FTFTerritoryFloorTestAccess::ProvisionReserves(*UpperC);
	Fixture.Place->RefreshGarrisonSnapshot();
	TestEqual(TEXT("Ground reserves stay on ground"),
		FindFloor(Fixture.Place->GetGarrisonSnapshot(), 0)->ReserveGuards, 1);
	TestEqual(TEXT("Per-floor reserves come from that floor's posts only"),
		FindFloor(Fixture.Place->GetGarrisonSnapshot(), 2)->ReserveGuards, 3);
	TestEqual(TEXT("A floor with no post keeps no reserves, even when others have some"),
		FindFloor(Fixture.Place->GetGarrisonSnapshot(), 5)->ReserveGuards, 0);

	// A Definition with no floor rows must leave the whole read model exactly as it was.
	// This is what keeps every asset authored before floors byte-for-byte unchanged.
	Fixture.Definition->Floors.Reset();
	Fixture.Place->RefreshGarrisonSnapshot();
	TestTrue(TEXT("No declared floors leaves the per-floor breakdown empty"),
		Fixture.Place->GetGarrisonSnapshot().Floors.IsEmpty());
	TestEqual(TEXT("Removing floor rows does not disturb the whole-Place totals"),
		Fixture.Place->GetGarrisonSnapshot().MaximumGuards, 4);

	Fixture.TearDown();
	return true;
}

/**
 * Determinism: the breakdown depends on the authored floor rows, not on the order posts
 * happened to load in. A streamed, shuffled post array must not change the read model.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorSnapshotOrder,
	"TerritoryFramework.Guards.Floors.SnapshotIgnoresPostLoadOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorSnapshotOrder::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorTest;

	const auto BuildInOrder = [this](const TArray<const TCHAR*>& Order,
		FTerritoryGarrisonSnapshot& OutSnapshot)
	{
		FFloorFixture Fixture;
		if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }
		Fixture.Definition->Floors = { MakeFloor(0, 0), MakeFloor(1, 0), MakeFloor(2, 0) };
		Fixture.Definition->GuardPosts = {
			MakePost(TEXT("A"), 0),
			MakePost(TEXT("B"), 1),
			MakePost(TEXT("C"), 2),
			MakePost(TEXT("D"), 1) };
		Fixture.Definition->ApplyToTerritory(Fixture.Place);
		for (const TCHAR* PostID : Order)
		{
			if (!Fixture.StandUp(PostID)) { Fixture.TearDown(); return false; }
		}
		Fixture.Place->RefreshGarrisonSnapshot();
		OutSnapshot = Fixture.Place->GetGarrisonSnapshot();
		Fixture.TearDown();
		return true;
	};

	FTerritoryGarrisonSnapshot Forward;
	FTerritoryGarrisonSnapshot Reversed;
	if (!BuildInOrder({ TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D") }, Forward)
		|| !BuildInOrder({ TEXT("D"), TEXT("C"), TEXT("B"), TEXT("A") }, Reversed))
	{
		return false;
	}

	TestEqual(TEXT("Post load order cannot change the number of floor entries"),
		Reversed.Floors.Num(), Forward.Floors.Num());
	TestTrue(TEXT("Two load orders produce an identical garrison read model"),
		Forward == Reversed);
	for (int32 Index = 0; Index < Forward.Floors.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("Floor entry %d keeps declaration order"), Index),
			Reversed.Floors[Index].FloorIndex, Forward.Floors[Index].FloorIndex);
	}

	return true;
}

/**
 * Failure paths: inconsistent floor authoring must be reported, and reported as an error.
 * A warning would be worse than useless here - the project's editor validation gate counts
 * warnings as a failed run, and a silently reduced quota would hide a missing post actor.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorValidation,
	"TerritoryFramework.Guards.Floors.ValidationRejectsInconsistentAuthoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorValidation::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
	using namespace TerritoryFloorTest;
	const TArray<FAssetData> Associated;

	// A consistent asset must validate clean, or the gate would fail on healthy content.
	{
		UTerritoryPlaceDefinition* Clean = NewObject<UTerritoryPlaceDefinition>();
		Clean->TerritoryTag = TestTag();
		Clean->Floors = { MakeFloor(0, 1), MakeFloor(2, 2) };
		Clean->GuardPosts = {
			MakePost(TEXT("Ground_A"), 0),
			MakePost(TEXT("Upper_A"), 2),
			MakePost(TEXT("Upper_B"), 2) };
		FDataValidationContext Context = MakeContext(Associated);
		Clean->IsDataValid(Context);
		TestEqual(TEXT("Consistent floor authoring reports no errors"),
			static_cast<int32>(Context.GetNumErrors()), 0);
		TestEqual(TEXT("Consistent floor authoring raises no warnings either"),
			static_cast<int32>(Context.GetNumWarnings()), 0);
	}

	// A quota above the floor's post count is unachievable, because one post holds one guard.
	{
		UTerritoryPlaceDefinition* OverQuota = NewObject<UTerritoryPlaceDefinition>();
		OverQuota->TerritoryTag = TestTag();
		OverQuota->Floors = { MakeFloor(2, 5) };
		OverQuota->GuardPosts = { MakePost(TEXT("Upper_A"), 2), MakePost(TEXT("Upper_B"), 2) };
		FDataValidationContext Context = MakeContext(Associated);
		OverQuota->IsDataValid(Context);
		TestEqual(TEXT("A quota above the floor's post count is an error"),
			static_cast<int32>(Context.GetNumErrors()), 1);
		TestTrue(TEXT("The quota error names the shortfall"),
			HasIssueContaining(Context, TEXT("only 2 guard post(s)")));
	}

	// A post on a floor the Place never declares would silently vanish from the read model.
	{
		UTerritoryPlaceDefinition* StrayPost = NewObject<UTerritoryPlaceDefinition>();
		StrayPost->TerritoryTag = TestTag();
		StrayPost->Floors = { MakeFloor(0, 0) };
		StrayPost->GuardPosts = { MakePost(TEXT("Attic_A"), 4) };
		FDataValidationContext Context = MakeContext(Associated);
		StrayPost->IsDataValid(Context);
		TestEqual(TEXT("A post on an undeclared floor is an error"),
			static_cast<int32>(Context.GetNumErrors()), 1);
		TestTrue(TEXT("The undeclared-floor error names the post"),
			HasIssueContaining(Context, TEXT("Attic_A")));
	}

	// Two rows sharing an index would make the story task's floor target ambiguous.
	{
		UTerritoryPlaceDefinition* Duplicate = NewObject<UTerritoryPlaceDefinition>();
		Duplicate->TerritoryTag = TestTag();
		Duplicate->Floors = { MakeFloor(3, 0), MakeFloor(3, 0) };
		Duplicate->GuardPosts = { MakePost(TEXT("A"), 3) };
		FDataValidationContext Context = MakeContext(Associated);
		Duplicate->IsDataValid(Context);
		TestTrue(TEXT("A duplicated floor index is an error"),
			HasIssueContaining(Context, TEXT("declared more than once")));
	}

	// The floor index doubles as the task's target, where -1 already means the whole Place,
	// so a negative floor could never be named by a quest.
	{
		UTerritoryPlaceDefinition* Negative = NewObject<UTerritoryPlaceDefinition>();
		Negative->TerritoryTag = TestTag();
		Negative->Floors = { MakeFloor(-1, 0) };
		FDataValidationContext Context = MakeContext(Associated);
		Negative->IsDataValid(Context);
		TestTrue(TEXT("A negative floor index is an error"),
			HasIssueContaining(Context, TEXT("negative")));
	}
#endif
	return true;
}

/**
 * Narrative integration. A floor target reuses the two garrison objectives instead of
 * adding node types, but it deliberately changes what they mean: a floor is physical.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorObjectives,
	"TerritoryFramework.Tales.Tasks.FloorGarrisonObjectives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorObjectives::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorTest;
	FFloorFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }

	Fixture.Definition->Floors = { MakeFloor(0, 0), MakeFloor(2, 2) };
	Fixture.Definition->GuardPosts = {
		MakePost(TEXT("Ground_A"), 0),
		MakePost(TEXT("Upper_A"), 2),
		MakePost(TEXT("Upper_B"), 2) };
	Fixture.Definition->ApplyToTerritory(Fixture.Place);

	ATerritoryGuardSpawnPoint* Ground = Fixture.StandUp(TEXT("Ground_A"));
	ATerritoryGuardSpawnPoint* UpperA = Fixture.StandUp(TEXT("Upper_A"));
	ATerritoryGuardSpawnPoint* UpperB = Fixture.StandUp(TEXT("Upper_B"));
	if (!TestNotNull(TEXT("Ground post bound"), Ground)
		|| !TestNotNull(TEXT("Floor 2 posts bound"), UpperA)
		|| !TestNotNull(TEXT("Floor 2 second post bound"), UpperB))
	{
		Fixture.TearDown();
		return false;
	}

	// Real reserves, provisioned the way ownership does before the first deployment.
	FTFTerritoryFloorTestAccess::ProvisionReserves(*UpperA);
	FTFTerritoryFloorTestAccess::ProvisionReserves(*UpperB);
	Fixture.Place->RefreshGarrisonSnapshot();

	UTerritoryStateTask* ClearFloor2 = NewObject<UTerritoryStateTask>();
	ClearFloor2->TargetTerritory = TestTag();
	ClearFloor2->Objective = ETerritoryStateTaskObjective::AllDefendersDefeated;
	ClearFloor2->TargetFloor = 2;

	// A floor holding defenders still to send is not cleared, even with nobody standing on
	// it yet. Without this the objective would complete the moment the quest began.
	TestTrue(TEXT("Floor 2 provisioned its reserves"),
		FindFloor(Fixture.Place->GetGarrisonSnapshot(), 2)->ReserveGuards > 0);
	TestFalse(TEXT("A floor with reserves left is not cleared"),
		ClearFloor2->IsObjectiveSatisfiedBy(Fixture.Place));

	// Exhaust the floor exactly as killing its defenders and their replacements would.
	FTFTerritoryFloorTestAccess::SetReserves(*UpperA, 0);
	FTFTerritoryFloorTestAccess::SetReserves(*UpperB, 0);
	Fixture.Place->RefreshGarrisonSnapshot();
	TestEqual(TEXT("Exhausting the reserves empties the floor's reserve reading"),
		FindFloor(Fixture.Place->GetGarrisonSnapshot(), 2)->ReserveGuards, 0);
	TestTrue(TEXT("An exhausted floor is cleared"),
		ClearFloor2->IsObjectiveSatisfiedBy(Fixture.Place));

	// The same Place still holds a staffed ground floor, so a floor target must not be
	// reading the Place-wide picture.
	TestEqual(TEXT("Floor 2's posts never leak into floor 0's capacity"),
		FindFloor(Fixture.Place->GetGarrisonSnapshot(), 0)->MaximumGuards, 1);

	// An undeclared floor must never be satisfied by falling back to the whole Place.
	UTerritoryStateTask* Undeclared = NewObject<UTerritoryStateTask>();
	Undeclared->TargetTerritory = TestTag();
	Undeclared->Objective = ETerritoryStateTaskObjective::AllDefendersDefeated;
	Undeclared->TargetFloor = 9;
	TestFalse(TEXT("A floor the Place does not declare is never satisfied"),
		Undeclared->IsObjectiveSatisfiedBy(Fixture.Place));

	// -1 keeps the Place-wide guard objective exactly as it behaved before floors existed.
	UTerritoryStateTask* WholePlace = NewObject<UTerritoryStateTask>();
	WholePlace->TargetTerritory = TestTag();
	WholePlace->Objective = ETerritoryStateTaskObjective::ReachDesiredGarrison;
	WholePlace->RequiredQuantity = 1;
	WholePlace->TargetFloor = -1;
	TestFalse(TEXT("A whole-Place guard target stays open while nothing is assigned"),
		WholePlace->IsObjectiveSatisfiedBy(Fixture.Place));

	// Assign Guards on a floor is physical: it counts guards actually standing there.
	UTerritoryStateTask* StaffFloor2 = NewObject<UTerritoryStateTask>();
	StaffFloor2->TargetTerritory = TestTag();
	StaffFloor2->Objective = ETerritoryStateTaskObjective::ReachDesiredGarrison;
	StaffFloor2->TargetFloor = 2;
	StaffFloor2->RequiredQuantity = 2;
	TestFalse(TEXT("A floor staffing objective stays open with no guard on the floor"),
		StaffFloor2->IsObjectiveSatisfiedBy(Fixture.Place));

	// The generated text must name the floor, or a designer cannot tell the two apart.
	TestTrue(TEXT("A floor task description names its floor"),
		ClearFloor2->GetTaskDescription().ToString().Contains(TEXT("floor 2")));
	TestTrue(TEXT("A whole-Place task description is unchanged"),
		!WholePlace->GetTaskDescription().ToString().Contains(TEXT("floor")));

	Fixture.TearDown();
	return true;
}

/**
 * Regression. A floor-filtered garrison objective must read the floor entry out of a snapshot
 * that is still alive, and must read the floor it is named after.
 *
 * ATerritoryVolume::GetGarrisonSnapshot() returns the read model by value, so a floor entry
 * taken from its return value dies with the full expression. A release allocator usually keeps
 * serving the freed bytes, which is why the same test passes either way under the default
 * allocator; Tools/Run-Tests.ps1 -Stomp is what turns the stale read into the access violation
 * it really is. Both legs below assert the value that was read as well as
 * exercising the read, and the two floor rows carry different counts, so a stale, a neighbouring
 * or a Place-wide read cannot produce the expected numbers.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorProgressSnapshotRead,
	"TerritoryFramework.Guards.Floors.ProgressReadsItsOwnFloorSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorProgressSnapshotRead::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorTest;
	FFloorFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);

	Fixture.Definition->Floors = { MakeFloor(0, 0), MakeFloor(2, 2) };
	Fixture.Definition->GuardPosts = {
		MakePost(TEXT("Ground_A"), 0),
		MakePost(TEXT("Upper_A"), 2) };
	if (!TestTrue(TEXT("Floor fixture definition applied"),
		Fixture.Definition->ApplyToTerritory(Fixture.Place))
		|| !TestNotNull(TEXT("Ground post bound"), Fixture.StandUp(TEXT("Ground_A")))
		|| !TestNotNull(TEXT("Floor 2 post bound"), Fixture.StandUp(TEXT("Upper_A"))))
	{
		Fixture.TearDown();
		return false;
	}

	// The reader only commits progress when its Tales component holds authority, so the fixture
	// needs a real authoritative owner rather than a bare task.
	auto* Controller = NewObject<ANarrativePlayerController>(Fixture.World->PersistentLevel);
	Controller->SetRole(ROLE_Authority);
	UTalesComponent* Tales = Controller->GetTalesComponent();
	auto* Registry = Fixture.World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!TestNotNull(TEXT("Authoritative Tales owner exists"), Tales)
		|| !TestNotNull(TEXT("Territory registry exists"), Registry))
	{
		Fixture.TearDown();
		return false;
	}
	if (!TestTrue(TEXT("Floor fixture Territory registered"),
		Registry->RegisterTerritory(Fixture.Place) == ETerritoryRegistrationResult::Success))
	{
		Fixture.TearDown();
		return false;
	}

	// Publish a read model whose floor rows disagree with each other and with the Place-wide
	// staffing target, so reading the wrong row is visible in the progress that follows.
	const auto Publish = [&](int32 GroundGuards, int32 UpperGuards)
	{
		FTerritoryGarrisonSnapshot Snapshot;
		Snapshot.ActiveGuards = GroundGuards + UpperGuards;
		Snapshot.DesiredGuards = 7;
		Snapshot.MaximumGuards = 3;
		FTerritoryFloorSnapshot Ground;
		Ground.FloorIndex = 0;
		Ground.ActiveGuards = GroundGuards;
		Ground.MaximumGuards = 1;
		FTerritoryFloorSnapshot Upper;
		Upper.FloorIndex = 2;
		Upper.ActiveGuards = UpperGuards;
		Upper.MaximumGuards = 2;
		Snapshot.Floors = { Ground, Upper };
		FTFTerritoryFloorTestAccess::SetGarrison(*Fixture.Place, Snapshot);
		return Snapshot;
	};

	const auto MakeTask = [&](int32 Floor)
	{
		auto* Task = NewObject<UTerritoryStateTask>(Tales);
		Task->TargetTerritory = TestTag();
		Task->Objective = ETerritoryStateTaskObjective::ReachDesiredGarrison;
		Task->TargetFloor = Floor;
		// Above every guard count here, so the objective stays a progress reading rather than
		// a completion.
		Task->RequiredQuantity = 9;
		Task->OwningComp = Tales;
		Task->MarkerSettings.bAddNavigationMarker = false;
		return Task;
	};

	Publish(1, 3);

	// Leg one: activation. BeginTask resolves the registered Place, binds it, and evaluates the
	// objective - the path that first took a floor entry out of a destroyed snapshot.
	UTerritoryStateTask* Upper = MakeTask(2);
	UTerritoryStateTask* Ground = MakeTask(0);
	Upper->BeginTask();
	Ground->BeginTask();
	TestEqual(TEXT("A floor task reads its own floor's guards"), Upper->CurrentProgress, 3);
	TestEqual(TEXT("A ground task does not read the upper floor's guards"), Ground->CurrentProgress, 1);

	// Leg two: the replica path. The garrison delegate hands its subscriber the read model by
	// value, and the floor entry is taken out of that copy.
	const FTerritoryGarrisonSnapshot Second = Publish(4, 5);
	Fixture.Place->OnGarrisonChanged.Broadcast(Fixture.Place, Second);
	TestEqual(TEXT("A floor task follows its own floor's new guard count"), Upper->CurrentProgress, 5);
	TestEqual(TEXT("A ground task follows its own floor's new guard count"), Ground->CurrentProgress, 4);

	// A floor the Place does not declare reads nothing. The two legs above prove this same reader
	// commits a non-zero count for a declared floor, so this zero is a real reading of an absent
	// floor row rather than a progress write that never happened.
	UTerritoryStateTask* Undeclared = MakeTask(9);
	Undeclared->BeginTask();
	TestEqual(TEXT("A floor the Place does not declare reads no guards"), Undeclared->CurrentProgress, 0);

	// The whole-Place reading of the same objective is deliberately not re-asserted here;
	// FTFTerritoryFloorObjectives already pins -1 to the Place staffing target, and this fixture
	// never commits ownership, so its staffing target is zero and could not discriminate.

	Upper->EndTask();
	Ground->EndTask();
	Undeclared->EndTask();
	Fixture.TearDown();
	return true;
}

/**
 * Blueprint contract. Floors are authorable data, the read model is a client read model, and
 * the spawn point's floor assignment is server-side configuration rather than saved state.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorContract,
	"TerritoryFramework.Guards.Floors.BlueprintAndReplicationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorContract::RunTest(const FString& Parameters)
{
	const auto HasProperty = [](const UStruct* Struct, const TCHAR* Name)
	{
		return Struct && Struct->FindPropertyByName(FName(Name)) != nullptr;
	};
	const auto HasFlag = [](const UStruct* Struct, const TCHAR* Name,
		EPropertyFlags Flag, bool bExpected)
	{
		const FProperty* Prop = Struct ? Struct->FindPropertyByName(FName(Name)) : nullptr;
		return Prop && Prop->HasAnyPropertyFlags(Flag) == bExpected;
	};

	const UStruct* FloorTemplate = FTerritoryFloorTemplate::StaticStruct();
	const UStruct* PostTemplate = FTerritoryGuardPostTemplate::StaticStruct();
	const UStruct* FloorSnapshot = FTerritoryFloorSnapshot::StaticStruct();
	const UStruct* GarrisonSnapshot = FTerritoryGarrisonSnapshot::StaticStruct();
	const UClass* DefinitionClass = UTerritoryDefinition::StaticClass();
	const UClass* TaskClass = UTerritoryStateTask::StaticClass();
	const UClass* PostClass = ATerritoryGuardSpawnPoint::StaticClass();
	const UClass* VolumeClass = ATerritoryVolume::StaticClass();

	TestTrue(TEXT("Floor rows expose their index"), HasProperty(FloorTemplate, TEXT("FloorIndex")));
	TestTrue(TEXT("Floor rows expose a display name"), HasProperty(FloorTemplate, TEXT("DisplayName")));
	TestTrue(TEXT("Floor rows expose their quota"), HasProperty(FloorTemplate, TEXT("DesiredGuards")));
	TestTrue(TEXT("Floor rows expose their Narrative events"),
		HasProperty(FloorTemplate, TEXT("FloorClearedEvents")));
	TestTrue(TEXT("A guard post carries its floor"),
		HasProperty(PostTemplate, TEXT("FloorIndex")));
	TestTrue(TEXT("A Definition carries its floors"), HasProperty(DefinitionClass, TEXT("Floors")));
	TestTrue(TEXT("A floor row is authorable in the Definition"),
		HasFlag(FloorTemplate, TEXT("FloorIndex"), CPF_Edit, true));

	TestTrue(TEXT("The per-floor read model carries its index"),
		HasProperty(FloorSnapshot, TEXT("FloorIndex")));
	TestTrue(TEXT("The per-floor read model carries its living guards"),
		HasProperty(FloorSnapshot, TEXT("ActiveGuards")));
	TestTrue(TEXT("The per-floor read model carries its quota"),
		HasProperty(FloorSnapshot, TEXT("DesiredGuards")));
	TestTrue(TEXT("The per-floor read model carries its capacity"),
		HasProperty(FloorSnapshot, TEXT("MaximumGuards")));
	TestTrue(TEXT("The per-floor read model carries its reserves"),
		HasProperty(FloorSnapshot, TEXT("ReserveGuards")));
	TestTrue(TEXT("The per-floor read model carries its pending deployments"),
		HasProperty(FloorSnapshot, TEXT("PendingDeployments")));
	TestTrue(TEXT("The garrison read model carries the per-floor breakdown"),
		HasProperty(GarrisonSnapshot, TEXT("Floors")));
	TestTrue(TEXT("The floor breakdown is visible to client UI"),
		HasFlag(GarrisonSnapshot, TEXT("Floors"), CPF_BlueprintVisible, true));

	TestTrue(TEXT("A story task can target one floor"),
		HasProperty(TaskClass, TEXT("TargetFloor")));
	TestTrue(TEXT("The floor target is authorable in a quest"),
		HasFlag(TaskClass, TEXT("TargetFloor"), CPF_Edit, true));

	// Derived state must not become durable state. These assertions guard against a later
	// change quietly persisting a second authority over the same guards.
	TestTrue(TEXT("The garrison read model is not saved"),
		HasFlag(VolumeClass, TEXT("GarrisonSnapshot"), CPF_SaveGame, false));
	TestTrue(TEXT("The garrison read model is replicated"),
		HasFlag(VolumeClass, TEXT("GarrisonSnapshot"), CPF_Net, true));
	TestTrue(TEXT("A floor entry is not saved"),
		HasFlag(FloorSnapshot, TEXT("FloorIndex"), CPF_SaveGame, false));
	TestTrue(TEXT("A post's floor assignment is not saved"),
		HasFlag(PostClass, TEXT("FloorIndex"), CPF_Transient, true));

	return true;
}

/**
 * Regression guard for the negative this batch closes: floors are regrouped from the posts,
 * so two Places with identical posts but different floor authoring still report the same
 * whole-Place totals, and no guard is counted twice.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorRegression,
	"TerritoryFramework.Guards.Floors.Regression.FloorsAreDerivedFromPosts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorRegression::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorTest;

	const auto BuildWithFloors = [this](bool bAuthorFloors, FTerritoryGarrisonSnapshot& Out)
	{
		FFloorFixture Fixture;
		if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }
		if (bAuthorFloors)
		{
			Fixture.Definition->Floors = { MakeFloor(0, 0), MakeFloor(2, 0) };
		}
		// Both variants stand up the same three posts; only the floor assignment differs,
		// so any divergence in the totals would be an invented guard.
		Fixture.Definition->GuardPosts = {
			MakePost(TEXT("Ground_A"), 0),
			MakePost(TEXT("Upper_A"), bAuthorFloors ? 2 : 0),
			MakePost(TEXT("Upper_B"), bAuthorFloors ? 2 : 0) };
		Fixture.Definition->ApplyToTerritory(Fixture.Place);
		if (!Fixture.StandUp(TEXT("Ground_A"))
			|| !Fixture.StandUp(TEXT("Upper_A"))
			|| !Fixture.StandUp(TEXT("Upper_B")))
		{
			Fixture.TearDown();
			return false;
		}
		Fixture.Place->RefreshGarrisonSnapshot();
		Out = Fixture.Place->GetGarrisonSnapshot();
		Fixture.TearDown();
		return true;
	};

	FTerritoryGarrisonSnapshot Authored;
	FTerritoryGarrisonSnapshot Legacy;
	if (!BuildWithFloors(true, Authored) || !BuildWithFloors(false, Legacy)) return false;

	TestEqual(TEXT("Floor authoring adds no guard to the whole-Place capacity"),
		Authored.MaximumGuards, Legacy.MaximumGuards);
	TestEqual(TEXT("Floor authoring changes no whole-Place reserve count"),
		Authored.ReserveGuards, Legacy.ReserveGuards);
	TestEqual(TEXT("Three posts never become four"), Authored.MaximumGuards, 3);
	TestEqual(TEXT("Per-floor capacity sums to the whole-Place capacity"),
		FindFloor(Authored, 0)->MaximumGuards + FindFloor(Authored, 2)->MaximumGuards,
		Authored.MaximumGuards);
	TestTrue(TEXT("Only the authored variant carries floor entries"),
		Legacy.Floors.IsEmpty() && !Authored.Floors.IsEmpty());

	return true;
}

#endif
