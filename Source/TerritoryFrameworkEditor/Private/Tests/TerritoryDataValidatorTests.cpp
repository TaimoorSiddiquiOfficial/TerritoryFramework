#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AssetRegistry/AssetData.h"
#include "AI/NPCDefinition.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "DataValidation/TerritoryDataValidator.h"
#include "Misc/DataValidation.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryDataValidatorModernApi,
	"TerritoryFramework.Editor.DataValidation.ModernApiExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryDataValidatorModernApi::RunTest(const FString& Parameters)
{
	UTerritoryDataValidator* Validator = NewObject<UTerritoryDataValidator>();
	UTerritoryCounterAttackProfile* Profile = NewObject<UTerritoryCounterAttackProfile>();
	const FAssetData AssetData(Profile);
	const TConstArrayView<FAssetData> NoAssociatedAssets;

	FDataValidationContext ValidContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestTrue(TEXT("Modern validator accepts counterattack profiles"),
		Validator->CanValidateAsset_Implementation(AssetData, Profile, ValidContext));
	TestEqual(TEXT("Valid profile completes the modern validation path"),
		Validator->ValidateLoadedAsset_Implementation(AssetData, Profile, ValidContext),
		EDataValidationResult::Valid);
	TestEqual(TEXT("Empty faction forces are a warning, not an error"),
		ValidContext.GetNumErrors(), 0u);
	TestTrue(TEXT("Empty faction forces emit a validation warning"),
		ValidContext.GetNumWarnings() > 0u);

	Profile->MaxConsecutiveSpawnFailures = 0;
	FDataValidationContext InvalidContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("Invalid profile fails the modern validation path"),
		Validator->ValidateLoadedAsset_Implementation(AssetData, Profile, InvalidContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("Invalid spawn failure bound emits an error"),
		InvalidContext.GetNumErrors() > 0u);

	Profile->MaxConsecutiveSpawnFailures = 5;
	UNPCDefinition* Definition = NewObject<UNPCDefinition>(Profile);
	Definition->NPCClassPath = ATerritoryAssaultCharacter::StaticClass();
	Definition->bAllowMultipleInstances = true;
	FTerritoryFactionAssaultConfig Force;
	Force.Faction = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	Force.AttackerDefinition = Definition;
	Profile->FactionForces = {Force};
	FDataValidationContext SpawnReadyContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("Native Territory assault class passes complete Narrative spawn validation"),
		Validator->ValidateLoadedAsset_Implementation(
			AssetData, Profile, SpawnReadyContext),
		EDataValidationResult::Valid);
	TestEqual(TEXT("Spawn-ready class emits no validation error"),
		SpawnReadyContext.GetNumErrors(), 0u);

	Definition->bAllowMultipleInstances = false;
	FDataValidationContext SingleInstanceContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("A multi-pawn force rejects a single-instance Narrative definition"),
		Validator->ValidateLoadedAsset_Implementation(
			AssetData, Profile, SingleInstanceContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("Single-instance force mismatch emits a validation error"),
		SingleInstanceContext.GetNumErrors() > 0u);
	Definition->bAllowMultipleInstances = true;

	Definition->NPCClassPath = ANarrativeNPCCharacter::StaticClass();
	FDataValidationContext WrongPawnContext(
		false,
		EDataValidationUsecase::Script,
		NoAssociatedAssets);
	TestEqual(TEXT("A non-Territory Narrative pawn is rejected for physical assaults"),
		Validator->ValidateLoadedAsset_Implementation(
			AssetData, Profile, WrongPawnContext),
		EDataValidationResult::Invalid);
	TestTrue(TEXT("Invalid physical pawn emits a validation error"),
		WrongPawnContext.GetNumErrors() > 0u);

	return true;
}

#endif
