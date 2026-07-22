#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritorySpatialIndex.h"
#include "Engine/World.h"

void UTerritoryRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	float CellSize = Settings ? Settings->SpatialCellSize : 2000.f;
	SpatialIndex.Initialize(CellSize);

	UE_LOG(LogTerritory, Log, TEXT("TerritoryRegistrySubsystem initialized (spatial cell: %.0fu)"), CellSize);
}

void UTerritoryRegistrySubsystem::Deinitialize()
{
	RegisteredTerritories.Empty();
	TagToTerritoryMap.Empty();
	GUIDToTerritoryMap.Empty();
	SpatialIndex.Clear();
	Super::Deinitialize();
}

void UTerritoryRegistrySubsystem::RegisterTerritory(ATerritoryVolume* Territory)
{
	if (!Territory) return;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebug = Settings && Settings->ShouldDebugRegistry();

	FGameplayTag Tag = Territory->GetTerritoryTag();
	FGuid GUID = Territory->GetActorGUID_Implementation();

	if (bDebug)
	{
		UE_LOG(LogTerritory, Log, TEXT("[Registry] Registering %s (tag=%s, GUID=%s)"),
			*Territory->GetActorLabel(), *Tag.ToString(), *GUID.ToString());
	}

	// ─── Duplicate Tag Validation ───
	if (Tag.IsValid())
	{
		if (const TWeakObjectPtr<ATerritoryVolume>* Existing = TagToTerritoryMap.Find(Tag))
		{
			if (Existing->IsValid() && Existing->Get() != Territory)
			{
				UE_LOG(LogTerritory, Error,
					TEXT("DUPLICATE TAG: %s already registered by %s, rejecting %s"),
					*Tag.ToString(), *Existing->Get()->GetName(), *Territory->GetName());
				return;
			}
		}
	}

	// ─── Duplicate GUID Validation ───
	if (GUID.IsValid())
	{
		if (const TWeakObjectPtr<ATerritoryVolume>* Existing = GUIDToTerritoryMap.Find(GUID))
		{
			if (Existing->IsValid() && Existing->Get() != Territory)
			{
				UE_LOG(LogTerritory, Error,
					TEXT("DUPLICATE GUID: %s already registered by %s, rejecting %s"),
					*GUID.ToString(), *Existing->Get()->GetName(), *Territory->GetName());
				return;
			}
		}
		GUIDToTerritoryMap.Add(GUID, Territory);
	}

	RegisteredTerritories.AddUnique(Territory);

	if (Tag.IsValid())
	{
		TagToTerritoryMap.Add(Tag, Territory);
	}

	// Add to spatial index
	SpatialIndex.Insert(Territory);

	OnTerritoryRegistered.Broadcast(Territory, false);
	UE_LOG(LogTerritory, Log, TEXT("Registered territory: %s (tag: %s, GUID: %s, cells: %d)"),
		*Territory->GetName(), *Tag.ToString(), *GUID.ToString(),
		SpatialIndex.GetCellCount());
}

void UTerritoryRegistrySubsystem::UnregisterTerritory(ATerritoryVolume* Territory)
{
	if (!Territory) return;

	RegisteredTerritories.Remove(Territory);

	// CRITICAL FIX: Only remove tag/GUID mappings if they still point to THIS actor.
	// A rejected duplicate actor could otherwise remove the valid actor's mappings.
	FGameplayTag Tag = Territory->GetTerritoryTag();
	if (Tag.IsValid())
	{
		if (const TWeakObjectPtr<ATerritoryVolume>* Existing = TagToTerritoryMap.Find(Tag))
		{
			if (Existing->Get() == Territory)
			{
				TagToTerritoryMap.Remove(Tag);
			}
		}
	}

	FGuid GUID = Territory->GetActorGUID_Implementation();
	if (GUID.IsValid())
	{
		if (const TWeakObjectPtr<ATerritoryVolume>* Existing = GUIDToTerritoryMap.Find(GUID))
		{
			if (Existing->Get() == Territory)
			{
				GUIDToTerritoryMap.Remove(GUID);
			}
		}
	}

	// Remove from spatial index
	SpatialIndex.Remove(Territory);

	OnTerritoryUnregistered.Broadcast(Territory, true);
}

