#include "AI/TerritoryConversationGoal.h"
#include "AI/NarrativeNPCController.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Social/TerritoryConversationMeetingPoint.h"

UTerritoryConversationGoal::UTerritoryConversationGoal(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	DefaultScore = 2.f;
	bSaveGoal = false;
	bRemoveOnSucceeded = true;
}

bool UTerritoryConversationGoal::ShouldCleanup_Implementation() const
{
	return !Meeting.IsValid() || !OwnerController || !SessionID.IsValid()
		|| !Meeting->OwnsGoal(this, Cast<ATerritoryGuardCharacter>(OwnerController->GetControlledNPC()))
		|| (GoalLifetime >= 0.f && GetGoalAgeSeconds() >= GoalLifetime);
}

float UTerritoryConversationGoal::GetGoalScore_Implementation() const
{
	return ShouldCleanup_Implementation() ? -1.f : Super::GetGoalScore_Implementation();
}
