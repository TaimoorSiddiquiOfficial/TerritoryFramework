#pragma once

class AActor;
class ANarrativeNPCController;
class UNPCGoalItem;

namespace TerritoryCombatTargetPolicy
{
	/** Snapshot a Native goal's Blueprint-overridable key exactly once and validate
	 * that same actor against current Territory combat admission. */
	const AActor* ResolveEligibleTarget(const ANarrativeNPCController* Controller, const UNPCGoalItem* Goal);
}
