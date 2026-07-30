#pragma once

#include "CoreMinimal.h"
#include "AI/Activities/NPCGoalItem.h"
#include "GameplayTagContainer.h"
#include "TerritoryAssaultGoal.generated.h"

class ATerritoryVolume;

/** Finite, non-saved Narrative goal directing one assault NPC to a Territory. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryAssaultGoal : public UNPCGoalItem
{
	GENERATED_BODY()

public:
	UTerritoryAssaultGoal(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintReadOnly, Category="Territory|Assault")
	FGuid AssaultID;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Assault", meta=(Categories="Territory"))
	FGameplayTag TargetTerritoryTag;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Assault")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Territory|Assault")
	TWeakObjectPtr<ATerritoryVolume> TargetTerritory;

	virtual float GetGoalScore_Implementation() const override;
	virtual FString GetDebugString_Implementation() const override;
	virtual bool ShouldCleanup_Implementation() const override;
};
