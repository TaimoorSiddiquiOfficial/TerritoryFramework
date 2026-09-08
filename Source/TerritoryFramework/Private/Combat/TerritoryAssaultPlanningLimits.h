#pragma once

#include "CoreMinimal.h"
#include "Templates/Greater.h"

// Pure calculations used by the existing server-owned assault planner.
namespace TerritoryAssaultPlanning
{
	inline constexpr int32 MaximumApproaches = 8;
	inline constexpr int32 MaximumVehicles = 8;

	inline int32 ResolveApproachCount(float PowerRatio, int32 ConfiguredMaximum)
	{
		const int32 Limit = FMath::Clamp(ConfiguredMaximum, 1, MaximumApproaches);
		if (FMath::IsNaN(PowerRatio)) return 1;
		if (!FMath::IsFinite(PowerRatio)) return PowerRatio > 0.f ? Limit : 1;
		// Clamp before float-to-int conversion; extreme finite power must not wrap
		// back to one approach. NaN and negative infinity conservatively select one.
		return FMath::FloorToInt(FMath::Clamp(PowerRatio, 1.f, static_cast<float>(Limit)));
	}

	inline void AccumulateVehicleCapacity(int32 RequestedCars, int32 SeatsPerCar,
		int32& AuthoredRoadMaximum, TArray<int32>& DeploymentCapacities)
	{
		const int32 AuthoredDeployments = FMath::Clamp(RequestedCars, 0, MaximumVehicles);
		AuthoredRoadMaximum = FMath::Min(MaximumVehicles,
			FMath::Clamp(AuthoredRoadMaximum, 0, MaximumVehicles) + AuthoredDeployments);
		for (int32 Index = 0; Index < AuthoredDeployments; ++Index)
		{
			DeploymentCapacities.Add(FMath::Max(0, SeatsPerCar));
		}
		// Difficulty never authorizes more than eight cars. Retaining the eight
		// largest capacities gives the existing seat planner the same answer without
		// expanding malformed authored counts or depending on approach order.
		DeploymentCapacities.Sort(TGreater<int32>());
		if (DeploymentCapacities.Num() > MaximumVehicles)
		{
			DeploymentCapacities.SetNum(MaximumVehicles, EAllowShrinking::No);
		}
	}
}
