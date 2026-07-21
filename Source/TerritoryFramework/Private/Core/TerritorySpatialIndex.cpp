#include "Core/TerritorySpatialIndex.h"
#include "Core/TerritoryVolume.h"

void FTerritorySpatialIndex::Initialize(float InCellSize)
{
	CellSize = FMath::Max(InCellSize, 500.f);
	Clear();
}

void FTerritorySpatialIndex::Clear()
{
	Cells.Empty();
}

FIntVector FTerritorySpatialIndex::WorldToCell(const FVector& Location) const
{
	return FIntVector(
		FMath::FloorToInt(Location.X / CellSize),
		FMath::FloorToInt(Location.Y / CellSize),
		FMath::FloorToInt(Location.Z / CellSize));
}

void FTerritorySpatialIndex::GetCellRange(const FBox& Bounds, FIntVector& OutMin, FIntVector& OutMax) const
{
	OutMin = WorldToCell(Bounds.Min);
	OutMax = WorldToCell(Bounds.Max);
}

void FTerritorySpatialIndex::Insert(ATerritoryVolume* Territory)
{
	if (!Territory) return;

	FBox Bounds = Territory->GetTerritoryBounds();
	FIntVector MinCell, MaxCell;
	GetCellRange(Bounds, MinCell, MaxCell);

	for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
			{
				FIntVector CellKey(X, Y, Z);
				TArray<TWeakObjectPtr<ATerritoryVolume>>& Cell = Cells.FindOrAdd(CellKey);
				Cell.AddUnique(Territory);
			}
		}
	}
}

void FTerritorySpatialIndex::Remove(ATerritoryVolume* Territory)
{
	if (!Territory) return;

	for (auto& Pair : Cells)
	{
		Pair.Value.RemoveAll([Territory](const TWeakObjectPtr<ATerritoryVolume>& Ptr)
		{
			return !Ptr.IsValid() || Ptr.Get() == Territory;
		});
	}

	// Clean up empty cells
	TArray<FIntVector> EmptyCells;
	for (const auto& Pair : Cells)
	{
		if (Pair.Value.Num() == 0)
		{
			EmptyCells.Add(Pair.Key);
		}
	}
	for (const FIntVector& Key : EmptyCells)
	{
		Cells.Remove(Key);
	}
}

TArray<ATerritoryVolume*> FTerritorySpatialIndex::QueryPoint(const FVector& WorldLocation) const
{
	TArray<ATerritoryVolume*> Result;

	FIntVector CellKey = WorldToCell(WorldLocation);
	const TArray<TWeakObjectPtr<ATerritoryVolume>>* Cell = Cells.Find(CellKey);
	if (!Cell) return Result;

	for (const TWeakObjectPtr<ATerritoryVolume>& Ptr : *Cell)
	{
		if (Ptr.IsValid() && Ptr->ContainsPoint(WorldLocation))
		{
			Result.Add(Ptr.Get());
		}
	}
	return Result;
}

TArray<ATerritoryVolume*> FTerritorySpatialIndex::QueryBox(const FBox& QueryBox) const
{
	TArray<ATerritoryVolume*> Result;
	TSet<ATerritoryVolume*> Visited;

	FIntVector MinCell, MaxCell;
	GetCellRange(QueryBox, MinCell, MaxCell);

	for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
			{
				FIntVector CellKey(X, Y, Z);
				const TArray<TWeakObjectPtr<ATerritoryVolume>>* Cell = Cells.Find(CellKey);
				if (!Cell) continue;

				for (const TWeakObjectPtr<ATerritoryVolume>& Ptr : *Cell)
				{
					if (Ptr.IsValid() && !Visited.Contains(Ptr.Get()))
					{
						Visited.Add(Ptr.Get());
						FBox TerrBounds = Ptr->GetTerritoryBounds();
						if (TerrBounds.Intersect(QueryBox))
						{
							Result.Add(Ptr.Get());
						}
					}
				}
			}
		}
	}
	return Result;
}

int32 FTerritorySpatialIndex::GetEntryCount() const
{
	int32 Count = 0;
	for (const auto& Pair : Cells)
	{
		Count += Pair.Value.Num();
	}
	return Count;
}
