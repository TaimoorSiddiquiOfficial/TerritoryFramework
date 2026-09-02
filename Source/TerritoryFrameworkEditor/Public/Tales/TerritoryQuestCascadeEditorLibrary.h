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

	/** Hidden functional adapters generated for state/branch conditions. */
	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	int32 CreatedConditionGates = 0;

	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	int32 CopiedConditions = 0;

	/** Unsupported state/branch condition rows removed from an existing Quest. */
	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	int32 RemovedQuestNodeConditions = 0;

	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	int32 CopiedEvents = 0;

	/** Automatic save events injected by the recipe checkpoint policy. */
	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	int32 CreatedCheckpointEvents = 0;

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
	/** Suggested NQ_ asset name derived from the recipe asset or Quest Name. */
	UFUNCTION(BlueprintPure, Category="Territory|Narrative Quest Cascade|Editor")
	static FString GetSuggestedQuestAssetName(
		const UTerritoryQuestCascadeRecipe* Recipe);

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

	/**
	 * Move legacy Quest state/branch Conditions into functional hidden condition-gate
	 * Tasks, then clear the unsupported Narrative Quest-node arrays.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Narrative Quest Cascade|Editor",
		meta=(DisplayName="Migrate Quest Node Conditions To Gate Tasks"))
	static FTerritoryQuestCascadeBuildReport MigrateQuestNodeConditionsToGateTasks(
		UQuestBlueprint* QuestBlueprint);
};
