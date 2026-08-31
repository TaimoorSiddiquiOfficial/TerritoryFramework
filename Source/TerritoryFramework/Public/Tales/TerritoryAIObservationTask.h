#pragma once

#include "CoreMinimal.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeActorProvider.h"
#include "Tales/QuestTask.h"
#include "TerritoryAIObservationTask.generated.h"

class ANarrativeNPCController;
class UNPCActivity;
class UNPCGoalItem;

/** Read-only Narrative AI milestones useful in quests and tutorials. */
UENUM(BlueprintType)
enum class ETerritoryAIObservationObjective : uint8
{
	ActorAvailable UMETA(DisplayName="Actor Is Available",
		ToolTip="Complete when the Narrative Actor Provider resolves a live actor."),
	NPCAlive UMETA(DisplayName="NPC Is Alive",
		ToolTip="Complete when the resolved Narrative NPC is alive."),
	NPCDead UMETA(DisplayName="NPC Dies",
		ToolTip="Complete when the resolved Narrative NPC enters its dead state."),
	ReachQuestOwner UMETA(DisplayName="AI Reaches Quest Player",
		ToolTip="Complete when the AI-controlled actor reaches the quest player's pawn."),
	ReachActor UMETA(DisplayName="AI Reaches Actor",
		ToolTip="Complete when the AI-controlled actor reaches the Destination Provider actor."),
	ReachLocation UMETA(DisplayName="AI Reaches Location",
		ToolTip="Complete when the AI-controlled actor reaches Destination Location."),
	HasGoalClass UMETA(DisplayName="AI Receives Goal",
		ToolTip="Complete when Narrative's activity component owns a goal of Goal Class."),
	RunsActivityClass UMETA(DisplayName="AI Runs Activity",
		ToolTip="Complete when Narrative selects an activity of Activity Class."),
	PerceivesQuestOwner UMETA(DisplayName="AI Detects Quest Player",
		ToolTip="Complete when the real AI Perception Component successfully senses the quest player."),
	LosesQuestOwner UMETA(DisplayName="AI Loses Quest Player",
		ToolTip="Complete only after this task observes the AI sense the quest player and then lose every successful stimulus."),
	EntersVehicle UMETA(DisplayName="AI Enters Vehicle",
		ToolTip="Complete when a Narrative NPC controller possesses a pawn other than its owned NPC."),
	LeavesVehicle UMETA(DisplayName="AI Leaves Vehicle",
		ToolTip="Complete only after this task observes a Narrative NPC controlling a vehicle and then controlling its owned NPC again."),
	ClaimsAttackToken UMETA(DisplayName="AI Claims Attack Token",
		ToolTip="Complete when Narrative grants this AI an attack token against the quest player."),
	ReleasesAttackToken UMETA(DisplayName="AI Releases Attack Token",
		ToolTip="Complete only after this task observes the AI hold and then release its attack token against the quest player.")
};

/**
 * Community Narrative Task that observes Narrative actor providers, activities,
 * goals, perception, vehicle possession, and attack tokens. It never commands AI.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="AI Observation Task",
		ToolTip="Narrative Task for AI story milestones without creating a second behaviour, perception, or attack-token system."))
class TERRITORYFRAMEWORK_API UTerritoryAIObservationTask : public UNarrativeTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category="Community Task|AI",
		meta=(ToolTip="Narrative Actor Provider for the NPC, its controller, or another observed actor. Find NPC is recommended for World Partition safe quests."))
	TObjectPtr<UNarrativeActorProvider> TargetProvider;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|AI")
	ETerritoryAIObservationObjective Objective =
		ETerritoryAIObservationObjective::ActorAvailable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category="Community Task|Destination",
		meta=(EditCondition="Objective == ETerritoryAIObservationObjective::ReachActor", EditConditionHides,
			ToolTip="Actor the observed AI must approach."))
	TObjectPtr<UNarrativeActorProvider> DestinationProvider;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|Destination",
		meta=(EditCondition="Objective == ETerritoryAIObservationObjective::ReachLocation", EditConditionHides))
	FVector DestinationLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|Destination",
		meta=(ClampMin="1.0", Units="cm",
			EditCondition="Objective == ETerritoryAIObservationObjective::ReachQuestOwner || Objective == ETerritoryAIObservationObjective::ReachActor || Objective == ETerritoryAIObservationObjective::ReachLocation",
			EditConditionHides,
			ToolTip="Maximum three-dimensional distance that counts as reached."))
	float DistanceTolerance = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|Narrative AI",
		meta=(EditCondition="Objective == ETerritoryAIObservationObjective::HasGoalClass", EditConditionHides))
	TSubclassOf<UNPCGoalItem> GoalClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|Narrative AI",
		meta=(EditCondition="Objective == ETerritoryAIObservationObjective::RunsActivityClass", EditConditionHides))
	TSubclassOf<UNPCActivity> ActivityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|AI",
		meta=(ToolTip="For positive state objectives, allow the task to complete when the state is already true at start. Loss, vehicle-exit, and token-release objectives always require this task to observe the earlier positive state first."))
	bool bCompleteIfAlreadySatisfied = true;

	/** Current read-only state. Transition objectives report only their current end state. */
	UFUNCTION(BlueprintPure, Category="Community Task|Preview",
		meta=(ToolTip="Checks current Narrative AI state without adding goals, selecting activities, changing perception, possession, or attack tokens."))
	bool IsAIStateSatisfiedBy(const AActor* Target) const;

protected:
	virtual void BeginTask() override;
	virtual void EndTask() override;
	virtual void TickTask_Implementation() override;
	virtual FText GetTaskDescription_Implementation() const override;
	virtual FText GetTaskProgressText_Implementation() const override;
	virtual FVector GetNavigationMarkerLocation_Implementation() const override;
	virtual AActor* GetNavigationMarkerAttachActor_Implementation() const override;

private:
	AActor* ResolveTarget() const;
	AActor* ResolveDestination() const;
	ANarrativeNPCController* ResolveController(const AActor* Target) const;
	AActor* ResolveSpatialActor(const AActor* Target) const;
	UNarrativeAbilitySystemComponent* ResolveNarrativeAbilitySystem(AActor* Target) const;
	bool IsPerceivingQuestOwner(const ANarrativeNPCController* Controller) const;
	bool IsControllingVehicle(const ANarrativeNPCController* Controller) const;
	bool HasReturnedToOwnedNPC(const ANarrativeNPCController* Controller) const;
	bool HasAttackToken(const ANarrativeNPCController* Controller) const;
	void BindTarget(AActor* Target);
	void UnbindTarget();
	void Evaluate(bool bInitialEvaluation);

	UFUNCTION() void HandleTargetReady(AActor* Actor);
	UFUNCTION() void HandleDestinationReady(AActor* Actor);
	UFUNCTION() void HandleDeathStateChanged(AActor* ChangedActor,
		UNarrativeAbilitySystemComponent* ChangedASC, const bool bIsDead);

	UPROPERTY() TWeakObjectPtr<AActor> CachedTarget;
	UPROPERTY() TWeakObjectPtr<AActor> CachedDestination;
	UPROPERTY() TWeakObjectPtr<UNarrativeAbilitySystemComponent> CachedAbilitySystem;
	bool bObservedPerception = false;
	bool bObservedVehicle = false;
	bool bObservedAttackToken = false;
	bool bObservedUnsatisfiedState = false;
};
