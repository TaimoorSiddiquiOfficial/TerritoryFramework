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
	TerritoryToCells.Empty();
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

	TArray<FIntVector>& OccupiedCells = TerritoryToCells.FindOrAdd(Territory);

	for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
			{
				FIntVector CellKey(X, Y, Z);
				TArray<TWeakObjectPtr<ATerritoryVolume>>& Cell = Cells.FindOrAdd(CellKey);
				Cell.AddUnique(Territory);
				OccupiedCells.AddUnique(CellKey);
			}
		}
	}
}

void FTerritorySpatialIndex::Remove(ATerritoryVolume* Territory)
{
	if (!Territory) return;

	// Use reverse map to find only cells this territory occupies (O(k) vs O(all cells))
	const TArray<FIntVector>* OccupiedCells = TerritoryToCells.Find(Territory);
	if (OccupiedCells)
	{
		for (const FIntVector& CellKey : *OccupiedCells)
		{
			if (TArray<TWeakObjectPtr<ATerritoryVolume>>* Cell = Cells.Find(CellKey))
			{
				Cell->RemoveAll([Territory](const TWeakObjectPtr<ATerritoryVolume>& Ptr)
				{
					return !Ptr.IsValid() || Ptr.Get() == Territory;
				});
				if (Cell->Num() == 0)
				{
					Cells.Remove(CellKey);
				}
			}
		}
		TerritoryToCells.Remove(Territory);
	}
}

void FTerritorySpatialIndex::Update(ATerritoryVolume* Territory)
{
	if (!Territory) return;
	Remove(Territory);
	Insert(Territory);
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
