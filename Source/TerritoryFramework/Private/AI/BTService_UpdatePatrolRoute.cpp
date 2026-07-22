#include "AI/BTService_UpdatePatrolRoute.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTService_UpdatePatrolRoute::UBTService_UpdatePatrolRoute()
{
	NodeName = "Update Patrol Route";
	Interval = 0.5f; // Check every 0.5 seconds
	RandomDeviation = 0.1f;

	// Read default from DeveloperSettings
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings)
	{
		ArrivalThreshold = Settings->DefaultPatrolArrivalThreshold;
	}

	PatrolSpawnPointKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdatePatrolRoute, PatrolSpawnPointKey), ATerritoryGuardSpawnPoint::StaticClass());
	PatrolNodeIndexKey.AddIntFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdatePatrolRoute, PatrolNodeIndexKey));
}

void UBTService_UpdatePatrolRoute::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!IsValid(AIController) || !IsValid(AIController->GetPawn())) return;

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return;

	ATerritoryGuardSpawnPoint* SpawnPoint = Cast<ATerritoryGuardSpawnPoint>(
		BB->GetValueAsObject(PatrolSpawnPointKey.SelectedKeyName));
	if (!SpawnPoint)
	{
		UE_LOG(LogTerritory, Verbose, TEXT("[PatrolAI] No PatrolSpawnPoint on blackboard"));
		return;
	}

	const TArray<FTerritoryPatrolNode>& Route = SpawnPoint->GetPatrolRoute();
	if (Route.Num() == 0)
	{
		UE_LOG(LogTerritory, Verbose, TEXT("[PatrolAI] SpawnPoint '%s' has empty patrol route"), *SpawnPoint->GetActorLabel());
		return;
	}

	int32 CurrentIndex = BB->GetValueAsInt(PatrolNodeIndexKey.SelectedKeyName);
	CurrentIndex = FMath::Clamp(CurrentIndex, 0, Route.Num() - 1);

	// Check if guard has arrived at the current node
	const FTerritoryPatrolNode& CurrentNode = Route[CurrentIndex];
	FVector NodeLocation = CurrentNode.Location;
	if (NodeLocation.IsNearlyZero())
	{
		NodeLocation = SpawnPoint->GetActorLocation();
	}

	FVector GuardLocation = AIController->GetPawn()->GetActorLocation();
	float Distance = FVector::Dist(GuardLocation, NodeLocation);

	if (Distance <= ArrivalThreshold)
	{
		// Advance to next node
		int32 NextIndex = CurrentIndex + 1;

		if (NextIndex >= Route.Num())
		{
			if (SpawnPoint->GetLoopPatrol())
			{
				NextIndex = 0; // Loop back to start
			}
			else
			{
				NextIndex = Route.Num() - 1; // Stay at last node
			}
		}

		BB->SetValueAsInt(PatrolNodeIndexKey.SelectedKeyName, NextIndex);

		const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
		if (Settings && Settings->ShouldDebugBT())
		{
			UE_LOG(LogTerritory, Log, TEXT("[PatrolAI] Advanced to node %d/%d (distance: %.0f)"),
				NextIndex, Route.Num(), Distance);
		}
	}
}

FString UBTService_UpdatePatrolRoute::GetStaticDescription() const
{
	return FString::Printf(TEXT("Update Patrol Route (threshold: %.0f)"), ArrivalThreshold);
}
