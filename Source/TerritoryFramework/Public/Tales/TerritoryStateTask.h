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
		ToolTip="Complete when the quest owner's pawn is inside the Territory bounds."),
	LeaveTerritory UMETA(DisplayName="Leave Territory",
		ToolTip="Complete only after the quest owner's pawn was inside and then leaves the Territory bounds.")
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
	void BindTerritory(ATerritoryVolume* Territory);
	void UnbindTerritory();
	void EvaluateCurrent(bool bInitialEvaluation);
	ATerritoryVolume* ResolveTerritory() const;
	FText ResolveTerritoryName() const;

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

	bool bWasInsideTarget = false;
};
