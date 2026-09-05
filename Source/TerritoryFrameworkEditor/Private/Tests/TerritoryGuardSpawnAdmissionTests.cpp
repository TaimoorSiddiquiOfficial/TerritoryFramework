#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFGuardSpawnAdmissionCallbacks,
	"TerritoryFramework.Guards.Regression.NativeSpawnCallbackAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGuardSpawnAdmissionCallbacks::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Guard admission world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	UNarrativeCharacterSubsystem* Characters = World->GetSubsystem<UNarrativeCharacterSubsystem>();
	UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	UNPCDefinition* Definition = NewObject<UNPCDefinition>();
	Definition->CharacterID = TEXT("AuditAdmissionGuardCharacter");
	Definition->NPCID = TEXT("AuditAdmissionGuardNPC");
	Definition->NPCClassPath = ATerritoryGuardCharacter::StaticClass();
	Definition->bAllowMultipleInstances = true;
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>();
	FMulticastDelegateProperty* SpawnEvent = FindFProperty<FMulticastDelegateProperty>(
		Characters->GetClass(), TEXT("OnNPCSpawned"));
	FScriptDelegate SpawnDelegate;
	SpawnDelegate.BindUFunction(Probe, GET_FUNCTION_NAME_CHECKED(UTerritoryAuditEventProbe, NPCSpawned));
	SpawnEvent->AddDelegate(SpawnDelegate, Characters);
	for (int32 Scenario = 0; Scenario < 7; ++Scenario)
	{
		ATerritoryProperty* Territory = World->SpawnActor<ATerritoryProperty>();
		Territory->SetActorGUID_Implementation(FGuid(281, Scenario + 1, 283, 284));
		Territory->GuardNPCDefinition = Definition;
		Territory->OwnershipData.OwningFaction = Heroes;
		Territory->OwnershipData.State = ETerritoryState::Claimed;
		ATerritoryGuardSpawnPoint* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>();
		Post->SetActorLocation(FVector(5000.f * (Scenario + 1), 0.f, 0.f));
		Post->SetActorGUID_Implementation(FGuid(285, Scenario + 1, 287, 288));
		Post->SetResolvedTerritory(Territory);
		Post->ReserveSlots = 3;
		Post->InitializeReserves();
		ATerritoryGuardSpawnPoint* SecondPost = nullptr;
		if (Scenario == 4)
		{
			SecondPost = World->SpawnActor<ATerritoryGuardSpawnPoint>();
			SecondPost->SetActorLocation(Post->GetActorLocation() + FVector(500.f, 0.f, 0.f));
			SecondPost->SetActorGUID_Implementation(FGuid(289, 290, 291, 292));
			SecondPost->SetResolvedTerritory(Territory);
		}
		if (Scenario == 5) Territory->SetRole(ROLE_SimulatedProxy);
		ANarrativeNPCCharacter* StagedGuard = nullptr;
		int32 SpawnCallbacks = 0;
		Probe->NPCSpawnCallback = [&](ANarrativeNPCCharacter* NPC)
		{
			StagedGuard = NPC;
			++SpawnCallbacks;
			if (Scenario == 1) Territory->ForceSetOwningFaction(Bandits);
			if (Scenario == 2) Post->SetResolvedTerritory(nullptr);
			if (Scenario == 3) Post->Destroy();
			if (Scenario == 4) Territory->ForceSetOwningFaction(Bandits);
			if (Scenario == 6)
			{
				// Seed the actual Native ASC's canonical dead state before the Volume binds it.
				UNarrativeAbilitySystemComponent* ASC = NPC->GetNarrativeAbilitySystemComponent();
				FindFProperty<FBoolProperty>(ASC->GetClass(), TEXT("bIsDead"))->SetPropertyValue_InContainer(ASC, true);
			}
		};
		bool bAdmitted;
		if (Scenario == 4)
		{
			Territory->SpawnGuardsToCount(2);
			bAdmitted = Territory->GetSpawnedGuardCount() > 0;
		}
		else bAdmitted = Territory->TrySpawnSingleGuard(Post);
		Probe->NPCSpawnCallback = nullptr;
		TestEqual(FString::Printf(TEXT("Scenario %d spawns only the permitted Native cohort"), Scenario), SpawnCallbacks, Scenario == 5 ? 0 : 1);
		TestEqual(FString::Printf(TEXT("Scenario %d admits only a still-valid deployment"), Scenario), bAdmitted, Scenario == 0);
		if (Scenario == 0)
		{
			TestEqual(TEXT("Normal guard occupies its post"), Post->GetActiveGuardCount(), 1);
			Territory->DespawnGuards();
		}
		else
		{
			TestTrue(TEXT("Rejected staged guard is removed from physical play with bounded cleanup"),
				!IsValid(StagedGuard) || StagedGuard->IsActorBeingDestroyed()
				|| (StagedGuard->IsHidden() && !StagedGuard->GetActorEnableCollision() && StagedGuard->GetLifeSpan() > 0.f));
			TestEqual(TEXT("Rejected guard contributes zero active garrison"), Territory->GetSpawnedGuardCount(), 0);
			TestEqual(TEXT("Rejected guard contributes zero defenders"), Territory->GetRegisteredDefenders().Num(), 0);
			TestEqual(TEXT("Rejected guard is absent from the post"), Post->GetActiveGuardCount(), 0);
			if (IsValid(Post) && !Post->IsActorBeingDestroyed())
			{
				FNarrativeActorRecord Record;
				TestTrue(TEXT("Native save captures rejected admission"), Save->CreateActorRecord(Post, Record));
				ATerritoryGuardSpawnPoint* LoadedPost = World->SpawnActor<ATerritoryGuardSpawnPoint>();
				Save->LoadActorFromRecord(LoadedPost, Record);
				TestEqual(TEXT("Native reload has no phantom guard"), LoadedPost->GetSavedActiveGuardCount(), 0);
				TestEqual(TEXT("Rejected admission spends no reserve"), LoadedPost->GetReserveCount(), 3);
				LoadedPost->Destroy();
			}
		}
		Territory->SetRole(ROLE_Authority);
		Territory->DespawnGuards();
		Territory->Destroy();
		if (IsValid(Post) && !Post->IsActorBeingDestroyed()) Post->Destroy();
		if (SecondPost) SecondPost->Destroy();
	}
	SpawnEvent->RemoveDelegate(SpawnDelegate, Characters);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
