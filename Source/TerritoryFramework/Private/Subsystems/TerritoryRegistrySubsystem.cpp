#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Engine/World.h"

void UTerritoryRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTerritory, Log, TEXT("TerritoryRegistrySubsystem initialized"));
}

void UTerritoryRegistrySubsystem::Deinitialize()
{
	RegisteredTerritories.Empty();
	TagToTerritoryMap.Empty();
	GUIDToTerritoryMap.Empty();
	Super::Deinitialize();
}

void UTerritoryRegistrySubsystem::RegisterTerritory(ATerritoryVolume* Territory)
{
	if (!Territory) return;

	FGameplayTag Tag = Territory->GetTerritoryTag();
	FGuid GUID = Territory->GetActorGUID_Implementation();

	// ─── Duplicate Tag Validation ───
	if (Tag.IsValid())
	{
		if (TWeakObjectPtr<ATerritoryVolume>* Existing = TagToTerritoryMap.Find(Tag))
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
		if (TWeakObjectPtr<ATerritoryVolume>* Existing = GUIDToTerritoryMap.Find(GUID))
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

	OnTerritoryRegistered.Broadcast(Territory, false);
	UE_LOG(LogTerritory, Log, TEXT("Registered territory: %s (tag: %s, GUID: %s)"),
		*Territory->GetName(), *Tag.ToString(), *GUID.ToString());
}

void UTerritoryRegistrySubsystem::UnregisterTerritory(ATerritoryVolume* Territory)
{
	if (!Territory) return;

	RegisteredTerritories.Remove(Territory);

	FGameplayTag Tag = Territory->GetTerritoryTag();
	if (Tag.IsValid())
	{
		TagToTerritoryMap.Remove(Tag);
	}

	FGuid GUID = Territory->GetActorGUID_Implementation();
	if (GUID.IsValid())
	{
		GUIDToTerritoryMap.Remove(GUID);
	}

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
	for (const TObjectPtr<ATerritoryVolume>& Territory : RegisteredTerritories)
	{
		if (Territory && Territory->ContainsPoint(WorldLocation))
		{
			return Territory;
		}
	}
	return nullptr;
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

		// A child territory has ParentTerritoryTag matching the query tag
		FGameplayTag ParentRef = Territory->GetParentTerritoryTag();
		if (ParentRef == ParentTag)
		{
			Result.Add(Territory);
		}
	}
	return Result;
}
