#pragma once

#include "CoreMinimal.h"
#include "AI/NarrativeNPCController.h"
#include "TerritoryNPCController.generated.h"

/** Narrative controller with the Territory save adapter on its existing activity component. */
UCLASS()
class TERRITORYFRAMEWORK_API ATerritoryNPCController : public ANarrativeNPCController
{
	GENERATED_BODY()
public:
	ATerritoryNPCController(const FObjectInitializer& ObjectInitializer);

	/**
	 * One NPC, one answer. The inherited implementation consults the Narrative faction table directly,
	 * so it bypasses every Territory engagement gate the possessed guard applies. Measured in live PIE
	 * on seven guards before this override existed: with the table Hostile, the guard pawn answered
	 * Neutral while its own controller answered Hostile, for the same target.
	 *
	 * The possessed ANarrativeCharacter is the authority on whether this NPC may engage, and
	 * ATerritoryGuardCharacter deliberately downgrades stale Narrative hostility to Neutral. Defer to
	 * it rather than computing a second, ungated answer here.
	 */
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
};
