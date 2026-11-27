#include "AI/TerritoryPatrolGoal.h"

#include "AI/NarrativeNPCController.h"
#include "Core/TerritoryGuardCharacter.h"

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
	if (TerritoryPatrol.Num() < 1)
	{
		return 0.f;
	}

	// The leash. A guard pulled out of its Place by an investigation or a chase scores its patrol
	// goal high enough for the existing Narrative goal scoring to walk it back to a contained node.
	// Nothing new moves the guard: the patrol activity already consumes this goal.
	if (const ATerritoryGuardCharacter* Guard = OwnerController
		? Cast<ATerritoryGuardCharacter>(OwnerController->GetPawn()) : nullptr)
	{
		if (Guard->IsBeyondTerritoryLeash())
		{
			return LeashReturnScore;
		}
	}

	return Super::GetGoalScore_Implementation();
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

bool UTerritoryPatrolGoal::DeliverPatrolSpot(int32 PatrolIdx)
{
	if (!TerritoryPatrol.IsValidIndex(PatrolIdx))
	{
		return false;
	}

	// The index is resolved against THIS array, because it is the array the patrol Blueprint
	// indexes with the same value. Asking the guard for a node instead would read the authored
	// route, which is a different array in a different order once the route is staggered.
	ATerritoryGuardCharacter* Guard = OwnerController
		? Cast<ATerritoryGuardCharacter>(OwnerController->GetPawn()) : nullptr;
	if (!Guard)
	{
		// Not a warning: a goal can outlive its pawn during teardown, and the patrol activity
		// itself is what stops being driven in that case.
		return false;
	}

	return Guard->DeliverPatrolSpot(TerritoryPatrol[PatrolIdx]);
}
