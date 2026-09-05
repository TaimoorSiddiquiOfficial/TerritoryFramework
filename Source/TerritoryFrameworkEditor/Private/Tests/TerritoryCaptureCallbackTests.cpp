#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/TerritoryAuditEventProbe.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Subsystems/TerritoryControlSubsystem.h"
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

#endif
