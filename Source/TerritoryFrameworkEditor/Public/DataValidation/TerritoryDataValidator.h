#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "TerritoryDataValidator.generated.h"

class ATerritoryVolume;
class ULevel;

/**
 * Editor data validator for TerritoryFramework assets.
 * Hooks into Unreal's Data Validation system (Validate Packages / CI).
 *
 * Checks for:
 * - Duplicate territory tags
 * - Missing/invalid territory GUIDs
 * - Duplicate GUIDs
 * - Missing parent territory references
 * - Self-referencing parent (cycle)
 * - Incorrect parent class type (Property→City, District→Property, etc.)
 * - Empty territory tags or display names
 * - Invalid faction prefixes
 * - Multiple TerritoryWorldState/SavableData actors
 * - Negative economy configuration
 */
UCLASS()
class TERRITORYFRAMEWORKEDITOR_API UTerritoryDataValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	UTerritoryDataValidator();

	// UEditorValidator interface
	virtual bool CanValidateAsset_Implementation(UObject* InAsset) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(
		UObject* InAsset, TArray<FText>& ValidationErrors) override;

	// Manual validation API (callable from editor utilities)
	static bool ValidateLevel(ULevel* Level, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
	static bool ValidateTerritory(ATerritoryVolume* Territory, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);

private:
	static void CheckDuplicateTags(ULevel* Level, TArray<FString>& OutErrors);
	static void CheckDuplicateGUIDs(ULevel* Level, TArray<FString>& OutErrors);
	static void CheckHierarchyIntegrity(ULevel* Level, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
	static void CheckSingletonActors(ULevel* Level, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
	static void CheckEconomyConfig(ATerritoryVolume* Territory, TArray<FString>& OutWarnings);
};
