#include "Core/TerritoryTypes.h"

DEFINE_LOG_CATEGORY(LogTerritory);

ETerritoryAvailability TerritoryResolveInitialAvailability(
	const ETerritoryInitialState InitialState,
	const ETerritoryAvailability InitialAvailability)
{
	// The only surviving job of the legacy Initial State value. Everything else defers to
	// Initial Availability, so clearing this branch for any reason would silently unlock every
	// Territory authored before Initial Availability existed. See the header for the long form.
	return InitialState == ETerritoryInitialState::Locked
		? ETerritoryAvailability::Locked
		: InitialAvailability;
}

ETerritoryState TerritoryResolveInitialPoliticalState(
	const ETerritoryInitialState InitialState,
	const bool bHasInitialOwningFaction)
{
	// Explicitly Unclaimed overrides a filled faction: that is what the option says it does.
	if (InitialState == ETerritoryInitialState::Unclaimed)
	{
		return ETerritoryState::Unclaimed;
	}

	// Automatic, Claimed and the legacy Locked are the same rule here, which is why this used to be
	// three near-identical switch bodies that had to be kept in step by hand. An owner means Claimed;
	// no owner means Unclaimed, so "Claimed with an empty faction" can never be produced.
	return bHasInitialOwningFaction
		? ETerritoryState::Claimed
		: ETerritoryState::Unclaimed;
}
