#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryTypes.h"
#include "GameplayTagContainer.h"
#include "Tales/QuestTask.h"
#include "TerritoryStateTask.generated.h"

class ATerritoryVolume;

/** Complementary Territory objectives that are not ownership, assault, or disguise tasks. */
UENUM(BlueprintType)
enum class ETerritoryStateTaskObjective : uint8
{
	BecomeAvailable UMETA(DisplayName="Unlock Territory",
		ToolTip="Complete when this Territory and every authored ancestor are unlocked."),
	BecomeLocked UMETA(DisplayName="Lock Territory",
		ToolTip="Complete when this Territory's local availability becomes Locked."),
	BecomeUnclaimed UMETA(DisplayName="Territory Becomes Unclaimed",
		ToolTip="Complete when political state becomes Unclaimed."),
	BecomeContested UMETA(DisplayName="Territory Becomes Contested",
		ToolTip="Complete once a valid contest begins. Capture progress ticks do not add extra progress."),
	BecomeClaimed UMETA(DisplayName="Territory Becomes Claimed",
		ToolTip="Complete when political state becomes Claimed. Use Capture Territory Task when one exact faction must own it."),
	AllDefendersDefeated UMETA(DisplayName="Defeat All Territory Defenders",
		ToolTip="Complete from the Territory's authoritative All Defenders Defeated event."),
	ReachDesiredGarrison UMETA(DisplayName="Assign Guards to Territory",
		ToolTip="Progress equals Desired Guards and completes at Narrative Required Quantity."),
	EnterTerritory UMETA(DisplayName="Enter Territory",
		ToolTip="Complete when the quest player's current Narrative character enters the Territory. This also follows the character while driving."),
	LeaveTerritory UMETA(DisplayName="Leave Territory",
		ToolTip="Complete after the same player character is seen inside and then outside. Respawn and a streamed Territory replacement do not count as leaving.")
};

/**
 * Narrative Task for Territory availability, political state, defenders, guards,
 * and player presence.
 *
 * Easy examples:
 * - Unlock Territory + Castle Hill Farm = "Unlock Castle Hill Farm".
 * - Territory Becomes Contested + Blacksmith = advance when the real contest starts.
 * - Assign Guards + Required Quantity 3 = "Assign 3 guards to Blacksmith".
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory State / Garrison Task",
		ToolTip="Narrative Task that follows one real Territory state, unlock, defender, guard, or bounds objective."))
class TERRITORYFRAMEWORK_API UTerritoryStateTask : public UNarrativeTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task|Objective",
		meta=(ToolTip="What must happen. Easy example: choose Unlock Territory for a quest that reveals Castle Hill Farm."))
	ETerritoryStateTaskObjective Objective =
		ETerritoryStateTaskObjective::BecomeAvailable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task|Target",
		meta=(Categories="Territory",
			ToolTip="Place, District, or City followed by this task. Easy example: Territory.HavenReach.CastleHill.Farm."))
	FGameplayTag TargetTerritory;

	/**
	 * Restrict a garrison objective to one authored floor of the Place, for the two
	 * objectives that read a garrison. -1 follows the whole Place and is the default, so an
	 * existing task is unchanged.
	 *
	 * A floor changes what the objective means, deliberately:
	 * - Assign Guards becomes physical. The guards must actually be standing on the floor,
	 *   because a floor has no separate staffing target to satisfy on paper.
	 * - Defeat Defenders becomes that floor's own defenders, taken from the replicated
	 *   per-floor snapshot instead of the Place-wide defeat event.
	 *
	 * Every other objective ignores this field.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task|Target",
		meta=(ClampMin="-1",
			EditCondition="Objective == ETerritoryStateTaskObjective::AllDefendersDefeated || Objective == ETerritoryStateTaskObjective::ReachDesiredGarrison",
			EditConditionHides,
			ToolTip="Which floor this objective follows. -1 means the whole Place. Easy example: 2 for 'clear the defenders on floor 2'."))
	int32 TargetFloor = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task|Objective",
		meta=(ToolTip="If the objective is already true when the quest reaches this task, complete immediately. Leave Territory always requires an observed inside-to-outside transition."))
	bool bCompleteIfAlreadySatisfied = true;

	/** Read-only helper used by Blueprint previews and behavioural tests. */
	UFUNCTION(BlueprintPure, Category="Territory Task|Preview",
		meta=(DisplayName="Is Territory Task Objective Satisfied",
			ToolTip="Checks the current runtime Territory without changing quest or Territory state."))
	bool IsObjectiveSatisfiedBy(const ATerritoryVolume* Territory) const;

protected:
	virtual void BeginTask() override;
	virtual void EndTask() override;
	virtual void TickTask_Implementation() override;
	virtual FText GetTaskDescription_Implementation() const override;
	virtual FText GetTaskProgressText_Implementation() const override;
	virtual FVector GetNavigationMarkerLocation_Implementation() const override;
	virtual AActor* GetNavigationMarkerAttachActor_Implementation() const override;

private:
	friend class FTFTerritoryObservationTaskLifecycle;
	void ObservePresence();
	void BindTerritory(ATerritoryVolume* Territory);
	void UnbindTerritory();
	void EvaluateCurrent(bool bInitialEvaluation);
	ATerritoryVolume* ResolveTerritory() const;
	FText ResolveTerritoryName() const;

	/** A negative TargetFloor means this task follows the whole Place, never a floor row. */
	bool IsFloorFiltered() const { return TargetFloor >= 0; }

	/** The replicated floor entry this task follows, or null when the Place declares no such floor. */
	const struct FTerritoryFloorSnapshot* FindTargetFloor(const ATerritoryVolume& Territory) const;

	UFUNCTION()
	void HandleTerritoryRegistered(ATerritoryVolume* Territory,
		bool bWasUnregistered);

	UFUNCTION()
	void HandleTerritoryUnregistered(ATerritoryVolume* Territory,
		bool bWasUnregistered);

	UFUNCTION()
	void HandleStateChanged(ATerritoryVolume* Territory,
		ETerritoryState NewState);

	UFUNCTION()
	void HandleAvailabilityChanged(ATerritoryVolume* Territory,
		ETerritoryAvailability NewAvailability);

	UFUNCTION()
	void HandleAllDefendersDefeated(ATerritoryVolume* Territory);

	UFUNCTION()
	void HandleGarrisonChanged(ATerritoryVolume* Territory,
		FTerritoryGarrisonSnapshot Snapshot);

	UPROPERTY()
	TWeakObjectPtr<ATerritoryVolume> CachedTerritory;

	TWeakObjectPtr<APawn> ObservedPawn;
	bool bWasInsideTarget = false;
	bool bHasPresenceObservation = false;
	bool bHasObjectiveObservation = false;
	bool bObservedObjectiveSatisfied = false;
};
