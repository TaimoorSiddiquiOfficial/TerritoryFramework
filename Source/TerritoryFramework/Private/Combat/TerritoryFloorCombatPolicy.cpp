#include "Combat/TerritoryFloorCombatPolicy.h"

FPrimaryAssetId UTerritoryFloorCombatPolicy::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("TerritoryFloorCombatPolicy"), GetFName());
}
