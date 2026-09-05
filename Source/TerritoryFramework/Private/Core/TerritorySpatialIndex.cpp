#include "Core/TerritorySpatialIndex.h"
#include "Core/TerritoryVolume.h"

namespace
{
	bool IsUsableSpatialBox(const FBox& Bounds)
	{
		return Bounds.IsValid && !Bounds.Min.ContainsNaN() && !Bounds.Max.ContainsNaN()
			&& Bounds.Min.X <= Bounds.Max.X && Bounds.Min.Y <= Bounds.Max.Y && Bounds.Min.Z <= Bounds.Max.Z;
	}
}

void FTerritorySpatialIndex::Initialize(float InCellSize)
{
	CellSize = FMath::IsFinite(InCellSize) ? FMath::Max(InCellSize, 500.f) : 2000.f;
	Clear();
}

void FTerritorySpatialIndex::Clear()
{
	Cells.Empty();
	TerritoryToCells.Empty();
	OversizedTerritories.Empty();
}

bool FTerritorySpatialIndex::WorldToCell(const FVector& Location, FIntVector& OutCell) const
{
	if (Location.ContainsNaN()) return false;
	const FVector Scaled = Location / CellSize;
	if (Scaled.X < MIN_int32 || Scaled.X > MAX_int32
		|| Scaled.Y < MIN_int32 || Scaled.Y > MAX_int32
		|| Scaled.Z < MIN_int32 || Scaled.Z > MAX_int32) return false;
	OutCell = FIntVector(FMath::FloorToInt(Scaled.X), FMath::FloorToInt(Scaled.Y), FMath::FloorToInt(Scaled.Z));
	return true;
}

bool FTerritorySpatialIndex::GetBoundedCellRange(const FBox& Bounds, FIntVector& OutMin, FIntVector& OutMax) const
{
	if (!IsUsableSpatialBox(Bounds) || !WorldToCell(Bounds.Min, OutMin) || !WorldToCell(Bounds.Max, OutMax)) return false;
	constexpr int64 MaximumGridCells = 4096;
	int64 CellCount = 1;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const int64 Width = static_cast<int64>(OutMax[Axis]) - OutMin[Axis] + 1;
		if (Width <= 0 || Width > MaximumGridCells / CellCount) return false;
		CellCount *= Width;
	}
	return true;
}

void FTerritorySpatialIndex::Insert(ATerritoryVolume* Territory)
{
	if (!IsValid(Territory)) return;
	Remove(Territory);

	FBox Bounds = Territory->GetTerritoryBounds();
	if (!IsUsableSpatialBox(Bounds)) return;
	FIntVector MinCell, MaxCell;
	TArray<FIntVector>& OccupiedCells = TerritoryToCells.FindOrAdd(Territory);
	if (!GetBoundedCellRange(Bounds, MinCell, MaxCell))
	{
		OversizedTerritories.Add(Territory);
		return;
	}

	for (int64 X = MinCell.X; X <= MaxCell.X; ++X)
	{
		for (int64 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int64 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
			{
				FIntVector CellKey(static_cast<int32>(X), static_cast<int32>(Y), static_cast<int32>(Z));
				TArray<TWeakObjectPtr<ATerritoryVolume>>& Cell = Cells.FindOrAdd(CellKey);
				Cell.AddUnique(Territory);
				OccupiedCells.Add(CellKey);
			}
		}
	}
}

void FTerritorySpatialIndex::Remove(ATerritoryVolume* Territory)
{
	if (!Territory) return;
	OversizedTerritories.Remove(Territory);

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
	Insert(Territory);
}

TArray<ATerritoryVolume*> FTerritorySpatialIndex::QueryPoint(const FVector& WorldLocation) const
{
	TArray<ATerritoryVolume*> Result;
	if (WorldLocation.ContainsNaN()) return Result;
	FIntVector CellKey;
	const TArray<TWeakObjectPtr<ATerritoryVolume>>* Cell = WorldToCell(WorldLocation, CellKey) ? Cells.Find(CellKey) : nullptr;
	if (Cell)
	{
		for (const TWeakObjectPtr<ATerritoryVolume>& Ptr : *Cell)
		{
			if (Ptr.IsValid() && Ptr->ContainsPoint(WorldLocation)) Result.Add(Ptr.Get());
		}
	}
	for (const TWeakObjectPtr<ATerritoryVolume>& Ptr : OversizedTerritories)
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
	if (!IsUsableSpatialBox(QueryBox)) return Result;
	TSet<ATerritoryVolume*> Visited;
	auto AddIfOverlapping = [&Result, &Visited, &QueryBox](const TWeakObjectPtr<ATerritoryVolume>& Ptr)
	{
		if (!Ptr.IsValid() || Visited.Contains(Ptr.Get())) return;
		Visited.Add(Ptr.Get());
		if (Ptr->GetTerritoryBounds().Intersect(QueryBox)) Result.Add(Ptr.Get());
	};

	FIntVector MinCell, MaxCell;
	if (!GetBoundedCellRange(QueryBox, MinCell, MaxCell))
	{
		for (const auto& Pair : TerritoryToCells) AddIfOverlapping(Pair.Key);
		return Result;
	}

	for (int64 X = MinCell.X; X <= MaxCell.X; ++X)
	{
		for (int64 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int64 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
			{
				FIntVector CellKey(static_cast<int32>(X), static_cast<int32>(Y), static_cast<int32>(Z));
				const TArray<TWeakObjectPtr<ATerritoryVolume>>* Cell = Cells.Find(CellKey);
				if (!Cell) continue;

				for (const TWeakObjectPtr<ATerritoryVolume>& Ptr : *Cell)
				{
					AddIfOverlapping(Ptr);
				}
			}
		}
	}
	for (const TWeakObjectPtr<ATerritoryVolume>& Ptr : OversizedTerritories) AddIfOverlapping(Ptr);
	return Result;
}

int32 FTerritorySpatialIndex::GetTotalCellEntries() const
{
	int32 Count = 0;
	for (const auto& Pair : Cells)
	{
		Count += Pair.Value.Num();
	}
	return Count;
}

void FTerritorySpatialIndex::RemoveInvalidTerritories()
{
	TArray<TWeakObjectPtr<ATerritoryVolume>> InvalidKeys;
	for (const auto& Pair : TerritoryToCells)
	{
		if (!Pair.Key.IsValid())
		{
			InvalidKeys.Add(Pair.Key);
			for (const FIntVector& CellKey : Pair.Value)
			{
				if (TArray<TWeakObjectPtr<ATerritoryVolume>>* Cell = Cells.Find(CellKey))
				{
					Cell->RemoveAll([](const TWeakObjectPtr<ATerritoryVolume>& Ptr) { return !Ptr.IsValid(); });
					if (Cell->Num() == 0)
					{
						Cells.Remove(CellKey);
					}
				}
			}
		}
	}
	for (const TWeakObjectPtr<ATerritoryVolume>& Key : InvalidKeys)
	{
		TerritoryToCells.Remove(Key);
		OversizedTerritories.Remove(Key);
	}
}
