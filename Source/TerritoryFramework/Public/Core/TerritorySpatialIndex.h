#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryTypes.h"
#include "TerritorySpatialIndex.generated.h"

class ATerritoryVolume;

/**
 * Grid-based spatial hash for O(1) territory location lookups.
 * Divides world space into cells of CellSize. Each territory is inserted
 * into every cell its bounding box overlaps. Point queries check only
 * the cell containing the point, then do precise ContainsPoint on candidates.
 */
USTRUCT()
struct FTerritorySpatialIndex
{
	GENERATED_BODY()

	FTerritorySpatialIndex() : CellSize(2000.f) {}

	void Initialize(float InCellSize);
	void Clear();

	void Insert(ATerritoryVolume* Territory);
	void Remove(ATerritoryVolume* Territory);

	/** Re-insert a territory after it has moved or been resized. */
	void Update(ATerritoryVolume* Territory);

	/** Returns candidate territories in the cell containing WorldLocation (O(1) hash + O(k) candidates) */
	TArray<ATerritoryVolume*> QueryPoint(const FVector& WorldLocation) const;

	/** Returns all territories whose bounds overlap the given box */
	TArray<ATerritoryVolume*> QueryBox(const FBox& QueryBox) const;

	int32 GetCellCount() const { return Cells.Num(); }
	int32 GetEntryCount() const;

private:
	UPROPERTY()
	float CellSize;

	// Cell key → list of territory weak pointers
	TMap<FIntVector, TArray<TWeakObjectPtr<ATerritoryVolume>>> Cells;

	// Reverse map: territory → set of cell keys it occupies (for O(k) Remove)
	TMap<TWeakObjectPtr<ATerritoryVolume>, TArray<FIntVector>> TerritoryToCells;

	FIntVector WorldToCell(const FVector& Location) const;
	void GetCellRange(const FBox& Bounds, FIntVector& OutMin, FIntVector& OutMax) const;
};
