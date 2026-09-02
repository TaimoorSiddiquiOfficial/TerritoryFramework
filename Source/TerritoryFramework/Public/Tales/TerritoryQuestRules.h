#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerritoryQuestRules.generated.h"

class UQuest;
class UTalesComponent;
class UWorld;

/** Narrative Pro quest state inspected by Territory conditions and counter rules. */
UENUM(BlueprintType)
enum class ETerritoryQuestStateRequirement : uint8
{
	NotStarted UMETA(DisplayName="Not Started"),
	InProgress UMETA(DisplayName="In Progress"),
	Succeeded UMETA(DisplayName="Succeeded"),
	Failed UMETA(DisplayName="Failed"),
	Finished UMETA(DisplayName="Finished (Success or Failure)"),
	StartedOrFinished UMETA(DisplayName="Started at Any Time")
};

/** One part of the normal Territory simulation that an active Narrative Quest may own temporarily. */
UENUM(BlueprintType)
enum class ETerritoryQuestOverrideEffect : uint8
{
	StateRules UMETA(DisplayName="State Rules and State Events"),
	AutomaticCapture UMETA(DisplayName="Automatic Capture and Contesting"),
	AutomaticCounterattacks UMETA(DisplayName="Automatic Counterattacks")
};

/**
 * Makes an assigned Narrative Quest the temporary authority for a Territory.
 * The Territory's live owner/state is never frozen: explicit Quest events may still
 * capture, unlock, or start a wave. Only the selected automatic/default rules pause.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryQuestRuntimeOverrideRule
{
	GENERATED_BODY()

	/** Quest whose saved Narrative state activates this override. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Override",
		meta=(ToolTip="Narrative Quest that temporarily owns this Territory's runtime flow. Easy example: Rescue the Prisoner controls the Place until the Quest succeeds or fails."))
	TSubclassOf<UQuest> QuestClass;

	/** In Progress is recommended. When the Quest ends, normal Territory rules resume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Override",
		meta=(ToolTip="Quest state that activates the override. In Progress is recommended so primary rules resume after success or failure."))
	ETerritoryQuestStateRequirement ActiveQuestState =
		ETerritoryQuestStateRequirement::InProgress;

	/** A City can own its Districts/Places; a District can own its Places. A Place has no children. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Override",
		meta=(ToolTip="Apply this rule to loaded child Territories too. Example: a City-wide lockdown Quest pauses automatic rules in every District and Place below the City."))
	bool bIncludeChildTerritories = false;

	/** Skip State Config entry/exit conditions and events while the Quest owns the flow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Override|Pause",
		meta=(ToolTip="Pause primary State Config conditions and entry/exit events. Explicit Quest capture or unlock events still change the live Territory, and skipped rewards are not replayed later."))
	bool bPauseStateRules = true;

	/** Stop bounds/capture-point pressure; explicit Quest mutations still work. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Override|Pause",
		meta=(ToolTip="Pause automatic capture points and story-bounds contesting. Use an explicit Narrative Territory capture event when the Quest decides the outcome."))
	bool bPauseAutomaticCapture = true;

	/** Stop default ownership-change and recurring counters; explicit Wave events still work. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Override|Pause",
		meta=(ToolTip="Pause automatic and recurring counterattacks. A Wave of Enemies Narrative Event is explicit Quest work and may still launch."))
	bool bPauseAutomaticCounterattacks = true;

	bool Pauses(ETerritoryQuestOverrideEffect Effect) const;
};

/** Small adapter over Narrative Pro's saved and replicated Tales quest state. */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryQuestRulesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Territory|Tales|Quest",
		meta=(DisplayName="Does Narrative Quest State Match"))
	static bool DoesQuestStateMatch(const UTalesComponent* TalesComponent,
		TSubclassOf<UQuest> QuestClass, ETerritoryQuestStateRequirement RequiredState);

	/** Pure truth-table helper used by runtime and native regression tests. */
	static bool DoesQuestStateMatchValues(ETerritoryQuestStateRequirement RequiredState,
		bool bStartedOrFinished, bool bInProgress, bool bSucceeded,
		bool bFailed, bool bFinished);

	/** Shared-world policy: one matching online player's Quest pauses the authored world rule. */
	static bool DoesAnyOnlinePlayerMatchQuestState(const UWorld* World,
		TSubclassOf<UQuest> QuestClass,
		ETerritoryQuestStateRequirement RequiredState,
		const UTalesComponent* OptionalContextTales = nullptr);
};
