#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"

#include "Abilities/TerritoryDistractionAbility.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryPropertyTags.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Interaction/TerritoryDistractionProjectile.h"
#include "Items/EquippableItem.h"
#include "NarrativeGameplayTags.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "UI/TerritoryJournalWidget.h"
#include "UObject/UnrealType.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFProjectDistractionAssetWiring,
	"TerritoryFramework.ProjectFixtures.DistractionAssetWiring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFProjectDistractionAssetWiring::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString RockPackage =
		TEXT("/Game/TerritoryFramework/Stealth/Equippable_Throwable_Rock");
	const FString BlacksmithPackage =
		TEXT("/Game/TerritoryFramework/Definitions/DA_Place_Blacksmith");
	if (!FPackageName::DoesPackageExist(RockPackage)
		|| !FPackageName::DoesPackageExist(BlacksmithPackage))
	{
		AddInfo(TEXT("Project distraction fixtures are not installed; core plugin remains independent."));
		return true;
	}

	UClass* RockClass = LoadClass<UEquippableItem>(nullptr,
		TEXT("/Game/TerritoryFramework/Stealth/Equippable_Throwable_Rock.Equippable_Throwable_Rock_C"));
	UClass* AbilityClass = LoadClass<UTerritoryDistractionAbility>(nullptr,
		TEXT("/Game/TerritoryFramework/Stealth/GA_TerritoryDistraction.GA_TerritoryDistraction_C"));
	const UEquippableItem* Rock = RockClass
		? Cast<UEquippableItem>(RockClass->GetDefaultObject()) : nullptr;
	const UTerritoryDistractionAbility* Ability = AbilityClass
		? Cast<UTerritoryDistractionAbility>(AbilityClass->GetDefaultObject()) : nullptr;
	const UTerritoryPlaceDefinition* Blacksmith =
		LoadObject<UTerritoryPlaceDefinition>(nullptr,
			TEXT("/Game/TerritoryFramework/Definitions/DA_Place_Blacksmith.DA_Place_Blacksmith"));

	TestNotNull(TEXT("Throwable Rock class loads"), RockClass);
	TestNotNull(TEXT("Territory distraction ability class loads"), AbilityClass);
	TestNotNull(TEXT("Blacksmith Definition loads"), Blacksmith);
	if (!Rock || !Ability || !Blacksmith) return false;

	TestTrue(TEXT("Rock directly inherits the Narrative Equippable Item base"),
		RockClass->GetSuperClass() == UEquippableItem::StaticClass());
	const FArrayProperty* AbilitiesProperty = FindFProperty<FArrayProperty>(
		RockClass, TEXT("EquipmentAbilities"));
	TestNotNull(TEXT("Narrative Equippable Item still exposes Equipment Abilities"),
		AbilitiesProperty);
	if (!AbilitiesProperty) return false;
	FScriptArrayHelper AbilityArray(AbilitiesProperty,
		AbilitiesProperty->ContainerPtrToValuePtr<void>(Rock));
	const FClassProperty* AbilityClassProperty =
		CastField<FClassProperty>(AbilitiesProperty->Inner);
	TestNotNull(TEXT("Equipment Abilities remains an array of ability classes"),
		AbilityClassProperty);
	if (!AbilityClassProperty) return false;
	const UClass* GrantedAbilityClass = AbilityArray.Num() > 0
		? Cast<UClass>(AbilityClassProperty->GetObjectPropertyValue(
			AbilityArray.GetRawPtr(0))) : nullptr;
	TestEqual(TEXT("Rock grants exactly one throw ability"),
		AbilityArray.Num(), 1);
	TestTrue(TEXT("Rock grants the configured Territory Blueprint ability"),
		AbilityArray.Num() == 1 && GrantedAbilityClass == AbilityClass);
	const FStructProperty* SlotsProperty = FindFProperty<FStructProperty>(
		RockClass, TEXT("EquippableSlots"));
	const FGameplayTagContainer* EquippableSlots = SlotsProperty
		? SlotsProperty->ContainerPtrToValuePtr<FGameplayTagContainer>(Rock)
		: nullptr;
	TestTrue(TEXT("Rock uses Narrative's canonical throwable mesh slot"),
		EquippableSlots && EquippableSlots->HasTagExact(
			FNarrativeGameplayTags::Get().Equipment_Slot_Throwable));
	const FClassProperty* EquipmentEffectProperty = FindFProperty<FClassProperty>(
		RockClass, TEXT("EquipmentEffect"));
	TestNull(TEXT("Rock does not apply a meaningless zero-value equipment effect"),
		EquipmentEffectProperty
			? Cast<UClass>(EquipmentEffectProperty->GetObjectPropertyValue_InContainer(Rock))
			: nullptr);
	const FFloatProperty* StealthProperty = FindFProperty<FFloatProperty>(
		RockClass, TEXT("StealthRating"));
	TestEqual(TEXT("Rock does not inherit the demo throwable's stealth bonus"),
		StealthProperty ? StealthProperty->GetPropertyValue_InContainer(Rock) : -1.f,
		0.f);

	const FStructProperty* RequiredTagsProperty = FindFProperty<FStructProperty>(
		AbilityClass, TEXT("ActivationRequiredTags"));
	const FGameplayTagContainer* RequiredTags = RequiredTagsProperty
		? RequiredTagsProperty->ContainerPtrToValuePtr<FGameplayTagContainer>(Ability)
		: nullptr;
	TestTrue(TEXT("Blacksmith capability gates the equipped throw ability"),
		RequiredTags && RequiredTags->HasTagExact(
			TerritoryPropertyTags::WeaponUpgradesBenefit));
	TestTrue(TEXT("Ability requires its equipped Narrative item source"),
		Ability->bRequireEquippedNarrativeItemSource);
	TestTrue(TEXT("Ability consumes its source only after a successful throw"),
		Ability->bConsumeSourceItemOnSuccessfulThrow);
	TestFalse(TEXT("Blacksmith no longer grants a duplicate throw ability spec"),
		Blacksmith->GameplayBenefits.ContainsByPredicate(
			[AbilityClass](const FTerritoryPropertyGameplayBenefit& Benefit)
			{
				return Benefit.GrantedAbilities.Contains(AbilityClass);
			}));
	TestTrue(TEXT("Blacksmith still grants the revocable capability tag"),
		Blacksmith->GameplayBenefits.ContainsByPredicate(
			[](const FTerritoryPropertyGameplayBenefit& Benefit)
			{
				return Benefit.BenefitTag
					== TerritoryPropertyTags::WeaponUpgradesBenefit;
			}));

	const ATerritoryDistractionProjectile* Projectile = Ability->ProjectileClass
		? Ability->ProjectileClass->GetDefaultObject<ATerritoryDistractionProjectile>()
		: nullptr;
	const UStaticMesh* PickupMesh = Rock->GetPickupMeshData(1).PickupMesh.LoadSynchronous();
	TestNotNull(TEXT("Configured distraction projectile loads"), Projectile);
	TestNotNull(TEXT("Rock pickup mesh loads"), PickupMesh);
	TestTrue(TEXT("Pickup and projectile use the same authored rock mesh"),
		Projectile && Projectile->Visual
		&& Projectile->Visual->GetStaticMesh() == PickupMesh);
	return true;
}

#endif
