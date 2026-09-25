// Tests for the level-scoped guard deployment checks on UTerritoryDataValidator:
// CheckPatrolContainment and CheckGuardDeploymentFeasibility, plus the post->Place resolution they
// share with CheckOrphanedSpawnPoints.
//
// Both checks answer questions about actors placed in a level, so every test here builds a real
// world and calls ValidateLevel. Nothing asserts on the validator's boolean result: these checks
// write warnings, and Scripts/Territory/editor_validation.py already treats one warning as a
// failure, so the warning itself is the contract under test.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/NPCDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "DataValidation/TerritoryDataValidator.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"

/**
 * The authored state these checks read that a test cannot otherwise write: BoundsShape and the
 * typed GuardSpawnPoints array are both protected. Naming one class keeps the seam in a single
 * place, the way FTFTerritoryFloorTestAccess holds the floor fixture's attachment seam.
 */
class FTFDeploymentValidatorSeams
{
public:
	static void ClearBoundsShape(ATerritoryVolume* Territory)
	{
		if (Territory)
		{
			Territory->BoundsShape = nullptr;
		}
	}

	static void AddAuthoredSpawnPoint(ATerritoryVolume* Territory, ATerritoryGuardSpawnPoint* Post)
	{
		if (Territory && Post)
		{
			Territory->GuardSpawnPoints.Add(Post);
		}
	}

	static bool HasGuardDefinition(const ATerritoryVolume* Territory)
	{
		return Territory && Territory->GuardNPCDefinition != nullptr;
	}

	static void ClearAuthoredSpawnPoints(ATerritoryVolume* Territory)
	{
		if (Territory)
		{
			Territory->GuardSpawnPoints.Empty();
		}
	}

	static bool HasBoundsShape(const ATerritoryVolume* Territory)
	{
		return Territory && Territory->BoundsShape != nullptr;
	}

	/** Re-author bounds with the same extent the native constructor uses, for a positive control. */
	static void RestoreAuthoredBounds(ATerritoryVolume* Territory)
	{
		if (!Territory || Territory->BoundsShape)
		{
			return;
		}
		UBoxComponent* Box = NewObject<UBoxComponent>(Territory, TEXT("RestoredBoundsShape"));
		Box->SetBoxExtent(FVector(500.f, 500.f, 200.f));
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->SetupAttachment(Territory->GetRootComponent());
		Box->RegisterComponent();
		Territory->BoundsShape = Box;
	}
};

namespace TerritoryDeploymentValidatorTests
{
/** The class both fixtures deploy. Native, so its CDO satisfies ValidateNarrativeSpawnDefinition. */
TSubclassOf<ATerritoryGuardCharacter> FixtureGuardClass()
{
	return ATerritoryGuardCharacter::StaticClass();
}

/** A real authored Place tag, so the fixture exercises the same tag path the map does. */
FGameplayTag FixturePlaceTag()
{
	return FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
}

/** A valid tag that names no loaded territory, for the "waiting for its owner" case. */
FGameplayTag UnloadedTerritoryTag()
{
	return FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.CastleHill.Farm"), false);
}

UNPCDefinition* MakeGuardDefinition()
{
	UNPCDefinition* Definition = NewObject<UNPCDefinition>();
	Definition->CharacterID = TEXT("DeploymentValidatorGuardCharacter");
	Definition->NPCID = TEXT("DeploymentValidatorGuardNPC");
	Definition->NPCClassPath = FixtureGuardClass();
	Definition->bAllowMultipleInstances = true;
	return Definition;
}

/**
 * One Place with its authored Definition, standing in a Game world so collision queries work.
 * A Game world plus a world context is the idiom the road and guard-admission suites already use
 * for tests that need real overlap results.
 */
struct FFixture
{
	UWorld* World = nullptr;
	ATerritoryProperty* Place = nullptr;
	UTerritoryPlaceDefinition* Definition = nullptr;

	FFixture()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!World)
		{
			return;
		}
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	}

	~FFixture()
	{
		Destroy();
	}

	void Destroy()
	{
		if (World)
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
			World = nullptr;
		}
	}

	/** Apply a Place Definition the way the content pipeline does, and keep it for post authoring. */
	ATerritoryProperty* SpawnPlace(const FString& DisplayName, const FGuid& StableGuid)
	{
		if (!World)
		{
			return nullptr;
		}
		ATerritoryProperty* Spawned = World->SpawnActor<ATerritoryProperty>(
			ATerritoryProperty::StaticClass(), FTransform::Identity);
		Definition = NewObject<UTerritoryPlaceDefinition>();
		Definition->TerritoryTag = FixturePlaceTag();
		Definition->DisplayName = FText::FromString(DisplayName);
		Definition->StableTerritoryGUID = StableGuid;
		Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
		Definition->DefaultGuardDefinition = MakeGuardDefinition();
		if (!Spawned || !Definition->ApplyToTerritory(Spawned))
		{
			return nullptr;
		}
		Place = Spawned;
		return Spawned;
	}

	/**
	 * Author the whole-Place staffing target and apply it, the way the content pipeline does.
	 * ApplyToTerritory is what copies InitialGuardCount onto the actor, so writing the Definition
	 * field alone would leave the actor sitting on its default.
	 */
	bool SetAuthoredGuardTarget(int32 Count)
	{
		if (!Place || !Definition)
		{
			return false;
		}
		Definition->InitialGuardCount = Count;
		return Definition->ApplyToTerritory(Place);
	}

	/**
	 * Author a guard-post row, then spawn and bind the post through the same two calls the
	 * Definition path uses. ApplyTerritoryDefinition is what copies OwnerTerritoryTag and
	 * FloorIndex off the row, so nothing here restates that mapping.
	 */
	ATerritoryGuardSpawnPoint* SpawnBoundPost(FName PostID, int32 FloorIndex, const FVector& Location)
	{
		if (!World || !Definition)
		{
			return nullptr;
		}
		FTerritoryGuardPostTemplate Row;
		Row.GuardPostID = PostID;
		Row.FloorIndex = FloorIndex;
		Row.RelativeTransform = FTransform(Location);
		Definition->GuardPosts.Add(Row);

		ATerritoryGuardSpawnPoint* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>(
			ATerritoryGuardSpawnPoint::StaticClass(), FTransform(Location));
		if (!Post)
		{
			return nullptr;
		}
		Post->SetDefinitionBinding(Definition, PostID);
		return Post->ApplyTerritoryDefinition() ? Post : nullptr;
	}

	/**
	 * Author a row and bind a post to it *without* applying the Definition, so the post keeps the
	 * transient defaults BeginPlay would later overwrite. That is the state a map loaded in the
	 * editor is in: the serialized binding exists, OwnerTerritoryTag and FloorIndex do not.
	 */
	ATerritoryGuardSpawnPoint* SpawnBoundButUnappliedPost(
		FName PostID, int32 FloorIndex, const FVector& Location)
	{
		if (!World || !Definition)
		{
			return nullptr;
		}
		FTerritoryGuardPostTemplate Row;
		Row.GuardPostID = PostID;
		Row.FloorIndex = FloorIndex;
		Row.RelativeTransform = FTransform(Location);
		Definition->GuardPosts.Add(Row);

		ATerritoryGuardSpawnPoint* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>(
			ATerritoryGuardSpawnPoint::StaticClass(), FTransform(Location));
		if (!Post)
		{
			return nullptr;
		}
		// Bound, not applied: SetDefinitionBinding writes the two serialized fields only, so the
		// transient copies stay at their defaults exactly as they are before Play.
		Post->SetDefinitionBinding(Definition, PostID);
		return Post;
	}

	/** A post bound to this Place only by its stable tag, with no authored row behind it. */
	ATerritoryGuardSpawnPoint* SpawnTagBoundPost(const FGameplayTag& OwnerTag, const FVector& Location)
	{
		if (!World)
		{
			return nullptr;
		}
		ATerritoryGuardSpawnPoint* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>(
			ATerritoryGuardSpawnPoint::StaticClass(), FTransform(Location));
		if (Post)
		{
			Post->OwnerTerritoryTag = OwnerTag;
		}
		return Post;
	}

	/** Ask the runtime's own resolution, so the test cannot drift from the transform under test. */
	bool ResolveDeployment(const ATerritoryGuardSpawnPoint* Post, FVector& OutLocation) const
	{
		FTransform Deployment;
		if (!Post || !Post->ResolveGuardDeploymentTransform(FixtureGuardClass(), Deployment))
		{
			return false;
		}
		OutLocation = Deployment.GetLocation();
		return true;
	}

	/** BlockAll cube: the same blocking actor the road suite uses to prove a collision query ran. */
	AStaticMeshActor* SpawnBlocker(const FVector& Location)
	{
		if (!World)
		{
			return nullptr;
		}
		AStaticMeshActor* Blocker = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), FTransform(Location));
		UStaticMeshComponent* Mesh = Blocker ? Blocker->GetStaticMeshComponent() : nullptr;
		if (!Mesh)
		{
			return nullptr;
		}
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		return Blocker;
	}
};

