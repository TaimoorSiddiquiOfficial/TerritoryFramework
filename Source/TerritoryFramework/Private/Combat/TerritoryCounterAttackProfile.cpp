#include "Combat/TerritoryCounterAttackProfile.h"

const FTerritoryFactionAssaultConfig* UTerritoryCounterAttackProfile::FindFactionForce(
	const FGameplayTag& Faction) const
{
	return FactionForces.FindByPredicate([&Faction](const FTerritoryFactionAssaultConfig& Config)
	{
		return Config.Faction == Faction;
	});
}
