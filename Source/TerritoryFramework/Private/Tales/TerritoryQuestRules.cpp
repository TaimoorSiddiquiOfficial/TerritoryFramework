#include "Tales/TerritoryQuestRules.h"

#include "Tales/Quest.h"
#include "Tales/TalesComponent.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Engine/World.h"

bool FTerritoryQuestRuntimeOverrideRule::Pauses(
	ETerritoryQuestOverrideEffect Effect) const
{
	switch (Effect)
	{
	case ETerritoryQuestOverrideEffect::StateRules: return bPauseStateRules;
	case ETerritoryQuestOverrideEffect::AutomaticCapture: return bPauseAutomaticCapture;
	case ETerritoryQuestOverrideEffect::AutomaticCounterattacks:
		return bPauseAutomaticCounterattacks;
	default: return false;
	}
}

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

bool UTerritoryQuestRulesLibrary::DoesAnyOnlinePlayerMatchQuestState(
	const UWorld* World, TSubclassOf<UQuest> QuestClass,
	ETerritoryQuestStateRequirement RequiredState,
	const UTalesComponent* OptionalContextTales)
{
	if (!QuestClass) return false;
	if (DoesQuestStateMatch(OptionalContextTales, QuestClass, RequiredState))
	{
		return true;
	}
	if (!World) return false;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator();
		It; ++It)
	{
		const ANarrativePlayerController* PC =
			Cast<ANarrativePlayerController>(It->Get());
		if (PC && PC->GetTalesComponent() != OptionalContextTales
			&& DoesQuestStateMatch(PC->GetTalesComponent(), QuestClass,
				RequiredState))
		{
			return true;
		}
	}
	return false;
}