void ValidateFixtureWorld(UWorld* World, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	OutErrors.Reset();
	OutWarnings.Reset();
	if (World)
	{
		UTerritoryDataValidator::ValidateLevel(World->PersistentLevel, OutErrors, OutWarnings);
	}
}

bool HasWarning(const TArray<FString>& Warnings, const FString& Fragment)
{
	return Warnings.ContainsByPredicate([&Fragment](const FString& Warning)
	{
		return Warning.Contains(Fragment);
	});
}

int32 CountWarnings(const TArray<FString>& Warnings, const FString& Fragment)
{
	int32 Count = 0;
	for (const FString& Warning : Warnings)
	{
		Count += Warning.Contains(Fragment) ? 1 : 0;
	}
	return Count;
}

const TCHAR* const OutsideOwnPlaceFragment =
	TEXT("lies outside this Place");
const TCHAR* const BlockedDeploymentFragment =
	TEXT("blocked at its authored deployment location");
const TCHAR* const UnstaffableFloorFragment =
	TEXT("no guard post on that floor can deploy one");
const TCHAR* const DeploymentReachFragment =
	TEXT("beyond the Place's Guard Spawn Count of");
const TCHAR* const OrphanedPostFragment =
	TEXT("Orphaned GuardSpawnPoint");
const TCHAR* const UnresolvedTagFragment =
	TEXT("does not resolve to a loaded territory");
const TCHAR* const AggregateTargetFragment =
	TEXT("targets aggregate Territory");
const TCHAR* const NoBoundDefinitionFragment =
	TEXT("no Territory Definition is bound");
const TCHAR* const NoGuardPostIDFragment =
	TEXT("with no Guard Post ID");
const TCHAR* const UnmatchedRowFragment =
	TEXT("matches no row in");
} // namespace TerritoryDeploymentValidatorTests

using namespace TerritoryDeploymentValidatorTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorPatrolLeavesOwnPlace,
	"TerritoryFramework.Editor.DataValidation.PatrolNodeLeavingItsOwnPlaceIsReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorPatrolLeavesOwnPlace::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Fixture territory tag exists in the project's tag table"), FixturePlaceTag().IsValid());

	FFixture Fixture;
	ATerritoryProperty* Place = Fixture.SpawnPlace(TEXT("Validator Own Place"), FGuid(5100, 1, 1, 1));
	if (!TestNotNull(TEXT("Fixture Place exists"), Place))
	{
		return false;
	}
	// This is also the file's fixture sanity check: the Place is authored the way the pipeline
	// authors it, so the checks under test are the only source of new findings.
	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	for (const FString& Error : Errors)
	{
		AddError(Error);
	}
	TestTrue(TEXT("The authored fixture Place itself validates without errors"), Errors.IsEmpty());

	ATerritoryGuardSpawnPoint* Post = Fixture.SpawnBoundPost(TEXT("Ground_A"), 0, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Bound guard post exists"), Post))
	{
		return false;
	}

	// A neighbouring volume whose bounds swallow the distant node. Without it the node would be
	// orphaned evidence as well; with it, this is exactly the case the any-territory sweep in
	// CheckOrphanedSpawnPoints counted as "bound" and therefore stayed silent about.
	ATerritoryVolume* Neighbour = Fixture.World->SpawnActor<ATerritoryVolume>(
		ATerritoryVolume::StaticClass(), FTransform(FVector(800.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("Neighbouring volume exists"), Neighbour))
	{
		return false;
	}

	const FVector OutsideNode(800.f, 0.f, 0.f);
	FTerritoryPatrolNode Node;
	Node.Location = OutsideNode;
	Post->PatrolRoute = {Node};

	TestFalse(TEXT("Scenario: the node lies outside the post's own Place"),
		Place->ContainsPoint(OutsideNode));
	TestTrue(TEXT("Scenario: the node lies inside the neighbouring volume, so today's any-territory sweep would call the post bound"),
		Neighbour->ContainsPoint(OutsideNode));

	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestTrue(TEXT("A patrol node outside its own Place is reported"),
		HasWarning(Warnings, OutsideOwnPlaceFragment));
	TestFalse(TEXT("The out-of-bounds node is not also reported as an orphaned post"),
		HasWarning(Warnings, OrphanedPostFragment));

	// Green: the same post with the node inside its own bounds must produce no containment warning.
	const FVector InsideNode(200.f, 0.f, 0.f);
	Node.Location = InsideNode;
	Post->PatrolRoute = {Node};
	TestTrue(TEXT("Scenario: the moved node lies inside the post's own Place"),
		Place->ContainsPoint(InsideNode));

	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestFalse(TEXT("A patrol node inside its own Place is not reported"),
		HasWarning(Warnings, OutsideOwnPlaceFragment));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorBlockedPostDeployment,
	"TerritoryFramework.Editor.DataValidation.BlockedGuardPostDeploymentIsReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorBlockedPostDeployment::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!TestNotNull(TEXT("Fixture Place exists"),
		Fixture.SpawnPlace(TEXT("Validator Blocked Place"), FGuid(5100, 2, 2, 2))))
	{
		return false;
	}
	ATerritoryGuardSpawnPoint* Post = Fixture.SpawnBoundPost(TEXT("Ground_A"), 0, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Bound guard post exists"), Post))
	{
		return false;
	}

	FVector Deployment;
	if (!TestTrue(TEXT("The post resolves a deployment transform"), Fixture.ResolveDeployment(Post, Deployment)))
	{
		return false;
	}

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestFalse(TEXT("An unobstructed post is not reported as blocked"),
		HasWarning(Warnings, BlockedDeploymentFragment));

	AStaticMeshActor* Blocker = Fixture.SpawnBlocker(Deployment);
	if (!TestNotNull(TEXT("Blocking actor exists"), Blocker))
	{
		return false;
	}
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestTrue(TEXT("A guard post blocked at its authored deployment location is reported"),
		HasWarning(Warnings, BlockedDeploymentFragment));

	// Causality: with collision switched off the same actor must stop producing the warning, so it
	// is the overlap result rather than the presence of an actor that drives the finding.
	Blocker->SetActorEnableCollision(false);
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestFalse(TEXT("Removing the obstruction clears the blocked-deployment warning"),
		HasWarning(Warnings, BlockedDeploymentFragment));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorUnstaffableFloor,
	"TerritoryFramework.Editor.DataValidation.FloorWithNoDeployablePostIsReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorUnstaffableFloor::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	ATerritoryProperty* Place = Fixture.SpawnPlace(TEXT("Validator Floor Place"), FGuid(5100, 3, 3, 3));
	if (!TestNotNull(TEXT("Fixture Place exists"), Place))
	{
		return false;
	}

	// Two declared floors that each want a defender: the ground post can deploy, the upper cannot.
	FTerritoryFloorTemplate GroundFloor;
	GroundFloor.FloorIndex = 0;
	GroundFloor.DisplayName = FText::FromString(TEXT("Ground"));
	GroundFloor.DesiredGuards = 1;
	FTerritoryFloorTemplate UpperFloor;
	UpperFloor.FloorIndex = 1;
	UpperFloor.DisplayName = FText::FromString(TEXT("Upper"));
	UpperFloor.DesiredGuards = 1;
	Fixture.Definition->Floors = {GroundFloor, UpperFloor};

	ATerritoryGuardSpawnPoint* GroundPost =
		Fixture.SpawnBoundPost(TEXT("Ground_A"), 0, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Ground post exists"), GroundPost))
	{
		return false;
	}
	ATerritoryGuardSpawnPoint* UpperPost =
		Fixture.SpawnBoundPost(TEXT("Upper_A"), 1, FVector(300.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Upper post exists"), UpperPost))
	{
		return false;
	}
	TestEqual(TEXT("Scenario: the upper post carries the authored floor index"), UpperPost->GetFloorIndex(), 1);

	FVector UpperDeployment;
	if (!TestTrue(TEXT("The upper post resolves a deployment transform"),
		Fixture.ResolveDeployment(UpperPost, UpperDeployment)))
	{
		return false;
	}

	TArray<FString> Errors;
	TArray<FString> Warnings;
	AStaticMeshActor* Blocker = Fixture.SpawnBlocker(UpperDeployment);
	if (!TestNotNull(TEXT("Blocking actor exists"), Blocker))
	{
		return false;
	}

	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestTrue(TEXT("A floor whose only post cannot deploy is reported as unstaffable"),
		HasWarning(Warnings, UnstaffableFloorFragment));
	TestTrue(TEXT("The unstaffable-floor warning names the authored floor"),
		HasWarning(Warnings, TEXT("floor 1 wants 1 guard(s)")));
	// The finding must say where the posts that *did* deploy are standing, or a dead floor is a dead
	// end: "no post is bound to that floor's row" and "the posts are bound to another floor's row"
	// read identically in the warning but need opposite content fixes.
	TestTrue(TEXT("The unstaffable-floor warning names the floors the surviving posts are bound to"),
		HasWarning(Warnings, TEXT("put them on floor(s) 0")));
	TestFalse(TEXT("The deployable ground floor is not reported as unstaffable"),
		HasWarning(Warnings, TEXT("floor 0 wants")));

	// Green: clearing the obstruction makes the same floor staffable again.
	Blocker->SetActorEnableCollision(false);
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestFalse(TEXT("A floor whose post can deploy is not reported as unstaffable"),
		HasWarning(Warnings, UnstaffableFloorFragment));
	TestFalse(TEXT("A deployable post is not reported as blocked"),
		HasWarning(Warnings, BlockedDeploymentFragment));

	// Every post refused: the other half of the same message. No floor can be staffed, and the
	// finding has to say so rather than name a credit set that does not exist. The first blocker has
	// to be restored for this — phase 2 switched its collision off, so floor 1 is staffable again
	// right now and the credit set is not yet empty.
	Blocker->SetActorEnableCollision(true);
	FVector GroundDeployment;
	if (!TestTrue(TEXT("The ground post resolves a deployment transform"),
		Fixture.ResolveDeployment(GroundPost, GroundDeployment)))
	{
		return false;
	}
	if (!TestNotNull(TEXT("Second blocking actor exists"), Fixture.SpawnBlocker(GroundDeployment)))
	{
		return false;
	}
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("Both floors are reported once neither post can deploy"),
		CountWarnings(Warnings, UnstaffableFloorFragment), 2);
	TestTrue(TEXT("The warning says no post is bound to a row on any floor when none deployed"),
		HasWarning(Warnings, TEXT("no guard post in this level is bound to a row on any floor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorPerFactionPlaceNotCalledDead,
	"TerritoryFramework.Editor.DataValidation.PerFactionPlaceWithNoDefaultIsNotCalledDead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorPerFactionPlaceNotCalledDead::RunTest(const FString& Parameters)
{
	// The owning faction is runtime state, so a Place that staffs its floors per faction declares no
	// default guard definition on purpose. Resolving the guard class the way the runtime does —
	// ResolveGuardDefinition(GetOwningFaction()) — then falls through to that null default, and with
	// Play stopped the faction is invalid so no per-faction entry can be selected either. Every post
	// on such a Place therefore resolves to no class, and a check that treats that as "this post
	// cannot deploy" calls correctly authored floors unstaffable.
	//
	// It did exactly that on HopDistrictTest: Blacksmith declares two per-faction definitions, no
	// default, and two floor-1 rows, which the validator reported as a floor that "can never be
	// staffed". The candidates are every definition the Place declares, and a post is refused only
	// when every one of them refuses it.
	FFixture Fixture;
	ATerritoryProperty* Place =
		Fixture.SpawnPlace(TEXT("Validator Per-Faction Place"), FGuid(5100, 10, 10, 10));
	if (!TestNotNull(TEXT("Fixture Place exists"), Place))
	{
		return false;
	}

	FTerritoryFactionGuardDefinition FactionEntry;
	FactionEntry.Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Police"), false);
	FactionEntry.NPCDefinition = MakeGuardDefinition();
	Fixture.Definition->DefaultGuardDefinition = nullptr;
	Fixture.Definition->FactionGuardDefinitions = {FactionEntry};
	if (!TestTrue(TEXT("The per-faction definition applies to the Place"),
		Fixture.Definition->ApplyToTerritory(Place)))
	{
		return false;
	}

	FTerritoryFloorTemplate UpperFloor;
	UpperFloor.FloorIndex = 1;
	UpperFloor.DisplayName = FText::FromString(TEXT("Upper"));
	UpperFloor.DesiredGuards = 1;
	Fixture.Definition->Floors = {UpperFloor};

	ATerritoryGuardSpawnPoint* Post =
		Fixture.SpawnBoundPost(TEXT("Upper_A"), 1, FVector(300.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Bound upper post exists"), Post))
	{
		return false;
	}

	// Scenario validity: this is the pre-Play state the finding came from, and the state the runtime
	// resolution cannot answer in. If any of these changed, the test would be proving nothing.
	TestTrue(TEXT("Scenario: the Place authors no default guard definition"),
		Fixture.Definition->DefaultGuardDefinition == nullptr);
	TestEqual(TEXT("Scenario: the Place authors one per-faction guard definition"),
		Fixture.Definition->FactionGuardDefinitions.Num(), 1);
	TestFalse(TEXT("Scenario: the owning faction is not known before Play"),
		Place->GetOwningFaction().IsValid());
	// The resolution the runtime uses, answering null: this is what the check must not read as
	// "this post cannot deploy". GuardNPCDefinition and FactionGuardDefinitions are protected, so
	// the scenario is pinned through the public resolver rather than by reading the fields.
	TestTrue(TEXT("Scenario: the runtime resolution answers no definition before Play"),
		Place->ResolveGuardDefinition(Place->GetOwningFaction()) == nullptr);

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("A floor staffed by a per-faction definition is not called dead"),
		CountWarnings(Warnings, UnstaffableFloorFragment), 0);
	TestEqual(TEXT("A clear post under a per-faction definition is not reported as blocked"),
		CountWarnings(Warnings, BlockedDeploymentFragment), 0);

	// Causality: block the same post and the refusal is real under every declared definition, so
	// the finding must come back — and must say how much it covered.
	FVector Deployment;
	if (!TestTrue(TEXT("The post resolves a deployment transform"),
		Fixture.ResolveDeployment(Post, Deployment)))
	{
		return false;
	}
	if (!TestNotNull(TEXT("Blocking actor exists"), Fixture.SpawnBlocker(Deployment)))
	{
		return false;
	}

	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("A per-faction post blocked under every declared definition is reported"),
		CountWarnings(Warnings, BlockedDeploymentFragment), 1);
	TestEqual(TEXT("The floor that post alone defends is reported dead"),
		CountWarnings(Warnings, UnstaffableFloorFragment), 1);
	TestTrue(TEXT("The finding states that every declared per-faction definition was checked"),
		HasWarning(Warnings, TEXT("declared per-faction guard definitions were checked")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorFindingsReportedOnce,
	"TerritoryFramework.Editor.DataValidation.EachDeploymentFindingIsReportedOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorFindingsReportedOnce::RunTest(const FString& Parameters)
{
	// All three checks share one post->Place resolver, so the failure this pins is a resolver that
	// reports as a side effect: every caller would then repeat the same warning for one defect.
	FFixture Fixture;
	ATerritoryProperty* Place = Fixture.SpawnPlace(TEXT("Validator Single Report Place"), FGuid(5100, 4, 4, 4));
	if (!TestNotNull(TEXT("Fixture Place exists"), Place))
	{
		return false;
	}

	FTerritoryFloorTemplate GroundFloor;
	GroundFloor.FloorIndex = 0;
	GroundFloor.DisplayName = FText::FromString(TEXT("Ground"));
	GroundFloor.DesiredGuards = 1;
	FTerritoryFloorTemplate UpperFloor;
	UpperFloor.FloorIndex = 1;
	UpperFloor.DisplayName = FText::FromString(TEXT("Upper"));
	UpperFloor.DesiredGuards = 1;
	Fixture.Definition->Floors = {GroundFloor, UpperFloor};

	ATerritoryGuardSpawnPoint* GroundPost =
		Fixture.SpawnBoundPost(TEXT("Ground_A"), 0, FVector::ZeroVector);
	ATerritoryGuardSpawnPoint* UpperPost =
		Fixture.SpawnBoundPost(TEXT("Upper_A"), 1, FVector(300.f, 0.f, 0.f));
	ATerritoryGuardSpawnPoint* Orphan =
		Fixture.SpawnTagBoundPost(FGameplayTag(), FVector(50000.f, 0.f, 0.f));
	ATerritoryGuardSpawnPoint* Waiting =
		Fixture.SpawnTagBoundPost(UnloadedTerritoryTag(), FVector(-300.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Ground post exists"), GroundPost)
		|| !TestNotNull(TEXT("Upper post exists"), UpperPost)
		|| !TestNotNull(TEXT("Orphan post exists"), Orphan)
		|| !TestNotNull(TEXT("Waiting post exists"), Waiting))
	{
		return false;
	}

	FTerritoryPatrolNode Node;
	Node.Location = FVector(2000.f, 0.f, 0.f);
	GroundPost->PatrolRoute = {Node};

	FVector UpperDeployment;
	if (!TestTrue(TEXT("The upper post resolves a deployment transform"),
		Fixture.ResolveDeployment(UpperPost, UpperDeployment)))
	{
		return false;
	}
	if (!TestNotNull(TEXT("Blocking actor exists"), Fixture.SpawnBlocker(UpperDeployment)))
	{
		return false;
	}

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);

	TestEqual(TEXT("One out-of-bounds patrol node is reported once"),
		CountWarnings(Warnings, OutsideOwnPlaceFragment), 1);
	TestEqual(TEXT("One blocked post is reported once"),
		CountWarnings(Warnings, BlockedDeploymentFragment), 1);
	TestEqual(TEXT("One unstaffable floor is reported once"),
		CountWarnings(Warnings, UnstaffableFloorFragment), 1);
	TestEqual(TEXT("One orphaned post is reported once"),
		CountWarnings(Warnings, OrphanedPostFragment), 1);
	TestEqual(TEXT("One unresolved owner tag is reported once"),
		CountWarnings(Warnings, UnresolvedTagFragment), 1);

	// The waiting post names a real tag and overlaps its Place, exactly like the unresolved case:
	// the runtime parks it until its owner loads, so it must not be reported as orphaned instead.
	TestEqual(TEXT("A post waiting for an unloaded owner is not reported as orphaned"),
		CountWarnings(Warnings, FString::Printf(TEXT("Orphaned GuardSpawnPoint '%s'"),
			*Waiting->GetActorLabel())), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorMissingBoundsSuppressesPatrolCascade,
	"TerritoryFramework.Editor.DataValidation.MissingBoundsShapeSuppressesPatrolCascade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorMissingBoundsSuppressesPatrolCascade::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	ATerritoryProperty* Place = Fixture.SpawnPlace(TEXT("Validator No Bounds Place"), FGuid(5100, 5, 5, 5));
	if (!TestNotNull(TEXT("Fixture Place exists"), Place))
	{
		return false;
	}
	ATerritoryGuardSpawnPoint* Post = Fixture.SpawnBoundPost(TEXT("Ground_A"), 0, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Bound guard post exists"), Post))
	{
		return false;
	}

	FTerritoryPatrolNode Node;
	Node.Location = FVector(2000.f, 0.f, 0.f);
	Post->PatrolRoute = {Node};

	FTFDeploymentValidatorSeams::ClearBoundsShape(Place);
	TestFalse(TEXT("Scenario: the Place has no bounds shape"),
		FTFDeploymentValidatorSeams::HasBoundsShape(Place));
	TestFalse(TEXT("Scenario: without bounds, no point is contained"),
		Place->ContainsPoint(Node.Location));

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);

	TestTrue(TEXT("The missing bounds shape is still reported by the bounds check"),
		HasWarning(Warnings, TEXT("BoundsShape is null")));
	TestEqual(TEXT("A missing bounds shape does not turn every patrol node into a finding"),
		CountWarnings(Warnings, OutsideOwnPlaceFragment), 0);
	// The deployment check reads collision in the world rather than the bounds shape, so an absent
	// shape must not make the post look undeployable either.
	TestEqual(TEXT("A missing bounds shape does not make the post look blocked"),
		CountWarnings(Warnings, BlockedDeploymentFragment), 0);

	// Positive control: with the authored bounds restored, the very same distant node must be
	// reported. Without this the test could pass because the check never ran at all.
	FTFDeploymentValidatorSeams::RestoreAuthoredBounds(Place);
	TestTrue(TEXT("Control: bounds are restored"), FTFDeploymentValidatorSeams::HasBoundsShape(Place));
	TestTrue(TEXT("Control: the far node is outside the restored bounds"),
		!Place->ContainsPoint(Node.Location));
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("Control: the same node is reported once the bounds exist"),
		CountWarnings(Warnings, OutsideOwnPlaceFragment), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorUnresolvableGuardStaysSilent,
	"TerritoryFramework.Editor.DataValidation.UnresolvableGuardDefinitionIsNotReportedByDeploymentCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorUnresolvableGuardStaysSilent::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!TestNotNull(TEXT("Fixture Place exists"),
		Fixture.SpawnPlace(TEXT("Validator Undefined Guard Place"), FGuid(5100, 6, 6, 6))))
	{
		return false;
	}
	// No default guard definition and no override on the row: the post has a Place but no guard,
	// which CheckGuardConfig already reports on its own terms.
	Fixture.Definition->DefaultGuardDefinition = nullptr;
	Fixture.Definition->ApplyToTerritory(Fixture.Place);
	if (!TestNotNull(TEXT("Unguarded post exists"),
		Fixture.SpawnBoundPost(TEXT("Ground_A"), 0, FVector::ZeroVector)))
	{
		return false;
	}
	if (!TestFalse(TEXT("Scenario: the Place has no guard definition"),
		FTFDeploymentValidatorSeams::HasGuardDefinition(Fixture.Place)))
	{
		return false;
	}

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);

	TestTrue(TEXT("The pre-existing guard config check reports the missing definition"),
		HasWarning(Warnings, TEXT("no default or per-faction NPC definition")));
	TestEqual(TEXT("A post with no resolvable guard class is not reported as unblocked-or-blocked by the deployment check"),
		CountWarnings(Warnings, BlockedDeploymentFragment), 0);
	TestEqual(TEXT("A post with no resolvable guard class is not reported as having no deployment transform"),
		CountWarnings(Warnings, TEXT("cannot resolve a deployment transform")), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorAuthoredTypedArrayBinding,
	"TerritoryFramework.Editor.DataValidation.AuthoredTypedArrayBindingNeedsNoTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorAuthoredTypedArrayBinding::RunTest(const FString& Parameters)
{
	// The resolution order's first branch: a post listed in a Territory's own typed array is bound
	// even with no owner tag and standing far outside any bounds. Getting this order wrong would
	// report posts the runtime happily deploys.
	FFixture Fixture;
	ATerritoryProperty* Place = Fixture.SpawnPlace(TEXT("Validator Typed Array Place"), FGuid(5100, 7, 7, 7));
	if (!TestNotNull(TEXT("Fixture Place exists"), Place))
	{
		return false;
	}

	ATerritoryGuardSpawnPoint* Post = Fixture.SpawnTagBoundPost(FGameplayTag(), FVector(50000.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Untagged post exists"), Post))
	{
		return false;
	}
	FTFDeploymentValidatorSeams::AddAuthoredSpawnPoint(Place, Post);

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("A post authored in the Territory's typed array is not reported as orphaned"),
		CountWarnings(Warnings, OrphanedPostFragment), 0);

	// Causality: the same untagged post outside the array is exactly the case the orphan check owns.
	FTFDeploymentValidatorSeams::ClearAuthoredSpawnPoints(Place);
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("The same untagged post is reported as orphaned once it is not authored on a Territory"),
		CountWarnings(Warnings, OrphanedPostFragment), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorPrePlayFloorCredit,
	"TerritoryFramework.Editor.DataValidation.SerializedBindingCarriesFloorBeforePlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorPrePlayFloorCredit::RunTest(const FString& Parameters)
{
	// FloorIndex is transient: ApplyTerritoryDefinition copies it from the bound row at BeginPlay, so
	// a map that has never been played reports its default zero for every post. Zero is a valid
	// floor, so the stale value is indistinguishable from an authored one and a check reading it
	// credits every post to ground and calls an authored upper floor unstaffable. The row is the
	// authority the copy comes from and the binding is serialized, so the row is what to read.
	FFixture Fixture;
	ATerritoryProperty* Place =
		Fixture.SpawnPlace(TEXT("Validator Preplay Floor Place"), FGuid(5100, 8, 8, 8));
	if (!TestNotNull(TEXT("Fixture Place exists"), Place))
	{
		return false;
	}

	FTerritoryFloorTemplate UpperFloor;
	UpperFloor.FloorIndex = 1;
	UpperFloor.DisplayName = FText::FromString(TEXT("Upper"));
	UpperFloor.DesiredGuards = 1;
	Fixture.Definition->Floors = {UpperFloor};

	ATerritoryGuardSpawnPoint* Post =
		Fixture.SpawnBoundButUnappliedPost(TEXT("Upper_A"), 1, FVector(300.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Bound upper post exists"), Post))
	{
		return false;
	}

	// Scenario validity: this is genuinely the pre-Play state, not an applied post with the floor
	// cleared. Both fields BeginPlay would fill in are still at their defaults.
	TestEqual(TEXT("Scenario: the unplayed post still reads the transient default floor"),
		Post->GetFloorIndex(), 0);
	TestFalse(TEXT("Scenario: the unplayed post has not had its owner tag copied yet"),
		Post->OwnerTerritoryTag.IsValid());
	const UTerritoryDefinition* const BoundDefinition = Place->GetTerritoryDefinition();
	if (!TestNotNull(TEXT("Scenario: the post's bound row is reachable from the Place"),
		BoundDefinition ? BoundDefinition->FindGuardPost(FName(TEXT("Upper_A"))) : nullptr))
	{
		return false;
	}

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);

	TestEqual(TEXT("A post bound to an upper floor row is credited to that floor before Play"),
		CountWarnings(Warnings, UnstaffableFloorFragment), 0);
	TestEqual(TEXT("The pre-Play post is not mistaken for an orphaned one"),
		CountWarnings(Warnings, OrphanedPostFragment), 0);

	// Causality: move the same post's row to ground and floor 1 has no authored post left, so the
	// warning must appear. That proves the credit follows the row rather than the fixture's shape.
	Fixture.Definition->GuardPosts[0].FloorIndex = 0;
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("Floor 1 is reported once no authored row stands on it"),
		CountWarnings(Warnings, UnstaffableFloorFragment), 1);
	// This is the case the clause is for: the post is bound to a row that puts it on floor 0, which
	// is invisible from the warning alone. Without the credit set the author cannot tell this from
	// having authored no floor-1 post at all.
	TestTrue(TEXT("The warning names floor 0 as where the bound posts actually stand"),
		HasWarning(Warnings, TEXT("put them on floor(s) 0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorBoundPostOutsidePlace,
	"TerritoryFramework.Editor.DataValidation.SerializedBindingOutranksContainment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorBoundPostOutsidePlace::RunTest(const FString& Parameters)
{
	// OwnerTerritoryTag is transient for the same reason FloorIndex is — one ApplyTerritoryDefinition
	// copies both off the bound row — so an unplayed map can hold a correctly bound post with no tag
	// at all. Falling straight through to containment would then bind such a post by geometry:
	// reporting a post the runtime attaches by its Definition as an orphan, and naming the wrong
	// Place for its patrol.
	FFixture Fixture;
	ATerritoryProperty* Place =
		Fixture.SpawnPlace(TEXT("Validator Bound Outside Place"), FGuid(5100, 9, 9, 9));
	if (!TestNotNull(TEXT("Fixture Place exists"), Place))
	{
		return false;
	}

	// Far outside the bounds the Place constructor authors, and bound to it only by its row.
	const FVector OutsideLocation(2000.f, 0.f, 0.f);
	ATerritoryGuardSpawnPoint* Post =
		Fixture.SpawnBoundButUnappliedPost(TEXT("Ground_A"), 0, OutsideLocation);
	if (!TestNotNull(TEXT("Bound post exists"), Post))
	{
		return false;
	}
	FTerritoryPatrolNode Node;
	Node.Location = OutsideLocation;
	Post->PatrolRoute = {Node};

	TestFalse(TEXT("Scenario: the post and its patrol node stand outside the Place bounds"),
		Place->ContainsPoint(OutsideLocation));

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("A post bound by its serialized row is not reported as orphaned for standing outside the bounds"),
		CountWarnings(Warnings, OrphanedPostFragment), 0);
	TestEqual(TEXT("Its out-of-bounds patrol node is reported against the Place it is bound to"),
		CountWarnings(Warnings, OutsideOwnPlaceFragment), 1);

	// Causality: drop the binding and the same post is exactly what the orphan check owns.
	Post->SetDefinitionBinding(nullptr, NAME_None);
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("The same post is reported as orphaned once its binding is gone"),
		CountWarnings(Warnings, OrphanedPostFragment), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorGuardPostBinding,
	"TerritoryFramework.Editor.DataValidation.UnmatchedGuardPostBindingIsReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorGuardPostBinding::RunTest(const FString& Parameters)
{
	// A post whose serialized binding cannot supply a row spawns nothing at all: BeginPlay's
	// ApplyTerritoryDefinition answers false, logs, and returns before any spawn. Every other check
	// is blind to it because the post keeps its Place, patrol route and guard definition — which is
	// why it needs its own finding rather than a fourth assertion inside the deployment check.
	FFixture Fixture;
	if (!TestNotNull(TEXT("Fixture Place exists"),
		Fixture.SpawnPlace(TEXT("Validator Binding Place"), FGuid(5100, 10, 10, 10))))
	{
		return false;
	}

	ATerritoryGuardSpawnPoint* Post =
		Fixture.SpawnBoundButUnappliedPost(TEXT("Ground_A"), 0, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Bound post exists"), Post))
	{
		return false;
	}

	TArray<FString> Errors;
	TArray<FString> Warnings;

	// Green: a row with this post's ID exists, so the binding resolves and nothing is reported.
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("A post whose row exists is not reported for its binding"),
		CountWarnings(Warnings, UnmatchedRowFragment), 0);
	TestEqual(TEXT("A bound post is not reported as having no Definition"),
		CountWarnings(Warnings, NoBoundDefinitionFragment), 0);

	// A Guard Post ID that no row carries: the case the editor builder gets wrong when it names a
	// row one way and writes the post's ID another.
	Post->SetDefinitionBinding(Fixture.Definition, TEXT("NoSuchRow"));
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("A Guard Post ID matching no row is reported once"),
		CountWarnings(Warnings, UnmatchedRowFragment), 1);
	TestTrue(TEXT("The finding names the ID that failed to match"),
		HasWarning(Warnings, TEXT("Guard Post ID 'NoSuchRow'")));

	// Bound to a Definition, but with no ID to look up in it.
	Post->SetDefinitionBinding(Fixture.Definition, NAME_None);
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("A bound post with no Guard Post ID is reported once"),
		CountWarnings(Warnings, NoGuardPostIDFragment), 1);
	TestEqual(TEXT("The unmatched-row finding clears with the ID that caused it"),
		CountWarnings(Warnings, UnmatchedRowFragment), 0);

	// Not even bound to a Definition.
	Post->SetDefinitionBinding(nullptr, NAME_None);
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("A post bound to no Definition is reported once"),
		CountWarnings(Warnings, NoBoundDefinitionFragment), 1);
	TestEqual(TEXT("The no-ID finding clears with the binding that caused it"),
		CountWarnings(Warnings, NoGuardPostIDFragment), 0);
	return true;
}

/**
 * A floor that stands past the target but claims a quota IS staffed, so it must not be reported.
 *
 * The floor-quota claim pass (ATerritoryGuardSpawnPoint::PlanFloorClaims, spent by
 * SpawnGuardsToCount) serves a floor authoring DesiredGuards > 0 ahead of the flat deployment order.
 * Here the upper floor is the *worse* placed post and the ground floor claims nothing, so the flat
 * order would spend the single guard on the ground floor and this check would report the upper floor
 * as unreachable - while the runtime now staffs it first. Asserting that inverted verdict is what
 * proves the rule spends the fill's own plan rather than restating it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorClaimedFloorBeyondReachIsStaffed,
	"TerritoryFramework.Editor.DataValidation.ClaimedFloorBeyondDeploymentReachIsNotReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorClaimedFloorBeyondReachIsStaffed::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	ATerritoryProperty* Place = Fixture.SpawnPlace(TEXT("Validator Claim Place"), FGuid(5100, 5, 5, 5));
	if (!TestNotNull(TEXT("Fixture Place exists"), Place))
	{
		return false;
	}
	if (!TestTrue(TEXT("The fixture authors a staffing target of one guard"),
		Fixture.SetAuthoredGuardTarget(1)))
	{
		return false;
	}

	FTerritoryFloorTemplate GroundFloor;
	GroundFloor.FloorIndex = 0;
	GroundFloor.DisplayName = FText::FromString(TEXT("Ground"));
	GroundFloor.DesiredGuards = 0; // Claims nothing: takes only what the flat order gives it.
	FTerritoryFloorTemplate UpperFloor;
	UpperFloor.FloorIndex = 1;
	UpperFloor.DisplayName = FText::FromString(TEXT("Upper"));
	UpperFloor.DesiredGuards = 1; // Claims the single guard outright.
	Fixture.Definition->Floors = {GroundFloor, UpperFloor};

	ATerritoryGuardSpawnPoint* GroundPost =
		Fixture.SpawnBoundPost(TEXT("Ground_A"), 0, FVector::ZeroVector);
	ATerritoryGuardSpawnPoint* UpperPost =
		Fixture.SpawnBoundPost(TEXT("Upper_A"), 1, FVector(300.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Ground post exists"), GroundPost)
		|| !TestNotNull(TEXT("Upper post exists"), UpperPost))
	{
		return false;
	}

	GroundPost->Priority = 100; // Better placed, but claims nothing.
	UpperPost->Priority = 50;   // Worse placed, but claims the quota.

	TestEqual(TEXT("Scenario: the authored staffing target is one guard"),
		Place->GetConfiguredGuardCount(), 1);

	// The claim itself, stated directly, so a failure below cannot be mistaken for a fixture problem.
	// Rank order is the sorted order: the ground post leads, the upper post follows.
	TArray<ATerritoryGuardSpawnPoint::FTerritoryFloorClaim> Claims;
	const TArray<int32> ResolvedFloors = {0, 1};
	const int32 Surplus = ATerritoryGuardSpawnPoint::PlanFloorClaims(
		ResolvedFloors, Fixture.Definition->Floors, 1, Claims);
	TestEqual(TEXT("Scenario: exactly one floor claims"), Claims.Num(), 1);
	TestEqual(TEXT("Scenario: the worse-placed upper floor is the one claiming"),
		Claims.Num() == 1 ? Claims[0].FloorIndex : INDEX_NONE, 1);
	TestEqual(TEXT("Scenario: the claim consumes the whole budget, leaving no flat surplus"),
		Surplus, 0);

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);

	TestFalse(TEXT("The floor the claim staffs is not reported as unreachable"),
		HasWarning(Warnings, TEXT("floor 1's guard post(s)")));
	TestEqual(TEXT("The unclaimed floor is the one now out of reach"),
		CountWarnings(Warnings, DeploymentReachFragment), 1);
	TestTrue(TEXT("And the finding names it"),
		HasWarning(Warnings, TEXT("floor 0's guard post(s)")));
	return true;
}

/**
 * A floor whose posts are clear but that the Place's own staffing target never reaches.
 *
 * This is deliberately *not* the shipped "no guard post on that floor can deploy one" case: both
 * posts here resolve a transform and are unobstructed. The floor is dead because SpawnGuardsToCount
 * fills front to back, one guard per post, and a whole-Place target that stops short of the floor
 * never spends a guard on it. That leaves the floor's reserve untouched forever, which IsCleared()
 * reads as "not cleared", so its FloorClearedEvents never fire and a floor-filtered Tales objective
 * on it can never be satisfied. TerritoryVolume.cpp:2957 records the trap; this is the editor
 * finding for it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorFloorBeyondDeploymentReach,
	"TerritoryFramework.Editor.DataValidation.FloorBeyondDeploymentReachIsReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorFloorBeyondDeploymentReach::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	ATerritoryProperty* Place = Fixture.SpawnPlace(TEXT("Validator Reach Place"), FGuid(5100, 4, 4, 4));
	if (!TestNotNull(TEXT("Fixture Place exists"), Place))
	{
		return false;
	}

	// The whole-Place staffing target, authored on the Definition through the same call that puts
	// every other Place setting on the actor.
	if (!TestTrue(TEXT("The fixture authors a staffing target of one guard"),
		Fixture.SetAuthoredGuardTarget(1)))
	{
		return false;
	}

	FTerritoryFloorTemplate GroundFloor;
	GroundFloor.FloorIndex = 0;
	GroundFloor.DisplayName = FText::FromString(TEXT("Ground"));
	GroundFloor.DesiredGuards = 1;
	FTerritoryFloorTemplate UpperFloor;
	UpperFloor.FloorIndex = 1;
	UpperFloor.DisplayName = FText::FromString(TEXT("Upper"));
	UpperFloor.DesiredGuards = 1;
	Fixture.Definition->Floors = {GroundFloor, UpperFloor};

	// Priority decides deployment order outright when the two differ, so the test does not rest on
	// the path-name tiebreak that only applies to equal priorities.
	ATerritoryGuardSpawnPoint* GroundPost =
		Fixture.SpawnBoundPost(TEXT("Ground_A"), 0, FVector::ZeroVector);
	ATerritoryGuardSpawnPoint* UpperPost =
		Fixture.SpawnBoundPost(TEXT("Upper_A"), 1, FVector(300.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Ground post exists"), GroundPost)
		|| !TestNotNull(TEXT("Upper post exists"), UpperPost))
	{
		return false;
	}
	GroundPost->Priority = 100;
	UpperPost->Priority = 50;

	TestEqual(TEXT("Scenario: the authored staffing target is one guard"),
		Place->GetConfiguredGuardCount(), 1);

	TArray<ATerritoryGuardSpawnPoint*> DeploymentOrder = {GroundPost, UpperPost};
	ATerritoryGuardSpawnPoint::SortForDeployment(DeploymentOrder);
	TestTrue(TEXT("Scenario: the higher-priority ground post deploys first"),
		DeploymentOrder[0] == GroundPost && DeploymentOrder[1] == UpperPost);
	TestTrue(TEXT("Scenario: the target of one reaches the ground post"),
		ATerritoryGuardSpawnPoint::IsReachedByDeploymentTarget(0, 1));
	TestFalse(TEXT("Scenario: the target of one stops before the upper post"),
		ATerritoryGuardSpawnPoint::IsReachedByDeploymentTarget(1, 1));

	// The upper post is clear: this must be reported as a reach problem, not as a blocked post or as
	// a floor with nothing deployable on it, or the two findings would be indistinguishable to the
	// author who has to fix it.
	FVector UpperDeployment;
	TestTrue(TEXT("Scenario: the upper post resolves a deployment transform"),
		Fixture.ResolveDeployment(UpperPost, UpperDeployment));

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);

	TestEqual(TEXT("A floor past the deployment reach is reported exactly once"),
		CountWarnings(Warnings, DeploymentReachFragment), 1);
	TestTrue(TEXT("The reach warning names the authored floor"),
		HasWarning(Warnings, TEXT("floor 1's guard post(s) stand at position(s) 2 of 2")));
	TestTrue(TEXT("The reach warning names the target the author has to raise"),
		HasWarning(Warnings, TEXT("raised above 1")));
	TestFalse(TEXT("The floor the target does reach is not reported"),
		HasWarning(Warnings, TEXT("floor 0's guard post(s)")));
	TestFalse(TEXT("A post that can deploy is not reported as blocked"),
		HasWarning(Warnings, BlockedDeploymentFragment));
	TestFalse(TEXT("A reachable-posts floor is not reported as unstaffable"),
		HasWarning(Warnings, UnstaffableFloorFragment));

	// Green by reordering: swapping the two priorities moves the floor the single guard lands on.
	// Nothing about the floors, the posts or the target changed, so only a rule that reads the real
	// deployment order can follow this.
	GroundPost->Priority = 50;
	UpperPost->Priority = 200;
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("Reordering deployment moves the finding to the floor now out of reach"),
		CountWarnings(Warnings, DeploymentReachFragment), 1);
	TestTrue(TEXT("The finding now names the ground floor"),
		HasWarning(Warnings, TEXT("floor 0's guard post(s)")));
	TestFalse(TEXT("The floor now reached first is no longer reported"),
		HasWarning(Warnings, TEXT("floor 1's guard post(s)")));

	// Green by target: raising the whole-Place garrison by one reaches the floor again.
	GroundPost->Priority = 100;
	UpperPost->Priority = 50;
	if (!TestTrue(TEXT("The fixture raises the staffing target to two"),
		Fixture.SetAuthoredGuardTarget(2)))
	{
		return false;
	}
	TestEqual(TEXT("Re-applying the Definition raised the staffing target"),
		Place->GetConfiguredGuardCount(), 2);
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("A staffing target that reaches the floor clears the finding"),
		CountWarnings(Warnings, DeploymentReachFragment), 0);
	// Guards the green leg itself: if re-applying had dropped the posts, both floors would now read
	// as having nothing deployable, and the finding would have cleared for the wrong reason.
	TestFalse(TEXT("The green leg was not achieved by losing the posts"),
		HasWarning(Warnings, UnstaffableFloorFragment));
	return true;
}

/**
 * The reach rule counts *accepted* posts, not raw positions, because the fill skips a post it
 * refuses and carries on down the order. A Place whose single guard is pushed past a blocked post
 * onto the next floor therefore does staff that floor, and reporting it would be a false finding on
 * content whose only real problem is the blocked post one floor down.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorReachFollowsAcceptedPosts,
	"TerritoryFramework.Editor.DataValidation.DeploymentReachCountsOnlyAcceptedPosts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorReachFollowsAcceptedPosts::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	ATerritoryProperty* Place = Fixture.SpawnPlace(TEXT("Validator Reach Skip Place"), FGuid(5100, 5, 5, 5));
	if (!TestNotNull(TEXT("Fixture Place exists"), Place))
	{
		return false;
	}

	if (!TestTrue(TEXT("The fixture authors a staffing target of one guard"),
		Fixture.SetAuthoredGuardTarget(1)))
	{
		return false;
	}

	FTerritoryFloorTemplate GroundFloor;
	GroundFloor.FloorIndex = 0;
	GroundFloor.DisplayName = FText::FromString(TEXT("Ground"));
	GroundFloor.DesiredGuards = 1;
	FTerritoryFloorTemplate UpperFloor;
	UpperFloor.FloorIndex = 1;
	UpperFloor.DisplayName = FText::FromString(TEXT("Upper"));
	UpperFloor.DesiredGuards = 1;
	Fixture.Definition->Floors = {GroundFloor, UpperFloor};

	ATerritoryGuardSpawnPoint* GroundPost =
		Fixture.SpawnBoundPost(TEXT("Ground_A"), 0, FVector::ZeroVector);
	ATerritoryGuardSpawnPoint* UpperPost =
		Fixture.SpawnBoundPost(TEXT("Upper_A"), 1, FVector(300.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Ground post exists"), GroundPost)
		|| !TestNotNull(TEXT("Upper post exists"), UpperPost))
	{
		return false;
	}
	GroundPost->Priority = 100;
	UpperPost->Priority = 50;

	FVector GroundDeployment;
	if (!TestTrue(TEXT("The ground post resolves a deployment transform"),
		Fixture.ResolveDeployment(GroundPost, GroundDeployment)))
	{
		return false;
	}
	AStaticMeshActor* Blocker = Fixture.SpawnBlocker(GroundDeployment);
	if (!TestNotNull(TEXT("Blocking actor exists"), Blocker))
	{
		return false;
	}

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);

	TestTrue(TEXT("The blocked ground post is reported"),
		HasWarning(Warnings, BlockedDeploymentFragment));
	TestTrue(TEXT("The blocked floor is reported as unstaffable"),
		HasWarning(Warnings, UnstaffableFloorFragment));
	// The one guard the target allows skips the refused post and stands on the upper floor, so the
	// upper floor is staffed and must not be reported as out of reach.
	TestEqual(TEXT("A floor the fill reaches past a refused post is not reported as out of reach"),
		CountWarnings(Warnings, DeploymentReachFragment), 0);

	// And the mirror image: clear the obstruction and the same single guard stops on the ground
	// floor, putting the upper floor back out of reach.
	Blocker->SetActorEnableCollision(false);
	ValidateFixtureWorld(Fixture.World, Errors, Warnings);
	TestEqual(TEXT("Clearing the obstruction puts the upper floor back out of reach"),
		CountWarnings(Warnings, DeploymentReachFragment), 1);
	TestTrue(TEXT("The finding names the upper floor"),
		HasWarning(Warnings, TEXT("floor 1's guard post(s)")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
