#include "AI/TerritoryAssaultActivity.h"

#include "AI/TerritoryAssaultGoal.h"
#include "AI/NarrativeNPCController.h"
#include "Navigation/PathFollowingComponent.h"

UTerritoryAssaultActivity::UTerritoryAssaultActivity(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedGoalType = UTerritoryAssaultGoal::StaticClass();
	bIsInterruptable = true;
}

bool UTerritoryAssaultActivity::SupportsAssaultGoals() const
{
	return SupportedGoalType == UTerritoryAssaultGoal::StaticClass();
}

bool UTerritoryAssaultActivity::RunActivity()
{
	const UTerritoryAssaultGoal* AssaultGoal = Cast<UTerritoryAssaultGoal>(ActivityGoal);
	if (!OwnerController || !AssaultGoal) return false;

	const EPathFollowingRequestResult::Type Result = OwnerController->MoveToLocation(
		AssaultGoal->TargetLocation, AcceptanceRadius, true, true, true, false, nullptr, true);
	return Result != EPathFollowingRequestResult::Failed;
}

bool UTerritoryAssaultActivity::EndActivity()
{
	if (OwnerController)
	{
		OwnerController->StopMovement();
	}
	return Super::EndActivity();
}
