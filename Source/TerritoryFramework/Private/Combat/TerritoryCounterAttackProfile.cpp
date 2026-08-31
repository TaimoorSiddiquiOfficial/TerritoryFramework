#include "Combat/TerritoryCounterAttackProfile.h"

const FTerritoryFactionAssaultConfig* UTerritoryCounterAttackProfile::FindFactionForce(
	const FGameplayTag& Faction) const
{
	return FactionForces.FindByPredicate([&Faction](const FTerritoryFactionAssaultConfig& Config)
	{
		return Config.Faction == Faction;
	});
}

TSoftClassPtr<ANarrativeVehicleBase> UTerritoryCounterAttackProfile::ResolveVehicleClass(
	const FTerritoryFactionAssaultConfig& Force,
	const FTerritoryAssaultApproach& Approach)
{
	return Force.SignatureVehicleClass.IsNull()
		? Approach.VehicleClass : Force.SignatureVehicleClass;
}

int32 UTerritoryCounterAttackProfile::ResolveVehicleCountForDifficulty(
	const FTerritoryFactionAssaultConfig& Force,
	ENarrativeGameplayDifficulty Difficulty, int32 AuthoredRoadMaximum,
	int32 FiniteForce)
{
	const int32 SafeMaximum = FMath::Clamp(
		FMath::Min(AuthoredRoadMaximum, FiniteForce), 0, 8);
	if (!Force.bScaleVehicleCountByNarrativeDifficulty)
	{
		return SafeMaximum;
	}

	int32 Requested = 1;
	switch (Difficulty)
	{
	case ENarrativeGameplayDifficulty::Hard: Requested = 2; break;
	case ENarrativeGameplayDifficulty::Insane: Requested = 3; break;
	case ENarrativeGameplayDifficulty::Easy:
	case ENarrativeGameplayDifficulty::Medium:
	default: Requested = 1; break;
	}
	if (const FTerritoryDifficultyVehicleCount* Override =
		Force.VehicleCountsByDifficulty.FindByPredicate(
			[Difficulty](const FTerritoryDifficultyVehicleCount& Entry)
			{
				return Entry.Difficulty == Difficulty;
			}))
	{
		Requested = Override->MaximumCars;
	}
	return FMath::Clamp(Requested, 0, SafeMaximum);
}
