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

	/** Unique ID linking troops, waves and saved records to the same assault. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Assault")
	FGuid AssaultID;

	/** Stable physical target identity; gameplay tag remains the readable semantic label. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Assault")
	FGuid TargetTerritoryGUID;

	/** Stable tag of the Territory targeted by this operation. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Assault", meta=(Categories="Territory"))
	FGameplayTag TargetTerritoryTag;

	/** World-space location this goal or result targets, in centimetres. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Assault")
	FVector TargetLocation = FVector::ZeroVector;

	/** Territory targeted by this operation or result. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Territory|Assault")
	TWeakObjectPtr<ATerritoryVolume> TargetTerritory;

	virtual float GetGoalScore_Implementation() const override;
	virtual FString GetDebugString_Implementation() const override;
	virtual bool ShouldCleanup_Implementation() const override;
};
