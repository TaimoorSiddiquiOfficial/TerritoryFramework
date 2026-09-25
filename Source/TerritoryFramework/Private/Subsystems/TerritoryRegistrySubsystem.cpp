#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritorySpatialIndex.h"
#include "Core/TerritoryFloorVolume.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UTerritoryRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	float CellSize = Settings ? Settings->SpatialCellSize : 2000.f;
	SpatialIndex.Initialize(CellSize);

	// Periodically check if any territory has moved/resized (every 2s — cheap bounds compare).
	// Each world owns a local query cache. Clients must reindex replicated movement too.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BoundsCheckTimerHandle,
			this,
			&UTerritoryRegistrySubsystem::PollBoundsChanges,
			2.f,
			true);
	}

	if (Settings && Settings->ShouldDebugRegistry())
	{
		UE_LOG(LogTerritory, Log, TEXT("[Registry] subsystem initialized (spatial cell: %.0fu)"), CellSize);
	}
}

void UTerritoryRegistrySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BoundsCheckTimerHandle);
	}
	RegisteredTerritories.Empty();
	TagToTerritoryMap.Empty();
	GUIDToTerritoryMap.Empty();
	SpatialIndex.Clear();
	FloorVolumesByPlaceTag.Empty();
	Super::Deinitialize();
}

void UTerritoryRegistrySubsystem::PollBoundsChanges()
{
	// Reindex local bounds after server edits or replicated movement. This mutates
	// only the per-world spatial cache, never territory ownership or replicated data.
	RegisteredTerritories.RemoveAll([](const TWeakObjectPtr<ATerritoryVolume>& Territory) { return !Territory.IsValid(); });
	for (const TWeakObjectPtr<ATerritoryVolume>& TerritoryPtr : RegisteredTerritories)
	{
		if (ATerritoryVolume* Territory = TerritoryPtr.Get())
		{
			Territory->CheckBoundsForReindex();
		}
	}

	// Cleanup stale entries from destroyed territories that were never properly removed.
	SpatialIndex.RemoveInvalidTerritories();

	// P2-N07: Prune stale tag/GUID map entries for destroyed territories
	for (auto It = TagToTerritoryMap.CreateIterator(); It; ++It)
	{
		if (!It->Value.IsValid()) It.RemoveCurrent();
	}
	for (auto It = GUIDToTerritoryMap.CreateIterator(); It; ++It)
	{
		if (!It->Value.IsValid()) It.RemoveCurrent();
	}
}

