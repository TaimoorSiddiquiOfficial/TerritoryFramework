#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_UpdatePatrolRoute.generated.h"

class ATerritoryGuardSpawnPoint;

/**
 * BT Service: Advances the patrol node index when the guard reaches a node.
 * Cycles through the patrol route, looping back to start if bLoopPatrol is true.
 *
 * Required Blackboard Keys:
 * - PatrolSpawnPoint (Object: ATerritoryGuardSpawnPoint)
 * - PatrolNodeIndex (Int: current index, advanced by this service)
 */
UCLASS()
class TERRITORYFRAMEWORK_API UBTService_UpdatePatrolRoute : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdatePatrolRoute();

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Patrol")
	FBlackboardKeySelector PatrolSpawnPointKey;

	UPROPERTY(EditAnywhere, Category = "Patrol")
	FBlackboardKeySelector PatrolNodeIndexKey;

	/** Distance threshold to consider the guard has "arrived" at the node */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "50.0"))
	float ArrivalThreshold = 100.f;
};
