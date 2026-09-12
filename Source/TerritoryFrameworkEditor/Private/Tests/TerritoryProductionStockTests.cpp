#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Economy/TerritoryProductionTags.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Items/AmmoItem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "UObject/UnrealType.h"

namespace
{
struct FProductionStockWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UTerritoryEconomySubsystem* Economy;
	ATerritoryGuardCharacter* Account;
	UNarrativeInventoryComponent* Inventory;
	ANarrativeGameState* Clock;
	FGameplayTag Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	FProductionStockWorld()
	{
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
		World->SetGameMode(FURL());
		Clock = NewObject<ANarrativeGameState>(World->PersistentLevel);
		Clock->SetRole(ROLE_Authority);
		World->SetGameState(Clock);
		Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
		Account = World->SpawnActor<ATerritoryGuardCharacter>();
		Cast<INarrativeTeamAgentInterface>(Account)->AddFaction(Faction);
		Inventory = Account->GetInventoryComponent();
		Inventory->SetCapacity(64);
		Inventory->SetWeightCapacity(1000.f);
		Economy->RegisterFactionResourceAccount(Faction, Account);
	}
	~FProductionStockWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	void Cycle(int32 Index)
	{
		FindFProperty<FFloatProperty>(Clock->GetClass(), TEXT("AccumulatedTime"))->SetPropertyValue_InContainer(Clock, 2400.f * Index);
	}
	int32 Count(UClass* Class) const { return Inventory->GetTotalQuantityOfItemExact(TSoftClassPtr<UNarrativeItem>(Class), false); }
	bool Run(const FTerritoryProductionRule& Rule, FTerritoryProductionResult& Result, int32 Batches = 1)
	{
		return Economy->ExecuteResourceRecipe(Account, Faction, Rule, 0, Batches, FGameplayTag(), Result);
	}
};

