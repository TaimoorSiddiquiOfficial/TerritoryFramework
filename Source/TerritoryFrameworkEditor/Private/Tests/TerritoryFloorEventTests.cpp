#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryFloorEventProbe.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

/**
 * The two floor story seams: a defender arrival callback and a once-per-transition floor-clear
 * callback, plus the authored Narrative events a clear dispatches.
 *
 * Both delegates are dynamic multicast, so they can only bind to a UFUNCTION and the tests
 * receive them through UTerritoryFloorEventProbe. The fixture reaches the private read model
 * and the two announcement entry points through FTFTerritoryFloorEventTestAccess, which stands
 * in for the defender death and guard deployment paths without spawning a Narrative NPC.
 */
class FTFTerritoryFloorEventTestAccess
{
public:
	/** Attach a post the way the Definition synchronizer does, without a level load. */
	static void AttachPost(ATerritoryVolume& Territory, ATerritoryGuardSpawnPoint* Post)
	{
		Territory.GuardSpawnPoints.Add(Post);
	}

	/** Publish the garrison read model, standing in for a deployment or a casualty. */
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

namespace TerritoryFloorEventTest
{
	/** The Place tag the sibling floor tests use, so the fixture matches real content. */
	FGameplayTag TestTag()
	{
		return FGameplayTag::RequestGameplayTag(
			TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	}

	FTerritoryGuardPostTemplate MakePost(const TCHAR* PostID, int32 FloorIndex)
	{
		FTerritoryGuardPostTemplate Post;
		Post.GuardPostID = FName(PostID);
		Post.StableGuardPostGUID = FGuid::NewGuid();
		Post.FloorIndex = FloorIndex;
		Post.ReserveSlots = 1;
		return Post;
	}

	FTerritoryFloorTemplate MakeFloor(int32 FloorIndex)
	{
		FTerritoryFloorTemplate Floor;
		Floor.FloorIndex = FloorIndex;
		Floor.DisplayName = FText::FromString(FString::Printf(TEXT("Floor %d"), FloorIndex));
		return Floor;
	}

	/** One floor's read model, with every field named so a test states its case exactly. */
	FTerritoryFloorSnapshot MakeFloorEntry(int32 FloorIndex, int32 ActiveGuards,
		int32 MaximumGuards, int32 ReserveGuards = 0, int32 PendingDeployments = 0)
	{
		FTerritoryFloorSnapshot Entry;
		Entry.FloorIndex = FloorIndex;
		Entry.ActiveGuards = ActiveGuards;
		Entry.MaximumGuards = MaximumGuards;
		Entry.ReserveGuards = ReserveGuards;
		Entry.PendingDeployments = PendingDeployments;
		Entry.DesiredGuards = MaximumGuards;
		return Entry;
	}

	/** A garrison read model built from explicit floor entries, with its totals summed. */
	FTerritoryGarrisonSnapshot MakeGarrison(TArray<FTerritoryFloorSnapshot> Floors)
	{
		FTerritoryGarrisonSnapshot Snapshot;
		Snapshot.Floors = MoveTemp(Floors);
		for (const FTerritoryFloorSnapshot& Floor : Snapshot.Floors)
		{
			Snapshot.ActiveGuards += Floor.ActiveGuards;
			Snapshot.MaximumGuards += Floor.MaximumGuards;
			Snapshot.ReserveGuards += Floor.ReserveGuards;
			Snapshot.PendingDeployments += Floor.PendingDeployments;
			Snapshot.DesiredGuards += Floor.DesiredGuards;
		}
		return Snapshot;
	}

	/**
	 * A Place declaring floors 0 and 2 (each with posts) and floor 5 (declared, no post), so
	 * a test can distinguish "cleared by someone" from "never held a defender".
	 */
	struct FFloorFixture
	{
		UWorld* World = nullptr;
		ATerritoryProperty* Place = nullptr;
		UTerritoryPlaceDefinition* Definition = nullptr;

		bool IsValid() const { return World && Place && Definition; }

		void TearDown()
		{
			if (World) World->DestroyWorld(false);
			World = nullptr;
			Place = nullptr;
			Definition = nullptr;
		}
	};

	bool BuildFixture(FFloorFixture& Out, FAutomationTestBase& Test)
	{
		Out.World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!Test.TestNotNull(TEXT("Floor event world exists"), Out.World)) return false;

		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		Out.Place = Out.World->SpawnActor<ATerritoryProperty>(
			ATerritoryProperty::StaticClass(), FTransform::Identity, SpawnParams);
		if (!Test.TestNotNull(TEXT("Floor event Place exists"), Out.Place)) return false;

		Out.Definition = NewObject<UTerritoryPlaceDefinition>();
		Out.Definition->TerritoryTag = TestTag();
		Out.Definition->DisplayName = FText::FromString(TEXT("Blacksmith"));
		Out.Definition->StableTerritoryGUID = FGuid::NewGuid();
		Out.Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
		Out.Definition->Floors = { MakeFloor(0), MakeFloor(2), MakeFloor(5) };
		Out.Definition->GuardPosts = {
			MakePost(TEXT("Ground_A"), 0),
			MakePost(TEXT("Upper_A"), 2),
			MakePost(TEXT("Upper_B"), 2) };
		if (!Test.TestTrue(TEXT("Floor event Definition applies to its Place"),
			Out.Definition->ApplyToTerritory(Out.Place))) return false;

		return Test.TestTrue(TEXT("Floor event fixture tag exists"), TestTag().IsValid());
	}

