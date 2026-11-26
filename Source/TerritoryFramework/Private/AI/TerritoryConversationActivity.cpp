#include "AI/TerritoryConversationActivity.h"
#include "AI/TerritoryConversationGoal.h"
#include "AI/NarrativeNPCController.h"
#include "BrainComponent.h"
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

	// This activity drives movement directly and runs no Behaviour Tree of its own. The activity that
	// preceded it can leave a live tree still issuing its own move requests, which supersede the
	// MoveTo below: measured in PIE, the meeting MoveTo returned RequestSuccessful while the guard
	// stood still and covered about one centimetre per second for the whole staging window, yet the
	// same pawn covered ~760 cm in ten seconds once its brain logic was stopped. Stop the logic here
	// so this activity is the only movement authority while the meeting stages. The next activity
	// restarts its own tree through RunBehaviorTree.
	if (UBrainComponent* Brain = OwnerController->GetBrainComponent())
	{
		Brain->StopLogic(TEXT("Territory conversation activity owns movement."));
	}

	// No partial path: an inaccessible meeting must time out without beginning story playback.
	// Project onto the navmesh because the authored slot sits at standing height above it.
	return OwnerController->MoveToLocation(Goal->Destination, Goal->ArrivalRadius,
		false, true, true, false, nullptr, false) != EPathFollowingRequestResult::Failed;
}

bool UTerritoryConversationActivity::EndActivity()
{
	if (OwnerController) OwnerController->StopMovement();
	return Super::EndActivity();
}
