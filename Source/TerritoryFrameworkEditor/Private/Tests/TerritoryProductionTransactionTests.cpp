#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Economy/TerritoryProductionTags.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UI/TerritoryUIBlueprintLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFProductionTransactionCallbacks,
	"TerritoryFramework.Production.Regression.TransactionCallbacksAndCompensation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFProductionTransactionCallbacks::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Recipe callback world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	for (int32 Scenario = 0; Scenario < 6; ++Scenario)
	{
		ATerritoryGuardCharacter* Account = World->SpawnActor<ATerritoryGuardCharacter>();
		Cast<INarrativeTeamAgentInterface>(Account)->AddFaction(Heroes);
		UNarrativeInventoryComponent* Inventory = Account->GetInventoryComponent();
		Inventory->SetCapacity(16);
		Inventory->SetWeightCapacity(100.f);
		const auto Count = [&](UClass* Class) { return Inventory->GetTotalQuantityOfItemExact(TSoftClassPtr<UNarrativeItem>(Class), false); };
		const auto Rate = [](UClass* Class, int32 Quantity)
		{
			FTerritoryResourceRate Amount; Amount.ItemClass = Class; Amount.QuantityPerCycle = Quantity; return Amount;
		};
		FTerritoryProductionRule Recipe;
		Recipe.RuleTag = TerritoryProductionTags::FarmLivestock;
		Recipe.Inputs = {Rate(UTerritoryAuditResourceA::StaticClass(), 2)};
		Recipe.Outputs = {Rate(UTerritoryAuditResourceB::StaticClass(), 1), Rate(UTerritoryAuditResourceC::StaticClass(), 1)};
		TestEqual(TEXT("Native seed adds the exact input"), Inventory->TryAddItemFromClass(UTerritoryAuditResourceA::StaticClass(), Scenario == 0 ? 4 : 2, false).AmountGiven, Scenario == 0 ? 4 : 2);
		if (Scenario == 3)
		{
			Recipe.Inputs.Add(Rate(UTerritoryAuditResourceB::StaticClass(), 1));
			Recipe.Outputs = {Rate(UTerritoryAuditResourceC::StaticClass(), 1)};
			Inventory->TryAddItemFromClass(UTerritoryAuditResourceB::StaticClass(), 1, false);
		}
		Inventory->PrepareForSave_Implementation();
		UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>();
		Inventory->OnItemAdded.AddDynamic(Probe, &UTerritoryAuditEventProbe::ItemAdded);
		Inventory->OnItemRemoved.AddDynamic(Probe, &UTerritoryAuditEventProbe::ItemRemoved);
		Economy->OnProductionSettled.AddDynamic(Probe, &UTerritoryAuditEventProbe::ProductionSettled);
		bool bCallbackRan = false, bNestedAccepted = false, bNestedTried = false, bPublishNestedTried = false;
		int32 Notifications = 0;
		Probe->ItemRemovedCallback = [&](UNarrativeItem* Item, int32 Amount)
		{
			if (Scenario == 0 && !bNestedTried)
			{
				bNestedTried = true;
				FTerritoryProductionResult Nested;
				bNestedAccepted |= Economy->ExecuteResourceRecipe(Account, Heroes, Recipe, 0, 1, FGameplayTag(), Nested);
			}
			if (Scenario == 3 && !bCallbackRan && Item->GetClass() == UTerritoryAuditResourceA::StaticClass())
			{
				bCallbackRan = true;
				Inventory->SetCapacity(1);
				for (UNarrativeItem* Stack : Inventory->FindItemsByClass(TSoftClassPtr<UNarrativeItem>(UTerritoryAuditResourceB::StaticClass()), false))
					CastChecked<UTerritoryAuditResourceB>(Stack)->bRejectRemoval = true;
			}
		};
		Probe->ItemCallback = [&]()
		{
			if (bCallbackRan) return;
			bCallbackRan = true;
			if (Scenario == 1 || Scenario == 2) Inventory->SetCapacity(1);
			if (Scenario == 2)
				for (UNarrativeItem* Stack : Inventory->GetItems()) CastChecked<UTerritoryAuditResourceA>(Stack)->bRejectRemoval = true;
			if (Scenario == 4) Inventory->Load_Implementation();
			if (Scenario == 5)
				for (UNarrativeItem* Stack : Inventory->FindItemsByClass(TSoftClassPtr<UNarrativeItem>(UTerritoryAuditResourceB::StaticClass()), false)) Inventory->ConsumeItem(Stack, Stack->GetQuantity());
		};
		Probe->ProductionCallback = [&]()
		{
			++Notifications;
			if (Scenario == 0 && !bPublishNestedTried)
			{
				bPublishNestedTried = true;
				FTerritoryProductionResult Nested;
				bNestedAccepted |= Economy->ExecuteResourceRecipe(Account, Heroes, Recipe, 0, 1, FGameplayTag(), Nested);
				Inventory->PrepareForSave_Implementation();
			}
		};
		FTerritoryProductionResult Result;
		const bool bSuccess = Economy->ExecuteResourceRecipe(Account, Heroes, Recipe, 0, 1, FGameplayTag(), Result);
		Probe->ItemCallback = nullptr; Probe->ItemRemovedCallback = nullptr; Probe->ProductionCallback = nullptr;
		Inventory->OnItemAdded.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::ItemAdded);
		Inventory->OnItemRemoved.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::ItemRemoved);
		Economy->OnProductionSettled.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::ProductionSettled);
		TestTrue(FString::Printf(TEXT("Scenario %d exercised a real Native callback"), Scenario), bCallbackRan);
		if (Scenario == 0)
		{
			TestTrue(TEXT("A complete outer recipe succeeds"), bSuccess && Result.bSuccess);
			TestFalse(TEXT("Item and settlement callbacks cannot nest recipes"), bNestedAccepted);
			TestEqual(TEXT("Rejected nested requests emit no recursive settlement event"), Notifications, 1);
			TestEqual(TEXT("Only one input batch is consumed"), Count(UTerritoryAuditResourceA::StaticClass()), 2);
			TestEqual(TEXT("Only one output batch is produced"), Count(UTerritoryAuditResourceB::StaticClass()), 1);
			Inventory->Load_Implementation();
			TestEqual(TEXT("Native save taken on settlement restores the completed recipe"), Count(UTerritoryAuditResourceC::StaticClass()), 1);
			Account->SetRole(ROLE_SimulatedProxy);
			TestFalse(TEXT("Clients cannot execute a recipe"), Economy->ExecuteResourceRecipe(Account, Heroes, Recipe, 0, 1, FGameplayTag(), Result));
			Account->SetRole(ROLE_Authority);
		}
		else
		{
			TestFalse(FString::Printf(TEXT("Scenario %d cannot claim a completed recipe"), Scenario), bSuccess || Result.bSuccess);
			TestEqual(TEXT("A failed or superseded recipe does not add the final output"), Count(UTerritoryAuditResourceC::StaticClass()), 0);
			if (Scenario == 1 || Scenario == 4 || Scenario == 5)
				TestEqual(TEXT("Compensation or load retains the input quantity"), Count(UTerritoryAuditResourceA::StaticClass()), 2);
			if (Scenario == 2 || Scenario == 3)
			{
				TestEqual(TEXT("Failed compensation is explicitly distinguishable"), StaticEnum<ETerritoryProductionStatus>()->GetNameStringByValue(static_cast<int64>(Result.Status)), FString(TEXT("RollbackIncomplete")));
				FTerritoryProductionSiteRecord Site;
				Site.TerritoryGUID = FGuid(391, Scenario, 393, 394);
				Site.OwnerFaction = Heroes;
				Site.LastStatus = Result.Status;
				Site.LastInputs = Result.InputsConsumed;
				Site.LastOutputs = Result.OutputsProduced;
				TArray<uint8> Bytes;
				FMemoryWriter Writer(Bytes);
				FObjectAndNameAsStringProxyArchive SaveArchive(Writer, false);
				SaveArchive.ArIsSaveGame = true;
				FTerritoryProductionSiteRecord::StaticStruct()->SerializeItem(SaveArchive, &Site, nullptr);
				FTerritoryProductionSiteRecord Loaded;
				FMemoryReader Reader(Bytes);
				FObjectAndNameAsStringProxyArchive LoadArchive(Reader, true);
				LoadArchive.ArIsSaveGame = true;
				FTerritoryProductionSiteRecord::StaticStruct()->SerializeItem(LoadArchive, &Loaded, nullptr);
				TestEqual(TEXT("Detached production records preserve failure state through save"), Loaded.LastStatus, ETerritoryProductionStatus::RollbackIncomplete);
				TestEqual(TEXT("Detached production records preserve their authored identity"), Loaded.TerritoryGUID, Site.TerritoryGUID);
				TestEqual(TEXT("Unreturned input is retained in the save read model"), Loaded.LastInputs.Num(), Site.LastInputs.Num());
				TestFalse(TEXT("The Blueprint status presentation exposes the failure"), UTerritoryUIBlueprintLibrary::GetProductionStatusText(Loaded.LastStatus).IsEmpty());
			}
			if (Scenario == 4) TestEqual(TEXT("A superseded request publishes no stale settlement"), Notifications, 0);
		}
		Account->Destroy();
	}
	World->DestroyWorld(false);
	TestEqual(TEXT("Appending failure statuses preserves existing Blueprint/save enum values"), static_cast<int32>(ETerritoryProductionStatus::AuthorityRejected), 9);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
