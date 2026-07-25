#pragma once

#include "CoreMinimal.h"
#include "AI/Activities/NPCGoalItem.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "TerritoryPatrolGoal.generated.h"

/** Narrative goal instance populated from a territory guard's assigned spawn point. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryPatrolGoal : public UNPCGoalItem
{
	GENERATED_BODY()

public:
	UTerritoryPatrolGoal(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Territory|Patrol")
	TArray<FTerritoryPatrolNode> TerritoryPatrol;

	virtual FString GetDebugString_Implementation() const override;
	virtual bool ShouldCleanup_Implementation() const override;
};
