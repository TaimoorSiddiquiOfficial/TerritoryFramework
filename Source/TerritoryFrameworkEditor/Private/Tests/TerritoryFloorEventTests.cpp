#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryFloorEventProbe.h"
#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "LevelSequence.h"
#include "Tales/TerritoryStoryEvents.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "UnrealFramework/NarrativeGameState.h"
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

	/**
	 * Register a post the way a streamed level does - through the resolved (weak) array, which is
	 * the call the post's own ownership resolution makes. AttachPost cannot stand in for it:
	 * GuardSpawnPoints holds a strong pointer that keeps a destroyed post readable until GC, so a
	 * post attached there can never model a stream-out.
	 */
	static void RegisterPost(ATerritoryVolume& Territory, ATerritoryGuardSpawnPoint* Post)
	{
		Territory.RegisterResolvedGuardSpawnPoint(Post);
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

	/**
	 * This Territory's clones of one floor's authored cleared events. Null means no clone array
	 * exists for that floor, which is the case for an undeclared floor and for one that authored
	 * no events - the same "nothing to run" state the dispatch treats as a no-op.
	 */
	static const TArray<TObjectPtr<UNarrativeEvent>>* FindFloorClearedEventClones(
		const ATerritoryVolume& Territory, int32 FloorIndex)
	{
		const FTerritoryFloorRuntimeEvents* RuntimeEvents =
			Territory.RuntimeFloorClearedEvents.Find(FloorIndex);
		return RuntimeEvents ? &RuntimeEvents->Events : nullptr;
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
		// A hand-built entry stands in for a producer that has seen this floor's posts, so it
		// states its counts as complete. FTerritoryFloorSnapshot defaults this to false so a real
		// producer that forgets it fails closed, which is why every fixture here has to say so
		// explicitly - the tests that do not are the ones asserting an unloaded floor.
		Entry.bCountsKnown = true;
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
		// Same claim as MakeFloorEntry, at Territory scope: this fixture models a published read
		// model, and the whole-Place AllDefendersDefeated objective requires the flag, so an
		// unset default here would make every whole-Place assertion read as "unknown".
		Snapshot.bCountsKnown = true;
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

	/**
	 * Stand up a post the way a streamed World Partition cell does: a fresh actor registered
	 * through the resolved array, so destroying it models the cell unloading rather than leaving
	 * a strong pointer behind for the rest of the test.
	 */
	ATerritoryGuardSpawnPoint* StreamIn(FFloorFixture& Fixture, const TCHAR* PostID)
	{
		if (!Fixture.IsValid()) return nullptr;
		ATerritoryGuardSpawnPoint* Post =
			NewObject<ATerritoryGuardSpawnPoint>(Fixture.World->PersistentLevel);
		if (!Post) return nullptr;
		Post->SetDefinitionBinding(Fixture.Definition, FName(PostID));
		if (!Post->ApplyTerritoryDefinition()) return nullptr;
		FTFTerritoryFloorEventTestAccess::RegisterPost(*Fixture.Place, Post);
		return Post;
	}

	/** One floor entry out of a snapshot by index, or null when the snapshot has none. */
	const FTerritoryFloorSnapshot* FindFloor(
		const FTerritoryGarrisonSnapshot& Snapshot, int32 FloorIndex)
	{
		return Snapshot.Floors.FindByPredicate(
			[FloorIndex](const FTerritoryFloorSnapshot& Entry)
			{
				return Entry.FloorIndex == FloorIndex;
			});
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
 * The second harm of the same defect, and the reason the completeness gate is a story fix and not
 * only a read-model tidy-up.
 *
 * A floor whose post sits in an unloaded cell contributes no counts. Before the gate that read as
 * "cleared" - authored capacity, every count zero - so the committed read model AND the pre-loss
 * read the death path captures at TerritoryVolume.cpp:2449 both said cleared, the observed-
 * transition compare at :2631 found no transition, and the floor's clear beat was lost for the rest
 * of the campaign. A later death could never recover it, because by then the floor still read
 * cleared. The gate makes the unloaded read *unknown* instead, so the first read that can actually
 * see an empty floor is a real transition and the beat fires there.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorStreamOutKeepsItsBeat,
	"TerritoryFramework.Guards.Floors.StreamedOutFloorKeepsItsClearBeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorStreamOutKeepsItsBeat::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorEventTest;
	FFloorFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }

	UTerritoryFloorEventProbe* Probe = NewObject<UTerritoryFloorEventProbe>();
	if (!BindProbe(*Fixture.Place, *Probe, *this)) { Fixture.TearDown(); return false; }

	// Floor 0 is the one-post floor, so a single stream-out empties its loaded set completely.
	ATerritoryGuardSpawnPoint* GroundA = StreamIn(Fixture, TEXT("Ground_A"));
	if (!TestNotNull(TEXT("The floor's post streams in"), GroundA))
	{
		Fixture.TearDown();
		return false;
	}
	Fixture.Place->RefreshGarrisonSnapshot();
	TestTrue(TEXT("A floor with its post standing knows its counts"),
		FindFloor(Fixture.Place->GetGarrisonSnapshot(), 0)->bCountsKnown);

	// The post's cell streams out. This is the committed read model a death on that floor would
	// capture as its pre-loss read, so the beat's fate is decided here.
	GroundA->Destroy();
	Fixture.Place->RefreshGarrisonSnapshot();
	const FTerritoryGarrisonSnapshot StreamedOut = Fixture.Place->GetGarrisonSnapshot();
	const FTerritoryFloorSnapshot* UnloadedFloor = FindFloor(StreamedOut, 0);
	if (!TestNotNull(TEXT("The streamed-out floor keeps its entry"), UnloadedFloor))
	{
		Fixture.TearDown();
		return false;
	}
	TestEqual(TEXT("The unloaded floor still holds its authored capacity"),
		UnloadedFloor->MaximumGuards, 1);
	TestFalse(TEXT("The unloaded floor's pre-loss read is not cleared"), UnloadedFloor->IsCleared());
	const TArray<FTerritoryFloorSnapshot> FloorsBeforeLoss = StreamedOut.Floors;

	// The cell streams back in with the fight over: the floor is fully loaded, empty, and so
	// genuinely clearable - the state the announcement is decided from.
	ATerritoryGuardSpawnPoint* Restored = StreamIn(Fixture, TEXT("Ground_A"));
	if (!TestNotNull(TEXT("The floor's post streams back in"), Restored))
	{
		Fixture.TearDown();
		return false;
	}
	Fixture.Place->RefreshGarrisonSnapshot();
	// Bound to a named snapshot, not read straight out of the accessor: GetGarrisonSnapshot()
	// returns the read model BY VALUE, so a floor entry taken from its return value dies with the
	// full expression and the assertions below would read freed bytes. The release allocator keeps
	// serving those bytes often enough to look plausible - the reading that exposed it here was an
	// impossible ActiveGuards of 646 on a floor whose post had just been streamed in empty.
	const FTerritoryGarrisonSnapshot Reloaded = Fixture.Place->GetGarrisonSnapshot();
	const FTerritoryFloorSnapshot* ReloadedFloor = FindFloor(Reloaded, 0);
	if (!TestNotNull(TEXT("The reloaded floor has an entry"), ReloadedFloor))
	{
		Fixture.TearDown();
		return false;
	}

	TestTrue(TEXT("Once every post is standing the floor knows its counts again"),
		ReloadedFloor->bCountsKnown);
	TestEqual(TEXT("The reloaded floor keeps its authored capacity"), ReloadedFloor->MaximumGuards, 1);
	TestEqual(TEXT("The reloaded floor has nobody standing"), ReloadedFloor->ActiveGuards, 0);
	TestEqual(TEXT("The reloaded floor holds no reserve"), ReloadedFloor->ReserveGuards, 0);
	TestEqual(TEXT("The reloaded floor queues no replacement"), ReloadedFloor->PendingDeployments, 0);
	TestTrue(TEXT("Once every post is standing the empty floor reads cleared"),
		ReloadedFloor->IsCleared());

	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, FloorsBeforeLoss);
	TestEqual(TEXT("A floor unloaded when its last defender fell still announces its clear"),
		Probe->FloorClearedCount, 1);
	TestEqual(TEXT("The announcement names the floor that was unloaded"),
		Probe->ClearedFloors.Num() == 1 ? Probe->ClearedFloors[0] : INDEX_NONE, 0);

	// The transition is now observed, so a second conclusion stays silent - the deferred beat is
	// announced once, not replayed on every later evaluation.
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, Reloaded.Floors);
	TestEqual(TEXT("The recovered beat is announced exactly once"), Probe->FloorClearedCount, 1);

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

	// The rows above only changed the Definition. A Territory executes its own clones, built when
	// the Definition is applied - the same rule the defender-died and all-defenders-defeated
	// arrays already follow - so the Definition has to be re-applied for the Territory to pick
	// them up. Asserting against the templates instead is what let the shared-template defect
	// pass: one authored object executed for every Territory referencing the Definition, and its
	// outer chain holds no world.
	TestTrue(TEXT("Re-applying the Definition rebuilds the floor event clones"),
		Fixture.Definition->ApplyToTerritory(Fixture.Place));

	const TArray<TObjectPtr<UNarrativeEvent>>* Clones =
		FTFTerritoryFloorEventTestAccess::FindFloorClearedEventClones(*Fixture.Place, 2);
	TestNotNull(TEXT("A floor that authors events has a cloned array for this Territory"), Clones);
	TestNull(TEXT("A floor that authors no events has no cloned array"),
		FTFTerritoryFloorEventTestAccess::FindFloorClearedEventClones(*Fixture.Place, 0));
	TestNull(TEXT("A declared floor with no post and no events has no cloned array"),
		FTFTerritoryFloorEventTestAccess::FindFloorClearedEventClones(*Fixture.Place, 5));
	if (!Clones) { Fixture.TearDown(); return false; }

	TestEqual(TEXT("Both authored events are cloned for this Territory"), Clones->Num(), 2);
	UTerritoryFloorClearedProbeEvent* ClonedUngated = Clones->Num() > 0
		? Cast<UTerritoryFloorClearedProbeEvent>((*Clones)[0]) : nullptr;
	UTerritoryFloorClearedProbeEvent* ClonedGated = Clones->Num() > 1
		? Cast<UTerritoryFloorClearedProbeEvent>((*Clones)[1]) : nullptr;
	TestNotNull(TEXT("The ungated clone is the authored probe type"), ClonedUngated);
	TestNotNull(TEXT("The gated clone is the authored probe type"), ClonedGated);
	if (!ClonedUngated || !ClonedGated) { Fixture.TearDown(); return false; }

	// Ground falls while floor 2 is still defended: the announcement happens, but ground
	// authored nothing, so no event may run.
	FTFTerritoryFloorEventTestAccess::SetGarrison(*Fixture.Place, MakeGarrison({
		MakeFloorEntry(0, 0, 1),
		MakeFloorEntry(2, 2, 2) }));
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, Fight::Defended());
	TestEqual(TEXT("A floor with no authored events runs none"), ClonedUngated->ExecutionCount, 0);

	// Floor 2 falls: the ungated clone runs and the blocked one stays suppressed.
	FTFTerritoryFloorEventTestAccess::SetGarrison(*Fixture.Place,
		MakeGarrison(Fight::Exhausted()));
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, {
		MakeFloorEntry(0, 0, 1),
		MakeFloorEntry(2, 2, 2) });
	TestEqual(TEXT("A cleared floor runs its authored event"), ClonedUngated->ExecutionCount, 1);
	TestEqual(TEXT("A failed condition suppresses the cleared-floor event"),
		ClonedGated->ExecutionCount, 0);

	// The condition is the only thing blocking the second event, so passing it must let the event
	// through on the next genuine transition. The clone owns its own copy of the Instanced
	// condition, so the switch is flipped on that copy: flipping the Definition's template would
	// reach a different object and prove nothing about what actually executed.
	UTerritoryFloorClearedProbeCondition* ClonedCondition = ClonedGated->Conditions.Num() > 0
		? Cast<UTerritoryFloorClearedProbeCondition>(ClonedGated->Conditions[0]) : nullptr;
	TestNotNull(TEXT("The gated clone carries its own copy of the Instanced condition"),
		ClonedCondition);
	if (ClonedCondition) ClonedCondition->bPasses = true;
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, {
		MakeFloorEntry(0, 0, 1),
		MakeFloorEntry(2, 1, 2) });
	TestEqual(TEXT("A passing condition lets the cleared-floor event run"),
		ClonedGated->ExecutionCount, 1);

	// The whole point of the clone: the Definition's own objects are authored templates and
	// nothing may ever execute them. Every transition above ran through the clones, so these two
	// counters are what fail if the dispatch ever goes back to the shared array.
	TestEqual(TEXT("The Definition's ungated cleared event never executes"),
		Ungated->ExecutionCount, 0);
	TestEqual(TEXT("The Definition's gated cleared event never executes"),
		Gated->ExecutionCount, 0);

	Fixture.TearDown();
	return true;
}

