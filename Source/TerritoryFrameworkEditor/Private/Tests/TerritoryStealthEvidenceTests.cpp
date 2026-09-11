#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "AI/TerritoryNPCController.h"
#include "AI/TerritoryInvestigationGoal.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStealthEvidenceIdentity,
	"TerritoryFramework.Stealth.Regression.CluesIdentityAndElapsedSight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFStealthEvidenceIdentity::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Evidence world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	World->CreateAISystem();
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Control = World->GetSubsystem<UTerritoryControlSubsystem>();
	auto* Territory = World->SpawnActor<ATerritoryProperty>();
	auto* Target = World->SpawnActor<ATerritoryGuardCharacter>();
	auto* OtherPlayer = World->SpawnActor<ATerritoryGuardCharacter>();
	auto* Observer = World->SpawnActor<AActor>();
	const FGameplayTag Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	Cast<INarrativeTeamAgentInterface>(Target)->AddFaction(Faction);
	Cast<INarrativeTeamAgentInterface>(OtherPlayer)->AddFaction(Faction);
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(61, 62, 63, 64);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	auto* Profile = NewObject<UTerritoryStealthProfile>(Definition);
	Definition->DefaultStealthProfile = Profile;
	Profile->EscalationScope = ETerritoryStealthEscalationScope::LocalAlarm;
	Definition->ApplyToTerritory(Territory);
	auto* Defender = World->SpawnActor<ATerritoryGuardCharacter>();
	FindFProperty<FObjectPropertyBase>(Defender->GetClass(), TEXT("OwningTerritory"))
		->SetObjectPropertyValue_InContainer(Defender, Territory);
	auto* DefenderController = World->SpawnActor<ATerritoryNPCController>();
	DefenderController->Possess(Defender);
	FindFProperty<FObjectPropertyBase>(UNPCActivityComponent::StaticClass(), TEXT("OwnerController"))
		->SetObjectPropertyValue_InContainer(DefenderController->GetActivityComponent(), DefenderController);
	Territory->RegisterDefender(Defender);
	auto Report = [&](ETerritoryStealthEvidence Evidence, float Strength, bool Confirmed = false, float Seconds = 0.25f)
	{
		return Control->ReportStealthEvidence(Territory, Target, Observer, Evidence,
			Strength, FVector::ZeroVector, FVector::ZeroVector, Confirmed, Seconds);
	};
	FTerritoryInfiltrationSnapshot Snapshot;
	for (ETerritoryStealthEvidence Evidence : {ETerritoryStealthEvidence::Gunshot,
		ETerritoryStealthEvidence::BulletImpact, ETerritoryStealthEvidence::Corpse,
		ETerritoryStealthEvidence::ThrowableDistraction})
	{
		Control->UnregisterInfiltrator(Territory, Target);
		for (int32 Index = 0; Index < 12; ++Index) Report(Evidence, 1.f);
		Control->GetInfiltrationSnapshot(Territory, Target, Snapshot);
		TestEqual(TEXT("Repeated anonymous clues can fill suspicion"), Snapshot.Suspicion, 1.f);
		TestEqual(TEXT("Anonymous clues do not identify a hidden player"), Snapshot.ExposureState, ETerritoryExposureState::Suspicious);
		TestEqual(TEXT("Anonymous evidence never grants capture pressure"), Control->GetActiveAttackers(Territory, Faction), 0);
	}
	Profile->bAnonymousEvidenceCanExpose = true;
	Report(ETerritoryStealthEvidence::Gunshot, 1.f);
	TestTrue(TEXT("Explicit story policy can allow exposure from clues"), Control->IsInfiltratorExposed(Territory, Target));
	Profile->bAnonymousEvidenceCanExpose = false;
	Control->ClearInfiltratorExposure(Territory, Target);
	Report(ETerritoryStealthEvidence::Sight, 0.4f, false, 0.f);
	Control->GetInfiltrationSnapshot(Territory, Target, Snapshot);
	TestEqual(TEXT("Visibility callback adds no elapsed sight suspicion"), Snapshot.Suspicion, 0.f);
	Report(ETerritoryStealthEvidence::Sight, 0.4f, false, 0.5f);
	Control->GetInfiltrationSnapshot(Territory, Target, Snapshot);
	TestEqual(TEXT("Partial sight uses the supplied half second"), Snapshot.Suspicion, 0.4f);
	TestEqual(TEXT("Current sight has one observer"), Snapshot.ConfirmingObserverCount, 1);
	Report(ETerritoryStealthEvidence::Sight, 0.f, false, 0.f);
	Control->GetInfiltrationSnapshot(Territory, Target, Snapshot);
	TestEqual(TEXT("Losing sight clears the observer"), Snapshot.ConfirmingObserverCount, 0);
	Report(ETerritoryStealthEvidence::Damage, 1.f, true);
	bool FoundInvestigation = false;
	auto* Investigation = Cast<UTerritoryInvestigationGoal>(Defender->GetActivityComponent()->GetGoalByKey(
		UTerritoryInvestigationGoal::StaticClass(), Target, FoundInvestigation));
	TestTrue(TEXT("Immediate Local Alarm assigns a Native investigation goal"), FoundInvestigation && Investigation);
	if (Investigation) TestTrue(TEXT("Local Alarm keeps confirmed identity on its investigation"), Investigation->bIdentityConfirmed);
	Report(ETerritoryStealthEvidence::Sight, 0.f, false, 0.f);
	TestTrue(TEXT("Lost sight does not erase confirmed exposure"), Control->IsInfiltratorExposed(Territory, Target));
	Control->RegisterInfiltrator(Territory, OtherPlayer, Faction);
	TestFalse(TEXT("Another player remains hidden"), Control->IsInfiltratorExposed(Territory, OtherPlayer));
	Control->ClearInfiltratorExposure(Territory, Target);
	TestFalse(TEXT("Explicit clearing removes confirmed exposure"), Control->IsInfiltratorExposed(Territory, Target));
	Profile->bFireWhileUnseenStartsInvestigation = false;
	TestFalse(TEXT("Disabled unseen gunshot policy rejects the evidence"), Report(ETerritoryStealthEvidence::Gunshot, 1.f));
	Profile->bUnseenDefenderDeathStartsInvestigation = false;
	TestFalse(TEXT("Disabled unseen death policy rejects the evidence"), Report(ETerritoryStealthEvidence::Corpse, 1.f));
	TestFalse(TEXT("Invalid strength cannot poison suspicion"), Report(ETerritoryStealthEvidence::Sight, NAN));
	TestFalse(TEXT("Invalid elapsed time cannot poison suspicion"), Report(ETerritoryStealthEvidence::Sight, 0.5f, false, NAN));
	TestFalse(TEXT("Empty evidence is rejected"), Report(ETerritoryStealthEvidence::None, 1.f));
	Control->UnregisterInfiltrator(Territory, Target);
	Control->RegisterInfiltrator(Territory, Target, Faction);
	Control->GetInfiltrationSnapshot(Territory, Target, Snapshot);
	TestEqual(TEXT("Re-register after unload starts fresh transient awareness"), Snapshot.ExposureState, ETerritoryExposureState::Undetected);
	const UFunction* Function = Control->FindFunction(TEXT("ReportStealthEvidence"));
	TestTrue(TEXT("Evidence Blueprint contract is authority only"), Function && Function->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));
	return true;
}

#endif
