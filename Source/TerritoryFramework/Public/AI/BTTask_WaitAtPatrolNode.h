#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_WaitAtPatrolNode.generated.h"

class ATerritoryGuardSpawnPoint;

/**
 * BT Task: Wait at the current patrol node for the configured duration.
 * Reads the WaitTime from the current FTerritoryPatrolNode in the patrol route.
 *
 * Required Blackboard Keys:
 * - PatrolSpawnPoint (Object: ATerritoryGuardSpawnPoint)
 * - PatrolNodeIndex (Int: current index in patrol route)
 */
UCLASS()
class TERRITORYFRAMEWORK_API UBTTask_WaitAtPatrolNode : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_WaitAtPatrolNode();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Patrol")
	FBlackboardKeySelector PatrolSpawnPointKey;

	UPROPERTY(EditAnywhere, Category = "Patrol")
	FBlackboardKeySelector PatrolNodeIndexKey;

	/** Override wait time. If <= 0, uses the patrol node's WaitTime. */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float OverrideWaitTime = 0.f;

	/** Minimum wait time fallback */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.5"))
	float MinWaitTime = 1.f;

private:
	float RemainingTime = 0.f;
};
