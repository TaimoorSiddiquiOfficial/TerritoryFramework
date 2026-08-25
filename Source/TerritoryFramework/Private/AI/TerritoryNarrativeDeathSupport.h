#pragma once

class ANarrativeNPCCharacter;
class UNarrativeAbilitySystemComponent;

namespace TerritoryNarrativeDeathSupport
{
	bool ResolveDeathState(const UNarrativeAbilitySystemComponent* AbilitySystem,
		bool bReportedIsDead);

	/** Stop Narrative activity scoring and detach goals before its controller is cleaned up. */
	bool PrepareForRemoval(ANarrativeNPCCharacter& Character);

	void FinalizePhysicalDeath(ANarrativeNPCCharacter& Character);
}
