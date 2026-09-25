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
	// One stop is a real patrol: the guard walks there and holds it. Only a goal with no stops
	// at all scores zero, which is how a post that has no patrol duty stays inert.
	return TerritoryPatrol.Num() >= 1 ? Super::GetGoalScore_Implementation() : 0.f;
}

FString UTerritoryPatrolGoal::GetDebugString_Implementation() const
{
	return FString::Printf(TEXT("Territory patrol (%d nodes)"), TerritoryPatrol.Num());
}

bool UTerritoryPatrolGoal::ShouldCleanup_Implementation() const
{
	// Mirrors the scoring gate above so the two cannot disagree. Nothing calls ShouldCleanup
	// today, so this is the contract a future caller inherits rather than live behaviour.
	return TerritoryPatrol.Num() < 1;
}