ATerritoryVolume* UTerritoryRegistrySubsystem::GetTerritoryByTag(const FGameplayTag& TerritoryTag) const
{
	const TWeakObjectPtr<ATerritoryVolume>* Found = TagToTerritoryMap.Find(TerritoryTag);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}

ATerritoryVolume* UTerritoryRegistrySubsystem::GetTerritoryByGUID(const FGuid& GUID) const
{
	const TWeakObjectPtr<ATerritoryVolume>* Found = GUIDToTerritoryMap.Find(GUID);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}

ATerritoryVolume* UTerritoryRegistrySubsystem::GetTerritoryAtLocation(const FVector& WorldLocation) const
{
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebugSpatial = Settings && Settings->ShouldDebugSpatial();

	TArray<ATerritoryVolume*> Candidates = SpatialIndex.QueryPoint(WorldLocation);

	if (Candidates.Num() == 0) return nullptr;

	// Return the most specific (smallest volume) territory.
	// Property < District < City — smallest bounds = most specific.
	ATerritoryVolume* Best = Candidates[0];
	float BestVolume = TNumericLimits<float>::Max();

	for (ATerritoryVolume* Candidate : Candidates)
	{
		if (!Candidate) continue;
		FBox Bounds = Candidate->GetTerritoryBounds();
		FVector Size = Bounds.GetSize();
		float Volume = Size.X * Size.Y * Size.Z;
		if (Volume < BestVolume)
		{
			BestVolume = Volume;
			Best = Candidate;
		}
	}

	if (bDebugSpatial)
	{
		UE_LOG(LogTerritory, Log, TEXT("[Spatial] QueryPoint(%s) → %d candidates, best=%s (volume=%.0f)"),
			*WorldLocation.ToString(), Candidates.Num(),
			Best ? *Best->GetTerritoryTag().ToString() : TEXT("null"), BestVolume);
	}

	return Best;
}

TArray<ATerritoryVolume*> UTerritoryRegistrySubsystem::GetTerritoriesAtLocation(const FVector& WorldLocation) const
{
	return SpatialIndex.QueryPoint(WorldLocation);
}

TArray<ATerritoryVolume*> UTerritoryRegistrySubsystem::GetTerritoriesInBox(const FBox& QueryBox) const
{
	return SpatialIndex.QueryBox(QueryBox);
}

TArray<ATerritoryVolume*> UTerritoryRegistrySubsystem::GetTerritoriesOwnedByFaction(const FGameplayTag& Faction) const
{
	TArray<ATerritoryVolume*> Result;
	for (const TObjectPtr<ATerritoryVolume>& Territory : RegisteredTerritories)
	{
		if (Territory && Territory->IsOwnedByFaction(Faction))
		{
			Result.Add(Territory);
		}
	}
	return Result;
}

TArray<ATerritoryVolume*> UTerritoryRegistrySubsystem::GetAllTerritories() const
{
	TArray<ATerritoryVolume*> Result;
	for (const TObjectPtr<ATerritoryVolume>& Territory : RegisteredTerritories)
	{
		if (Territory)
		{
			Result.Add(Territory);
		}
	}
	return Result;
}

int32 UTerritoryRegistrySubsystem::GetTerritoryCount() const
{
	return RegisteredTerritories.Num();
}

int32 UTerritoryRegistrySubsystem::GetTerritoryCountForFaction(const FGameplayTag& Faction) const
{
	int32 Count = 0;
	for (const TObjectPtr<ATerritoryVolume>& Territory : RegisteredTerritories)
	{
		if (Territory && Territory->IsOwnedByFaction(Faction))
		{
			++Count;
		}
	}
	return Count;
}

TArray<ATerritoryVolume*> UTerritoryRegistrySubsystem::GetChildTerritories(const FGameplayTag& ParentTag) const
{
	TArray<ATerritoryVolume*> Result;
	if (!ParentTag.IsValid()) return Result;

	for (const TObjectPtr<ATerritoryVolume>& Territory : RegisteredTerritories)
	{
		if (!Territory) continue;

		FGameplayTag ParentRef = Territory->GetParentTerritoryTag();
		if (ParentRef == ParentTag)
		{
			Result.Add(Territory);
		}
	}
	return Result;
}