	/** Stand up the actor for one authored post row and bind it the way the builder does. */
	ATerritoryGuardSpawnPoint* StandUp(FFloorFixture& Fixture, const TCHAR* PostID)
	{
		if (!Fixture.IsValid()) return nullptr;
		ATerritoryGuardSpawnPoint* Post =
			NewObject<ATerritoryGuardSpawnPoint>(Fixture.World->PersistentLevel);
		if (!Post) return nullptr;
		Post->SetDefinitionBinding(Fixture.Definition, FName(PostID));
		if (!Post->ApplyTerritoryDefinition()) return nullptr;
		FTFTerritoryFloorEventTestAccess::AttachPost(*Fixture.Place, Post);
		return Post;
	}

	/** Bind the probe to both real delegates through the Blueprint-assignable properties. */
	bool BindProbe(ATerritoryVolume& Territory, UTerritoryFloorEventProbe& Probe,
		FAutomationTestBase& Test)
	{
		FMulticastDelegateProperty* ClearedProperty = FindFProperty<FMulticastDelegateProperty>(
			ATerritoryVolume::StaticClass(), TEXT("OnFloorCleared"));
		FMulticastDelegateProperty* SpawnedProperty = FindFProperty<FMulticastDelegateProperty>(
			ATerritoryVolume::StaticClass(), TEXT("OnDefenderSpawned"));
		if (!Test.TestNotNull(TEXT("OnFloorCleared is a multicast delegate property"), ClearedProperty)
			|| !Test.TestNotNull(TEXT("OnDefenderSpawned is a multicast delegate property"), SpawnedProperty))
		{
			return false;
		}

		FScriptDelegate ClearedDelegate;
		ClearedDelegate.BindUFunction(&Probe,
			GET_FUNCTION_NAME_CHECKED(UTerritoryFloorEventProbe, FloorCleared));
		ClearedProperty->AddDelegate(ClearedDelegate, &Territory);

		FScriptDelegate SpawnedDelegate;
		SpawnedDelegate.BindUFunction(&Probe,
			GET_FUNCTION_NAME_CHECKED(UTerritoryFloorEventProbe, DefenderSpawned));
		SpawnedProperty->AddDelegate(SpawnedDelegate, &Territory);
		return true;
	}

	/** The floors as they stand at each moment of the fight the callback tests replay. */
	namespace Fight
	{
		/** Both floors staffed: ground one guard, floor 2 two. */
		TArray<FTerritoryFloorSnapshot> Defended()
		{
			return { MakeFloorEntry(0, 1, 1), MakeFloorEntry(2, 2, 2) };
		}

		/** Ground has fallen and holds nothing; floor 2 is down to its last defender. */
		TArray<FTerritoryFloorSnapshot> GroundFallen()
		{
			return { MakeFloorEntry(0, 0, 1), MakeFloorEntry(2, 1, 2) };
		}

