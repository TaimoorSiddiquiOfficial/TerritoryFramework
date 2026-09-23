#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"

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

#endif
