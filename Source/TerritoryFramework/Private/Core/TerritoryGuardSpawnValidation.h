#pragma once

#include "CoreMinimal.h"

namespace TerritoryGuardSpawnValidation
{
	inline bool IsPlacementAcceptable(const FTransform& Expected, const FTransform& Actual)
	{
		constexpr float HorizontalTolerance = 1.f;
		constexpr float FloorSnapTolerance = 10.f;
		constexpr float RotationTolerance = 0.001f;

		const FVector Delta = Actual.GetLocation() - Expected.GetLocation();
		return FMath::Abs(Delta.X) <= HorizontalTolerance
			&& FMath::Abs(Delta.Y) <= HorizontalTolerance
			&& FMath::Abs(Delta.Z) <= FloorSnapTolerance
			&& Actual.GetRotation().Equals(Expected.GetRotation(), RotationTolerance);
	}
}
