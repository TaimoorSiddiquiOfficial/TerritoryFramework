#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerritoryDefinitionEditorLibrary.generated.h"

class UTerritoryCityDefinition;

USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORKEDITOR_API FTerritoryDefinitionSyncReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|Definition")
	int32 UpdatedActors = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Definition")
	int32 CreatedActors = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Definition")
	TArray<FString> Errors;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Definition")
	TArray<FString> Warnings;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Definition")
	bool bSucceeded = false;
};

/** Editor-only builder for a complete City -> District -> Place definition tree. */
UCLASS()
class TERRITORYFRAMEWORKEDITOR_API UTerritoryDefinitionEditorLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Updates actors already linked by definition/tag and optionally creates missing
	 * Blueprint actors from each template. Runtime state is not changed in PIE.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Definition|Editor",
		meta=(DisplayName="Synchronize Territory City In Current Level"))
	static FTerritoryDefinitionSyncReport SynchronizeCityInCurrentLevel(
		UTerritoryCityDefinition* CityDefinition,
		FTransform NewCityAnchor,
		bool bCreateMissingActors,
		bool bMoveExistingActorsToDefinitionTransforms);
};
