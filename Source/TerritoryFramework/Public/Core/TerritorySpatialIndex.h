#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryTypes.h"
#include "TerritorySpatialIndex.generated.h"

class ATerritoryVolume;

/**
 * Grid-based spatial hash for O(1) territory location lookups.
 * Divides world space into cells of CellSize. Each territory is inserted
 * into a bounded number of overlapping cells. Oversized volumes use a small
 * fallback set; oversized box queries scan registered bounds instead of empty space.
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
	int32 GetTotalCellEntries() const;

	/** Removes entries for destroyed/GC'd territories from both the forward and reverse maps. */
	void RemoveInvalidTerritories();

private:
	UPROPERTY()
	float CellSize;

	// Cell key → list of territory weak pointers
	TMap<FIntVector, TArray<TWeakObjectPtr<ATerritoryVolume>>> Cells;

	// Reverse map: territory → set of cell keys it occupies (for O(k) Remove)
	TMap<TWeakObjectPtr<ATerritoryVolume>, TArray<FIntVector>> TerritoryToCells;
	TSet<TWeakObjectPtr<ATerritoryVolume>> OversizedTerritories;

	bool WorldToCell(const FVector& Location, FIntVector& OutCell) const;
	bool GetBoundedCellRange(const FBox& Bounds, FIntVector& OutMin, FIntVector& OutMax) const;
};