/**
 * One Definition, two Territories: each executes its own floor event instance, and each instance
 * resolves the gameplay world from its own outer chain.
 *
 * This is the defect the live floor-staging run exposed. A Definition-owned event is a single
 * shared object, so every Territory referencing the Definition executes that one instance; and
 * its outer chain holds no world, so when the death that empties a floor records no killer the
 * transition context is empty, TerritoryTales::ResolveWorld falls through to the event's own
 * GetWorld(), and a world-dependent beat returns silently with no log line. Reserves dying without
 * a recorded instigator are exactly that case, and a floor cleared by its last reserve is the
 * common way a floor empties.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorClearedEventCloning,
	"TerritoryFramework.Guards.Floors.ClearedEventsAreClonedPerTerritoryAndResolveAWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorClearedEventCloning::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorEventTest;
	FFloorFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }

	UTerritoryFloorClearedProbeEvent* Authored = NewObject<UTerritoryFloorClearedProbeEvent>();
	Fixture.Definition->Floors[1].FloorClearedEvents = { Authored };
	if (!TestTrue(TEXT("The Definition re-applies with the authored floor event"),
		Fixture.Definition->ApplyToTerritory(Fixture.Place)))
	{
		Fixture.TearDown();
		return false;
	}

	// A second Territory on the same Definition, which is the only way to see one shared event
	// object being executed for two Places.
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryProperty* Other = Fixture.World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("A second Place exists"), Other)) { Fixture.TearDown(); return false; }
	if (!TestTrue(TEXT("The same Definition applies to the second Place"),
		Fixture.Definition->ApplyToTerritory(Other)))
	{
		Fixture.TearDown();
		return false;
	}

	const TArray<TObjectPtr<UNarrativeEvent>>* FirstClones =
		FTFTerritoryFloorEventTestAccess::FindFloorClearedEventClones(*Fixture.Place, 2);
	const TArray<TObjectPtr<UNarrativeEvent>>* SecondClones =
		FTFTerritoryFloorEventTestAccess::FindFloorClearedEventClones(*Other, 2);
	TestNotNull(TEXT("The first Place clones the authored floor event"), FirstClones);
	TestNotNull(TEXT("The second Place clones the authored floor event"), SecondClones);
	if (!FirstClones || !SecondClones || FirstClones->IsEmpty() || SecondClones->IsEmpty())
	{
		Fixture.TearDown();
		return false;
	}

	UTerritoryFloorClearedProbeEvent* First = Cast<UTerritoryFloorClearedProbeEvent>(
		(*FirstClones)[0]);
	UTerritoryFloorClearedProbeEvent* Second = Cast<UTerritoryFloorClearedProbeEvent>(
		(*SecondClones)[0]);
	TestNotNull(TEXT("The first Place has its own floor event instance"), First);
	TestNotNull(TEXT("The second Place has its own floor event instance"), Second);
	if (!First || !Second) { Fixture.TearDown(); return false; }

	TestTrue(TEXT("Neither Place executes the Definition's shared event object"),
		First != Authored && Second != Authored);
	TestTrue(TEXT("The two Places do not share one floor event object"), First != Second);
	TestTrue(TEXT("Each clone is outered to its own Territory"),
		First->GetOuter() == Fixture.Place && Second->GetOuter() == Other);

	// The mechanism, stated directly. With no killer on the deciding death the context is empty
	// and world resolution falls through to the event object itself.
	TestNull(TEXT("A Definition-owned floor event resolves no world"),
		TerritoryTales::ResolveWorld(Authored, nullptr, nullptr, nullptr));
	TestNotNull(TEXT("A floor event clone resolves its Territory's world"),
		TerritoryTales::ResolveWorld(First, nullptr, nullptr, nullptr));
	TestTrue(TEXT("The clone resolves the very world its Territory stands in"),
		TerritoryTales::ResolveWorld(First, nullptr, nullptr, nullptr) == Fixture.World);

	// And the two instances are genuinely independent counters, not aliases of one object.
	FTFTerritoryFloorEventTestAccess::SetGarrison(*Fixture.Place,
		MakeGarrison(Fight::Exhausted()));
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place, {
		MakeFloorEntry(0, 0, 1),
		MakeFloorEntry(2, 2, 2) });
	TestEqual(TEXT("The first Place's instance runs for the first Place"),
		First->ExecutionCount, 1);
	TestEqual(TEXT("The second Place's instance does not run for the first Place"),
		Second->ExecutionCount, 0);
	TestEqual(TEXT("The Definition's shared template never executes"),
		Authored->ExecutionCount, 0);

	Fixture.TearDown();
	return true;
}

/**
 * The live defect, end to end: a floor that empties with no recorded killer still plays its
 * authored cutscene.
 *
 * A floor's last defender is usually a reserve walking in, and a reserve death records no killer,
 * so the transition context handed to the cleared-floor events is empty
 * (FTFTerritoryFloorEventTestAccess::ConcludeFight defaults it to exactly that). With an empty
 * context TerritoryTales::ResolveWorld has nothing to prefer and falls through to
 * ContextObject->GetWorld() - which is null for a Definition-owned event, whose outer chain is the
 * authoring DataAsset and a transient package. The beat then returns before it resolves an
 * audience, with no log line, so the floor silently loses its story.
 *
 * This is the one assertion in the batch that proves the consequence a player would notice: a
 * sequence actor exists. The world-resolution and single-instance assertions above prove the
 * mechanism; this proves the mechanism is wired to the vendor factory.
 *
 * A cutscene audience is resolved through the world's controller iterator, and that iterator is
 * only populated once actors are initialized for play (AController::PostInitializeComponents ->
 * UWorld::AddController), so this world is built the way the cinematic tests build theirs rather
 * than the bare world the other floor tests use.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorClearedEventPlaysItsCutscene,
	"TerritoryFramework.Guards.Floors.ClearedEventPlaysItsAuthoredCutscene",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorClearedEventPlaysItsCutscene::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorEventTest;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Floor cutscene world exists"), World)) return false;

	auto TearDownWorld = [World]()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};

	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	ANarrativeGameState* Clock = NewObject<ANarrativeGameState>(World->PersistentLevel);
	Clock->SetRole(ROLE_Authority);
	World->SetGameState(Clock);
	World->InitializeActorsForPlay(FURL());

	// A viewer whose pawn carries the faction the floor event addresses. The pawn is what real
	// players put their Narrative membership on.
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	if (!TestTrue(TEXT("The floor cutscene audience faction exists"), Heroes.IsValid()))
	{
		TearDownWorld();
		return false;
	}
	APlayerController* Viewer = World->SpawnActor<APlayerController>();
	ATerritoryGuardCharacter* Body = World->SpawnActor<ATerritoryGuardCharacter>();
	if (!TestNotNull(TEXT("A viewer controller exists"), Viewer)
		|| !TestNotNull(TEXT("The viewer has a pawn"), Body))
	{
		TearDownWorld();
		return false;
	}
	if (UGameInstance* Instance = World->GetGameInstance())
	{
		// ULocalPlayer is ClassWithin=Engine, so its outer must be the engine even though the
		// game instance owns it.
		if (ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine))
		{
			Instance->AddLocalPlayer(LocalPlayer, FPlatformUserId::CreateFromInternalId(0));
			Viewer->SetPlayer(LocalPlayer);
		}
	}
	Viewer->Possess(Body);
	if (INarrativeTeamAgentInterface* TeamAgent = Cast<INarrativeTeamAgentInterface>(Body))
	{
		TeamAgent->AddFaction(Heroes);
	}

	FFloorFixture Fixture;
	Fixture.World = World;
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	Fixture.Place = World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Floor cutscene Place exists"), Fixture.Place))
	{
		TearDownWorld();
		return false;
	}

	Fixture.Definition = NewObject<UTerritoryPlaceDefinition>();
	Fixture.Definition->TerritoryTag = TestTag();
	Fixture.Definition->DisplayName = FText::FromString(TEXT("Blacksmith"));
	Fixture.Definition->StableTerritoryGUID = FGuid::NewGuid();
	Fixture.Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Fixture.Definition->Floors = { MakeFloor(2) };
	Fixture.Definition->GuardPosts = { MakePost(TEXT("Upper_A"), 2) };

	// The cutscene is authored on the Definition, outered to it, exactly as DA_Place_Blacksmith
	// carries the one the live run inspected. Applying the Definition is what gives the Territory
	// its own clone; without that step the Place would still be holding the previous configuration.
	ULevelSequence* Sequence = NewObject<ULevelSequence>(World);
	Sequence->Initialize();
	UTerritoryPlayCutsceneEvent* Cutscene =
		NewObject<UTerritoryPlayCutsceneEvent>(Fixture.Definition);
	Cutscene->AudienceFaction = Heroes;
	Cutscene->CutsceneSequence = Sequence;
	Fixture.Definition->Floors[0].FloorClearedEvents = { Cutscene };

	if (!TestTrue(TEXT("The cutscene Definition applies to its Place"),
		Fixture.Definition->ApplyToTerritory(Fixture.Place)))
	{
		TearDownWorld();
		return false;
	}

	StandUp(Fixture, TEXT("Upper_A"));

	// Floor 2 empties with nobody left alive, one reserve spent and no deployment pending: a real
	// transition from "still defended" to cleared, driven through the empty-context path.
	FTFTerritoryFloorEventTestAccess::SetGarrison(*Fixture.Place,
		MakeGarrison({ MakeFloorEntry(2, 0, 1) }));
	FTFTerritoryFloorEventTestAccess::ConcludeFight(*Fixture.Place,
		{ MakeFloorEntry(2, 1, 1) });

	int32 CutsceneCount = 0;
	for (TActorIterator<ANarrativeLevelSequenceActor> It(World); It; ++It)
	{
		++CutsceneCount;
	}
	TestEqual(TEXT("A floor cleared with no recorded killer still plays its authored cutscene"),
		CutsceneCount, 1);

	TearDownWorld();
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
