#include "Tales/TerritoryNarrativeQuestStarter.h"

#include "Core/TerritoryTypes.h"
#include "EngineUtils.h"
#include "Tales/Quest.h"
#include "Tales/TalesComponent.h"
#include "TimerManager.h"
#include "UnrealFramework/NarrativePlayerController.h"

ATerritoryNarrativeQuestStarter::ATerritoryNarrativeQuestStarter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void ATerritoryNarrativeQuestStarter::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) return;

	if (!QuestClass)
	{
		UE_LOG(LogTerritory, Error,
			TEXT("Territory Narrative Quest Starter %s has no Quest Class."),
			*GetPathName());
		return;
	}

	const float SafeInterval = FMath::Max(0.1f, RetryInterval);
	const float SafeInitialDelay = FMath::Max(0.f, InitialDelay);
	GetWorldTimerManager().SetTimer(ReadinessTimer, this,
		&ATerritoryNarrativeQuestStarter::TryStartPendingPlayers,
		SafeInterval, true, SafeInitialDelay);
	if (SafeInitialDelay <= KINDA_SMALL_NUMBER)
	{
		TryStartPendingPlayers();
	}
}

void ATerritoryNarrativeQuestStarter::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	StopReadinessPolling();
	HandledTalesComponents.Reset();
	Super::EndPlay(EndPlayReason);
}

void ATerritoryNarrativeQuestStarter::TryStartPendingPlayers()
{
	if (!HasAuthority() || !QuestClass)
	{
		StopReadinessPolling();
		return;
	}
	if (!bStartForEveryPlayer && bSingleAudienceHandled)
	{
		StopReadinessPolling();
		return;
	}

	for (auto It = HandledTalesComponents.CreateIterator(); It; ++It)
	{
		if (!It->IsValid()) It.RemoveCurrent();
	}

	bool bFoundEligiblePlayer = false;
	bool bAllFoundPlayersReady = true;
	for (TActorIterator<ANarrativePlayerController> It(GetWorld()); It; ++It)
	{
		ANarrativePlayerController* Controller = *It;
		if (!IsValid(Controller) || Controller->IsActorBeingDestroyed()) continue;
		if (!bStartForEveryPlayer && bSingleAudienceHandled) break;

		bFoundEligiblePlayer = true;
		bool bPlayerReady = false;
		TryStartForController(Controller, bPlayerReady);
		bAllFoundPlayersReady &= bPlayerReady;
		if (!bStartForEveryPlayer && bSingleAudienceHandled) break;
	}

	if (!bKeepPollingForLateJoiningPlayers && bFoundEligiblePlayer
		&& bAllFoundPlayersReady)
	{
		StopReadinessPolling();
	}
}

bool ATerritoryNarrativeQuestStarter::TryStartForController(
	ANarrativePlayerController* Controller, bool& bOutPlayerReady)
{
	bOutPlayerReady = false;
	if (!IsValid(Controller) || !IsValid(Controller->GetPawn())) return false;

	UTalesComponent* Tales = Controller->GetTalesComponent();
	if (!IsValid(Tales) || Tales->bIsLoading) return false;
	APawn* NarrativePawn = Tales->GetOwningPawn();
	if (!IsValid(NarrativePawn)) return false;

	bOutPlayerReady = true;
	if (HandledTalesComponents.Contains(Tales)) return true;
	if (Tales->IsQuestStartedOrFinished(QuestClass))
	{
		HandledTalesComponents.Add(Tales);
		bSingleAudienceHandled = true;
		return true;
	}

	if (UQuest* StartedQuest = Tales->BeginQuest(QuestClass, StartFromID))
	{
		HandledTalesComponents.Add(Tales);
		bSingleAudienceHandled = true;
		UE_LOG(LogTerritory, Log,
			TEXT("Territory Quest Starter %s began %s for %s after Narrative became ready."),
			*GetName(), *GetNameSafe(StartedQuest), *GetNameSafe(Controller));
		return true;
	}

	bOutPlayerReady = false;
	return false;
}

void ATerritoryNarrativeQuestStarter::StopReadinessPolling()
{
	GetWorldTimerManager().ClearTimer(ReadinessTimer);
}
