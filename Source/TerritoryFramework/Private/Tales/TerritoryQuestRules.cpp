#include "Tales/TerritoryQuestRules.h"

#include "Tales/Quest.h"
#include "Tales/TalesComponent.h"

bool UTerritoryQuestRulesLibrary::DoesQuestStateMatch(
	const UTalesComponent* TalesComponent, TSubclassOf<UQuest> QuestClass,
	ETerritoryQuestStateRequirement RequiredState)
{
	if (!TalesComponent || !QuestClass) return false;

	return DoesQuestStateMatchValues(RequiredState,
		TalesComponent->IsQuestStartedOrFinished(QuestClass),
		TalesComponent->IsQuestInProgress(QuestClass),
		TalesComponent->IsQuestSucceeded(QuestClass),
		TalesComponent->IsQuestFailed(QuestClass),
		TalesComponent->IsQuestFinished(QuestClass));
}

bool UTerritoryQuestRulesLibrary::DoesQuestStateMatchValues(
	ETerritoryQuestStateRequirement RequiredState, bool bStartedOrFinished,
	bool bInProgress, bool bSucceeded, bool bFailed, bool bFinished)
{
	switch (RequiredState)
	{
	case ETerritoryQuestStateRequirement::NotStarted: return !bStartedOrFinished;
	case ETerritoryQuestStateRequirement::InProgress: return bInProgress;
	case ETerritoryQuestStateRequirement::Succeeded: return bSucceeded;
	case ETerritoryQuestStateRequirement::Failed: return bFailed;
	case ETerritoryQuestStateRequirement::Finished: return bFinished;
	case ETerritoryQuestStateRequirement::StartedOrFinished: return bStartedOrFinished;
	default: return false;
	}
}
