#pragma once

#include "CoreMinimal.h"

/** Pure arithmetic shared by loaded presentation and durable campaign rate reduction. */
namespace TerritoryCurrencyRates
{
	inline int32 Income(int32 Base, int32 BonusPerLevel, int32 UpgradeLevel,
		float CapitalMultiplier, bool bAllowed)
	{
		if (!bAllowed) return 0;
		const double Multiplier = FMath::IsFinite(CapitalMultiplier)
			? FMath::Max(0.0, static_cast<double>(CapitalMultiplier)) : 0.0;
		const int64 ScaledBase = static_cast<int64>(FMath::Clamp(
			static_cast<double>(FMath::Max(0, Base)) * Multiplier, 0.0, static_cast<double>(MAX_int32)));
		const int64 Bonus = static_cast<int64>(FMath::Max(0, UpgradeLevel)) * FMath::Max(0, BonusPerLevel);
		return static_cast<int32>(FMath::Min<int64>(ScaledBase + Bonus, MAX_int32));
	}

	inline int32 Upkeep(int32 CostPerGuard, int32 DesiredGarrison)
	{
		return static_cast<int32>(FMath::Min<int64>(MAX_int32,
			static_cast<int64>(FMath::Max(0, CostPerGuard)) * FMath::Max(0, DesiredGarrison)));
	}
}
