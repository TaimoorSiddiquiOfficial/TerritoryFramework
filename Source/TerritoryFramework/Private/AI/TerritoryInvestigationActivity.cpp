#include "AI/TerritoryInvestigationActivity.h"

#include "AI/TerritoryInvestigationGoal.h"
#include "AI/NarrativeNPCController.h"
#include "Navigation/PathFollowingComponent.h"

UTerritoryInvestigationActivity::UTerritoryInvestigationActivity(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedGoalType = UTerritoryInvestigationGoal::StaticClass();
	bIsInterruptable = true;
}

bool UTerritoryInvestigationActivity::SupportsTerritoryInvestigationGoals() const
{
	return SupportedGoalType == UTerritoryInvestigationGoal::StaticClass();
}

bool UTerritoryInvestigationActivity::RunActivity()
{
	const UTerritoryInvestigationGoal* InvestigationGoal =
		Cast<UTerritoryInvestigationGoal>(ActivityGoal);
	if (!OwnerController || !InvestigationGoal) return false;

	const EPathFollowingRequestResult::Type Result = OwnerController->MoveToLocation(
		InvestigationGoal->InvestigationLocation,
		InvestigationGoal->AcceptanceRadius,
		true, true, true, false, nullptr, true);
	return Result != EPathFollowingRequestResult::Failed;
}

bool UTerritoryInvestigationActivity::EndActivity()
{
	if (OwnerController)
	{
		OwnerController->StopMovement();
	}
	return Super::EndActivity();
}
