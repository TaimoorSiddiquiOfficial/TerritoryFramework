#pragma once

#include "CoreMinimal.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "GameplayTagContainer.h"
#include "Tales/QuestTask.h"
#include "TerritoryAssaultTask.generated.h"

class ATerritoryVolume;

/** Story-friendly objectives backed by the real durable counterattack record. */
UENUM(BlueprintType)
enum class ETerritoryAssaultTaskObjective : uint8
{
	RepelAttack UMETA(DisplayName="Repel the Counterattack",
		ToolTip="Complete after the finite attacking force is defeated."),
	TerritoryTaken UMETA(DisplayName="Allow the Faction to Take the Territory",
		ToolTip="Complete when the selected attacking faction physically completes takeover."),
	KillAttackers UMETA(DisplayName="Defeat Counterattack Enemies",
		ToolTip="Progress follows Killed Attackers and Narrative Required Quantity."),
	AssaultActivated UMETA(DisplayName="Counterattack Reaches the Territory",
		ToolTip="Complete when warning and staging become a physically active battle."),
	FinalFightStarted UMETA(DisplayName="Chase Target Starts Final Fight",
		ToolTip="Complete when the damaged or blocked story target abandons its vehicle."),
	TargetEscaped UMETA(DisplayName="Chase Target Escaped",
		ToolTip="Complete when the target reaches the road exit or the player loses chase distance."),
	WarningIssued UMETA(DisplayName="Counterattack Warning Issued",
		ToolTip="Complete after the durable assault record confirms its warning was delivered."),
	RecaptureCountdownStarted UMETA(DisplayName="Enemy Starts Territory Takeover Countdown",
		ToolTip="Complete when physical attackers clear local resistance and begin the unattended recapture countdown."),
	StoryPursuitStarted UMETA(DisplayName="Boss Fight or Chase Started",
		ToolTip="Complete when an explicit Story Pursuit becomes a physical encounter."),
	StoryBossDefeated UMETA(DisplayName="Defeat Story Boss",
		ToolTip="Complete when a Story Pursuit ends with its finite attackers removed. For one boss, set Planned Force Override to 1."),
	StoryEncounterResolved UMETA(DisplayName="Boss Fight or Chase Resolved",
		ToolTip="Complete when an explicit Story Pursuit reaches any durable terminal outcome."),
	StoryTargetReachedExit UMETA(DisplayName="Chase Target Reaches Exit",
		ToolTip="Complete only when the fleeing target reaches the authored Road Guide exit."),
	StoryChaseDistanceLost UMETA(DisplayName="Player Loses Chase Distance",
		ToolTip="Complete only when every participating player remains outside chase range for the full grace period."),
	WithdrawnAttackers UMETA(DisplayName="Force Attackers to Withdraw",
		ToolTip="Progress follows the durable Withdrawn Force count and Narrative Required Quantity."),
	AssaultCancelled UMETA(DisplayName="Counterattack Is Cancelled",
		ToolTip="Complete when diplomacy, quest rules, ownership, route, capability, or an explicit story action cancels the assault.")
};

/**
 * Narrative Pro quest task for Territory counterattacks and boss chases.
 *
 * Easy examples:
 * - Repel Attack + Blacksmith means "Defend the Blacksmith".
 * - Kill Attackers + Required Quantity 3 means "Defeat three Bandit reinforcements".
 * - Final Fight Started advances the quest when an underboss leaves his damaged car.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Counterattack / Chase Task",
		ToolTip="Narrative Task driven by one durable Territory counterattack or story chase record."))
class TERRITORYFRAMEWORK_API UTerritoryAssaultTask : public UNarrativeTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task|Target",
		meta=(Categories="Territory",
			ToolTip="Territory whose assault records drive this task. Easy example: Territory.HavenReach.MarketSquare.Blacksmith."))
	FGameplayTag TargetTerritory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task|Filter",
		meta=(Categories="Narrative.Factions",
			ToolTip="Optional attacking faction filter. Leave empty when any hostile faction may satisfy the objective."))
	FGameplayTag AttackingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task|Filter",
		meta=(ToolTip="Optional stable Story Pursuit Scenario ID. Leave empty for an ordinary strategic counterattack."))
	FName ScenarioID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task|Objective",
		meta=(ToolTip="Story outcome or progress watched by this task. Easy example: Repel the Counterattack completes only after every finite attacker is removed."))
	ETerritoryAssaultTaskObjective Objective =
		ETerritoryAssaultTaskObjective::RepelAttack;

	/** Read-only helper for previews, Blueprint branches, and behavioural tests. */
	UFUNCTION(BlueprintPure, Category="Territory Task|Preview",
		meta=(DisplayName="Does Assault Record Satisfy Territory Task",
			ToolTip="Checks this task against one durable assault record without changing the quest, combatants, or Territory."))
	bool IsObjectiveSatisfiedByRecord(const FTerritoryAssaultRecord& Assault) const;

	/** Progress value read from one matching durable record. Zero means this objective is not quantity based. */
	UFUNCTION(BlueprintPure, Category="Territory Task|Preview",
		meta=(DisplayName="Get Territory Assault Task Progress"))
	int32 GetObjectiveProgressFromRecord(const FTerritoryAssaultRecord& Assault) const;

protected:
	virtual void BeginTask() override;
	virtual void EndTask() override;
	virtual FText GetTaskDescription_Implementation() const override;
	virtual FText GetTaskProgressText_Implementation() const override;
	virtual FVector GetNavigationMarkerLocation_Implementation() const override;
	virtual AActor* GetNavigationMarkerAttachActor_Implementation() const override;

private:
	UFUNCTION()
	void HandleAssaultChanged(const FTerritoryAssaultRecord& Assault);

	bool Matches(const FTerritoryAssaultRecord& Assault) const;
	void EvaluateRecord(const FTerritoryAssaultRecord& Assault);
	ATerritoryVolume* ResolveTerritory() const;
};
