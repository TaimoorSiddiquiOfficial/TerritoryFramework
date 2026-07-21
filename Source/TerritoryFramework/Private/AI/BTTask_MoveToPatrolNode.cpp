#include "AI/BTTask_MoveToPatrolNode.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"

UBTTask_MoveToPatrolNode::UBTTask_MoveToPatrolNode()
{
	NodeName = "Move To Patrol Node";

	PatrolSpawnPointKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveToPatrolNode, PatrolSpawnPointKey), ATerritoryGuardSpawnPoint::StaticClass());
	PatrolNodeIndexKey.AddIntFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveToPatrolNode, PatrolNodeIndexKey));
	TargetLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveToPatrolNode, TargetLocationKey));
}

EBTNodeResult::Type UBTTask_MoveToPatrolNode::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Failed;

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return EBTNodeResult::Failed;

	// Get the patrol spawn point from blackboard
	ATerritoryGuardSpawnPoint* SpawnPoint = Cast<ATerritoryGuardSpawnPoint>(
		BB->GetValueAsObject(PatrolSpawnPointKey.SelectedKeyName));
	if (!SpawnPoint)
	{
		UE_LOG(LogTerritory, Warning, TEXT("[PatrolAI] No PatrolSpawnPoint on blackboard"));
		return EBTNodeResult::Failed;
	}

	// Get the patrol route
	const TArray<FTerritoryPatrolNode>& Route = SpawnPoint->GetPatrolRoute();
	if (Route.Num() == 0)
	{
		// No patrol route — stay at spawn point
		BB->SetValueAsVector(TargetLocationKey.SelectedKeyName, SpawnPoint->GetActorLocation());
		return EBTNodeResult::Succeeded;
	}

	// Get current node index
	int32 NodeIndex = BB->GetValueAsInt(PatrolNodeIndexKey.SelectedKeyName);
	NodeIndex = FMath::Clamp(NodeIndex, 0, Route.Num() - 1);

	const FTerritoryPatrolNode& CurrentNode = Route[NodeIndex];
	FVector TargetLocation = CurrentNode.Location;

	// If location is zero, use spawn point location as fallback
	if (TargetLocation.IsNearlyZero())
	{
		TargetLocation = SpawnPoint->GetActorLocation();
	}

	// Set the target location on blackboard
	BB->SetValueAsVector(TargetLocationKey.SelectedKeyName, TargetLocation);

	// Issue move command
	EPathFollowingRequestResult::Type MoveResult = AIController->MoveToLocation(
		TargetLocation,
		AcceptanceRadius,
		true,  // StopOnOverlap
		true,  // UsePathfinding
		true,  // ProjectDestinationToNavigation
		false, // CanStrafe
		nullptr, // FilterClass
		true   // AllowPartialPath
	);

	if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		return EBTNodeResult::Succeeded;
	}

	if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
	{
		// Wait for movement to finish
		return EBTNodeResult::InProgress;
	}

	// Failed to start path
	UE_LOG(LogTerritory, Warning, TEXT("[PatrolAI] MoveTo failed for node %d at %s"),
		NodeIndex, *TargetLocation.ToString());
	return EBTNodeResult::Failed;
}

void UBTTask_MoveToPatrolNode::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

FString UBTTask_MoveToPatrolNode::GetStaticDescription() const
{
	return FString::Printf(TEXT("Patrol Node (Acceptance: %.0f)"), AcceptanceRadius);
}
