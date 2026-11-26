#include "Spawn/TerritorySpawnTypes.h"

#include "Core/TerritoryDeveloperSettings.h"

bool ShouldLogTerritorySpawn()
{
	// Defined once for the whole module. See the declaration in TerritorySpawnTypes.h for why this is
	// not a file-local helper: unity builds put several .cpp files in one translation unit, so two
	// same-named file-local helpers become an ambiguous overload.
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	return Settings && Settings->bEnableDebug;
}

// ─── FTerritorySpawnRecord ───

void FTerritorySpawnRecord::SetScalar(FName Key, float Value)
{
	if (Key.IsNone()) return;
	for (FTerritorySpawnScalar& Entry : Scalars)
	{
		if (Entry.Key == Key)
		{
			Entry.Value = Value;
			return;
		}
	}
	Scalars.Emplace(Key, Value);
}

bool FTerritorySpawnRecord::GetScalar(FName Key, float& OutValue) const
{
	if (Key.IsNone()) return false;
	for (const FTerritorySpawnScalar& Entry : Scalars)
	{
		if (Entry.Key == Key)
		{
			OutValue = Entry.Value;
			return true;
		}
	}
	return false;
}

float FTerritorySpawnRecord::GetScalarOrDefault(FName Key, float DefaultValue) const
{
	float Found = DefaultValue;
	GetScalar(Key, Found);
	return Found;
}

bool FTerritorySpawnRecord::RemoveScalar(FName Key)
{
	if (Key.IsNone()) return false;
	return Scalars.RemoveAll([Key](const FTerritorySpawnScalar& Entry)
	{
		return Entry.Key == Key;
	}) > 0;
}

void FTerritorySpawnRecord::PrepareScalarsForSave()
{
	if (Scalars.Num() <= 1) return;

	// Sort by key first so the array order is a pure function of the contents. Iteration order of a
	// map is not a save-stability guarantee, and unsorted arrays make unrelated saves differ byte-wise.
	Scalars.Sort([](const FTerritorySpawnScalar& A, const FTerritorySpawnScalar& B)
	{
		return A.Key.LexicalLess(B.Key);
	});

	// Collapse duplicates keeping the last written value. SetScalar already de-duplicates, so this
	// only matters for records produced by hand, by migration, or by an older writer.
	TArray<FTerritorySpawnScalar> Deduped;
	Deduped.Reserve(Scalars.Num());
	for (const FTerritorySpawnScalar& Entry : Scalars)
	{
		if (Entry.Key.IsNone()) continue;
		if (Deduped.Num() > 0 && Deduped.Last().Key == Entry.Key)
		{
			Deduped.Last().Value = Entry.Value;
			continue;
		}
		Deduped.Add(Entry);
	}
	Scalars = MoveTemp(Deduped);
}

bool FTerritorySpawnRecord::IsTransformUsable() const
{
	// Mirrors the project's existing ValidTransform rule: a NaN coordinate or an absurd magnitude is
	// a corrupt record, and fabricating a position for it would put an entity somewhere nobody chose.
	constexpr double MaxAbsCoordinate = 1.0e9;

	if (!Transform.IsValid()) return false;

	const FVector Location = Transform.GetLocation();
	if (Location.ContainsNaN()) return false;
	if (FMath::Abs(Location.X) > MaxAbsCoordinate) return false;
	if (FMath::Abs(Location.Y) > MaxAbsCoordinate) return false;
	if (FMath::Abs(Location.Z) > MaxAbsCoordinate) return false;

	const FRotator Rotation = Transform.Rotator();
	if (Rotation.ContainsNaN()) return false;

	const FVector Scale = Transform.GetScale3D();
	if (Scale.ContainsNaN()) return false;
	if (Scale.IsNearlyZero()) return false;

	return true;
}

bool FTerritorySpawnRecord::IsSchemaReadable() const
{
	// A record written by newer code may carry fields whose meaning this build cannot guess. Refuse
	// it and leave the slot dormant; overwriting a newer save is the one unrecoverable mistake here.
	return SchemaVersion >= MinimumSupportedSchemaVersion
		&& SchemaVersion <= CurrentSchemaVersion;
}

void FTerritorySpawnRecord::ResetInstanceState()
{
	SpawnedGuid.Invalidate();
	Transform = FTransform::Identity;
	bHasInstance = false;
	bWasDestroyed = false;
	StateTags.Reset();
	Scalars.Reset();
	LastTransitionGameTime = -1.0;
}
