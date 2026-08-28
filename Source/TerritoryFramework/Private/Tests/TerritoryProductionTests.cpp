#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Economy/TerritoryProductionProfile.h"
#include "Economy/TerritoryProductionTags.h"
#include "Economy/TerritoryFactionResourceAccountComponent.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "GameFramework/Actor.h"
#include "Items/AmmoItem.h"
#include "Items/InventoryComponent.h"
#include "Items/NarrativeItem.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryProductionProfileBehaviorTest,
	"TerritoryFramework.Production.Behavior.ProfileValidationAndScaling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryProductionProfileBehaviorTest::RunTest(const FString& Parameters)
{
	FTerritoryResourceRate Rate;
	Rate.ItemClass = UNarrativeItem::StaticClass();
	Rate.QuantityPerCycle = 5;
	Rate.QuantityPerUpgradeLevel = 2;
	int32 Quantity = 0;
	TestTrue(TEXT("A valid resource rate scales"),
		UTerritoryProductionProfile::CalculateScaledQuantity(Rate, 3, 2, Quantity));
	TestEqual(TEXT("Upgrade and catch-up scaling are deterministic"), Quantity, 22);
	TestTrue(TEXT("More upgrades never reduce a valid output"), Quantity >= 10);

	Rate.QuantityPerCycle = MAX_int32;
	Rate.QuantityPerUpgradeLevel = MAX_int32;
	TestFalse(TEXT("Overflowing resource multiplication is rejected"),
		UTerritoryProductionProfile::CalculateScaledQuantity(Rate, 2, 2, Quantity));

	FTerritoryProductionRule Rule;
	Rule.RuleTag = TerritoryProductionTags::FarmLivestock;
	FTerritoryResourceRate Output;
	Output.ItemClass = UNarrativeItem::StaticClass();
	Output.QuantityPerCycle = 5;
	Rule.Outputs.Add(Output);
	FText Failure;
	TestTrue(TEXT("A tagged output rule is valid"),
		UTerritoryProductionProfile::IsRuleConfigurationValid(Rule, Failure));

	Rule.Inputs.Add(Output);
	TestFalse(TEXT("Input/output class overlap is rejected for atomic rollback"),
		UTerritoryProductionProfile::IsRuleConfigurationValid(Rule, Failure));
	Rule.Inputs.Reset();
	Rule.Outputs.Add(Output);
	TestFalse(TEXT("Duplicate output rates are rejected"),
		UTerritoryProductionProfile::IsRuleConfigurationValid(Rule, Failure));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryProductionCycleBehaviorTest,
	"TerritoryFramework.Production.Behavior.CycleAndStateGates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryProductionCycleBehaviorTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("New sites do not receive retroactive production"),
		UTerritoryProductionProfile::CalculatePendingCycleCount(INDEX_NONE, 50, 7), 0);
	TestEqual(TEXT("Same cycle cannot settle twice"),
		UTerritoryProductionProfile::CalculatePendingCycleCount(50, 50, 7), 0);
	TestEqual(TEXT("A normal day processes exactly once"),
		UTerritoryProductionProfile::CalculatePendingCycleCount(50, 51, 7), 1);
	TestEqual(TEXT("Save catch-up is bounded"),
		UTerritoryProductionProfile::CalculatePendingCycleCount(10, 50, 7), 7);

	FTerritoryProductionRule Rule;
	Rule.RuleTag = TerritoryProductionTags::FarmLivestock;
	Rule.MinimumUpgradeLevel = 1;
	FText Failure;
	TestFalse(TEXT("Upgrade gate blocks under-level sites"),
		UTerritoryProductionProfile::CanRuleRunForState(
			Rule, ETerritoryState::Claimed, 0, Failure));
	TestFalse(TEXT("Contested production is paused"),
		UTerritoryProductionProfile::CanRuleRunForState(
			Rule, ETerritoryState::Contested, 1, Failure));
	TestTrue(TEXT("Claimed upgraded production is eligible"),
		UTerritoryProductionProfile::CanRuleRunForState(
			Rule, ETerritoryState::Claimed, 1, Failure));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryProductionInventoryTransactionTest,
	"TerritoryFramework.Production.Behavior.NarrativeInventoryAtomicTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryProductionInventoryTransactionTest::RunTest(const FString& Parameters)
{
	AActor* Account = NewObject<AActor>();
	UNarrativeInventoryComponent* Inventory =
		NewObject<UNarrativeInventoryComponent>(Account);
	Account->AddInstanceComponent(Inventory);
	Inventory->SetCapacity(8);
	Inventory->SetWeightCapacity(100.f);

	const FItemAddResult SeedResult = Inventory->TryAddItemFromClass(
		UNarrativeItem::StaticClass(), 4, false);
	TestEqual(TEXT("Narrative inventory accepts the recipe input"),
		SeedResult.AmountGiven, 4);

	FTerritoryProductionRule Recipe;
	Recipe.RuleTag = TerritoryProductionTags::FarmLivestock;
	FTerritoryResourceRate Input;
	Input.ItemClass = UNarrativeItem::StaticClass();
	Input.QuantityPerCycle = 2;
	Recipe.Inputs.Add(Input);
	FTerritoryResourceRate Output;
	Output.ItemClass = UAmmoItem::StaticClass();
	Output.QuantityPerCycle = 3;
	Recipe.Outputs.Add(Output);

	UTerritoryEconomySubsystem* Economy = NewObject<UTerritoryEconomySubsystem>();
	FTerritoryProductionResult Result;
	const bool bSettled = Economy->ExecuteResourceRecipeOnInventory(
		Inventory, FGameplayTag(), Recipe, 0, 1, Result);
	TestTrue(TEXT("A complete recipe settles"), bSettled);
	TestTrue(TEXT("A successful recipe returns a verified result"), Result.bSuccess);
	TestEqual(TEXT("The input class is debited exactly"),
		Inventory->GetTotalQuantityOfItemExact(
			TSoftClassPtr<UNarrativeItem>(UNarrativeItem::StaticClass()), false), 2);
	TestEqual(TEXT("The output class is credited exactly"),
		Inventory->GetTotalQuantityOfItemExact(
			TSoftClassPtr<UNarrativeItem>(UAmmoItem::StaticClass()), false), 3);

	FTerritoryProductionResult MissingInputResult;
	const bool bRejected = Economy->ExecuteResourceRecipeOnInventory(
		Inventory, FGameplayTag(), Recipe, 0, 2, MissingInputResult);
	TestFalse(TEXT("A recipe with insufficient input is rejected"), bRejected);
	TestEqual(TEXT("Missing input is reported precisely"), MissingInputResult.Status,
		ETerritoryProductionStatus::MissingInput);
	TestEqual(TEXT("A rejected recipe does not consume any input"),
		Inventory->GetTotalQuantityOfItemExact(
			TSoftClassPtr<UNarrativeItem>(UNarrativeItem::StaticClass()), false), 2);
	TestEqual(TEXT("A rejected recipe does not add any output"),
		Inventory->GetTotalQuantityOfItemExact(
			TSoftClassPtr<UNarrativeItem>(UAmmoItem::StaticClass()), false), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryProductionResourceRoutingTest,
	"TerritoryFramework.Production.Behavior.SinglePlayerInventoryRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryProductionResourceRoutingTest::RunTest(const FString& Parameters)
{
	TArray<ANarrativePlayerCharacter*> Players;
	TestNull(TEXT("No online player provides no implicit resource account"),
		UTerritoryEconomySubsystem::SelectSoleOnlineResourceAccount(Players));

	ANarrativePlayerCharacter* First = NewObject<ANarrativePlayerCharacter>();
	Players.Add(First);
	TestEqual(TEXT("The sole Narrative player inventory is the deterministic single-player account"),
		UTerritoryEconomySubsystem::SelectSoleOnlineResourceAccount(Players), First);

	Players.Add(NewObject<ANarrativePlayerCharacter>());
	TestNull(TEXT("Multiple faction players require an explicit shared resource account"),
		UTerritoryEconomySubsystem::SelectSoleOnlineResourceAccount(Players));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryProductionReflectionContractTest,
	"TerritoryFramework.Production.Contract.AuthoritySaveReplicationBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryProductionReflectionContractTest::RunTest(const FString& Parameters)
{
	const UClass* EconomyClass = UTerritoryEconomySubsystem::StaticClass();
	const UFunction* RegisterAccount = EconomyClass->FindFunctionByName(TEXT("RegisterFactionResourceAccount"));
	const UFunction* ProcessProduction = EconomyClass->FindFunctionByName(TEXT("ProcessResourceProduction"));
	const UFunction* ExecuteRecipe = EconomyClass->FindFunctionByName(TEXT("ExecuteResourceRecipe"));
	const UFunction* GetResourceAccount = EconomyClass->FindFunctionByName(TEXT("GetFactionResourceAccount"));
	TestNotNull(TEXT("Resource account registration is exposed"), RegisterAccount);
	TestNotNull(TEXT("Production processing is exposed"), ProcessProduction);
	TestNotNull(TEXT("Crafting-compatible recipe transaction is exposed"), ExecuteRecipe);
	TestNotNull(TEXT("The effective Narrative item recipient is inspectable"), GetResourceAccount);
	if (RegisterAccount)
	{
		TestTrue(TEXT("Resource account registration is authority-only"),
			RegisterAccount->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));
	}
	if (ProcessProduction)
	{
		TestTrue(TEXT("Production mutation is authority-only"),
			ProcessProduction->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));
	}
	if (ExecuteRecipe)
	{
		TestTrue(TEXT("Recipe mutation is authority-only"),
			ExecuteRecipe->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));
	}

	TestNotNull(TEXT("Property exposes an optional production profile"),
		ATerritoryProperty::StaticClass()->FindPropertyByName(TEXT("ProductionProfile")));
#if WITH_EDITORONLY_DATA
	TestTrue(TEXT("Resource routing component is Blueprint spawnable"),
		UTerritoryFactionResourceAccountComponent::StaticClass()->HasMetaData(TEXT("BlueprintSpawnableComponent")));
#endif
	TestNotNull(TEXT("Resource routing exposes registration state"),
		UTerritoryFactionResourceAccountComponent::StaticClass()->FindFunctionByName(
			TEXT("IsResourceAccountRegistered")));
	TestNotNull(TEXT("Resource routing exposes bounded retry configuration"),
		UTerritoryFactionResourceAccountComponent::StaticClass()->FindPropertyByName(
			TEXT("MaxRegistrationAttempts")));
	TestNotNull(TEXT("Single-player automatic resource routing is an authored policy"),
		EconomyClass->FindPropertyByName(TEXT("bUseSoleOnlineFactionPlayerInventory")));
	const UFunction* BuildProductionView =
		UTerritoryUIBlueprintLibrary::StaticClass()->FindFunctionByName(
			TEXT("BuildProductionSiteOperationsView"));
	TestNotNull(TEXT("One-site production UI projection is exposed"), BuildProductionView);
	if (BuildProductionView)
	{
		TestTrue(TEXT("Production UI projection is side-effect free"),
			BuildProductionView->HasAnyFunctionFlags(FUNC_BlueprintPure));
	}

	const UClass* WorldStateClass = ATerritoryWorldState::StaticClass();
	auto HasFlag = [WorldStateClass](FName Name, EPropertyFlags Flag)
	{
		const FProperty* Property = WorldStateClass->FindPropertyByName(Name);
		return Property && Property->HasAnyPropertyFlags(Flag);
	};
	TestTrue(TEXT("Production sites replicate for late join"),
		HasFlag(TEXT("ReplicatedProductionSites"), CPF_Net));
	TestTrue(TEXT("Resource snapshots replicate for late join"),
		HasFlag(TEXT("ReplicatedResourceSnapshots"), CPF_Net));
	TestTrue(TEXT("Production checkpoints save"),
		HasFlag(TEXT("SavedProductionCheckpoints"), CPF_SaveGame));
	TestTrue(TEXT("World Partition site records save"),
		HasFlag(TEXT("SavedProductionSites"), CPF_SaveGame));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryProductionUIRegressionTest,
	"TerritoryFramework.Production.UI.FiltersSearchAndRevision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryProductionUIRegressionTest::RunTest(const FString& Parameters)
{
	FTerritoryDistrictOperationsView View;
	View.bRegistered = true;
	FTerritoryProductionSiteOperationsView Site;
	Site.DisplayName = FText::FromString(TEXT("Riverside Farm"));
	Site.ActiveRuleTag = TerritoryProductionTags::FarmLivestock;
	Site.Status = ETerritoryProductionStatus::MissingInput;
	Site.bBlocked = true;
	FTerritoryResourceOperationsView Resource;
	Resource.ItemClass = UNarrativeItem::StaticClass();
	Resource.DisplayName = FText::FromString(TEXT("Grain"));
	Resource.InputPerCycle = 2;
	Site.Resources.Add(Resource);
	View.ProductionSites.Add(Site);
	View.BlockedProductionSiteCount = 1;

	TestTrue(TEXT("Blocked production filter includes the district"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(
			View, ETerritoryOperationsFilter::ProductionBlocked));
	TestTrue(TEXT("Missing-input filter uses the exact status"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(
			View, ETerritoryOperationsFilter::MissingInputs));
	TestTrue(TEXT("Resource names are searchable"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchSearch(View, TEXT("grain farm")));

	const int32 BlockedRevision =
		UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(View);
	View.ProductionSites[0].Status = ETerritoryProductionStatus::Produced;
	View.ProductionSites[0].Resources[0].StoredQuantity = 5;
	const int32 ProducedRevision =
		UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(View);
	TestNotEqual(TEXT("Production status and stock changes invalidate widgets"),
		BlockedRevision, ProducedRevision);
	return true;
}

#endif
