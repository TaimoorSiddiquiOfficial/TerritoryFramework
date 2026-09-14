#pragma once

#include "Tales/TalesComponent.h"
#include "Tales/NarrativePartyComponent.h"
#include "TerritoryDialogueLifecycleProbe.generated.h"

/** Observes Native's real reliable-exit dispatch in the no-network native fixture. */
UCLASS()
class UTerritoryDialogueLifecycleProbe : public UTalesComponent
{
	GENERATED_BODY()
public:
	int32 ExitDispatches = 0;
	int32 PartyExitDispatches = 0;
	virtual void ClientExitDialogue_Implementation(const EExitDialogueReason Reason) override
	{
		++ExitDispatches;
		Super::ClientExitDialogue_Implementation(Reason);
	}
	virtual void ClientExitPartyDialogue_Implementation(const EExitDialogueReason Reason) override
	{
		++PartyExitDispatches;
		Super::ClientExitPartyDialogue_Implementation(Reason);
	}
};

/** Counts calls to the real Native group exit, without replacing its implementation. */
UCLASS()
class UTerritoryPartyDialogueProbe : public UNarrativePartyComponent
{
	GENERATED_BODY()
public:
	int32 GroupExits = 0;
	virtual void ExitDialogue(const EExitDialogueReason Reason) override
	{
		++GroupExits;
		Super::ExitDialogue(Reason);
	}
};