FTerritoryResourceRate StockRate(UClass* Class, int32 Quantity)
{
	FTerritoryResourceRate Rate; Rate.ItemClass = Class; Rate.QuantityPerCycle = Quantity; return Rate;
}
FTerritoryProductionRule RefillRule()
{
	FTerritoryProductionRule Rule;
	Rule.RuleTag = TerritoryProductionTags::FarmLivestock;
	Rule.DisplayName = FText::FromString(TEXT("Ammo refill"));
	Rule.Outputs = {StockRate(UTerritoryAuditResourceB::StaticClass(), 1000)};
	FTerritoryProductionStockCap Cap; Cap.ItemClass = UTerritoryAuditResourceB::StaticClass(); Cap.MaximumQuantity = 300;
	Rule.OutputStockCaps = {Cap};
	return Rule;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFProductionStockLimits,
	"TerritoryFramework.Production.Regression.StockLimitsAndRuleNotifications",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFProductionStockLimits::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	FProductionStockWorld F;
	UClass* Ammo = UTerritoryAuditResourceB::StaticClass();
	UClass* Input = UAmmoItem::StaticClass();
	UClass* Other = UTerritoryAuditResourceC::StaticClass();
	FTerritoryProductionRule Rule = RefillRule();
	FTerritoryProductionResult Result;
	F.Inventory->TryAddItemFromClass(Ammo, 295, false);
	TestTrue(TEXT("A 1000-item output fills only the missing five across Native stacks"), F.Run(Rule, Result));
	TestEqual(TEXT("Refill lands on the exact stock cap"), F.Count(Ammo), 300);
	TestEqual(TEXT("Verified result reports only the five credited items"), Result.OutputsProduced[0].Quantity, 5);
	TestFalse(TEXT("A full target cannot produce again"), F.Run(Rule, Result));
	TestEqual(TEXT("Expected full stock has its own status"), Result.Status, ETerritoryProductionStatus::StockLimited);
	TestTrue(TEXT("A blocked result contains no items"), Result.InputsConsumed.IsEmpty() && Result.OutputsProduced.IsEmpty());
	F.Inventory->ConsumeItemsOfClass(Ammo, 1);
	TestTrue(TEXT("Spending one item allows a one-item refill even for many batches"), F.Run(Rule, Result, 5));
	TestEqual(TEXT("Batch scaling never bypasses the cap"), F.Count(Ammo), 300);
	F.Inventory->TryAddItemFromClass(Ammo, 7, false);
	TestFalse(TEXT("Existing stock above the cap blocks production"), F.Run(Rule, Result));
	TestEqual(TEXT("A cap never deletes existing excess stock"), F.Count(Ammo), 307);
	F.Inventory->ConsumeItemsOfClass(Ammo, 17);
	F.Inventory->TryAddItemFromClass(Input, 10, false);
	Rule.Inputs = {StockRate(Input, 2)};
	Rule.Outputs[0].QuantityPerCycle = 20;
	TestFalse(TEXT("A paid recipe waits for its complete output to fit"), F.Run(Rule, Result));
	TestEqual(TEXT("A cap-blocked recipe consumes no inputs"), F.Count(Input), 10);
	F.Inventory->ConsumeItemsOfClass(Ammo, 10);
	TestTrue(TEXT("A complete paid recipe may reach the cap exactly"), F.Run(Rule, Result));
	TestEqual(TEXT("The successful paid recipe consumes exactly one batch"), F.Count(Input), 8);
	Rule.Inputs.Reset();
	Rule.OutputStockCaps.Reset();
	Rule.Outputs = {StockRate(Other, 1)};
	FTerritoryProductionStockCondition Check; Check.ItemClass = Ammo; Check.Quantity = 300;
	const ETerritoryIntegerComparison Operations[] = {ETerritoryIntegerComparison::Equal, ETerritoryIntegerComparison::NotEqual,
		ETerritoryIntegerComparison::AtLeast, ETerritoryIntegerComparison::AtMost, ETerritoryIntegerComparison::GreaterThan, ETerritoryIntegerComparison::LessThan};
	for (const auto Operation : Operations)
	{
		Check.Comparison = Operation;
		for (const int32 Threshold : {299, 300, 301})
		{
			Check.Quantity = Threshold; Rule.InventoryStopConditions = {Check};
			const int32 Before = F.Count(Other);
			const bool bBlocked = UTerritoryGarrisonCondition::CompareValues(300, Operation, Threshold);
			TestEqual(TEXT("Each stock comparison controls real Native production at both boundaries"), F.Run(Rule, Result), !bBlocked);
			TestEqual(TEXT("The exact comparison produces either zero or one item"), F.Count(Other), Before + (bBlocked ? 0 : 1));
		}
	}
	TestTrue(TEXT("Counts above int32 cannot wrap below a stock threshold"),
		UTerritoryGarrisonCondition::CompareValues(int64(MAX_int32) + 1, ETerritoryIntegerComparison::GreaterThan, MAX_int32));
	Check.Comparison = ETerritoryIntegerComparison::AtLeast; Check.Quantity = 300;
	Rule.InventoryStopConditions = {Check};
	Rule.InventoryStopConditions[0].bEnabled = false;
	TestTrue(TEXT("Disabled checks do not block production"), F.Run(Rule, Result));
	Rule.InventoryStopConditions.Add(Check);
	TestFalse(TEXT("Any enabled matching check pauses the whole rule"), F.Run(Rule, Result));
	Rule.InventoryStopConditions[1].ItemClass = nullptr;
	TestFalse(TEXT("An enabled malformed check fails closed"), F.Run(Rule, Result));
	TestEqual(TEXT("Malformed settings report invalid profile"), Result.Status, ETerritoryProductionStatus::InvalidProfile);
	Rule = RefillRule();
	const FTerritoryProductionStockCap DuplicateCap = Rule.OutputStockCaps[0];
	Rule.OutputStockCaps.Add(DuplicateCap);
	TestFalse(TEXT("Duplicate enabled output caps are rejected"), F.Run(Rule, Result));
	Rule.OutputStockCaps[1].bEnabled = false;
	Rule.OutputStockCaps[0].ItemClass = Other;
	TestFalse(TEXT("A cap must name a real output"), F.Run(Rule, Result));
	Rule.OutputStockCaps.Reset();
	Rule.Outputs = {StockRate(UTerritoryAuditResourceA::StaticClass(), 1)};
	F.Inventory->ConsumeItemsOfClass(Ammo, 1);
	const int32 ChildBefore = F.Count(Ammo);
	TestFalse(TEXT("Native child-stack routing cannot credit the wrong output class"), F.Run(Rule, Result));
	TestEqual(TEXT("A rejected base-item recipe leaves the child stack unchanged"), F.Count(Ammo), ChildBefore);
	TestTrue(TEXT("The unsafe exact-class transaction changes no items"), Result.InputsConsumed.IsEmpty() && Result.OutputsProduced.IsEmpty());
	Rule = RefillRule();
	F.Account->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("A client cannot bypass the server recipe gate"), F.Run(Rule, Result));
	F.Account->SetRole(ROLE_Authority);

	// Exercise the actual notification handler using Native player-state identity.
	UClass* ControllerClass = LoadClass<ANarrativePlayerController>(nullptr,
		TEXT("/NarrativePro/Pro/Core/BP/Framework/BP_NarrativePlayerController.BP_NarrativePlayerController_C"));
	if (!TestNotNull(TEXT("Native authored controller supplies stable save identity"), ControllerClass)) return false;
	auto* Controller = F.World->SpawnActor<ANarrativePlayerController>(ControllerClass);
	auto* PlayerState = F.World->SpawnActor<ANarrativePlayerState>();
	FindFProperty<FObjectPropertyBase>(Controller->GetClass(), TEXT("PlayerState"))->SetObjectPropertyValue_InContainer(Controller, PlayerState);
	Cast<INarrativeTeamAgentInterface>(PlayerState)->AddFaction(F.Faction);
	auto* Management = NewObject<UTerritoryPlayerManagementComponent>(Controller);
	Controller->AddInstanceComponent(Management);
	auto* Settings = GetMutableDefault<UTerritoryDeveloperSettings>();
	TGuardValue<FTerritoryNotificationSettings> NotificationGuard(Settings->Notifications, FTerritoryNotificationSettings());
	Settings->Notifications.bRecordResourceProduction = true;
	Settings->Notifications.bShowResourceEarningsOnHUD = true;
	Settings->Notifications.MinimumResourceUnitsForHUDNotification = 1;
	F.Inventory->ConsumeItemsOfClass(Ammo, 1);
	Rule.Notifications.SuccessTitle = FText::FromString(TEXT("{Rule}: +{Quantity}"));
	Rule.Notifications.SuccessMessage = FText::FromString(TEXT("Added {Quantity}; used {Inputs}."));
	TestTrue(TEXT("A custom-text refill succeeds"), F.Run(Rule, Result));
	Management->HandleProductionSettled(Result);
	TestTrue(TEXT("A controller without a connection or local player cannot loop or show a message"), Management->GetLiveEvents().IsEmpty());
	Controller->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	Management->HandleProductionSettled(Result);
	const auto Messages = Management->GetLiveEvents();
	TestEqual(TEXT("The enabled rule creates one existing-feed entry"), Messages.Num(), 1);
	if (!Messages.IsEmpty())
	{
		TestEqual(TEXT("Custom title uses the authored name and actual partial amount"), Messages[0].Headline.ToString(), FString(TEXT("Ammo refill: +2")));
		TestEqual(TEXT("Custom body uses verified quantities"), Messages[0].Detail.ToString(), FString(TEXT("Added 2; used 0.")));
	}
	Result.Notifications.bEnabled = false;
	Result.Notifications.SuccessTitle = FText::FromString(TEXT("Must stay silent"));
	Management->HandleProductionSettled(Result);
	TestEqual(TEXT("Master off suppresses HUD/feed without disabling settlement"), Management->GetLiveEvents().Num(), 1);
	F.Run(Rule, Result);
	Management->HandleProductionSettled(Result);
	TestEqual(TEXT("Stock-limited cycles are silent by default"), Management->GetLiveEvents().Num(), 1);
	Result.Notifications.bNotifyAtStockLimit = true;
	Result.Notifications.BlockedTitle = FText::FromString(TEXT("Stock is full"));
	Result.Notifications.BlockedMessage = FText::FromString(TEXT("{Reason}"));
	Management->HandleProductionSettled(Result);
	TestEqual(TEXT("An explicit stock-limit message is supported"), Management->GetLiveEvents().Num(), 2);
	Result.Notifications.bNotifyWhenBlocked = false;
	Result.Status = ETerritoryProductionStatus::MissingInput;
	Management->HandleProductionSettled(Result);
	TestEqual(TEXT("Other blocked messages have their own switch"), Management->GetLiveEvents().Num(), 2);
	Result.bSuccess = true; Result.Status = ETerritoryProductionStatus::Produced;
	Result.Notifications.bNotifyOnSuccess = false;
	Management->HandleProductionSettled(Result);
	TestEqual(TEXT("Success messages have their own switch"), Management->GetLiveEvents().Num(), 2);
	const auto* RPC = Management->GetClass()->FindFunctionByName(TEXT("ClientReceiveProductionResult"));
	TestTrue(TEXT("Verified messages use an owning-client reliable RPC"), RPC && RPC->HasAllFunctionFlags(FUNC_Net | FUNC_NetClient | FUNC_NetReliable));
	TestNotNull(TEXT("Blueprint rules expose notification controls"), FTerritoryProductionRule::StaticStruct()->FindPropertyByName(TEXT("Notifications")));
	TestNotNull(TEXT("Blueprint rules expose inventory checks"), FTerritoryProductionRule::StaticStruct()->FindPropertyByName(TEXT("InventoryStopConditions")));
	TestEqual(TEXT("Stock limit has readable UI status"), UTerritoryUIBlueprintLibrary::GetProductionStatusText(ETerritoryProductionStatus::StockLimited).ToString(), FString(TEXT("Stock limit reached")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFProductionStockRestore,
	"TerritoryFramework.Production.Regression.StockLimitCatchupRestoreAndCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFProductionStockRestore::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	FProductionStockWorld F;
	UClass* Ammo = UTerritoryAuditResourceB::StaticClass();
	UClass* Input = UAmmoItem::StaticClass();
	UClass* Watch = UTerritoryAuditResourceC::StaticClass();
	auto* Profile = NewObject<UTerritoryProductionProfile>();
	Profile->Rules = {RefillRule()};
	Profile->Rules[0].Notifications.bEnabled = false;
	FTerritoryProductionSiteRecord Site;
	Site.StateRulesVersion = 1; Site.TerritoryGUID = FGuid(711, 712, 713, 714);
	Site.OwnerFaction = F.Faction; Site.TerritoryState = ETerritoryState::Claimed; Site.ProductionProfile = Profile;
	FTerritoryProductionCheckpoint Checkpoint;
	Checkpoint.TerritoryGUID = Site.TerritoryGUID; Checkpoint.OwnerFaction = F.Faction;
	Checkpoint.RuleTag = Profile->Rules[0].RuleTag; Checkpoint.LastProcessedCycle = 0;
	F.Cycle(7);
	F.Economy->RestoreProductionState({Checkpoint}, {Site}, {});
	F.Economy->ProcessResourceProduction();
	TestEqual(TEXT("A restored detached site uses live Native stock through seven missed cycles"), F.Count(Ammo), 300);
	TestEqual(TEXT("Stock-limited catch-up cycles expire through the current clock"), F.Economy->GetProductionCheckpoints()[0].LastProcessedCycle, int64(7));
	TestEqual(TEXT("Detached site retains the precise limit status"), F.Economy->GetAllProductionSites()[0].LastStatus, ETerritoryProductionStatus::StockLimited);
	const auto SavedSites = F.Economy->GetAllProductionSites();
	const auto SavedCheckpoints = F.Economy->GetProductionCheckpoints();
	F.Inventory->PrepareForSave_Implementation();
	F.Inventory->ConsumeItemsOfClass(Ammo, 200);
	F.Inventory->Load_Implementation();
	F.Economy->RestoreProductionState(SavedCheckpoints, SavedSites, F.Economy->GetAllResourceSnapshots());
	TestEqual(TEXT("Narrative inventory restores the exact capped stock"), F.Count(Ammo), 300);
	F.Inventory->ConsumeItemsOfClass(Ammo, 1);
	F.Economy->ProcessResourceProduction();
	TestEqual(TEXT("Loading or same-cycle calls cannot replay expired full-stock cycles"), F.Count(Ammo), 299);
	F.Cycle(8); F.Economy->ProcessResourceProduction();
	TestEqual(TEXT("The next cycle resumes with only the missing amount"), F.Count(Ammo), 300);
	F.Economy->UnregisterFactionResourceAccount(F.Faction, F.Account);
	F.Cycle(9); F.Economy->ProcessResourceProduction();
	TestEqual(TEXT("An unavailable inventory is not mistaken for zero stock"), F.Economy->GetAllProductionSites()[0].LastStatus, ETerritoryProductionStatus::StorageUnavailable);
	F.Economy->RegisterFactionResourceAccount(F.Faction, F.Account);
	F.Economy->ProcessResourceProduction();
	TestEqual(TEXT("A returning resource account is checked before deferred production"), F.Count(Ammo), 300);

	// A real Native input callback changes a check-only item. Cancel the recipe,
	// restore its input and leave that independent external change alone.
	FTerritoryProductionRule Rule = RefillRule();
	Rule.OutputStockCaps.Reset(); Rule.Outputs[0].QuantityPerCycle = 1;
	Rule.Inputs = {StockRate(Input, 2)};
	FTerritoryProductionStockCondition Condition; Condition.ItemClass = Watch; Condition.Quantity = 1;
	Rule.InventoryStopConditions = {Condition};
	F.Inventory->TryAddItemFromClass(Input, 2, false);
	auto* Probe = NewObject<UTerritoryAuditEventProbe>();
	bool bCallback = false;
	Probe->ItemRemovedCallback = [&](UNarrativeItem*, int32)
	{
		if (!bCallback) { bCallback = true; F.Inventory->TryAddItemFromClass(Watch, 1, false); }
	};
	F.Inventory->OnItemRemoved.AddDynamic(Probe, &UTerritoryAuditEventProbe::ItemRemoved);
	FTerritoryProductionResult Result;
	TestFalse(TEXT("A stock-check change during a real Native callback cancels the recipe"), F.Run(Rule, Result));
	Probe->ItemRemovedCallback = nullptr;
	F.Inventory->OnItemRemoved.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::ItemRemoved);
	TestTrue(TEXT("The actual callback ran"), bCallback);
	TestEqual(TEXT("Cancellation returns the recipe input"), F.Count(Input), 2);
	TestEqual(TEXT("Cancellation adds no output"), F.Count(Ammo), 300);
	TestEqual(TEXT("Compensation preserves independently changed check-only stock"), F.Count(Watch), 1);
	TestEqual(TEXT("An independent check-only change is not a rollback failure"), Result.Status, ETerritoryProductionStatus::SettlementChanged);
	return true;
}

#endif
