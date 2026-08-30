#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritorySpatialIndex.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UTerritoryRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	float CellSize = Settings ? Settings->SpatialCellSize : 2000.f;
	SpatialIndex.Initialize(CellSize);

	// Periodically check if any territory has moved/resized (every 2s — cheap bounds compare).
	// Server-only — spatial index mutations on clients would conflict with replication.
	if (UWorld* World = GetWorld(); World && World->GetNetMode() != NM_Client)
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
	Super::Deinitialize();
}

void UTerritoryRegistrySubsystem::PollBoundsChanges()
{
	// P2-08: Spatial index is server-authoritative. Clients use their own local actor bounds
	// for GetTerritoryAtLocation queries, which is correct for static level-placed territories.
	// For runtime-moved/resized territories, call UpdateTerritoryBounds explicitly on the server
	// and replicate the new transform via the volume's ReplicatedMovement or a custom RepNotify.
	// P2-N06: RegisteredTerritories uses weak refs — null check handles stale entries
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
	if (!Territory) return ETerritoryRegistrationResult::InvalidTerritory;

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

void UTerritoryRegistrySubsystem::UpdateTerritoryBounds(ATerritoryVolume* Territory)
{
	if (!Territory) return;
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
