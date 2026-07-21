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
	Super::Deinitialize();
}

void UTerritoryRegistrySubsystem::RegisterTerritory(ATerritoryVolume* Territory)
{
	if (!Territory) return;

	RegisteredTerritories.AddUnique(Territory);

	FGameplayTag Tag = Territory->GetTerritoryTag();
	if (Tag.IsValid())
	{
		TagToTerritoryMap.Add(Tag, Territory);
	}

	OnTerritoryRegistered.Broadcast(Territory, false);
	UE_LOG(LogTerritory, Log, TEXT("Registered territory: %s (%s)"),
		*Territory->GetName(), *Tag.ToString());
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

	OnTerritoryUnregistered.Broadcast(Territory, true);
}

ATerritoryVolume* UTerritoryRegistrySubsystem::GetTerritoryByTag(const FGameplayTag& TerritoryTag) const
{
	const TWeakObjectPtr<ATerritoryVolume>* Found = TagToTerritoryMap.Find(TerritoryTag);
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
	// TODO: Implement parent-child hierarchy lookup when ParentTerritoryTag is used on volumes
	return Result;
}
