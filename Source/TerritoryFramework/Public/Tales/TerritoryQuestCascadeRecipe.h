#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Tales/QuestTask.h"
#include "Tales/NarrativeEvent.h"
#include "TerritoryQuestCascadeRecipe.generated.h"

/**
 * The kind of Narrative Quest state a cascade recipe will create.
 * Objective states keep the quest running. Success and Failure states end it.
 */
UENUM(BlueprintType)
enum class ETerritoryQuestCascadeStateType : uint8
{
	Objective UMETA(DisplayName="Objective",
		ToolTip="The quest remains active and exposes its outgoing branches. Easy example: Reach the fort, then start the Clear Guards objective."),
	Success UMETA(DisplayName="Success",
		ToolTip="Reaching this state completes the Narrative Quest. Easy example: the owner accepts the handover and the mission succeeds."),
	Failure UMETA(DisplayName="Failure",
		ToolTip="Reaching this state fails the Narrative Quest. Easy example: the rescue target dies before the player escapes.")
};

/**
 * One Narrative Quest branch generated from a reusable recipe.
 * Every task in Tasks must complete. To author alternatives, create two branches.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryQuestCascadeBranch
{
	GENERATED_BODY()

	/** Stable ID used by Narrative save data and Blueprint node selectors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Branch",
		meta=(ToolTip="A unique stable ID for this route. Do not rename it after shipping saves. Easy example: ClearBlacksmithDefenders."))
	FName BranchID = NAME_None;

	/** Optional journal/editor explanation for this route. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Branch", meta=(MultiLine=true,
		ToolTip="What the player must accomplish on this route. Easy example: Defeat the guards and claim the Blacksmith for the Regime."))
	FText Description;

	/** ID of the recipe state entered after every task on this branch is complete. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Branch",
		meta=(ToolTip="The State ID reached when every task below completes. Easy example: HandoverProperty."))
	FName DestinationStateID = NAME_None;

	/** Hide this branch from the standard Narrative objective UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Branch",
		meta=(ToolTip="Hide the route from the player while keeping it active. Easy example: a secret fail route triggered by a hidden timer task."))
	bool bHidden = false;

	/**
	 * Reusable Narrative tasks for this route. Narrative requires every task in
	 * this array to complete before advancing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category="Branch|Tasks",
		meta=(TitleProperty="DescriptionOverride",
			ToolTip="All tasks in this list are AND requirements. Easy example: Kill 3 Guards plus Capture Blacksmith must both complete. For Kill OR Sneak, create two branches instead."))
	TArray<TObjectPtr<UNarrativeTask>> Tasks;

	/** Narrative Events copied onto the generated branch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category="Branch|Events",
		meta=(ToolTip="Events run by Narrative when this route starts, ends, or both, according to each event's Event Runtime. Easy example: declare war when the assault route starts."))
	TArray<TObjectPtr<UNarrativeEvent>> Events;
};

/** One state and all of the routes that may leave it. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryQuestCascadeState
{
	GENERATED_BODY()

	/** Stable ID used by Narrative save data and Blueprint node selectors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="State",
		meta=(ToolTip="A unique stable ID for this story step. The Start State ID must name one of these rows. Easy example: AssaultBlacksmith."))
	FName StateID = NAME_None;

	/** Whether this is a normal objective, success ending, or failure ending. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="State",
		meta=(ToolTip="Choose Objective for normal play, Success to complete the quest, or Failure to fail it. Easy example: PropertySecured is Success."))
	ETerritoryQuestCascadeStateType Type =
		ETerritoryQuestCascadeStateType::Objective;

	/** Player-facing state/journal text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="State", meta=(MultiLine=true,
		ToolTip="The current story situation shown by Narrative. Easy example: The defenders are down. Find the owner and negotiate a handover."))
	FText Description;

	/** Narrative Events copied onto the generated state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category="State|Events",
		meta=(ToolTip="Events run by Narrative when this state starts, ends, or both. Easy example: unlock the Farm when the Blacksmith success state begins."))
	TArray<TObjectPtr<UNarrativeEvent>> Events;

	/** Routes leaving an Objective state. Terminal states must leave this empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="State|Routes",
		meta=(TitleProperty="BranchID",
			ToolTip="Alternative routes from this state. Each route owns one or more AND tasks. Easy example: Assault and Stealth are two alternative branches leading to the same Handover state."))
	TArray<FTerritoryQuestCascadeBranch> Branches;
};

/** Result of validating a reusable story cascade before a Quest is created. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryQuestCascadeValidation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	TArray<FText> Errors;

	UPROPERTY(BlueprintReadOnly, Category="Quest Cascade")
	TArray<FText> Warnings;
};

/**
 * Reusable, edit-time story recipe for creating a normal Narrative Quest.
 *
 * This asset is not a second quest runtime. The Territory editor copies its
 * state/branch/task templates into a new UQuestBlueprint. Narrative Pro remains
 * the only runtime, replication, journal, marker, and save-game authority.
 *
 * Easy example:
 *   Assault -> [Kill Enemy + Capture Territory] -> Handover -> [Talk/Interact]
 *   -> Success.
 */
UCLASS(BlueprintType,
	meta=(DisplayName="Territory Narrative Quest Cascade Recipe",
		ToolTip="Build reusable story patterns that generate ordinary Narrative Quests. Easy example: reuse a Kill Guards -> Capture Place -> Handover -> Success pattern for every District Place."))
class TERRITORYFRAMEWORK_API UTerritoryQuestCascadeRecipe final
	: public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Name copied into the generated Narrative Quest journal entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest",
		meta=(ToolTip="Player-facing quest name copied into Narrative. Easy example: Liberate the Blacksmith."))
	FText QuestName;

	/** Description copied into the generated Narrative Quest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(MultiLine=true,
		ToolTip="Player-facing quest summary copied into Narrative. Easy example: Remove the occupation and convince the owner to support your faction."))
	FText QuestDescription;

	/** State used as Narrative's root/start state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest",
		meta=(ToolTip="The State ID where the quest begins. It must be an Objective state. Easy example: ApproachBlacksmith."))
	FName StartStateID = NAME_None;

	/** Complete reusable quest graph specification. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest",
		meta=(TitleProperty="StateID",
			ToolTip="All objective and ending states in the reusable story. Easy example: Approach, ClearDefenders, Handover, Success, and OwnerDiedFailure."))
	TArray<FTerritoryQuestCascadeState> States;

	/** Check IDs, destinations, task templates, endings, and reachability. */
	UFUNCTION(BlueprintPure, Category="Territory|Narrative Quest Cascade")
	FTerritoryQuestCascadeValidation ValidateRecipe() const;

	/** Easy-English, read-only preview useful during quest planning and review. */
	UFUNCTION(BlueprintPure, Category="Territory|Narrative Quest Cascade")
	FString BuildPlainTextPreview() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context)
		const override;
#endif
};

