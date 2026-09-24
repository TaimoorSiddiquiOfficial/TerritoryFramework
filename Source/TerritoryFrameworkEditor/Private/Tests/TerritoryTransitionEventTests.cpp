#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TerritoryAuditEventProbe.h"
#include "TerritoryTransitionFrameProbe.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTransitionEventReconciliation,
	"TerritoryFramework.Capture.Regression.EventsObserveReconciledDependencies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTransitionEventReconciliation::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Transition test world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Property = World->SpawnActor<ATerritoryProperty>();
	Property->SetActorGUID_Implementation(FGuid(923, 2026, 52, 1));
	auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	FTerritoryOwnershipData Claimed = Property->GetOwnershipData();
	Claimed.State = ETerritoryState::Claimed;
	Claimed.OwningFaction = Heroes;
	Claimed.ControlProgress = 1.f;
	Claimed.DesiredGuardCount = 0;
	TestTrue(TEXT("Initial owner is committed"), Property->CommitOwnershipData(Claimed));
	Property->SetUpgradeLevel(2);
	TestEqual(TEXT("Fixture has an upgrade to reset"), Property->GetUpgradeLevel(), 2);
	auto* CaptureEvent = NewObject<UTerritoryAuditNarrativeEvent>(Property);
	FNarrativeActorRecord CapturedRecord;
	int32 CaptureEventCount = 0;
	CaptureEvent->Callback = [&]()
	{
		++CaptureEventCount;
		TestEqual(TEXT("Tales sees the committed new owner"), Property->GetOwningFaction(), Bandits);
		TestEqual(TEXT("Tales sees reset upgrades in both tenure events"), Property->GetUpgradeLevel(), 0);
		TestTrue(TEXT("An event can save the reconciled property"), Save->CreateActorRecord(Property, CapturedRecord));
		TestFalse(TEXT("Transition callbacks cannot commit competing state"),
			Property->CommitOwnershipData(Claimed));
	};
	auto& Config = Property->RuntimeStateConfigs.FindOrAdd(ETerritoryState::Claimed);
	Config.EntryEvents = {CaptureEvent};
	Config.ExitEvents = {CaptureEvent};
	FTerritoryOwnershipData Captured = Property->GetOwnershipData();
	Captured.OwningFaction = Bandits;
	TestTrue(TEXT("A direct Claimed-to-Claimed handover completes"), Property->CommitOwnershipData(Captured));
	TestEqual(TEXT("Both ownership tenure events run once"), CaptureEventCount, 2);
	Property->RuntimeStateConfigs.Empty();
	Property->SetUpgradeLevel(3);
	Save->LoadActorFromRecord(Property, CapturedRecord);
	TestEqual(TEXT("Saving inside a Tales event preserves reset upgrades on reload"), Property->GetUpgradeLevel(), 0);
	TestEqual(TEXT("Saving inside a Tales event preserves the new owner"), Property->GetOwningFaction(), Bandits);

	auto AddGuard = [&]()
	{
		auto* Guard = World->SpawnActor<ATerritoryGuardCharacter>();
		Property->SpawnedGuards.Add(Guard);
		Property->RegisterDefender(Guard);
		Property->RefreshGarrisonSnapshot();
		return Guard;
	};
	auto* Guard = AddGuard();
	TestEqual(TEXT("Fixture has one active guard before locking"), Property->GetSpawnedGuardCount(), 1);
	auto* LockEvent = NewObject<UTerritoryAuditNarrativeEvent>(Property);
	auto* UnlockEvent = NewObject<UTerritoryAuditNarrativeEvent>(Property);
	int32 LockEvents = 0;
	int32 UnlockEvents = 0;
	FNarrativeActorRecord LockedRecord;
	LockEvent->Callback = [&]()
	{
		++LockEvents;
		TestTrue(TEXT("Lock event sees locked availability"), Property->IsLocked());
		TestEqual(TEXT("Lock event sees reconciled guard count"), Property->GetSpawnedGuardCount(), 0);
		TestEqual(TEXT("Lock event sees no retired defender registrations"), Property->GetRegisteredDefenders().Num(), 0);
		TestTrue(TEXT("The retired guard is gone before the lock event"),
			!IsValid(Guard) || Guard->IsActorBeingDestroyed());
		TestTrue(TEXT("Lock event can save the reconciled actor"), Save->CreateActorRecord(Property, LockedRecord));
	};
	UnlockEvent->Callback = [&]()
	{
		++UnlockEvents;
		TestFalse(TEXT("Unlock event sees unlocked availability"), Property->IsLocked());
	};
	auto& LockConfig = Property->RuntimeStateConfigs.FindOrAdd(ETerritoryState::Locked);
	LockConfig.EntryEvents = {LockEvent};
	LockConfig.ExitEvents = {UnlockEvent};
	TestTrue(TEXT("Lock commit completes"), Property->LockTerritoryWithContext(FText(), FTerritoryTransitionContext()));
	TestEqual(TEXT("Lock event runs once"), LockEvents, 1);
	TestTrue(TEXT("Unlock commit completes"), Property->TryUnlockWithContext(FTerritoryTransitionContext(), true));
	TestEqual(TEXT("Unlock event runs once"), UnlockEvents, 1);
	Property->RuntimeStateConfigs.Empty();
	Save->LoadActorFromRecord(Property, LockedRecord);
	TestTrue(TEXT("Save inside lock event restores locked availability"), Property->IsLocked());
	TestEqual(TEXT("Save inside lock event does not restore retired guards"), Property->GetSpawnedGuardCount(), 0);

	Property->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client cannot unlock or reconcile server state"),
		Property->TryUnlockWithContext(FTerritoryTransitionContext(), true));
	TestTrue(TEXT("Rejected client request preserves availability"), Property->IsLocked());
	Property->SetRole(ROLE_Authority);
	Property->TryUnlockWithContext(FTerritoryTransitionContext(), true);

	// Guard retirement invokes external Native callbacks. A nested campaign
	// load must invalidate the old commit before any old transition events run.
	FNarrativeActorRecord BeforeLock;
	Save->CreateActorRecord(Property, BeforeLock);
	Guard = AddGuard();
	auto* Probe = NewObject<UTerritoryAuditEventProbe>();
	Probe->DestroyedCallback = [&](AActor*) { Save->LoadActorFromRecord(Property, BeforeLock); };
	Guard->OnDestroyed.AddDynamic(Probe, &UTerritoryAuditEventProbe::ActorDestroyed);
	Property->RuntimeStateConfigs.FindOrAdd(ETerritoryState::Locked).EntryEvents = {LockEvent};
	TestFalse(TEXT("A campaign load during reconciliation supersedes the lock"),
		Property->LockTerritoryWithContext(FText(), FTerritoryTransitionContext()));
	TestEqual(TEXT("Superseded lock does not emit another Tales event"), LockEvents, 1);
	TestFalse(TEXT("Superseded lock preserves the loaded availability"), Property->IsLocked());
	Probe->DestroyedCallback = nullptr;
	Property->RuntimeStateConfigs.Empty();
	TestTrue(TEXT("A later valid commit still works after interrupted reconciliation"),
		Property->LockTerritoryWithContext(FText(), FTerritoryTransitionContext()));
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Deferred ancestor transition frames
//
// A child's commit publishes its summary part-way through, and that publish reconciles
// the child's loaded ancestors immediately. The ancestor therefore committed - and fired
// its own City/District events - while the child still had unreconciled guards and
// upgrade state and had not yet run its own state events. The fix defers only the
// ancestor's volume commit to the end of the outermost transition frame, so the ancestor
// still commits once, through the one existing path, with the live child context.
//
// These tests build a real loaded hierarchy in an unbegun world. BeginPlay is what binds
// an ancestor to its loaded children's ownership delegates, so in an unbegun world the
// WorldState's own ancestor reconcile is the ONLY path that can commit a loaded ancestor:
// the deferral is exercised rather than shadowed by the delegate cascade. The queue is
// read through FTFTransitionFrameProbe, the friend seam declared in TerritoryWorldState.h.
// ═══════════════════════════════════════════════════════════════════════════════

