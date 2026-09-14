#pragma once

#include "Tales/TalesComponent.h"
#include "TerritoryDialogueLifecycleProbe.generated.h"

/** Observes Native's real reliable-exit dispatch in the no-network native fixture. */
UCLASS()
class UTerritoryDialogueLifecycleProbe : public UTalesComponent
{
	GENERATED_BODY()
public:
	int32 ExitDispatches = 0;
	virtual void ClientExitDialogue_Implementation(const EExitDialogueReason Reason) override
	{
		++ExitDispatches;
		Super::ClientExitDialogue_Implementation(Reason);
	}
};