ETerritoryRegistrationResult UTerritoryRegistrySubsystem::RegisterTerritory(ATerritoryVolume* Territory)
{
	if (!IsValid(Territory) || Territory->IsActorBeingDestroyed()
		|| Territory->GetWorld() != GetWorld()) return ETerritoryRegistrationResult::InvalidTerritory;

	// P2-01: Reject territories with no usable identity
	FGameplayTag Tag = Territory->GetTerritoryTag();
	FGuid GUID = Territory->GetActorGUID_Implementation();

	if (!Tag.IsValid())
	{
		UE_LOG(LogTerritory, Error, TEXT("[Registry] Rejecting %s — invalid TerritoryTag"),
			*Territory->GetName());
		return ETerritoryRegistrationResult::InvalidTerritory;
	}
	if (!GUID.IsValid())
	{
		UE_LOG(LogTerritory, Error, TEXT("[Registry] Rejecting %s — invalid TerritoryGUID"),
			*Territory->GetName());
		return ETerritoryRegistrationResult::InvalidTerritory;
	}
	if (RegisteredTerritories.Contains(Territory))
	{
		// Identity changes require explicit removal before re-admission. Repeating
		// admission with the same identity refreshes bounds without replaying events.
		if (GetTerritoryByTag(Tag) != Territory || GetTerritoryByGUID(GUID) != Territory)
		{
			return ETerritoryRegistrationResult::InvalidTerritory;
		}
		SpatialIndex.Update(Territory);
		return ETerritoryRegistrationResult::Success;
	}

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebug = Settings && Settings->ShouldDebugRegistry();

	if (bDebug)
	{
		UE_LOG(LogTerritory, Log, TEXT("[Registry] Registering %s (tag=%s, GUID=%s)"),
			*Territory->GetName(), *Tag.ToString(), *GUID.ToString());
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
				return ETerritoryRegistrationResult::DuplicateTag;
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
				return ETerritoryRegistrationResult::DuplicateGUID;
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
	if (!RegisteredTerritories.Contains(Territory) || GetTerritoryByTag(Tag) != Territory
		|| GetTerritoryByGUID(GUID) != Territory) return ETerritoryRegistrationResult::InvalidTerritory;
	if (bDebug)
	{
		UE_LOG(LogTerritory, Log, TEXT("[Registry] Registered territory: %s (tag: %s, GUID: %s, cells: %d)"),
			*Territory->GetName(), *Tag.ToString(), *GUID.ToString(),
			SpatialIndex.GetCellCount());
	}

	return ETerritoryRegistrationResult::Success;
}

void UTerritoryRegistrySubsystem::UnregisterTerritory(ATerritoryVolume* Territory)
{
	if (!Territory) return;

	if (RegisteredTerritories.Remove(Territory) == 0) return;

	// Remove this actor's admitted keys even if a load/migration changed its fields.
	// Never let a rejected duplicate remove another actor's registered identity.
	for (auto It = TagToTerritoryMap.CreateIterator(); It; ++It)
	{
		if (It->Value.Get() == Territory) It.RemoveCurrent();
	}

	for (auto It = GUIDToTerritoryMap.CreateIterator(); It; ++It)
	{
		if (It->Value.Get() == Territory) It.RemoveCurrent();
	}

	// Remove from spatial index
	SpatialIndex.Remove(Territory);

	OnTerritoryUnregistered.Broadcast(Territory, true);
}

void UTerritoryRegistrySubsystem::UpdateTerritoryBounds(ATerritoryVolume* Territory)
{
	if (!IsValid(Territory) || !RegisteredTerritories.Contains(Territory)) return;
	SpatialIndex.Update(Territory);
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

	// Return the most specific territory.
	// Priority: class type (Property > District > City), then smallest bounds volume as tiebreaker.
	ATerritoryVolume* Best = nullptr;
	int32 BestPriority = -1;
	float BestVolume = TNumericLimits<float>::Max();

	for (ATerritoryVolume* Candidate : Candidates)
	{
		if (!Candidate) continue;

		// Class priority: Property=3, District=2, City=1, Volume=0
		int32 ClassPriority = 0;
		if (Candidate->IsA(ATerritoryProperty::StaticClass())) ClassPriority = 3;
		else if (Candidate->IsA(ATerritoryDistrict::StaticClass())) ClassPriority = 2;
		else if (Candidate->IsA(ATerritoryCity::StaticClass())) ClassPriority = 1;

		FBox Bounds = Candidate->GetTerritoryBounds();
		FVector Size = Bounds.GetSize();
		float Volume = Size.X * Size.Y * Size.Z;

		if (ClassPriority > BestPriority || (ClassPriority == BestPriority && Volume < BestVolume))
		{
			BestPriority = ClassPriority;
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
	for (const TWeakObjectPtr<ATerritoryVolume>& TerritoryPtr : RegisteredTerritories)
	{
		if (ATerritoryVolume* Territory = TerritoryPtr.Get())
		{
			if (Territory->IsOwnedByFaction(Faction))
			{
				Result.Add(Territory);
			}
		}
	}
	return Result;
}

TArray<ATerritoryVolume*> UTerritoryRegistrySubsystem::GetAllTerritories() const
{
	TArray<ATerritoryVolume*> Result;
	for (const TWeakObjectPtr<ATerritoryVolume>& TerritoryPtr : RegisteredTerritories)
	{
		if (ATerritoryVolume* Territory = TerritoryPtr.Get())
		{
			Result.Add(Territory);
		}
	}
	return Result;
}

int32 UTerritoryRegistrySubsystem::GetTerritoryCount() const
{
	// P2-05: Count only valid (non-stale) entries
	int32 Count = 0;
	for (const TWeakObjectPtr<ATerritoryVolume>& Ptr : RegisteredTerritories)
	{
		if (Ptr.IsValid()) ++Count;
	}
	return Count;
}

int32 UTerritoryRegistrySubsystem::GetTerritoryCountForFaction(const FGameplayTag& Faction) const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ATerritoryVolume>& TerritoryPtr : RegisteredTerritories)
	{
		if (ATerritoryVolume* Territory = TerritoryPtr.Get())
		{
			if (Territory->IsOwnedByFaction(Faction))
			{
				++Count;
			}
		}
	}
	return Count;
}

TArray<ATerritoryVolume*> UTerritoryRegistrySubsystem::GetChildTerritories(const FGameplayTag& ParentTag) const
{
	TArray<ATerritoryVolume*> Result;
	if (!ParentTag.IsValid()) return Result;

	for (const TWeakObjectPtr<ATerritoryVolume>& TerritoryPtr : RegisteredTerritories)
	{
		if (ATerritoryVolume* Territory = TerritoryPtr.Get())
		{
			FGameplayTag ParentRef = Territory->GetParentTerritoryTag();
			if (ParentRef == ParentTag)
			{
				Result.Add(Territory);
			}
		}
	}
	return Result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Floor regions
// ═══════════════════════════════════════════════════════════════════════════════

bool UTerritoryRegistrySubsystem::RegisterFloorVolume(ATerritoryFloorVolume* FloorVolume)
{
	if (!IsValid(FloorVolume) || FloorVolume->IsActorBeingDestroyed()
		|| FloorVolume->GetWorld() != GetWorld())
	{
		return false;
	}

	const FGameplayTag PlaceTag = FloorVolume->GetOwnerTerritoryTag();
	if (!PlaceTag.IsValid())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[Registry] Rejecting floor volume %s - it claims no Place tag, so no floor lookup could ever reach it"),
			*FloorVolume->GetName());
		return false;
	}

	// A negative index would be indistinguishable from "this location is on no authored floor", which
	// is the answer callers act on. Rejecting keeps that meaning unambiguous.
	const int32 FloorIndex = FloorVolume->FloorIndex;
	if (FloorIndex < 0)
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[Registry] Rejecting floor volume %s - floor index %d is negative, which would collide with the 'unresolved' answer"),
			*FloorVolume->GetName(), FloorIndex);
		return false;
	}

	const FGuid VolumeGUID = FloorVolume->FloorVolumeGUID;
	if (!VolumeGUID.IsValid())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[Registry] Rejecting floor volume %s - no editor-baked FloorVolumeGUID, so overlapping floor regions of %s could not be ordered deterministically"),
			*FloorVolume->GetName(), *PlaceTag.ToString());
		return false;
	}

	// A duplicated GUID makes the tie-break non-total. Refusing the second volume keeps the answer
	// deterministic and loud; UTerritoryDataValidator reports the same condition on the asset.
	for (const TPair<FGameplayTag, TArray<TWeakObjectPtr<ATerritoryFloorVolume>>>& Pair : FloorVolumesByPlaceTag)
	{
		for (const TWeakObjectPtr<ATerritoryFloorVolume>& ExistingPtr : Pair.Value)
		{
			const ATerritoryFloorVolume* Existing = ExistingPtr.Get();
			if (Existing && Existing != FloorVolume && Existing->FloorVolumeGUID == VolumeGUID)
			{
				UE_LOG(LogTerritory, Error,
					TEXT("[Registry] Rejecting floor volume %s - FloorVolumeGUID %s is already claimed by %s"),
					*FloorVolume->GetName(), *VolumeGUID.ToString(), *Existing->GetName());
				return false;
			}
		}
	}

	FloorVolumesByPlaceTag.FindOrAdd(PlaceTag).AddUnique(FloorVolume);
	return true;
}

