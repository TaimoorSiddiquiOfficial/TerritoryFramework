#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AssetRegistry/AssetData.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "DataValidation/TerritoryDataValidator.h"
#include "Misc/DataValidation.h"

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

	return true;
}

#endif
