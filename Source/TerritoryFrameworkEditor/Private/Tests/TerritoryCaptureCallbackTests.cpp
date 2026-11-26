#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/TerritoryAuditEventProbe.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryWorldState.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCaptureCallbackInvalidation,
	"TerritoryFramework.Capture.Regression.CallbackInvalidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCaptureCallbackInvalidation::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Callback world exists"), World)) return false;
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	if (!TestTrue(TEXT("Capture fixture has server GameMode authority"), World->SetGameMode(FURL())))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>();
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transient;
	ATerritoryProperty* Territory = World->SpawnActor<ATerritoryProperty>(Spawn);
	ATerritoryGuardCharacter* Attacker = World->SpawnActor<ATerritoryGuardCharacter>(Spawn);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	Cast<INarrativeTeamAgentInterface>(Attacker)->AddFaction(Heroes);
	UTerritoryPlaceDefinition* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(51, 52, 53, 54);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->DefaultStealthProfile = NewObject<UTerritoryStealthProfile>(Definition);
	Definition->DefaultStealthProfile->bAllowStealthInfiltration = true;
	Definition->ApplyToTerritory(Territory);
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>(World);
	int32 StateCallbacks = 0;
	Probe->StateCallback = [&](ATerritoryVolume* Changed, ETerritoryState State)
	{
		if (State == ETerritoryState::Contested)
		{
			++StateCallbacks;
			Control->ClearCaptureTrackingOnly(Changed);
		}
	};
	Territory->OnTerritoryStateChangedDelegate.AddDynamic(Probe, &UTerritoryAuditEventProbe::StateChanged);
	TestFalse(TEXT("A registration removed by the state callback cannot report success"),
		Control->TryRegisterAttacker(Territory, Attacker, Heroes));
	TestEqual(TEXT("Real state callback ran once"), StateCallbacks, 1);
	TestEqual(TEXT("Callback leaves no capture participant"), Control->GetActiveAttackers(Territory, Heroes), 0);
	Territory->OnTerritoryStateChangedDelegate.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::StateChanged);
	Control->ResetCapture(Territory);

	int32 ExposureCallbacks = 0;
	Probe->EvidenceCallback = [&](ATerritoryVolume* Changed, AActor* Target)
	{
		Control->UnregisterInfiltrator(Changed, Target);
	};
	Probe->ExposureCallback = [&](ATerritoryVolume*, AActor*, ETerritoryExposureState)
	{
		++ExposureCallbacks;
	};
	Control->OnStealthEvidenceReported.AddDynamic(Probe, &UTerritoryAuditEventProbe::EvidenceReported);
	Control->OnExposureChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::ExposureChanged);
	TestTrue(TEXT("Stealth evidence is accepted before its callback withdraws the target"),
		Control->ReportStealthEvidence(Territory, Attacker, nullptr,
			ETerritoryStealthEvidence::Damage, 1.f, FVector::ZeroVector, FVector::ZeroVector, true));
	FTerritoryInfiltrationSnapshot Snapshot;
	TestFalse(TEXT("Evidence callback removed infiltration state"),
		Control->GetInfiltrationSnapshot(Territory, Attacker, Snapshot));
	TestEqual(TEXT("Removed infiltration cannot emit a stale exposure event"), ExposureCallbacks, 0);
	TestEqual(TEXT("Removed infiltration cannot re-register as a contester"),
		Control->GetActiveAttackers(Territory, Heroes), 0);
	Control->OnStealthEvidenceReported.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::EvidenceReported);
	ATerritoryGuardCharacter* SecondTarget = World->SpawnActor<ATerritoryGuardCharacter>(Spawn);
	Cast<INarrativeTeamAgentInterface>(SecondTarget)->AddFaction(Heroes);
	Definition->DefaultStealthProfile->ThrowableDistractionSuspicion = 0.1f;
	Definition->DefaultStealthProfile->SuspicionDecayPerSecond = 100.f;
	for (AActor* Target : {static_cast<AActor*>(Attacker), static_cast<AActor*>(SecondTarget)})
	{
		Control->ReportStealthEvidence(Territory, Target, nullptr,
			ETerritoryStealthEvidence::ThrowableDistraction, 0.1f,
			FVector::ZeroVector, FVector::ZeroVector, false);
	}
	ExposureCallbacks = 0;
	Probe->ExposureCallback = [&](ATerritoryVolume* Changed, AActor*, ETerritoryExposureState NewState)
	{
		if (NewState == ETerritoryExposureState::Undetected)
		{
			++ExposureCallbacks;
			Control->UnregisterInfiltrator(Changed, Attacker);
			Control->UnregisterInfiltrator(Changed, SecondTarget);
		}
	};
	Control->ProcessEvent(Control->FindFunctionChecked(TEXT("OnCaptureTick")), nullptr);
	TestEqual(TEXT("Decay callback safely removes every target during a tick"), ExposureCallbacks, 1);
	TestFalse(TEXT("No second stale decay notification restores the removed target"),
		Control->GetInfiltrationSnapshot(Territory, SecondTarget, Snapshot));
	Control->OnExposureChanged.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::ExposureChanged);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCaptureRecoveryAndLeaderContracts,
	"TerritoryFramework.Capture.Regression.RecoveryAndLeaderContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCaptureRecoveryAndLeaderContracts::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	if (!TestTrue(TEXT("Authority fixture"), World->SetGameMode(FURL()))) return false;
	auto* Control = World->GetSubsystem<UTerritoryControlSubsystem>();
	auto* Territory = World->SpawnActor<ATerritoryProperty>();
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(20261005, 3, 1, 1);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->ApplyToTerritory(Territory);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	TMap<FGameplayTag, ATerritoryGuardCharacter*> Actors;
	for (const FGameplayTag Faction : {Heroes, Bandits})
	{
		auto* Attacker = World->SpawnActor<ATerritoryGuardCharacter>();
		Cast<INarrativeTeamAgentInterface>(Attacker)->AddFaction(Faction);
		Actors.Add(Faction, Attacker);
		TestTrue(TEXT("A living faction registers"), Control->TryRegisterAttacker(Territory, Attacker, Faction));
	}
	Control->AddCaptureProgress(Territory, Heroes, 0.6f);
	Control->AddCaptureProgress(Territory, Bandits, 0.2f);
	TestEqual(TEXT("Manual progress publishes the highest faction's progress"), Territory->GetControlProgress(), 0.6f);
	TestEqual(TEXT("Manual progress publishes that same faction"), Territory->GetOwnershipData().ContestingFaction, Heroes);
	Territory->SetRole(ROLE_SimulatedProxy);
	// A client's subsystem never owns server participation maps.
	auto* ClientRead = NewObject<UTerritoryControlSubsystem>(World);
	TestTrue(TEXT("Client capture status reads replicated actor state"), ClientRead->IsCaptureInProgress(Territory));
	TestEqual(TEXT("Client capture progress reads replicated actor state"), ClientRead->GetCaptureProgress(Territory), Territory->GetControlProgress());
	int32 Living, Pressure;
	TestFalse(TEXT("Clients have no exact participant count"), ClientRead->TryGetCaptureParticipants(Territory, Heroes, Living, Pressure));
	TestEqual(TEXT("Unknown count is not authoritative zero"), Living, -1);
	Territory->SetRole(ROLE_Authority);
	TestTrue(TEXT("Server participant query is ready"), Control->TryGetCaptureParticipants(Territory, Heroes, Living, Pressure));
	TestEqual(TEXT("Server pressure counts a living registered attacker"), Pressure, 1);
	Control->AddCaptureProgress(Territory, Bandits, 0.4f);
	TestEqual(TEXT("Equal progress and living count use lexical faction order"), Control->GetContestingFaction(Territory), Bandits);
	auto* ExtraHero = World->SpawnActor<ATerritoryGuardCharacter>();
	Cast<INarrativeTeamAgentInterface>(ExtraHero)->AddFaction(Heroes);
	Control->RegisterAttacker(Territory, ExtraHero, Heroes);
	Control->AddCaptureProgress(Territory, Heroes, 0.f);
	TestEqual(TEXT("Equal progress favors more living attackers"), Control->GetContestingFaction(Territory), Heroes);
	auto* CompletionGate = NewObject<UTerritoryAuditCondition>(Territory);
	CompletionGate->Callback = [] { return false; };
	const_cast<TMap<ETerritoryState, FTerritoryStateConfig>&>(Territory->GetStateConfigs())
		.FindOrAdd(ETerritoryState::Claimed).EntryConditions.Add(CompletionGate);
	Control->AddCaptureProgress(Territory, Heroes, 0.4f);
	TestEqual(TEXT("Rejected completion keeps the other faction's participation"), Control->GetActiveAttackers(Territory, Bandits), 1);
	TestFalse(TEXT("Rejected completion transfers no ownership"), Territory->GetOwningFaction().IsValid());
	auto* Gate = NewObject<UTerritoryAuditCondition>(Territory);
	bool bAllowRecovery = false;
	Gate->Callback = [&] { return bAllowRecovery; };
	const_cast<TMap<ETerritoryState, FTerritoryStateConfig>&>(Territory->GetStateConfigs())
		.FindOrAdd(ETerritoryState::Unclaimed).EntryConditions.Add(Gate);
	TestEqual(TEXT("Structured reset reports a condition rejection"),
		Control->TryResetCapture(Territory).Result, ETerritoryMutationResult::Rejected_ConditionsFailed);
	TestEqual(TEXT("Rejected reset preserves contested state"), Territory->GetTerritoryState(), ETerritoryState::Contested);
	TestEqual(TEXT("Rejected reset preserves Heroes participation"), Control->GetActiveAttackers(Territory, Heroes), 2);
	TestEqual(TEXT("Rejected reset preserves Bandits participation"), Control->GetActiveAttackers(Territory, Bandits), 1);
	TestTrue(TEXT("Rejected reset remains tracked for retry"), Control->IsCaptureInProgress(Territory));
	bAllowRecovery = true;
	Control->ProcessEvent(Control->FindFunctionChecked(TEXT("OnCaptureTick")), nullptr);
	TestEqual(TEXT("Allowed retry commits recovery"), Territory->GetTerritoryState(), ETerritoryState::Unclaimed);
	TestEqual(TEXT("Committed recovery releases Heroes exactly once"), Control->GetActiveAttackers(Territory, Heroes), 0);
	TestEqual(TEXT("Committed recovery releases Bandits exactly once"), Control->GetActiveAttackers(Territory, Bandits), 0);
	TestTrue(TEXT("Server zero participants is known"), Control->TryGetCaptureParticipants(Territory, Heroes, Living, Pressure));
	TestEqual(TEXT("Known empty count is zero"), Living, 0);
	TestEqual(TEXT("Repeated recovery is idempotently successful"), Control->TryResetCapture(Territory).Result, ETerritoryMutationResult::Success);

	Control->RegisterAttacker(Territory, Actors[Heroes], Heroes);
	Control->AddCaptureProgress(Territory, Heroes, 0.3f);
	auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	Save->UpdateSaveObject(true);
	FNarrativeActorRecord Record;
	TestTrue(TEXT("Native records a contested state"), Save->CreateActorRecord(Territory, Record));
	bool bLoadInCondition = true;
	Gate->Callback = [&]
	{
		if (bLoadInCondition)
		{
			bLoadInCondition = false;
			Save->LoadActorFromRecord(Territory, Record);
		}
		return true;
	};
	TestEqual(TEXT("Condition-driven load cannot publish old recovery success"),
		Control->TryResetCapture(Territory).Result, ETerritoryMutationResult::Failed_FinalStateMismatch);
	TestEqual(TEXT("Newly loaded contest survives the old operation"), Territory->GetTerritoryState(), ETerritoryState::Contested);
	TestEqual(TEXT("Newly loaded progress survives the old operation"), Control->GetCaptureProgress(Territory), 0.3f);

	auto* Directory = World->SpawnActor<ATerritoryWorldState>();
	FReplicatedCaptureSummary Summary;
	Summary.TerritoryTag = Definition->TerritoryTag;
	Summary.TerritoryGUID = Definition->StableTerritoryGUID;
	Summary.State = ETerritoryState::Contested;
	Summary.ContestingFaction = Heroes;
	Summary.ControlProgress = 0.3f;
	Directory->SetCaptureSummary(Summary);
	ETerritoryState ReadState;
	FGameplayTag ReadFaction;
	float ReadProgress;
	TestTrue(TEXT("Unloaded tag resolves the existing directory"), UTerritoryBlueprintLibrary::TryGetTerritoryCaptureState(
		World, Definition->TerritoryTag, ReadState, ReadFaction, ReadProgress));
	TestEqual(TEXT("Unloaded progress matches the saved read model"), ReadProgress, 0.3f);
	TestFalse(TEXT("Missing directory row is explicitly unready"), UTerritoryBlueprintLibrary::TryGetTerritoryCaptureState(
		World, Bandits, ReadState, ReadFaction, ReadProgress));
	Gate->Callback = [Territory] { Territory->Destroy(); return true; };
	TestEqual(TEXT("Condition-driven destruction cannot publish recovery success"),
		Control->TryResetCapture(Territory).Result, ETerritoryMutationResult::Failed_FinalStateMismatch);
	return true;
}

#endif
