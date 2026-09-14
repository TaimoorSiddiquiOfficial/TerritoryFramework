#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeParty.h"
#include "TerritoryNarrativeParty.generated.h"

/** Narrative's party actor with the Territory reply-policy adapter in its existing Tales slot.
 * Spawn on the server and use the inherited Add/Remove Party Member workflow.
 */
UCLASS(Blueprintable, meta=(DisplayName="Territory Narrative Party"))
class TERRITORYFRAMEWORK_API ATerritoryNarrativeParty : public ANarrativeParty
{
	GENERATED_BODY()
public:
	ATerritoryNarrativeParty(const FObjectInitializer& ObjectInitializer);
	virtual void AddPartyMember(ANarrativePlayerState* PS) override;
	virtual void RemovePartyMember(ANarrativePlayerState* PS) override;
};
