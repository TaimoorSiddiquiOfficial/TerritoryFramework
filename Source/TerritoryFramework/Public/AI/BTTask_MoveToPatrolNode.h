#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MoveToPatrolNode.generated.h"

class ATerritoryGuardSpawnPoint;
struct FTerritoryPatrolNode;

/**
 * BT Task: Move the guard NPC to the current patrol node location.
 * Reads the patrol route from the guard's assigned ATerritoryGuardSpawnPoint
 * and writes TargetLocation, TargetRotation, and Delay to the blackboard.
 *
 * Required Blackboard Keys:
 * - PatrolSpawnPoint (Object: ATerritoryGuardSpawnPoint)
 * - PatrolNodeIndex (Int: current index in patrol route)
 * - TargetLocation (Vector: set by this task)
 * - TargetRotation (Rotator: set by this task for RotateToGoal)
 * - Delay (Float: set by this task for WaitBlackboardTime)
 */
UCLASS()
class TERRITORYFRAMEWORK_API UBTTask_MoveToPatrolNode : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MoveToPatrolNode();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** Blackboard key for the patrol spawn point (ATerritoryGuardSpawnPoint) */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	FBlackboardKeySelector PatrolSpawnPointKey;

	/** Blackboard key for the current patrol node index (int) */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	FBlackboardKeySelector PatrolNodeIndexKey;

	/** Blackboard key for the target location (vector) — written by this task */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	FBlackboardKeySelector TargetLocationKey;

	/** Blackboard key for the target rotation (rotator) — written by this task for RotateToGoal */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	FBlackboardKeySelector TargetRotationKey;

	/** Blackboard key for the wait delay (float) — written by this task for WaitBlackboardTime */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	FBlackboardKeySelector DelayKey;

	/** Acceptance radius for reaching the patrol node */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "10.0"))
	float AcceptanceRadius = 50.f;
};
