#include "AI/TerritoryConversationActivity.h"
#include "AI/TerritoryConversationGoal.h"
#include "AI/NarrativeNPCController.h"
#include "Navigation/PathFollowingComponent.h"

UTerritoryConversationActivity::UTerritoryConversationActivity(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	SupportedGoalType = UTerritoryConversationGoal::StaticClass();
	bIsInterruptable = true;
}

bool UTerritoryConversationActivity::RunActivity()
{
	const auto* Goal = Cast<UTerritoryConversationGoal>(ActivityGoal);
	if (!OwnerController || !Goal || Goal->ShouldCleanup()) return false;
	// No partial path: an inaccessible meeting must time out without beginning story playback.
	return OwnerController->MoveToLocation(Goal->Destination, Goal->ArrivalRadius,
		false, true, false, false, nullptr, false) != EPathFollowingRequestResult::Failed;
}

bool UTerritoryConversationActivity::EndActivity()
{
	if (OwnerController) OwnerController->StopMovement();
	return Super::EndActivity();
}
