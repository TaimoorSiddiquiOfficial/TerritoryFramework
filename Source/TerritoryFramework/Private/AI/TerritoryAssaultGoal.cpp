#include "AI/TerritoryAssaultGoal.h"

UTerritoryAssaultGoal::UTerritoryAssaultGoal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultScore = 5.f;
	bRemoveOnSucceeded = false;
	bSaveGoal = false;
	GoalLifetime = -1.f;
}

float UTerritoryAssaultGoal::GetGoalScore_Implementation() const
{
	return AssaultID.IsValid() && TargetTerritoryTag.IsValid() ? Super::GetGoalScore_Implementation() : -1.f;
}

FString UTerritoryAssaultGoal::GetDebugString_Implementation() const
{
	return FString::Printf(TEXT("Territory assault %s -> %s"),
		*AssaultID.ToString(), *TargetTerritoryTag.ToString());
}

bool UTerritoryAssaultGoal::ShouldCleanup_Implementation() const
{
	// World Partition may temporarily unload the target. The participant component
	// owns terminal cleanup, so streaming alone must not destroy the goal.
	return !AssaultID.IsValid() || !TargetTerritoryTag.IsValid();
}