namespace
{
	/** Authored definitions plus the loaded actors that mirror them. */
	struct FFrameFixture
	{
		UWorld* World = nullptr;
		UTerritoryCityDefinition* City = nullptr;
		UTerritoryDistrictDefinition* District = nullptr;
		UTerritoryPlaceDefinition* Place = nullptr;
		ATerritoryWorldState* State = nullptr;
		ATerritoryCity* LoadedCity = nullptr;
		ATerritoryDistrict* LoadedDistrict = nullptr;
		ATerritoryProperty* LoadedPlace = nullptr;
		FGameplayTag CityTag;
		FGameplayTag DistrictTag;
		FGameplayTag PlaceTag;
		FGameplayTag Heroes;
		FGameplayTag Bandits;
	};

	bool BuildLoadedHierarchy(FAutomationTestBase& Test, FFrameFixture& F, bool bIncludeCity)
	{
		const auto Tag = [](const TCHAR* Value)
		{ return FGameplayTag::RequestGameplayTag(Value); };
		F.Heroes = Tag(TEXT("Narrative.Factions.Heroes"));
		F.Bandits = Tag(TEXT("Narrative.Factions.Bandits"));
		F.CityTag = Tag(TEXT("Territory.HavenReach"));
		F.DistrictTag = Tag(TEXT("Territory.HavenReach.MarketSquare"));
		F.PlaceTag = Tag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));

		F.World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!Test.TestNotNull(TEXT("Transition frame world exists"), F.World)) return false;
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(F.World);

		F.Place = NewObject<UTerritoryPlaceDefinition>();
		F.District = NewObject<UTerritoryDistrictDefinition>();
		F.Place->TerritoryTag = F.PlaceTag;
		F.District->TerritoryTag = F.DistrictTag;
		F.Place->StableTerritoryGUID = FGuid::NewGuid();
		F.District->StableTerritoryGUID = FGuid::NewGuid();
		F.Place->InitialState = ETerritoryInitialState::Unclaimed;
		F.District->InitialState = ETerritoryInitialState::Unclaimed;
		F.Place->InitialAvailability = ETerritoryAvailability::Unlocked;
		F.District->InitialAvailability = ETerritoryAvailability::Unlocked;
		F.Place->InitialGuardCount = 0;
		F.District->InitialGuardCount = 0;
		// ApplyToTerritory copies this onto the Property, and SetUpgradeLevel clamps to it.
		F.Place->MaxUpgradeLevel = 3;
		F.District->Places = {F.Place};
		if (bIncludeCity)
		{
			F.City = NewObject<UTerritoryCityDefinition>();
			F.City->TerritoryTag = F.CityTag;
			F.City->StableTerritoryGUID = FGuid::NewGuid();
			F.City->InitialState = ETerritoryInitialState::Unclaimed;
			F.City->InitialAvailability = ETerritoryAvailability::Unlocked;
			F.City->InitialGuardCount = 0;
			F.City->Districts = {F.District};
			F.City->RefreshHierarchyLinks();
		}
		else
		{
			F.District->RefreshHierarchyLinks();
		}

		F.State = F.World->SpawnActor<ATerritoryWorldState>();
		if (bIncludeCity) F.State->CampaignCities = {F.City};

		UTerritoryRegistrySubsystem* Registry =
			F.World->GetSubsystem<UTerritoryRegistrySubsystem>();
		if (!Test.TestNotNull(TEXT("Territory registry exists"), Registry)) return false;

		// Child before ancestor, deliberately: see the block comment above.
		F.LoadedPlace = F.World->SpawnActor<ATerritoryProperty>();
		F.Place->ApplyToTerritory(F.LoadedPlace);
		Registry->RegisterTerritory(F.LoadedPlace);
		F.LoadedDistrict = F.World->SpawnActor<ATerritoryDistrict>();
		F.District->ApplyToTerritory(F.LoadedDistrict);
		Registry->RegisterTerritory(F.LoadedDistrict);
		if (bIncludeCity)
		{
			F.LoadedCity = F.World->SpawnActor<ATerritoryCity>();
			F.City->ApplyToTerritory(F.LoadedCity);
			Registry->RegisterTerritory(F.LoadedCity);
		}
		F.State->RefreshStrategicDirectory();
		return true;
	}

	/**
	 * One authoritative commit on the child, with the fixture owning every field the
	 * assertions read. DesiredGuardCount is pinned to zero so the guard lifecycle is
	 * deterministic: an owner change despawns and, at zero desired, spawns nothing.
	 */
	bool CommitChildOwner(FFrameFixture& F, FGameplayTag Owner, int32 DesiredGuards)
	{
		FTerritoryOwnershipData Data = F.LoadedPlace->GetOwnershipData();
		Data.OwningFaction = Owner;
		Data.State = Owner.IsValid() ? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
		Data.ControlProgress = Owner.IsValid() ? 1.f : 0.f;
		Data.DesiredGuardCount = DesiredGuards;
		return F.LoadedPlace->CommitOwnershipData(Data);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTransitionFrameDeferral,
	"TerritoryFramework.Capture.Regression.AncestorEventsObserveTheSourcesReconciledState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTransitionFrameDeferral::RunTest(const FString& Parameters)
{
	FFrameFixture F;
	if (!BuildLoadedHierarchy(*this, F, /*bIncludeCity=*/false)) return false;
	ON_SCOPE_EXIT { F.World->DestroyWorld(false); GEngine->DestroyWorldContext(F.World); };
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);

	// Settle. This commit itself goes through the frame, so it is also the premise that a
	// normal transition leaves nothing behind.
	TestTrue(TEXT("Premise: the settling commit completes"), CommitChildOwner(F, F.Heroes, 0));
	TestEqual(TEXT("Premise: the loaded District follows its settled Place"),
		F.LoadedDistrict->GetOwningFaction(), F.Heroes);
	TestEqual(TEXT("Premise: the frame is closed after a normal commit"),
		FTFTransitionFrameProbe::Depth(F.State), 0);
	TestEqual(TEXT("Premise: nothing is left queued after a normal commit"),
		FTFTransitionFrameProbe::Queued(F.State), 0);

	auto* Guard = F.World->SpawnActor<ATerritoryGuardCharacter>();
	FTFTransitionFrameProbe::AddGuard(F.LoadedPlace, Guard);
	TestEqual(TEXT("Premise: the child holds one guard"), F.LoadedPlace->GetSpawnedGuardCount(), 1);
	F.LoadedPlace->SetUpgradeLevel(2);
	TestEqual(TEXT("Premise: the child has an upgrade to reset"), F.LoadedPlace->GetUpgradeLevel(), 2);

	int32 AncestorEvents = 0;
	int32 ObservedUpgrade = INDEX_NONE;
	int32 ObservedGuards = INDEX_NONE;
	FGameplayTag ObservedOwner;
	auto* DistrictEvent = NewObject<UTerritoryAuditNarrativeEvent>(F.LoadedDistrict);
	DistrictEvent->Callback = [&]()
	{
		++AncestorEvents;
		ObservedOwner = F.LoadedPlace->GetOwningFaction();
		ObservedUpgrade = F.LoadedPlace->GetUpgradeLevel();
		ObservedGuards = F.LoadedPlace->GetSpawnedGuardCount();
	};
	FTFTransitionFrameProbe::InstallStateEvent(F.LoadedDistrict, ETerritoryState::Claimed, true, DistrictEvent);

	TestTrue(TEXT("The capture completes"), CommitChildOwner(F, F.Bandits, 0));

	TestEqual(TEXT("The ancestor event ran exactly once"), AncestorEvents, 1);
	TestEqual(TEXT("The ancestor event observes the child's committed new owner"),
		ObservedOwner, F.Bandits);
	// The discriminator. Committed mid-child-transition, the ancestor would read the
	// upgrade the child had before its own ownership-dependent reconciliation ran.
	TestEqual(TEXT("The ancestor event observes the child's cleared upgrade level"),
		ObservedUpgrade, 0);
	TestEqual(TEXT("The ancestor event observes the child's post-despawn guard count"),
		ObservedGuards, 0);
	TestEqual(TEXT("The ancestor event observes the same child guard state the transition settles on"),
		ObservedGuards, F.LoadedPlace->GetSpawnedGuardCount());
	TestEqual(TEXT("The deferred ancestor still commits"), F.LoadedDistrict->GetOwningFaction(),
		F.Bandits);
	TestEqual(TEXT("The frame closed after the deferred commit"),
		FTFTransitionFrameProbe::Depth(F.State), 0);
	TestEqual(TEXT("The deferred queue drained"), FTFTransitionFrameProbe::Queued(F.State), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTransitionFrameExactlyOnce,
	"TerritoryFramework.Capture.Regression.AncestorEventsStillRunExactlyOnceOnADeferredCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTransitionFrameExactlyOnce::RunTest(const FString& Parameters)
{
	FFrameFixture F;
	if (!BuildLoadedHierarchy(*this, F, /*bIncludeCity=*/false)) return false;
	ON_SCOPE_EXIT { F.World->DestroyWorld(false); GEngine->DestroyWorldContext(F.World); };
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);

	TestTrue(TEXT("Premise: the settling commit completes"), CommitChildOwner(F, F.Heroes, 0));

	TArray<FString> Trace;
	int32 ChildEvents = 0;
	int32 AncestorEvents = 0;
	FGameplayTag AncestorSeenByChild;
	FGameplayTag ChildSeenByAncestor;
	auto* ChildEvent = NewObject<UTerritoryAuditNarrativeEvent>(F.LoadedPlace);
	ChildEvent->Callback = [&]()
	{
		++ChildEvents;
		AncestorSeenByChild = F.LoadedDistrict->GetOwningFaction();
		Trace.Add(TEXT("child"));
	};
	auto* DistrictEvent = NewObject<UTerritoryAuditNarrativeEvent>(F.LoadedDistrict);
	DistrictEvent->Callback = [&]()
	{
		++AncestorEvents;
		ChildSeenByAncestor = F.LoadedPlace->GetOwningFaction();
		Trace.Add(TEXT("ancestor"));
	};
	FTFTransitionFrameProbe::InstallStateEvent(F.LoadedPlace, ETerritoryState::Claimed, true, ChildEvent);
	FTFTransitionFrameProbe::InstallStateEvent(F.LoadedDistrict, ETerritoryState::Claimed, true, DistrictEvent);

	TestTrue(TEXT("The capture completes"), CommitChildOwner(F, F.Bandits, 0));

	TestEqual(TEXT("The child's own event ran exactly once"), ChildEvents, 1);
	TestEqual(TEXT("The ancestor's event ran exactly once"), AncestorEvents, 1);
	// The claim, stated directly: the source transition has not yet been observed by its
	// ancestor when the source's own event runs.
	TestEqual(TEXT("The child's event observes the ancestor still on the old owner"),
		AncestorSeenByChild, F.Heroes);
	TestEqual(TEXT("The ancestor's event observes the child on the new owner"),
		ChildSeenByAncestor, F.Bandits);
	TestEqual(TEXT("The child's events run before the ancestor's, in that order"),
		FString::Join(Trace, TEXT("|")), FString(TEXT("child|ancestor")));
	TestEqual(TEXT("The deferred ancestor committed"), F.LoadedDistrict->GetOwningFaction(),
		F.Bandits);
	TestEqual(TEXT("The queue drained after the single deferred commit"),
		FTFTransitionFrameProbe::Queued(F.State), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTransitionFrameNestedTwoLevel,
	"TerritoryFramework.Capture.Regression.DeferredAncestorReconcileSurvivesANestedTwoLevelCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTransitionFrameNestedTwoLevel::RunTest(const FString& Parameters)
{
	FFrameFixture F;
	if (!BuildLoadedHierarchy(*this, F, /*bIncludeCity=*/true)) return false;
	ON_SCOPE_EXIT { F.World->DestroyWorld(false); GEngine->DestroyWorldContext(F.World); };
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);

	TestTrue(TEXT("Premise: the settling commit completes"), CommitChildOwner(F, F.Heroes, 0));
	TestEqual(TEXT("Premise: the loaded City follows its settled Place"),
		F.LoadedCity->GetOwningFaction(), F.Heroes);
	TestEqual(TEXT("Premise: nothing is left queued after a normal commit"),
		FTFTransitionFrameProbe::Queued(F.State), 0);

	TArray<FString> Trace;
	int32 PlaceEvents = 0;
	int32 DistrictEvents = 0;
	int32 CityEvents = 0;
	FGameplayTag DistrictSeenByCity;
	auto* PlaceEvent = NewObject<UTerritoryAuditNarrativeEvent>(F.LoadedPlace);
	PlaceEvent->Callback = [&]() { ++PlaceEvents; Trace.Add(TEXT("place")); };
	auto* DistrictEvent = NewObject<UTerritoryAuditNarrativeEvent>(F.LoadedDistrict);
	DistrictEvent->Callback = [&]() { ++DistrictEvents; Trace.Add(TEXT("district")); };
	auto* CityEvent = NewObject<UTerritoryAuditNarrativeEvent>(F.LoadedCity);
	CityEvent->Callback = [&]()
	{
		++CityEvents;
		DistrictSeenByCity = F.LoadedDistrict->GetOwningFaction();
		Trace.Add(TEXT("city"));
	};
	FTFTransitionFrameProbe::InstallStateEvent(F.LoadedPlace, ETerritoryState::Claimed, true, PlaceEvent);
	FTFTransitionFrameProbe::InstallStateEvent(F.LoadedDistrict, ETerritoryState::Claimed, true, DistrictEvent);
	FTFTransitionFrameProbe::InstallStateEvent(F.LoadedCity, ETerritoryState::Claimed, true, CityEvent);

	TestTrue(TEXT("The capture completes"), CommitChildOwner(F, F.Bandits, 0));

	TestEqual(TEXT("Each level of the cascade fired exactly once: Place"), PlaceEvents, 1);
	TestEqual(TEXT("Each level of the cascade fired exactly once: District"), DistrictEvents, 1);
	TestEqual(TEXT("Each level of the cascade fired exactly once: City"), CityEvents, 1);
	// Two queued entries named the City - one for the child it was told about directly and one
	// queued by the District's own nested commit - and the City still committed once.
	TestEqual(TEXT("The two-level cascade runs bottom-up, each level once"),
		FString::Join(Trace, TEXT("|")), FString(TEXT("place|district|city")));
	TestEqual(TEXT("The City observes a District that has already committed"),
		DistrictSeenByCity, F.Bandits);
	TestEqual(TEXT("The whole hierarchy settled on the new owner"),
		F.LoadedCity->GetOwningFaction(), F.Bandits);
	TestEqual(TEXT("The frame closed after the nested cascade"),
		FTFTransitionFrameProbe::Depth(F.State), 0);
	TestEqual(TEXT("The deferred queue drained across both levels"),
		FTFTransitionFrameProbe::Queued(F.State), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTransitionFrameFailurePaths,
	"TerritoryFramework.Capture.Regression.DeferredAncestorReconcileFailurePaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTransitionFrameFailurePaths::RunTest(const FString& Parameters)
{
	FFrameFixture F;
	if (!BuildLoadedHierarchy(*this, F, /*bIncludeCity=*/false)) return false;
	ON_SCOPE_EXIT { F.World->DestroyWorld(false); GEngine->DestroyWorldContext(F.World); };
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);

	UTerritoryRegistrySubsystem* Registry = F.World->GetSubsystem<UTerritoryRegistrySubsystem>();
	TestTrue(TEXT("Premise: the settling commit completes"), CommitChildOwner(F, F.Heroes, 0));

	// ── An early return false inside the frame ────────────────────────────────
	// Guard retirement invokes external Native callbacks. A nested campaign load there
	// invalidates the commit, which returns false several statements early. The frame scope
	// is RAII precisely so that path still closes the frame and still drains it.
	auto* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>();
	auto* Reborn = F.World->SpawnActor<ATerritoryGuardCharacter>();
	TestTrue(TEXT("Premise: the child takes a live guard"),
		FTFTransitionFrameProbe::AddGuard(F.LoadedPlace, Reborn));
	TestEqual(TEXT("Premise: the child holds one guard"), F.LoadedPlace->GetSpawnedGuardCount(), 1);
	FNarrativeActorRecord BeforeCapture;
	TestTrue(TEXT("Premise: the pre-capture record is written"),
		Save->CreateActorRecord(F.LoadedPlace, BeforeCapture));
	auto* Probe = NewObject<UTerritoryAuditEventProbe>();
	Probe->DestroyedCallback = [&](AActor*)
	{ Save->LoadActorFromRecord(F.LoadedPlace, BeforeCapture); };
	Reborn->OnDestroyed.AddDynamic(Probe, &UTerritoryAuditEventProbe::ActorDestroyed);

	// Asserted rather than inferred. The nested load sets Ar.ArIsSaveGame before Actor->Serialize
	// (NarrativeSaveSubsystem.cpp:749-777), which bumps GarrisonLoadGeneration
	// (TerritoryVolume.cpp:817) while this commit holds an earlier CommitLoadGeneration - so the
	// commit really does take one of the four early return-false paths, and the frame assertions
	// below are about that path rather than about a commit that quietly succeeded.
	TestFalse(TEXT("A nested campaign load makes the commit return false early"),
		CommitChildOwner(F, F.Bandits, 0));
	TestEqual(TEXT("A superseded commit does not leave the child on the new owner"),
		F.LoadedPlace->GetOwningFaction(), F.Heroes);
	TestEqual(TEXT("An early return false still closes the frame"),
		FTFTransitionFrameProbe::Depth(F.State), 0);
	TestEqual(TEXT("An early return false still drains the deferred queue"),
		FTFTransitionFrameProbe::Queued(F.State), 0);
	// The drain re-resolves the child by tag, finds it back on its old owner, and commits
	// nothing. That is the whole reason the queue holds tags rather than captured state.
	TestEqual(TEXT("The ancestor committed nothing for a superseded capture"),
		F.LoadedDistrict->GetOwningFaction(), F.Heroes);
	TestTrue(TEXT("The frame never drains while a drain owns the queue"),
		!FTFTransitionFrameProbe::bDraining(F.State));
	Probe->DestroyedCallback = nullptr;

	// ── An ancestor destroyed by a callback before the drain ──────────────────
	ATerritoryDistrict* Doomed = F.LoadedDistrict;
	auto* Destroyer = NewObject<UTerritoryAuditNarrativeEvent>(F.LoadedPlace);
	Destroyer->Callback = [&]()
	{
		// A real Narrative condition can destroy actors synchronously mid-commit.
		Registry->UnregisterTerritory(Doomed);
		Doomed->Destroy();
	};
	FTFTransitionFrameProbe::InstallStateEvent(F.LoadedPlace, ETerritoryState::Claimed, true,
		Destroyer);

	TestTrue(TEXT("A capture whose ancestor is destroyed mid-frame still completes"),
		CommitChildOwner(F, F.Bandits, 0));
	TestTrue(TEXT("The destroyed ancestor is gone"), !IsValid(Doomed));
	TestEqual(TEXT("The frame still closed after the ancestor was destroyed"),
		FTFTransitionFrameProbe::Depth(F.State), 0);
	TestEqual(TEXT("The queued entry drained into nothing rather than dangling"),
		FTFTransitionFrameProbe::Queued(F.State), 0);
	TestEqual(TEXT("The child still committed with no ancestor left to notify"),
		F.LoadedPlace->GetOwningFaction(), F.Bandits);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTransitionFrameDeterminism,
	"TerritoryFramework.Capture.Regression.DeferredAncestorReconcileIsDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTransitionFrameDeterminism::RunTest(const FString& Parameters)
{
	FFrameFixture F;
	if (!BuildLoadedHierarchy(*this, F, /*bIncludeCity=*/true)) return false;
	ON_SCOPE_EXIT { F.World->DestroyWorld(false); GEngine->DestroyWorldContext(F.World); };
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);

	TestTrue(TEXT("Premise: the settling commit completes"), CommitChildOwner(F, F.Heroes, 0));

	TArray<FString> Trace;
	auto* PlaceEvent = NewObject<UTerritoryAuditNarrativeEvent>(F.LoadedPlace);
	PlaceEvent->Callback = [&]() { Trace.Add(TEXT("place")); };
	auto* DistrictEvent = NewObject<UTerritoryAuditNarrativeEvent>(F.LoadedDistrict);
	DistrictEvent->Callback = [&]() { Trace.Add(TEXT("district")); };
	auto* CityEvent = NewObject<UTerritoryAuditNarrativeEvent>(F.LoadedCity);
	CityEvent->Callback = [&]() { Trace.Add(TEXT("city")); };
	FTFTransitionFrameProbe::InstallStateEvent(F.LoadedPlace, ETerritoryState::Claimed, true, PlaceEvent);
	FTFTransitionFrameProbe::InstallStateEvent(F.LoadedDistrict, ETerritoryState::Claimed, true, DistrictEvent);
	FTFTransitionFrameProbe::InstallStateEvent(F.LoadedCity, ETerritoryState::Claimed, true, CityEvent);

	const FString Expected(TEXT("place|district|city"));
	TSet<FString> DistinctTraces;
	for (int32 Iteration = 0; Iteration < 10; ++Iteration)
	{
		const FGameplayTag Target = (Iteration % 2 == 0) ? F.Bandits : F.Heroes;
		Trace.Reset();
		TestTrue(FString::Printf(TEXT("Iteration %d commits"), Iteration),
			CommitChildOwner(F, Target, 0));
		const FString Joined = FString::Join(Trace, TEXT("|"));
		TestEqual(FString::Printf(TEXT("Iteration %d drains the same order"), Iteration),
			Joined, Expected);
		DistinctTraces.Add(Joined);
		// The cap must never be reached legitimately: a dropped entry would show here as a
		// permanently stale ancestor.
		TestEqual(FString::Printf(TEXT("Iteration %d leaves the queue empty"), Iteration),
			FTFTransitionFrameProbe::Queued(F.State), 0);
		TestEqual(FString::Printf(TEXT("Iteration %d settles the City"), Iteration),
			F.LoadedCity->GetOwningFaction(), Target);
	}
	// Drain order comes from an ordered array of authored parents, never from TMap/TSet
	// iteration, so ten captures cannot disagree.
	TestEqual(TEXT("Ten captures produced exactly one drain order"), DistinctTraces.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTransitionFrameSaveLoad,
	"TerritoryFramework.Capture.Regression.DeferredAncestorCommitKeepsTheReadModelCurrent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTransitionFrameSaveLoad::RunTest(const FString& Parameters)
{
	FFrameFixture F;
	if (!BuildLoadedHierarchy(*this, F, /*bIncludeCity=*/true)) return false;
	ON_SCOPE_EXIT { F.World->DestroyWorld(false); GEngine->DestroyWorldContext(F.World); };
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);

	TestTrue(TEXT("Premise: the settling commit completes"), CommitChildOwner(F, F.Heroes, 0));

	// The deferral moves the ancestor's *volume commit*, and with it the ancestor's row -
	// PublishTerritorySummary runs inside ReconcileLoadedAncestor, which is the deferred half.
	// It does NOT move the source's own row: PublishCaptureSummary at the top of the commit
	// stays where it is, which is the read-model visibility the early publish exists to
	// protect (the source's own state events evaluate conflict protection against it).
	//
	// ReconcileUnloadedHierarchy returns early for a *loaded* parent (:815, "Loaded parents
	// still commit through their own hierarchy lifecycle"), so the ancestor row of this
	// fixture was never on the unloaded early path to begin with: pre-fix it was published at
	// the mid-commit reconcile, from a child that had not yet reset upgrades or reconciled
	// guards. Post-fix nothing publishes it until the ancestor actually commits. That is the
	// trade this fix makes deliberately - an ancestor row derived from a half-reconciled child
	// is not merely early, it is wrong - and the row is current before the commit returns.
	FGameplayTag SourceRowInCallback;
	FGameplayTag DistrictRowInCallback;
	FGameplayTag CityRowInCallback;
	bool bReductionCompleteInCallback = false;
	auto* ChildEvent = NewObject<UTerritoryAuditNarrativeEvent>(F.LoadedPlace);
	ChildEvent->Callback = [&]()
	{
		SourceRowInCallback = F.State->GetCaptureSummary(F.PlaceTag).CurrentOwner;
		DistrictRowInCallback = F.State->GetCaptureSummary(F.DistrictTag).CurrentOwner;
		CityRowInCallback = F.State->GetCaptureSummary(F.CityTag).CurrentOwner;
		bReductionCompleteInCallback = F.State->IsHierarchyReductionComplete(F.DistrictTag);
		F.State->ExportPersistentState();
	};
	FTFTransitionFrameProbe::InstallStateEvent(F.LoadedPlace, ETerritoryState::Claimed, true, ChildEvent);

	TestTrue(TEXT("The capture completes"), CommitChildOwner(F, F.Bandits, 0));

	TestEqual(TEXT("A callback mid-commit sees its own territory's row already current"),
		SourceRowInCallback, F.Bandits);
	TestEqual(TEXT("A loaded ancestor's row is not published from a half-reconciled child"),
		DistrictRowInCallback, F.Heroes);
	TestEqual(TEXT("A loaded ancestor's row is not published from a half-reconciled child: City"),
		CityRowInCallback, F.Heroes);
	TestTrue(TEXT("A callback mid-commit still reads a complete reduction"),
		bReductionCompleteInCallback);
	TestEqual(TEXT("The ancestor committed by the end of the frame"),
		F.LoadedDistrict->GetOwningFaction(), F.Bandits);
	TestEqual(TEXT("The ancestor row is current by the time the commit returns"),
		F.State->GetCaptureSummary(F.DistrictTag).CurrentOwner, F.Bandits);
	TestEqual(TEXT("The City row is current by the time the commit returns"),
		F.State->GetCaptureSummary(F.CityTag).CurrentOwner, F.Bandits);
	FTFTransitionFrameProbe::ClearStateEvents(F.LoadedPlace, ETerritoryState::Claimed);

	// A save taken inside a transition is a real boundary. It records what is true at that
	// instant: the source has committed, its loaded ancestors have not. What matters is that
	// the directory it writes is not *incomplete* - it reduces, and a restored publication
	// reconciles the deferred ancestor back to the committed value.
	F.State->ImportPersistentState();
	TestEqual(TEXT("The mid-commit save restores the source's committed row"),
		F.State->GetCaptureSummary(F.PlaceTag).CurrentOwner, F.Bandits);
	TestEqual(TEXT("The mid-commit save restores the ancestor row as of the save instant"),
		F.State->GetCaptureSummary(F.DistrictTag).CurrentOwner, F.Heroes);
	TestTrue(TEXT("The restored directory still reduces completely"),
		F.State->IsHierarchyReductionComplete(F.CityTag));
	F.State->PublishTerritorySummary(F.LoadedPlace);
	TestEqual(TEXT("The reconciled ancestor row follows its restored child"),
		F.State->GetCaptureSummary(F.DistrictTag).CurrentOwner, F.Bandits);
	TestEqual(TEXT("The reconciled City row follows its restored child"),
		F.State->GetCaptureSummary(F.CityTag).CurrentOwner, F.Bandits);
	return true;
}

#endif
