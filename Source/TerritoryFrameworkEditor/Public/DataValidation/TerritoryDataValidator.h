#pragma once

#include "CoreMinimal.h"
#include "TerritoryDataValidator.generated.h"

class ATerritoryVolume;
class ULevel;

/**
 * Editor data validator for territory assets.
 * Checks for: duplicate tags, missing GUIDs, invalid faction tags,
 * hierarchy cycles, missing parent references, and orphaned territories.
 */
UCLASS()
class TERRITORYFRAMEWORKEDITOR_API UTerritoryDataValidator : public UObject
{
	GENERATED_BODY()

public:
	static void Register();
	static void Unregister();

	/** Validate all territory actors in a level */
	static bool ValidateLevel(ULevel* Level, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);

	/** Validate a single territory actor */
	static bool ValidateTerritory(ATerritoryVolume* Territory, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);

	/** Check for duplicate tags across all territories in a level */
	static void CheckDuplicateTags(ULevel* Level, TArray<FString>& OutErrors);

	/** Check for duplicate GUIDs across all territories in a level */
	static void CheckDuplicateGUIDs(ULevel* Level, TArray<FString>& OutErrors);

	/** Check parent-child hierarchy integrity */
	static void CheckHierarchyIntegrity(ULevel* Level, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
};
