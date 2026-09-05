#pragma once

class ANarrativeNPCCharacter;
class UNarrativeAbilitySystemComponent;

namespace TerritoryNarrativeDeathSupport
{
	bool ResolveDeathState(const UNarrativeAbilitySystemComponent* AbilitySystem,
		bool bReportedIsDead);

	/** Stop Narrative activity scoring and detach goals before its controller is cleaned up. */
	bool PrepareForRemoval(ANarrativeNPCCharacter& Character);

	/**
	 * Remove an NPC from play immediately, but defer actor destruction long enough
	 * for Narrative Blueprint latent actions to observe the deactivated component.
	 */
	void ScheduleRemoval(ANarrativeNPCCharacter& Character,
		float LatentCleanupGraceSeconds = 0.75f);

	void FinalizePhysicalDeath(ANarrativeNPCCharacter& Character);
}
