#include "AI/TerritoryAssaultGoal.h"

UTerritoryAssaultGoal::UTerritoryAssaultGoal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Narrative Pro's attack generator uses a base score of 3. Keep the durable
	// movement goal below combat so a detected defender can take over, while the
	// non-expiring goal remains available for resumption after combat.
	DefaultScore = 2.f;
	bRemoveOnSucceeded = false;
	bSaveGoal = false;
	GoalLifetime = -1.f;
}

float UTerritoryAssaultGoal::GetGoalScore_Implementation() const
{
	return AssaultID.IsValid() && TargetTerritoryGUID.IsValid()
		&& TargetTerritoryTag.IsValid() ? Super::GetGoalScore_Implementation() : -1.f;
}

FString UTerritoryAssaultGoal::GetDebugString_Implementation() const
{
	return FString::Printf(TEXT("Territory assault %s -> %s [%s]"),
		*AssaultID.ToString(), *TargetTerritoryTag.ToString(),
		*TargetTerritoryGUID.ToString(EGuidFormats::Digits));
}

bool UTerritoryAssaultGoal::ShouldCleanup_Implementation() const
{
	// World Partition may temporarily unload the target. The participant component
	// owns terminal cleanup, so streaming alone must not destroy the goal.
	return !AssaultID.IsValid() || !TargetTerritoryGUID.IsValid()
		|| !TargetTerritoryTag.IsValid();
}
