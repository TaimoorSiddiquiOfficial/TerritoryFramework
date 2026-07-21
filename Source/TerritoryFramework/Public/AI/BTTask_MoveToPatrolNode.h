#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MoveToPatrolNode.generated.h"

class ATerritoryGuardSpawnPoint;
struct FTerritoryPatrolNode;

/**
 * BT Task: Move the guard NPC to the current patrol node location.
 * Reads the patrol route from the guard's assigned ATerritoryGuardSpawnPoint
 * and moves to the node at the current patrol index (stored on blackboard).
 *
 * Required Blackboard Keys:
 * - PatrolSpawnPoint (Object: ATerritoryGuardSpawnPoint)
 * - PatrolNodeIndex (Int: current index in patrol route)
 * - TargetLocation (Vector: set by this task for MoveTo)
 */
UCLASS()
class TERRITORYFRAMEWORK_API UBTTask_MoveToPatrolNode : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MoveToPatrolNode();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** Blackboard key selector for the patrol spawn point (ATerritoryGuardSpawnPoint) */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	FBlackboardKeySelector PatrolSpawnPointKey;

	/** Blackboard key selector for the current patrol node index (int) */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	FBlackboardKeySelector PatrolNodeIndexKey;

	/** Blackboard key selector for the target location to move to (vector) */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	FBlackboardKeySelector TargetLocationKey;

	/** Acceptance radius for reaching the patrol node */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "10.0"))
	float AcceptanceRadius = 50.f;
};
