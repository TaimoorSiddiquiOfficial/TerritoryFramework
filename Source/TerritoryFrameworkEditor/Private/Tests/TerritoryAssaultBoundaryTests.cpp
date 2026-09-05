#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"

namespace
{
	FTerritoryAssaultRecord MakeWaitingAssault()
	{
		FTerritoryAssaultRecord Record;
		Record.AssaultID = FGuid::NewGuid();
		Record.TargetTerritoryGUID = FGuid::NewGuid();
		Record.TargetTerritory = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"));
		Record.AttackingFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
		Record.DefendingFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
		Record.State = ETerritoryAssaultState::WaitingForPlayerProximity;
		Record.PlannedForce = 5;
		Record.PendingReserveForce = 5;
		Record.DecisionSeed = 7341;
		return Record;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultUnloadedPeace,
	"TerritoryFramework.CounterAttack.Regression.UnloadedTargetPeaceCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultUnloadedPeace::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Streaming boundary world exists"), World)) return false;
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	const FTerritoryAssaultRecord Waiting = MakeWaitingAssault();
	for (const EDiplomacyState PeaceState : {EDiplomacyState::None, EDiplomacyState::Alliance,
		EDiplomacyState::TradeAgreement, EDiplomacyState::NonAggression, EDiplomacyState::Ceasefire})
	{
		Diplomacy->SetDiplomacyState(Waiting.AttackingFaction, Waiting.DefendingFaction, EDiplomacyState::War);
		Counter->RestorePersistentState({Waiting});
		Diplomacy->SetDiplomacyState(Waiting.AttackingFaction, Waiting.DefendingFaction, PeaceState);
		FTerritoryAssaultRecord Result;
		TestTrue(TEXT("Cancellation retains the durable assault record"), Counter->GetAssault(Waiting.AssaultID, Result));
		TestEqual(TEXT("A blocking treaty cancels an unloaded target immediately"), Result.State, ETerritoryAssaultState::Cancelled);
		TestEqual(TEXT("No attacker spawns during cancellation"), Result.AliveForce, 0);
		TestEqual(TEXT("Cancelled reserve is permanently withdrawn"), Result.WithdrawnForce, 5);
		TestEqual(TEXT("Cancellation retains the deterministic decision"), Result.DecisionSeed, Waiting.DecisionSeed);
		Counter->RestorePersistentState(Counter->GetPersistentState());
		Counter->GetAssault(Waiting.AssaultID, Result);
		TestEqual(TEXT("Reload cannot revive a treaty-cancelled warning"), Result.State, ETerritoryAssaultState::Cancelled);
	}
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultSupersededCancellation,
	"TerritoryFramework.CounterAttack.Regression.SupersededCancellationCallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultSupersededCancellation::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Assault callback world exists"), World)) return false;
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	const FTerritoryAssaultRecord Waiting = MakeWaitingAssault();
	Counter->RestorePersistentState({Waiting});
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>();
	int32 CounterEvents = 0;
	Probe->AssaultCallback = [Counter, Waiting](const FTerritoryAssaultRecord& Changed)
	{
		if (Changed.State == ETerritoryAssaultState::Cancelled) Counter->RestorePersistentState({Waiting});
	};
	Probe->CounterEventCallback = [&CounterEvents](const FTerritoryCounterAttackStateEvent&) { ++CounterEvents; };
	Counter->OnAssaultChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::AssaultChanged);
	Counter->OnCounterHappened.AddDynamic(Probe, &UTerritoryAuditEventProbe::CounterEvent);
	TestFalse(TEXT("Superseded cancellation cannot return verified success"), Counter->CancelAssault(Waiting.AssaultID));
	TestEqual(TEXT("Superseded cancellation cannot emit a stale transition event"), CounterEvents, 0);
	FTerritoryAssaultRecord Result;
	Counter->GetAssault(Waiting.AssaultID, Result);
	TestEqual(TEXT("Callback-restored authoritative state is retained"), Result.State, Waiting.State);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultHistoryAndSeatBounds,
	"TerritoryFramework.CounterAttack.Regression.HistoryAndSeatBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultHistoryAndSeatBounds::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Large valid seat capacities cannot overflow into a negative force"),
		UTerritoryCounterAttackSubsystem::ResolveVehicleOnlyPlannedForce(MAX_int32, 2, {MAX_int32, MAX_int32}), MAX_int32);
	TestEqual(TEXT("Seat capacity remains bounded by the requested finite force"),
		UTerritoryCounterAttackSubsystem::ResolveVehicleOnlyPlannedForce(5, 2, {MAX_int32, MAX_int32}), 5);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	TGuardValue<int32> HistoryLimit(GetMutableDefault<UTerritoryDeveloperSettings>()->MaxRetainedAssaultRecords, -1);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Assault history world exists"), World)) return false;
	ATerritoryWorldState* State = NewObject<ATerritoryWorldState>(World->PersistentLevel);
	State->SetRole(ROLE_Authority);
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	FScriptDelegate Listener;
	Listener.BindUFunction(State, TEXT("OnAssaultChangedLive"));
	Counter->OnAssaultChanged.Add(Listener);
	FTerritoryAssaultRecord Record = MakeWaitingAssault();
	Counter->OnAssaultChanged.Broadcast(Record);
	TestEqual(TEXT("Invalid history limit never discards an active schedule"), State->GetAllAssaultSummaries().Num(), 1);
	Record.State = ETerritoryAssaultState::Cancelled;
	Counter->OnAssaultChanged.Broadcast(Record);
	TestEqual(TEXT("Negative terminal history limit safely removes the last row"), State->GetAllAssaultSummaries().Num(), 0);
	World->DestroyWorld(false);
	return true;
}

#endif