		/** Floor 2 is between waves: nobody standing, one reserve still queued behind. */
		TArray<FTerritoryFloorSnapshot> BetweenWaves()
		{
			return { MakeFloorEntry(0, 0, 1), MakeFloorEntry(2, 0, 2, 1) };
		}

		/** Every defender and replacement is gone from both floors. */
		TArray<FTerritoryFloorSnapshot> Exhausted()
		{
			return { MakeFloorEntry(0, 0, 1), MakeFloorEntry(2, 0, 2) };
		}
	}
}

/**
 * The cleared rule itself. Both the per-floor objective and the floor-cleared event read this
 * one function, so every branch here is a branch of the objective too.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorClearedRule,
	"TerritoryFramework.Guards.Floors.ClearedRuleRequiresAnExhaustedFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorClearedRule::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorEventTest;

	TestTrue(TEXT("A floor with no living defender, no reserve and no pending deployment is cleared"),
		MakeFloorEntry(0, 0, 1).IsCleared());

	// The reinforcement gap: a floor between waves still has a fight left in it.
	TestFalse(TEXT("A floor holding a reserve is not cleared even with nobody alive"),
		MakeFloorEntry(0, 0, 3, 2).IsCleared());
	TestFalse(TEXT("A floor with a deployment already on its way is not cleared"),
		MakeFloorEntry(0, 0, 3, 0, 1).IsCleared());
	TestFalse(TEXT("A floor with a living defender is not cleared"),
		MakeFloorEntry(0, 1, 3).IsCleared());

	// A floor with no post was never defended, so announcing it would be a fight nobody fought.
	TestFalse(TEXT("A declared floor with no post is never cleared"),
		MakeFloorEntry(5, 0, 0).IsCleared());
	return true;
}

/**
 * A floor is announced cleared only on an observed transition, because a listener acts on this
 * by advancing quests and playing cutscenes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorEventCallbacks,
	"TerritoryFramework.Guards.Floors.ClearedEventFiresOnceOnObservedTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorEventCallbacks::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorEventTest;
	FFloorFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }

	UTerritoryFloorEventProbe* Probe = NewObject<UTerritoryFloorEventProbe>();
	if (!BindProbe(*Fixture.Place, *Probe, *this)) { Fixture.TearDown(); return false; }

	// Ground falls with nothing left to send, while floor 2 keeps fighting.
	FTFTerritoryFloorEventTestAccess::SetGarrison(*Fixture.Place,
		MakeGarrison(Fight::GroundFallen()));
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, Fight::Defended());

	TestEqual(TEXT("Exactly one floor is announced cleared"), Probe->FloorClearedCount, 1);
	TestEqual(TEXT("The floor that lost its last defender is the one announced"),
		Probe->ClearedFloors.Num() == 1 ? Probe->ClearedFloors[0] : INDEX_NONE, 0);

	// A repeated conclusion - a re-evaluation, a reload, a second casualty on the same floor -
	// sees a floor that was already empty when the loss was captured, so it stays silent.
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, Fight::GroundFallen());
	TestEqual(TEXT("An already-cleared floor is never announced twice"), Probe->FloorClearedCount, 1);

	// A floor that queued a replacement has not been cleared. Without the reserve requirement
	// this would announce a clear the moment the first wave died.
	FTFTerritoryFloorEventTestAccess::SetGarrison(*Fixture.Place,
		MakeGarrison(Fight::BetweenWaves()));
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, Fight::GroundFallen());
	TestEqual(TEXT("A floor holding a reserve is not announced as cleared"),
		Probe->FloorClearedCount, 1);

	// First observation: a save loaded after the fight, or a freshly claimed Place, must not
	// replay a story beat for a floor nobody was ever seen defending.
	FTFTerritoryFloorEventTestAccess::SetGarrison(*Fixture.Place,
		MakeGarrison(Fight::Exhausted()));
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place,
		TArray<FTerritoryFloorSnapshot>());
	TestEqual(TEXT("A floor whose first observation is empty is not announced"),
		Probe->FloorClearedCount, 1);

	// A declared floor that never had a post cannot be cleared, because announcing it would
	// claim a fight that never happened.
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, {
		MakeFloorEntry(0, 0, 1),
		MakeFloorEntry(2, 0, 2),
		MakeFloorEntry(5, 0, 0) });
	TestEqual(TEXT("A declared floor with no post is never announced"),
		Probe->FloorClearedCount, 1);

	// Floor 2's replacement died, so the same call must now announce floor 2 and only floor 2.
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, Fight::BetweenWaves());
	TestEqual(TEXT("Floor 2 is announced once it too loses its last defender"),
		Probe->FloorClearedCount, 2);
	TestEqual(TEXT("The second announcement names floor 2"),
		Probe->ClearedFloors.Num() == 2 ? Probe->ClearedFloors[1] : INDEX_NONE, 2);

	TestEqual(TEXT("A floor-clear announcement is not a defender arrival"),
		Probe->DefenderSpawnedCount, 0);

	Fixture.TearDown();
	return true;
}

/**
 * A defender arrival carries the floor the guard was staged on, and the empty-context path the
 * spawn point's abandoned-reserve conclusion uses must still be safe.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorSpawnAnnouncement,
	"TerritoryFramework.Guards.Floors.DefenderSpawnEventCarriesItsFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorSpawnAnnouncement::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorEventTest;
	FFloorFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }

	UTerritoryFloorEventProbe* Probe = NewObject<UTerritoryFloorEventProbe>();
	if (!BindProbe(*Fixture.Place, *Probe, *this)) { Fixture.TearDown(); return false; }

	ATerritoryGuardSpawnPoint* Upper = StandUp(Fixture, TEXT("Upper_A"));
	ATerritoryGuardSpawnPoint* Ground = StandUp(Fixture, TEXT("Ground_A"));
	if (!TestNotNull(TEXT("Floor 2 post stands up"), Upper)
		|| !TestNotNull(TEXT("Ground post stands up"), Ground))
	{
		Fixture.TearDown();
		return false;
	}

	TestEqual(TEXT("A post reports the floor it was bound to"), Upper->GetFloorIndex(), 2);

	AActor* FirstGuard = Fixture.World->SpawnActor<AActor>();
	AActor* SecondGuard = Fixture.World->SpawnActor<AActor>();
	FTFTerritoryFloorEventTestAccess::AnnounceSpawn(*Fixture.Place, FirstGuard, Upper);
	FTFTerritoryFloorEventTestAccess::AnnounceSpawn(*Fixture.Place, SecondGuard, Ground);

	TestEqual(TEXT("Every deployment is announced once"), Probe->DefenderSpawnedCount, 2);
	TestEqual(TEXT("A floor 2 deployment reports floor 2"),
		Probe->SpawnedFloors.Num() == 2 ? Probe->SpawnedFloors[0] : INDEX_NONE, 2);
	TestEqual(TEXT("A ground deployment reports floor zero"),
		Probe->SpawnedFloors.Num() == 2 ? Probe->SpawnedFloors[1] : INDEX_NONE, 0);
	TestEqual(TEXT("The announced guard is the deployed guard"),
		Probe->LastSpawnedGuard.Get(), SecondGuard);

	// A guard deployed without a post is still an arrival, but belongs to no authored floor.
	FTFTerritoryFloorEventTestAccess::AnnounceSpawn(*Fixture.Place, FirstGuard, nullptr);
	TestEqual(TEXT("A deployment with no post reports no floor"),
		Probe->SpawnedFloors.Num() == 3 ? Probe->SpawnedFloors[2] : 0, INDEX_NONE);

	TestEqual(TEXT("A defender arrival is not a floor clear"), Probe->FloorClearedCount, 0);

	// The abandoned-reserve conclusion runs with an empty context by design; it must not crash
	// and must not invent a clear for a floor that still holds a living defender.
	FTFTerritoryFloorEventTestAccess::SetGarrison(*Fixture.Place,
		MakeGarrison(Fight::Defended()));
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, {});
	TestEqual(TEXT("An empty-context conclusion announces nothing on a still-defended floor"),
		Probe->FloorClearedCount, 0);

	Fixture.TearDown();
	return true;
}

/**
 * The story half: a cleared floor runs its authored Narrative events, and their conditions
 * still gate them the way every other Territory story hook is gated.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorClearedEvents,
	"TerritoryFramework.Guards.Floors.ClearedEventRunsAuthoredNarrativeEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorClearedEvents::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorEventTest;
	FFloorFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }

	UTerritoryFloorClearedProbeEvent* Ungated = NewObject<UTerritoryFloorClearedProbeEvent>();
	UTerritoryFloorClearedProbeEvent* Gated = NewObject<UTerritoryFloorClearedProbeEvent>();
	UTerritoryFloorClearedProbeCondition* Blocking =
		NewObject<UTerritoryFloorClearedProbeCondition>();
	Blocking->bPasses = false;
	Gated->Conditions.Add(Blocking);

	// Only floor 2 authors events, so a ground clear must run nothing at all. Floors are
	// declared {0, 2, 5}, so index 1 is floor 2.
	Fixture.Definition->Floors[1].FloorClearedEvents = { Ungated, Gated };
	Fixture.Definition->Floors[0].FloorClearedEvents = {};

	// Ground falls while floor 2 is still defended: the announcement happens, but ground
	// authored nothing, so no event may run.
	FTFTerritoryFloorEventTestAccess::SetGarrison(*Fixture.Place, MakeGarrison({
		MakeFloorEntry(0, 0, 1),
		MakeFloorEntry(2, 2, 2) }));
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, Fight::Defended());
	TestEqual(TEXT("A floor with no authored events runs none"), Ungated->ExecutionCount, 0);

	// Floor 2 falls: the ungated event runs and the blocked one stays suppressed.
	FTFTerritoryFloorEventTestAccess::SetGarrison(*Fixture.Place,
		MakeGarrison(Fight::Exhausted()));
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, {
		MakeFloorEntry(0, 0, 1),
		MakeFloorEntry(2, 2, 2) });
	TestEqual(TEXT("A cleared floor runs its authored event"), Ungated->ExecutionCount, 1);
	TestEqual(TEXT("A failed condition suppresses the cleared-floor event"),
		Gated->ExecutionCount, 0);

	// The condition is the only thing blocking the second event, so passing it must let the
	// event through on the next genuine transition.
	Blocking->bPasses = true;
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, {
		MakeFloorEntry(0, 0, 1),
		MakeFloorEntry(2, 1, 2) });
	TestEqual(TEXT("A passing condition lets the cleared-floor event run"),
		Gated->ExecutionCount, 1);

	Fixture.TearDown();
	return true;
}

/**
 * The Blueprint contract for the two new seams, the shared cleared rule, and the authoring
 * surface a designer uses to hang story off a floor.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorEventContract,
	"TerritoryFramework.Guards.Floors.EventBlueprintContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorEventContract::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorEventTest;
	const UClass* VolumeClass = ATerritoryVolume::StaticClass();

	struct FExpectedDelegate
	{
		const TCHAR* Name;
		int32 NumParams;
	};
	const FExpectedDelegate Expected[] = {
		{ TEXT("OnFloorCleared"), 2 },
		{ TEXT("OnDefenderSpawned"), 3 } };
	for (const FExpectedDelegate& Entry : Expected)
	{
		FMulticastDelegateProperty* Property = FindFProperty<FMulticastDelegateProperty>(
			VolumeClass, Entry.Name);
		TestNotNull(*FString::Printf(TEXT("%s is reflected on the Territory volume"), Entry.Name),
			Property);
		if (!Property) continue;
		TestTrue(*FString::Printf(TEXT("%s is Blueprint-assignable, so a widget can bind it"),
			Entry.Name), Property->HasAnyPropertyFlags(CPF_BlueprintAssignable));
		TestEqual(*FString::Printf(TEXT("%s carries %d parameters"), Entry.Name, Entry.NumParams),
			Property->SignatureFunction ? Property->SignatureFunction->NumParms : -1,
			Entry.NumParams);
	}

	// The cleared rule reaches Blueprint through the library, because UHT does not reflect
	// UFUNCTIONs declared inside a USTRUCT. A widget must read this one rule rather than
	// restating it from the raw counts, or it could disagree with the quest that completes on it.
	const UClass* LibraryClass = UTerritoryBlueprintLibrary::StaticClass();
	UFunction* IsCleared = LibraryClass->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UTerritoryBlueprintLibrary, IsTerritoryFloorCleared));
	TestNotNull(TEXT("The cleared rule is exposed to Blueprint"), IsCleared);
	if (IsCleared)
	{
		TestTrue(TEXT("The cleared rule is a Blueprint-pure query"),
			IsCleared->HasAnyFunctionFlags(FUNC_BlueprintPure));
		TestTrue(TEXT("The cleared rule is static, so a widget needs no library instance"),
			IsCleared->HasAnyFunctionFlags(FUNC_Static));
		TestEqual(TEXT("The cleared rule takes one floor entry and returns a verdict"),
			static_cast<int32>(IsCleared->NumParms), 2);
	}

	UFunction* GetFloorGuards = LibraryClass->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UTerritoryBlueprintLibrary, GetTerritoryFloorGuards));
	TestNotNull(TEXT("A widget can read one floor's replicated guard counts"), GetFloorGuards);
	if (GetFloorGuards)
	{
		TestTrue(TEXT("Reading one floor is a Blueprint-pure query"),
			GetFloorGuards->HasAnyFunctionFlags(FUNC_BlueprintPure));
		TestTrue(TEXT("Reading one floor reports whether the floor exists"),
			GetFloorGuards->GetReturnProperty() != nullptr);
	}

	// Floor story reactions are authoring data on the floor row, the same shape as the
	// Definition's other Narrative event arrays.
	const FArrayProperty* FloorClearedEvents = FindFProperty<FArrayProperty>(
		FTerritoryFloorTemplate::StaticStruct(), TEXT("FloorClearedEvents"));
	TestNotNull(TEXT("A floor row carries its authored cleared events"), FloorClearedEvents);
	if (FloorClearedEvents)
	{
		TestTrue(TEXT("Authored cleared events are editable on the floor row"),
			FloorClearedEvents->HasAnyPropertyFlags(CPF_Edit));
		// UHT puts the Instanced flag on the array's inner property - that is what makes each
		// element an inlined subobject a designer authors in place - and advertises
		// CPF_ContainsInstancedReference on the array itself.
		TestTrue(TEXT("Authored cleared events are instanced, like the Definition's other events"),
			FloorClearedEvents->Inner != nullptr
				&& FloorClearedEvents->Inner->HasAnyPropertyFlags(CPF_InstancedReference));
		TestTrue(TEXT("The floor row advertises that it contains instanced references"),
			FloorClearedEvents->HasAnyPropertyFlags(CPF_ContainsInstancedReference));
	}

	// The Blueprint path must reach the same read model the tests and the event read, and must
	// report "no such floor" rather than handing back a zeroed floor that does not exist.
	FFloorFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }
	FTFTerritoryFloorEventTestAccess::SetGarrison(*Fixture.Place,
		MakeGarrison(Fight::BetweenWaves()));

	FTerritoryFloorSnapshot ReadFloor;
	TestFalse(TEXT("A floor the Territory never declares is reported as absent"),
		UTerritoryBlueprintLibrary::GetTerritoryFloorGuards(
			Fixture.Place, 9, ReadFloor));
	TestTrue(TEXT("A declared floor is found"),
		UTerritoryBlueprintLibrary::GetTerritoryFloorGuards(
			Fixture.Place, 2, ReadFloor));
	TestEqual(TEXT("The Blueprint read returns the floor's replicated reserve count"),
		ReadFloor.ReserveGuards, 1);
	TestFalse(TEXT("A floor still holding a reserve is not cleared through the Blueprint rule"),
		UTerritoryBlueprintLibrary::IsTerritoryFloorCleared(ReadFloor));

	TestTrue(TEXT("An exhausted floor reads cleared through the Blueprint rule"),
		UTerritoryBlueprintLibrary::IsTerritoryFloorCleared(MakeFloorEntry(2, 0, 2)));

	Fixture.TearDown();
	return true;
}

#endif
