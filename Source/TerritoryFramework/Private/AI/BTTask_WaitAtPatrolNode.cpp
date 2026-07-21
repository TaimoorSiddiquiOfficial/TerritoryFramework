#include "AI/BTTask_WaitAtPatrolNode.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryTypes.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_WaitAtPatrolNode::UBTTask_WaitAtPatrolNode()
{
	NodeName = "Wait At Patrol Node";
	bNotifyTick = true;

	PatrolSpawnPointKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_WaitAtPatrolNode, PatrolSpawnPointKey), ATerritoryGuardSpawnPoint::StaticClass());
	PatrolNodeIndexKey.AddIntFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_WaitAtPatrolNode, PatrolNodeIndexKey));
}

EBTNodeResult::Type UBTTask_WaitAtPatrolNode::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return EBTNodeResult::Failed;

	float WaitTime = OverrideWaitTime;

	// If no override, read from patrol node
	if (WaitTime <= 0.f)
	{
		ATerritoryGuardSpawnPoint* SpawnPoint = Cast<ATerritoryGuardSpawnPoint>(
			BB->GetValueAsObject(PatrolSpawnPointKey.SelectedKeyName));

		if (SpawnPoint)
		{
			const TArray<FTerritoryPatrolNode>& Route = SpawnPoint->GetPatrolRoute();
			int32 NodeIndex = BB->GetValueAsInt(PatrolNodeIndexKey.SelectedKeyName);
			NodeIndex = FMath::Clamp(NodeIndex, 0, Route.Num() - 1);

			if (Route.Num() > 0)
			{
				WaitTime = Route[NodeIndex].WaitTime;
			}
		}
	}

	// Apply minimum
	WaitTime = FMath::Max(WaitTime, MinWaitTime);
	RemainingTime = WaitTime;

	return EBTNodeResult::InProgress;
}

void UBTTask_WaitAtPatrolNode::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	RemainingTime -= DeltaSeconds;

	if (RemainingTime <= 0.f)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

FString UBTTask_WaitAtPatrolNode::GetStaticDescription() const
{
	if (OverrideWaitTime > 0.f)
	{
		return FString::Printf(TEXT("Wait %.1fs (override)"), OverrideWaitTime);
	}
	return TEXT("Wait at patrol node (from route)");
}