void UTerritoryRegistrySubsystem::UnregisterFloorVolume(ATerritoryFloorVolume* FloorVolume)
{
	if (!FloorVolume) return;

	for (auto It = FloorVolumesByPlaceTag.CreateIterator(); It; ++It)
	{
		TArray<TWeakObjectPtr<ATerritoryFloorVolume>>& Volumes = It.Value();
		Volumes.RemoveAll([FloorVolume](const TWeakObjectPtr<ATerritoryFloorVolume>& Ptr)
		{
			const ATerritoryFloorVolume* Candidate = Ptr.Get();
			return !Candidate || Candidate == FloorVolume;
		});
		if (Volumes.IsEmpty())
		{
			It.RemoveCurrent();
		}
	}
}

int32 UTerritoryRegistrySubsystem::GetFloorAtLocation(
	const ATerritoryVolume* Place, const FVector& WorldLocation) const
{
	if (!IsValid(Place)) return INDEX_NONE;

	const FGameplayTag PlaceTag = Place->GetTerritoryTag();
	if (!PlaceTag.IsValid()) return INDEX_NONE;

	const TArray<TWeakObjectPtr<ATerritoryFloorVolume>>* Volumes = FloorVolumesByPlaceTag.Find(PlaceTag);
	if (!Volumes) return INDEX_NONE;

	const ATerritoryFloorVolume* Best = nullptr;
	double BestVolume = TNumericLimits<double>::Max();
	FString BestGUID;

	for (const TWeakObjectPtr<ATerritoryFloorVolume>& VolumePtr : *Volumes)
	{
		const ATerritoryFloorVolume* Candidate = VolumePtr.Get();
		if (!Candidate || !Candidate->ContainsPoint(WorldLocation)) continue;

		// Most specific region wins, with a GUID tie-break so the order is total. This mirrors
		// ATerritoryGuardSpawnPoint::ChooseMostSpecificTerritory's smallest-bounds-then-name rule,
		// and the total order is what keeps the answer independent of World Partition iteration.
		const double CandidateVolume = Candidate->GetFloorBoundsVolume();
		const FString CandidateGUID = Candidate->FloorVolumeGUID.ToString();
		if (!Best
			|| CandidateVolume < BestVolume
			|| (FMath::IsNearlyEqual(CandidateVolume, BestVolume) && CandidateGUID < BestGUID))
		{
			Best = Candidate;
			BestVolume = CandidateVolume;
			BestGUID = CandidateGUID;
		}
	}

	return Best ? Best->FloorIndex : INDEX_NONE;
}

