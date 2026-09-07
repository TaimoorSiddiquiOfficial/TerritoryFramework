#pragma once
#include "CoreMinimal.h"

namespace TerritoryRoadSurfaceExtraction
{
	struct FGrid
	{
		int32 Width = 0;
		int32 Height = 0;
		float Spacing = 200.f;
		FVector Origin = FVector::ZeroVector;
		TArray<uint8> Road;
		TArray<float> Heights;
	};
	struct FNetwork
	{
		TArray<TArray<FVector>> Roads;
		TArray<TArray<FVector>> Junctions;
		int32 OpenEnds = 0;
	};
	/** Deterministic bounded medial-axis extraction of a single collision height band. */
	bool Extract(const FGrid& Grid, float RoadWidth, float MaximumStep,
		FNetwork& Out, FString& Failure);
}
