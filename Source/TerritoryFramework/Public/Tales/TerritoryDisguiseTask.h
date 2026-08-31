#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryDisguiseProfile.h"
#include "GameplayTagContainer.h"
#include "Tales/QuestTask.h"
#include "TerritoryDisguiseTask.generated.h"

class ATerritoryVolume;

UENUM(BlueprintType)
enum class ETerritoryDisguiseTaskObjective : uint8
{
	EquipDisguise UMETA(DisplayName="Equip a Disguise"),
	EnterTerritoryAccepted UMETA(DisplayName="Enter Territory With Accepted Cover"),
	PassIdentityCheck UMETA(DisplayName="Pass an Identity Check"),
	CoverCompromised UMETA(DisplayName="Have Cover Compromised"),
	RestoreCover UMETA(DisplayName="Restore Cover Identity"),
	RemoveDisguise UMETA(DisplayName="Remove the Disguise"),
	ExitTerritoryUndetected UMETA(DisplayName="Leave Territory With Cover Intact")
};

/** Narrative quest objectives for uniforms, checkpoints, and double-agent missions. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Disguise Mission Task",
		ToolTip="Narrative Task for disguise, checkpoint, cover exposure, and double-agent objectives."))
class TERRITORYFRAMEWORK_API UTerritoryDisguiseTask : public UNarrativeTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task|Objective",
		meta=(ToolTip="Disguise outcome followed by the quest. Easy example: Pass an Identity Check advances after guards accept the player's cover."))
	ETerritoryDisguiseTaskObjective Objective =
		ETerritoryDisguiseTaskObjective::EquipDisguise;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task|Target",
		meta=(Categories="Territory",
			ToolTip="Required by enter/exit objectives and useful as an identity-check filter."))
	FGameplayTag TargetTerritory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Task|Filter",
		meta=(Categories="Narrative.Factions",
			ToolTip="Optional perceived or observing faction filter. Easy example: Bandits for a Bandit uniform mission."))
	FGameplayTag Faction;

protected:
	virtual void BeginTask() override;
	virtual void EndTask() override;
	virtual void TickTask_Implementation() override;
	virtual FText GetTaskDescription_Implementation() const override;
	virtual FVector GetNavigationMarkerLocation_Implementation() const override;
	virtual AActor* GetNavigationMarkerAttachActor_Implementation() const override;

private:
	UFUNCTION()
	void HandleDisguiseChanged(AActor* Target, ETerritoryDisguiseChange Change,
		FGameplayTag ObserverFaction, ATerritoryVolume* Territory,
		const FTerritoryDisguiseSnapshot& Snapshot);

	ATerritoryVolume* ResolveTerritory() const;
	bool MatchesFaction(const FTerritoryDisguiseSnapshot& Snapshot,
		FGameplayTag ObserverFaction) const;
	bool bWasInsideTarget = false;
};
