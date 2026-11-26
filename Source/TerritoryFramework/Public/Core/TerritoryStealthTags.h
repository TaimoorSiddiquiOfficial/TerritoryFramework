#pragma once

#include "NativeGameplayTags.h"

namespace TerritoryStealthTags
{
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Exposed);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(StealthAbility);
	/** Narrative-compatible GAS asset tag used by the distraction throw ability. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DistractionAbility);
	/** Gameplay Event emitted after the server commits and spawns a distraction. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DistractionThrownEvent);
	/** Gameplay Event emitted when the projectile reports its first impact noise. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DistractionImpactEvent);
	/**
	 * Gameplay Event a throw montage's Territory Throw Release notify sends at the release frame.
	 * The ability waits for it before spawning; designers pick the tag per notify, so the same
	 * notify works for any montage-driven throw.
	 */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DistractionReleaseEvent);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DistractionThrowable);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InvestigationActive);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DisguiseActive);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DisguiseCompromised);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DisguiseActivatedEvent);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DisguiseRemovedEvent);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DisguiseCompromisedEvent);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DisguiseRestoredEvent);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DisguiseCheckPassedEvent);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DisguiseCheckFailedEvent);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DisguiseOfficerClearance);
}
