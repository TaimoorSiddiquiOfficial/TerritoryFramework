#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDirectionalDiplomacyBridge,
	"TerritoryFramework.Diplomacy.Regression.DirectionalObservationAndExplicitCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDirectionalDiplomacyBridge::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Server world exists"), World)) return false;
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	// Native GameState lacks a native stable actor GUID implementation. As in
	// the existing startup test, avoid its unrelated SpawnActor save callback.
	auto* Native = NewObject<ANarrativeGameState>(World->PersistentLevel);
	World->SetGameState(Native);
	Native->SetFactionAttitude(Heroes, Bandits, ETeamAttitude::Friendly);
	Native->SetFactionAttitude(Bandits, Heroes, ETeamAttitude::Hostile);
	auto* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	auto* State = World->SpawnActor<ATerritoryWorldState>();
	State->SetActorGUID_Implementation(FGuid(923, 2026, 51, 1));
	Diplomacy->OnWorldBeginPlay(*World);
	State->SubscribeToLiveUpdates();
	State->PublishDiplomacyReadModel();
	World->GetTimerManager().Tick(0.01f);

	auto ExpectDirections = [&](ETeamAttitude::Type Forward, ETeamAttitude::Type Reverse)
	{
		TestEqual(TEXT("Narrative retains the requested forward direction"),
			Native->GetOneWayFactionAttitude(Heroes, Bandits), Forward);
		TestEqual(TEXT("Narrative retains the requested reverse direction"),
			Native->GetOneWayFactionAttitude(Bandits, Heroes), Reverse);
	};
	TestEqual(TEXT("Strategic policy conservatively observes hostility"),
		Diplomacy->GetDiplomacyState(Heroes, Bandits), EDiplomacyState::War);
	TestTrue(TEXT("Startup publishes observation provenance"),
		State->GetTreatyBetween(Heroes, Bandits).bNarrativeObserved);
	ExpectDirections(ETeamAttitude::Friendly, ETeamAttitude::Hostile);
	Diplomacy->SyncToGameState();
	Native->SetFactionAttitude(Heroes, Bandits, ETeamAttitude::Friendly);
	ExpectDirections(ETeamAttitude::Friendly, ETeamAttitude::Hostile);

	FNarrativeActorRecord ObservedSave;
	TestTrue(TEXT("Native actor save serializes observed provenance"),
		Save->CreateActorRecord(State, ObservedSave));
	Diplomacy->RestorePersistentState({}, {}, {});
	ExpectDirections(ETeamAttitude::Friendly, ETeamAttitude::Hostile);
	Save->LoadActorFromRecord(State, ObservedSave);
	Save->OnFinishedLoad.Broadcast();
	TestTrue(TEXT("Native actor restore retains observation provenance"),
		Diplomacy->GetAllTreaties().Num() == 1
		&& Diplomacy->GetAllTreaties()[0].bNarrativeObserved);
	ExpectDirections(ETeamAttitude::Friendly, ETeamAttitude::Hostile);

	// Exercise the actual OnRep hydration in two separate client worlds. No
	// socket transport is claimed here; that remains an integrated PIE gate.
	for (int32 ClientIndex = 0; ClientIndex < 2; ++ClientIndex)
	{
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* ClientWorld = UWorld::CreateWorld(EWorldType::Game, false);
		Context.SetCurrentWorld(ClientWorld);
		ClientWorld->NextURL = TEXT("127.0.0.1");
		TestEqual(TEXT("Client authority fixture has a real client net mode"),
			ClientWorld->GetNetMode(), NM_Client);
		auto* ClientNative = NewObject<ANarrativeGameState>(ClientWorld->PersistentLevel);
		ClientWorld->SetGameState(ClientNative);
		ClientNative->SetFactionAttitude(Heroes, Bandits, ETeamAttitude::Friendly);
		ClientNative->SetFactionAttitude(Bandits, Heroes, ETeamAttitude::Hostile);
		auto* Replica = ClientWorld->SpawnActor<ATerritoryWorldState>();
		Replica->SetRole(ROLE_SimulatedProxy);
		Replica->ReplicatedTreaties = State->ReplicatedTreaties;
		Replica->OnRep_DiplomacyState();
		auto* ClientDiplomacy = ClientWorld->GetSubsystem<UTerritoryDiplomacySubsystem>();
		TestTrue(TEXT("Late-join hydration preserves provenance"),
			ClientDiplomacy->GetAllTreaties().Num() == 1
			&& ClientDiplomacy->GetAllTreaties()[0].bNarrativeObserved);
		ClientDiplomacy->FormAlliance(Heroes, Bandits);
		ClientDiplomacy->LoadFromGameState();
		ClientDiplomacy->SyncToGameState();
		TestEqual(TEXT("Client commands cannot replace server treaty state"),
			ClientDiplomacy->GetDiplomacyState(Heroes, Bandits), EDiplomacyState::War);
		TestEqual(TEXT("Client hydration and rejected commands do not write Native"),
			ClientNative->GetOneWayFactionAttitude(Heroes, Bandits), ETeamAttitude::Friendly);
		ClientWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(ClientWorld);
	}

	Native->SetFactionAttitude(Bandits, Heroes, ETeamAttitude::Neutral);
	ExpectDirections(ETeamAttitude::Friendly, ETeamAttitude::Neutral);
	TestEqual(TEXT("Neutral in one direction respects remaining friendliness"),
		Diplomacy->GetDiplomacyState(Heroes, Bandits), EDiplomacyState::Alliance);
	Native->SetFactionAttitude(Heroes, Bandits, ETeamAttitude::Neutral);
	TestEqual(TEXT("Both directions neutral removes the observation"),
		State->GetAllTreaties().Num(), 0);
	// A saved projection may be older than the Native actor loaded afterwards.
	Save->LoadActorFromRecord(State, ObservedSave);
	Save->OnFinishedLoad.Broadcast();
	ExpectDirections(ETeamAttitude::Neutral, ETeamAttitude::Neutral);
	TestEqual(TEXT("Load completion refreshes stale observed saves without write-back"),
		State->GetAllTreaties().Num(), 0);

	Native->SetFactionAttitude(Heroes, Bandits, ETeamAttitude::Friendly);
	Native->SetFactionAttitude(Bandits, Heroes, ETeamAttitude::Hostile);
	Diplomacy->DeclareWar(Heroes, Bandits);
	ExpectDirections(ETeamAttitude::Hostile, ETeamAttitude::Hostile);
	TestFalse(TEXT("Explicit same-state command claims bilateral ownership"),
		State->GetTreatyBetween(Heroes, Bandits).bNarrativeObserved);
	Diplomacy->SignTradeAgreement(Heroes, Bandits, 50.f);
	const double Expiry = State->GetTreatyBetween(Heroes, Bandits).ExpiryGameTime;
	Native->SetFactionAttitude(Heroes, Bandits, ETeamAttitude::Friendly);
	TestEqual(TEXT("Compatible Native reassertion retains a rich timed trade"),
		Diplomacy->GetDiplomacyState(Heroes, Bandits), EDiplomacyState::TradeAgreement);
	TestEqual(TEXT("Compatible Native reassertion retains expiry"),
		State->GetTreatyBetween(Heroes, Bandits).ExpiryGameTime, Expiry);
	Native->SetFactionAttitude(Heroes, Bandits, ETeamAttitude::Hostile);
	ExpectDirections(ETeamAttitude::Hostile, ETeamAttitude::Friendly);
	TestTrue(TEXT("External incompatible change relinquishes bilateral ownership"),
		State->GetTreatyBetween(Heroes, Bandits).bNarrativeObserved);

	FTreatyRecord Ceasefire;
	Ceasefire.FactionA = Heroes;
	Ceasefire.FactionB = Bandits;
	Ceasefire.State = EDiplomacyState::Ceasefire;
	Ceasefire.bPermanent = false;
	Ceasefire.SignedGameTime = 10.f;
	Ceasefire.ExpiryGameTime = 90.f;
	TestFalse(TEXT("Legacy/default records retain authored interpretation"),
		Ceasefire.bNarrativeObserved);
	Diplomacy->RestorePersistentState({Ceasefire}, {}, {});
	State->PublishDiplomacyReadModel();
	Native->SetFactionAttitude(Heroes, Bandits, ETeamAttitude::Neutral);
	Native->SetFactionAttitude(Bandits, Heroes, ETeamAttitude::Neutral);
	TestEqual(TEXT("Neutral reassertion does not erase a ceasefire"),
		Diplomacy->GetDiplomacyState(Heroes, Bandits), EDiplomacyState::Ceasefire);
	TestEqual(TEXT("Neutral reassertion retains the ceasefire's expiry"),
		State->GetTreatyBetween(Heroes, Bandits).ExpiryGameTime, 90.0);
	ExpectDirections(ETeamAttitude::Neutral, ETeamAttitude::Neutral);

	FNarrativeActorRecord AuthoredSave;
	TestTrue(TEXT("Native actor save serializes an authored ceasefire"),
		Save->CreateActorRecord(State, AuthoredSave));
	Diplomacy->DeclareWar(Heroes, Bandits);
	Save->LoadActorFromRecord(State, AuthoredSave);
	// Simulate Native's actor restoring after the Territory actor.
	Native->FactionAllianceMap.FindChecked(Heroes).AttitudeMap[Bandits] = ETeamAttitude::Hostile;
	Native->FactionAllianceMap.FindChecked(Bandits).AttitudeMap[Heroes] = ETeamAttitude::Hostile;
	Save->OnFinishedLoad.Broadcast();
	TestEqual(TEXT("Authored metadata still wins after either actor load order"),
		Diplomacy->GetDiplomacyState(Heroes, Bandits), EDiplomacyState::Ceasefire);
	ExpectDirections(ETeamAttitude::Neutral, ETeamAttitude::Neutral);

	Native->FactionAllianceMap.Reset();
	Native->SetFactionAttitude(Heroes, Bandits, ETeamAttitude::Friendly);
	Diplomacy->SyncToGameState();
	TestFalse(TEXT("Observing a one-way relation never invents a reverse entry"),
		Native->FactionAllianceMap.Contains(Bandits));
	TestTrue(TEXT("An external one-way relation publishes observed provenance"),
		State->GetTreatyBetween(Heroes, Bandits).bNarrativeObserved);

	State->UnsubscribeFromLiveUpdates();
	World->DestroyWorld(false);
	return true;
}

#endif
