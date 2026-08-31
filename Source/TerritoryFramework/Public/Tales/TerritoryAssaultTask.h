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
		ToolTip="Complete when the target reaches the road exit or the player loses chase distance.")
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
	meta=(DisplayName="Territory Counterattack / Chase Task"))
class TERRITORYFRAMEWORK_API UTerritoryAssaultTask : public UNarrativeTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task",
		meta=(Categories="Territory",
			ToolTip="Territory whose assault records drive this task."))
	FGameplayTag TargetTerritory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task",
		meta=(Categories="Narrative.Factions",
			ToolTip="Optional attacking faction filter. Leave empty when any hostile faction may satisfy the objective."))
	FGameplayTag AttackingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task",
		meta=(ToolTip="Optional stable Story Pursuit Scenario ID. Leave empty for an ordinary strategic counterattack."))
	FName ScenarioID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task",
		meta=(ToolTip="Story outcome or progress watched by this task."))
	ETerritoryAssaultTaskObjective Objective =
		ETerritoryAssaultTaskObjective::RepelAttack;

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
