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
 * - Missing bounds shape
 * - Guard config mismatch (BT without NPC definition, etc.)
 * - Duplicate territory display names
 * - Missing parent tag on districts/properties
 * - Orphaned guard spawn points (no parent territory)
 * - City with parent tag set (cities are top-level)
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
	static void CheckDuplicateDisplayNames(ULevel* Level, TArray<FString>& OutWarnings);
	static void CheckGuardConfig(ATerritoryVolume* Territory, TArray<FString>& OutWarnings);
	static void CheckBoundsShape(ATerritoryVolume* Territory, TArray<FString>& OutWarnings);
	static void CheckOrphanedSpawnPoints(ULevel* Level, TArray<FString>& OutWarnings);
	static void CheckMissingParentTags(ULevel* Level, TArray<FString>& OutWarnings);
};
