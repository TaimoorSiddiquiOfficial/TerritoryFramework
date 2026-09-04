#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/TerritoryDistractionAbility.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryPropertyTags.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "UI/TerritoryJournalWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPropertyBenefitAuthoringContract,
	"TerritoryFramework.PropertyBenefits.AuthoringContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPropertyBenefitAuthoringContract::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestNotNull(TEXT("Place Definitions expose a semantic Property role"),
		UTerritoryPlaceDefinition::StaticClass()->FindPropertyByName(
			TEXT("PropertyRoleTag")));
	TestNotNull(TEXT("Place Definitions expose upgrade-level Gameplay Benefits"),
		UTerritoryPlaceDefinition::StaticClass()->FindPropertyByName(
			TEXT("GameplayBenefits")));
	const UScriptStruct* Benefit = FTerritoryPropertyGameplayBenefit::StaticStruct();
	TestNotNull(TEXT("Benefit tiers grant a stable Gameplay Tag"),
		Benefit->FindPropertyByName(TEXT("BenefitTag")));
	TestNotNull(TEXT("Benefit tiers grant Narrative abilities"),
		Benefit->FindPropertyByName(TEXT("GrantedAbilities")));
	TestNotNull(TEXT("Benefit tiers grant revocable Gameplay Effects"),
		Benefit->FindPropertyByName(TEXT("GrantedGameplayEffects")));
	TestNotNull(TEXT("Benefit tiers expose Narrative weapon unlocks"),
		Benefit->FindPropertyByName(TEXT("UnlockedWeaponItems")));
	TestTrue(TEXT("Arms Shop role tag is registered"),
		TerritoryPropertyTags::ArmsShopRole.GetTag().IsValid());
	TestTrue(TEXT("Weapon Upgrades benefit tag is registered"),
		TerritoryPropertyTags::WeaponUpgradesBenefit.GetTag().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPropertyBenefitNarrativeRuntimeContract,
	"TerritoryFramework.PropertyBenefits.NarrativeRuntimeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPropertyBenefitNarrativeRuntimeContract::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UClass* Management = UTerritoryPlayerManagementComponent::StaticClass();
	TestNotNull(TEXT("Owning player bridge reconciles Property benefits"),
		Management->FindFunctionByName(TEXT("RefreshOwnedPropertyBenefits")));
	TestNotNull(TEXT("Blueprints can inspect aggregate Property benefit tags"),
		Management->FindFunctionByName(TEXT("GetGrantedPropertyBenefitTags")));
	TestNotNull(TEXT("Journal can request the existing authoritative Property upgrade path"),
		Management->FindFunctionByName(TEXT("RequestUpgradeProperty")));
	const UFunction* UpgradeRPC = Management->FindFunctionByName(
		TEXT("ServerRequestUpgradeProperty"));
	TestNotNull(TEXT("Property upgrade request has a server RPC"), UpgradeRPC);
	if (UpgradeRPC)
	{
		TestTrue(TEXT("Property upgrade RPC is server-authoritative"),
			UpgradeRPC->HasAnyFunctionFlags(FUNC_Net | FUNC_NetServer));
		TestTrue(TEXT("Property upgrade RPC is reliable"),
			UpgradeRPC->HasAnyFunctionFlags(FUNC_NetReliable));
	}
	TestNotNull(TEXT("Property upgrade result is available to UI and Blueprints"),
		Management->FindPropertyByName(TEXT("OnPropertyUpgradeResult")));
	TestNotNull(TEXT("Territory UI exposes one typed row per Property benefit"),
		FTerritoryPropertyBenefitOperationsView::StaticStruct());
	const UScriptStruct* View = FTerritoryPropertyBenefitOperationsView::StaticStruct();
	TestNotNull(TEXT("Benefit UI exposes the next upgrade cost"),
		View->FindPropertyByName(TEXT("NextUpgradeCost")));
	TestNotNull(TEXT("Benefit UI exposes server-request eligibility"),
		View->FindPropertyByName(TEXT("bCanRequestUpgrade")));
	TestNotNull(TEXT("Territory UI can query District Property benefits"),
		UTerritoryUIBlueprintLibrary::StaticClass()->FindFunctionByName(
			TEXT("GetPropertyBenefitOperationsViews")));
	TestNotNull(TEXT("Territory Journal exposes the generated Benefits tab button"),
		UTerritoryJournalWidget::StaticClass()->FindPropertyByName(
			TEXT("Btn_BenefitsDetailTab")));
	TestTrue(TEXT("Distraction throw is a Narrative Gameplay Ability available to benefit tiers"),
		UTerritoryDistractionAbility::StaticClass()->IsChildOf(
			UNarrativeGameplayAbility::StaticClass()));
	return true;
}

#endif
