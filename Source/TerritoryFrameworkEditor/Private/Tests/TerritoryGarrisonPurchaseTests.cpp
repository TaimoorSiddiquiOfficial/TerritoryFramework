#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFGarrisonPurchaseCallbacks,
	"TerritoryFramework.Guards.Regression.GarrisonPurchaseCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGarrisonPurchaseCallbacks::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Garrison transaction world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	UNPCDefinition* Definition = NewObject<UNPCDefinition>();
	Definition->CharacterID = TEXT("GarrisonTransactionCharacter");
	Definition->NPCID = TEXT("GarrisonTransactionNPC");
	Definition->NPCClassPath = ATerritoryGuardCharacter::StaticClass();
	Definition->bAllowMultipleInstances = true;
	ATerritoryGuardCharacter* Account = World->SpawnActor<ATerritoryGuardCharacter>();
	Cast<INarrativeTeamAgentInterface>(Account)->AddFaction(Heroes);
	UNarrativeInventoryComponent* Inventory = Account->GetInventoryComponent();
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>();
	Inventory->OnCurrencyChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::CurrencyChanged);
	UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	UNarrativeCharacterSubsystem* Characters = World->GetSubsystem<UNarrativeCharacterSubsystem>();
	FMulticastDelegateProperty* SpawnEvent = FindFProperty<FMulticastDelegateProperty>(Characters->GetClass(), TEXT("OnNPCSpawned"));
	FScriptDelegate SpawnDelegate;
	SpawnDelegate.BindUFunction(Probe, GET_FUNCTION_NAME_CHECKED(UTerritoryAuditEventProbe, NPCSpawned));
	SpawnEvent->AddDelegate(SpawnDelegate, Characters);

	for (int32 Scenario = 0; Scenario < 5; ++Scenario)
	{
		ATerritoryProperty* Territory = World->SpawnActor<ATerritoryProperty>();
		Territory->SetActorGUID_Implementation(FGuid(381, Scenario + 1, 383, 384));
		Territory->GuardNPCDefinition = Definition;
		Territory->OwnershipData.OwningFaction = Heroes;
		Territory->OwnershipData.State = ETerritoryState::Claimed;
		Territory->OwnershipData.ControlProgress = 1.f;
		Territory->OwnershipData.DesiredGuardCount = 0;
		Territory->OwnershipData.GuardRecruitmentCost = 100;
		ATerritoryGuardSpawnPoint* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>();
		Post->SetActorLocation(FVector(5000.f * (Scenario + 1), 0.f, 100.f));
		Post->SetActorGUID_Implementation(FGuid(385, Scenario + 1, 387, 388));
		Post->SetResolvedTerritory(Territory);
		Post->ReserveSlots = 3;
		Post->InitializeReserves();
		Territory->OnGarrisonChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::GarrisonChanged);
		Inventory->SetCurrency(2000);
		FNarrativeActorRecord Before;
		TestTrue(TEXT("Native archive captures the starting campaign"), Save->CreateActorRecord(Territory, Before));
		FNarrativeActorRecord PaidTerritory, PaidPost;
		int32 CurrencyCallbacks = 0;
		int32 SpawnCallbacks = 0;
		bool bNestedAccepted = false;
		Probe->NPCSpawnCallback = [&](ANarrativeNPCCharacter* NPC)
		{
			++SpawnCallbacks;
			bNestedAccepted |= Territory->TrySetDesiredGuardCount(Account, 1).bSuccess;
			if (Scenario == 1) Post->SetResolvedTerritory(nullptr);
			if (Scenario == 2) Inventory->SetCurrency(0);
			if (Scenario == 4) Save->LoadActorFromRecord(Territory, Before);
		};
		Probe->CurrencyCallback = [&]()
		{
			++CurrencyCallbacks;
			if (Scenario == 2) return; // External spending during NPC initialization.
			TestEqual(TEXT("Native debit observers see the complete staffing target"), Territory->GetDesiredGuardCount(), 1);
			TestEqual(TEXT("Native debit observers see the live purchased guard"), Territory->GetSpawnedGuardCount(), 1);
			TestEqual(TEXT("Native debit observers see the final replicated read model"), Territory->GetGarrisonSnapshot().DesiredGuards, 1);
			bNestedAccepted |= Territory->TrySetDesiredGuardCount(Account, 0).bSuccess;
			FTerritoryOwnershipData Changed = Territory->GetOwnershipData();
			Changed.OwningFaction = Bandits;
			TestFalse(TEXT("Currency callbacks cannot change owner inside recruitment"), Territory->CommitOwnershipData(Changed));
			TestTrue(TEXT("Native save captures paid staffing inside its currency callback"), Save->CreateActorRecord(Territory, PaidTerritory));
			TestTrue(TEXT("Native save captures the purchased post occupant"), Save->CreateActorRecord(Post, PaidPost));
			Inventory->PrepareForSave_Implementation();
			if (Scenario == 3) Save->LoadActorFromRecord(Territory, Before);
		};
		Probe->GarrisonCallback = [&]()
		{
			bNestedAccepted |= Territory->TrySetDesiredGuardCount(Account, 0).bSuccess;
		};
		const FTerritoryGarrisonMutationResult Result = Territory->TrySetDesiredGuardCount(Account, 1);
		Probe->NPCSpawnCallback = nullptr;
		Probe->CurrencyCallback = nullptr;
		Probe->GarrisonCallback = nullptr;
		TestTrue(TEXT("A real Narrative NPC spawn callback was exercised"), SpawnCallbacks > 0);
		TestFalse(TEXT("NPC, wallet and publication callbacks cannot nest a purchase"), bNestedAccepted);
		TestEqual(TEXT("Recruitment and rollback never spend reserve guards"), Post->GetReserveCount(), 3);
		if (Scenario == 0)
		{
			TestTrue(TEXT("One complete paid deployment succeeds"), Result.bSuccess);
			TestEqual(TEXT("Payment emits one Native currency callback"), CurrencyCallbacks, 1);
			TestEqual(TEXT("One quoted cost is debited"), Inventory->GetCurrency(), 1900);
			TestTrue(TEXT("A later independent withdrawal is allowed"), Territory->TrySetDesiredGuardCount(Account, 0).bSuccess);
			Inventory->SetCurrency(0);
			Save->LoadActorFromRecord(Post, PaidPost);
			Save->LoadActorFromRecord(Territory, PaidTerritory);
			Inventory->Load_Implementation();
			TestEqual(TEXT("Callback save restores the purchased target"), Territory->GetDesiredGuardCount(), 1);
			TestEqual(TEXT("Callback save restores the post active count"), Post->GetSavedActiveGuardCount(), 1);
			TestEqual(TEXT("Callback save restores the same paid balance"), Inventory->GetCurrency(), 1900);
			Territory->SetRole(ROLE_SimulatedProxy);
			TestFalse(TEXT("Clients cannot withdraw server guards"), Territory->TrySetDesiredGuardCount(Account, 0).bSuccess);
			TestEqual(TEXT("Rejected client request keeps the target"), Territory->GetDesiredGuardCount(), 1);
			Territory->SetRole(ROLE_Authority);
		}
		else
		{
			TestFalse(TEXT("Placement, payment or reload failure cannot report success"), Result.bSuccess);
			TestEqual(TEXT("Failure preserves or restores the zero target"), Territory->GetDesiredGuardCount(), 0);
			TestEqual(TEXT("A failed request leaves no admitted guards"), Territory->GetSpawnedGuardCount(), 0);
			if (Scenario == 1 || Scenario == 4)
			{
				TestEqual(TEXT("Failed deployment or superseded staging takes no currency"), Inventory->GetCurrency(), 2000);
				TestEqual(TEXT("No debit/refund callbacks occur before a complete deployment"), CurrencyCallbacks, 0);
			}
			if (Scenario == 2) TestEqual(TEXT("External spending is not overwritten by a fabricated refund"), Inventory->GetCurrency(), 0);
			if (Scenario == 3) TestEqual(TEXT("A territory-only reload does not fabricate an inventory refund"), Inventory->GetCurrency(), 1900);
		}
		Territory->DespawnGuards();
		Post->Destroy();
		Territory->Destroy();
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
