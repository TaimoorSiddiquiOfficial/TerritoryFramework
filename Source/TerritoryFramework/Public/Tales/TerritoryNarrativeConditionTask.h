#pragma once

#include "CoreMinimal.h"
#include "Tales/QuestTask.h"
#include "TerritoryNarrativeConditionTask.generated.h"

class UNarrativeCondition;
class UNarrativeNodeBase;

/**
 * Makes Narrative Conditions functional inside a Narrative Quest branch.
 *
 * Narrative Pro currently evaluates conditions for Dialogue nodes and Events,
 * but not for Quest states/branches. This task uses Narrative's own condition
 * evaluator (including Not, target filters, and party policies), so generated
 * Territory missions remain normal Narrative Quests without a second runtime.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Wait For Narrative Conditions",
		ToolTip="Completes only while every Narrative Condition passes. Use this for a manually authored Quest branch, or let a Territory Quest Cascade generate it automatically."))
class TERRITORYFRAMEWORK_API UTerritoryNarrativeConditionTask final
	: public UNarrativeTask
{
	GENERATED_BODY()

public:
	UTerritoryNarrativeConditionTask();

	/** All requirements use AND logic. Use inherited Not on one condition to invert it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category="Condition Gate",
		meta=(ToolTip="Every condition must pass. Easy example: Diplomacy is War AND the player is inside the target Place. Use the condition's inherited Not checkbox for a negative requirement."))
	TArray<TObjectPtr<UNarrativeCondition>> Conditions;

	/** Friendly wording used in the quest journal when this task is visible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Condition Gate",
		meta=(ToolTip="Optional player-friendly requirement name. Easy example: Wait until the owner is ready to negotiate. Generated gates are hidden, so this mainly helps manually authored quests."))
	FText RequirementDescription;

	/** How often the server reevaluates the conditions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Condition Gate",
		meta=(ClampMin="0.05", ClampMax="10.0", Units="s",
			ToolTip="Server check interval. 0.25 seconds is responsive and inexpensive for story conditions. Use a slower value for conditions that do not need instant reaction."))
	float EvaluationInterval = 0.25f;

	/** When true, a condition only needs to pass once; otherwise it must remain true until the route completes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Condition Gate",
		meta=(ToolTip="Disabled means the requirements must stay true until every task on the route completes. Enable for a one-time checkpoint such as 'the player was seen once'."))
	bool bLatchOnceSatisfied = false;

	UFUNCTION(BlueprintPure, Category="Territory|Narrative Conditions")
	bool AreGateConditionsMet() const;

	virtual FText GetTaskDescription_Implementation() const override;
	virtual FText GetTaskNodeDescription_Implementation() const override;

protected:
	virtual void BeginTask() override;
	virtual void TickTask_Implementation() override;
	virtual void EndTask() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UNarrativeNodeBase> ConditionProbe;

	bool bHasLatched = false;
	mutable bool bEvaluatingConditions = false;
	friend class FTFTalesConditionCallbacks;
};
