#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerritoryQuestCascadeEditorLibrary.generated.h"

class UQuestBlueprint;
class UTerritoryQuestCascadeRecipe;

/** Report returned after materializing a cascade recipe into Narrative Pro. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORKEDITOR_API FTerritoryQuestCascadeBuildReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	TObjectPtr<UQuestBlueprint> QuestAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	FString QuestPackageName;

	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	int32 CreatedStates = 0;

	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	int32 CreatedBranches = 0;

	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	int32 CreatedTasks = 0;

	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	TArray<FText> Errors;

	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	TArray<FText> Warnings;
};

/**
 * Editor-only bridge from a reusable Territory recipe to Narrative's real Quest
 * Blueprint graph. Generated assets are ordinary UQuestBlueprint assets and do
 * not depend on the recipe at runtime.
 */
UCLASS()
class TERRITORYFRAMEWORKEDITOR_API UTerritoryQuestCascadeEditorLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Create a uniquely named Narrative Quest beside the recipe.
	 * Easy example: DA_QC_LiberatePlace creates NQ_LiberatePlace in the same folder.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Narrative Quest Cascade|Editor",
		meta=(DisplayName="Create Narrative Quest Beside Cascade Recipe"))
	static FTerritoryQuestCascadeBuildReport CreateQuestBesideRecipe(
		UTerritoryQuestCascadeRecipe* Recipe);

	/** Create a uniquely named Narrative Quest in a selected content folder. */
	UFUNCTION(BlueprintCallable, Category="Territory|Narrative Quest Cascade|Editor",
		meta=(DisplayName="Create Narrative Quest From Cascade Recipe"))
	static FTerritoryQuestCascadeBuildReport CreateQuestFromRecipe(
		UTerritoryQuestCascadeRecipe* Recipe,
		const FString& DestinationContentPath,
		const FString& DesiredAssetName);

	/**
	 * Materialize a recipe into an empty Narrative Quest. Existing authored nodes
	 * are never overwritten; use Create Quest for the normal safe workflow.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Narrative Quest Cascade|Editor",
		meta=(DisplayName="Build Empty Narrative Quest From Cascade Recipe"))
	static FTerritoryQuestCascadeBuildReport BuildEmptyQuestFromRecipe(
		UQuestBlueprint* EmptyQuest,
		UTerritoryQuestCascadeRecipe* Recipe);
};

