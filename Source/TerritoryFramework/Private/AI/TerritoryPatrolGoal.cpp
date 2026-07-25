#include "AI/TerritoryPatrolGoal.h"

UTerritoryPatrolGoal::UTerritoryPatrolGoal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultScore = 1.f;
	bRemoveOnSucceeded = false;
	bSaveGoal = false;
}

float UTerritoryPatrolGoal::GetGoalScore_Implementation() const
{
	return TerritoryPatrol.Num() >= 2 ? Super::GetGoalScore_Implementation() : 0.f;
}

FString UTerritoryPatrolGoal::GetDebugString_Implementation() const
{
	return FString::Printf(TEXT("Territory patrol (%d nodes)"), TerritoryPatrol.Num());
}

bool UTerritoryPatrolGoal::ShouldCleanup_Implementation() const
{
	return TerritoryPatrol.Num() < 2;
}
