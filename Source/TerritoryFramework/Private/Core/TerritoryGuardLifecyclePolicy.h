#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryTypes.h"
#include "GameplayTagContainer.h"

enum class ETerritoryGuardLifecycleAction : uint8
{
	Preserve,
	Retire,
	Restore,
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

		if (NewState == ETerritoryState::Locked)
		{
			return ETerritoryGuardLifecycleAction::Retire;
		}

		if (OldState == ETerritoryState::Locked
			&& NewState == ETerritoryState::Claimed
			&& NewOwner.IsValid())
		{
			return ETerritoryGuardLifecycleAction::Restore;
		}

		// Claim/contest transitions preserve the exact surviving garrison. Capture
		// pressure must never despawn defenders or grant free replacements.
		return ETerritoryGuardLifecycleAction::Preserve;
	}
}
