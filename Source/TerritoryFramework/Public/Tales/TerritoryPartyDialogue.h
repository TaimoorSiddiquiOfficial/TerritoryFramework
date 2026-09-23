#pragma once

#include "CoreMinimal.h"
#include "Tales/Dialogue.h"
#include "TerritoryPartyDialogue.generated.h"

/** Optional Native dialogue extension for continuing after a remote owner leaves.
 * Reparent only dialogues used by continuing parties. Native still owns playback,
 * node events, quest state, client copies and speaker grants.
 */
UCLASS(Blueprintable, BlueprintType, meta=(DisplayName="Territory Party Dialogue"))
class TERRITORYFRAMEWORK_API UTerritoryPartyDialogue : public UDialogue
{
	GENERATED_BODY()
public:
	/** Local viewport transfers need a project presentation adapter. The default
	 * supports server context migration between remote members; other cases end
	 * through Native without replaying events. Override both methods as a pair. */
	virtual bool CanTransferPartyContext(APlayerController* NextController) const;
	virtual bool TransferPartyContext(APlayerController* NextController);
};
