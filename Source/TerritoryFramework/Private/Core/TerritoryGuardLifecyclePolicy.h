#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryTypes.h"
#include "GameplayTagContainer.h"

enum class ETerritoryGuardLifecycleAction : uint8
{
	Preserve,
	ReplaceForNewOwner
};

namespace TerritoryGuardLifecyclePolicy
{
	inline ETerritoryGuardLifecycleAction DetermineAction(
		const FGameplayTag& OldOwner, const FGameplayTag& NewOwner,
		ETerritoryState OldState, ETerritoryState NewState)
	{
		if (OldOwner != NewOwner)
		{
			return ETerritoryGuardLifecycleAction::ReplaceForNewOwner;
		}

		if (OldState == NewState)
		{
			return ETerritoryGuardLifecycleAction::Preserve;
		}

		// Only two actions exist, deliberately. There is no Locked branch here:
		// ATerritoryVolume::CommitOwnershipData rejects a Locked control state, and
		// locking writes ETerritoryAvailability instead, so no transition can reach
		// one. Garrison changes on lock/unlock belong to
		// ATerritoryVolume::ReconcileAvailabilityDependentSystems(), which calls
		// DespawnGuards() when availability becomes Locked and re-spawns to the
		// desired count on the next unlock. Do not reintroduce a Locked branch.

		// Claim/contest transitions preserve the exact surviving garrison. Capture
		// pressure must never despawn defenders or grant free replacements.
		return ETerritoryGuardLifecycleAction::Preserve;
	}
}