TArray<ATerritoryFloorVolume*> UTerritoryRegistrySubsystem::GetFloorVolumesForPlace(
	const ATerritoryVolume* Place) const
{
	TArray<ATerritoryFloorVolume*> Result;
	if (!IsValid(Place)) return Result;

	const FGameplayTag PlaceTag = Place->GetTerritoryTag();
	if (!PlaceTag.IsValid()) return Result;

	if (const TArray<TWeakObjectPtr<ATerritoryFloorVolume>>* Volumes = FloorVolumesByPlaceTag.Find(PlaceTag))
	{
		for (const TWeakObjectPtr<ATerritoryFloorVolume>& VolumePtr : *Volumes)
		{
			if (ATerritoryFloorVolume* Volume = VolumePtr.Get())
			{
				Result.Add(Volume);
			}
		}
	}
	return Result;
}

bool UTerritoryRegistrySubsystem::HasAuthoredFloorVolumes(const ATerritoryVolume* Place) const
{
	return !GetFloorVolumesForPlace(Place).IsEmpty();
}

TArray<ATerritoryFloorVolume*> UTerritoryRegistrySubsystem::GetAllFloorVolumes() const
{
	TArray<ATerritoryFloorVolume*> Result;
	for (const TPair<FGameplayTag, TArray<TWeakObjectPtr<ATerritoryFloorVolume>>>& Pair : FloorVolumesByPlaceTag)
	{
		for (const TWeakObjectPtr<ATerritoryFloorVolume>& VolumePtr : Pair.Value)
		{
			if (ATerritoryFloorVolume* Volume = VolumePtr.Get())
			{
				Result.Add(Volume);
			}
		}
	}
	return Result;
}
