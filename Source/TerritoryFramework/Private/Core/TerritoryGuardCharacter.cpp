#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "AI/Activities/NPCActivityConfiguration.h"
#include "Tales/TriggerSet.h"

ATerritoryGuardCharacter::ATerritoryGuardCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool ATerritoryGuardCharacter::ShouldRespawn_Implementation() const
{
	return false;
}

void ATerritoryGuardCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ATerritoryGuardCharacter::ConfigureTerritorySpawn(
	UNPCDefinition* Definition,
	const FGameplayTag& ExactFaction,
	const FGuid& TerritoryGuid,
	const FGuid& SaveGuid,
	const FTransform& InSpawnTransform,
	FName SpawnPointName,
	UNPCActivityConfiguration* OptionalActivityOverride,
	const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides)
{
	// Fill ALL SpawnInfo fields that Narrative activities need.
	// BPA_ReturnToSpawn reads SpawnTransform — without it, SetupBlackboard fails.

	SpawnInfo.SpawnAssignedSaveGUID = SaveGuid;
	SpawnInfo.SpawnTransform = InSpawnTransform;
	SpawnInfo.SpawnName = SpawnPointName;

	// TerritoryVolume is not a Narrative NPCSpawnComponent.
	// Mark OwningSpawnerGUID as the territory's GUID for identification,
	// but OwningSpawn is left null since there's no UNPCSpawnComponent.
	SpawnInfo.OwningSpawnerGUID = TerritoryGuid;

	// Store territory context for BP-accessible ReturnToTerritory activity
	TerritoryHomeTransform = InSpawnTransform;

	// Override factions to exactly the territory owner
	if (ExactFaction.IsValid())
	{
		SpawnInfo.SpawnParams.bOverride_DefaultFactions = true;
		SpawnInfo.SpawnParams.DefaultFactions.Reset();
		SpawnInfo.SpawnParams.DefaultFactions.AddTag(ExactFaction);
	}

	// Optional activity configuration override
	if (OptionalActivityOverride)
	{
		SpawnInfo.SpawnParams.bOverride_ActivityConfiguration = true;
		SpawnInfo.SpawnParams.ActivityConfiguration = FSoftObjectPath(OptionalActivityOverride);
	}

	// Optional trigger set overrides
	if (!OptionalTriggerOverrides.IsEmpty())
	{
		SpawnInfo.SpawnParams.bOverride_TriggerSets = true;
		SpawnInfo.SpawnParams.TriggerSets = OptionalTriggerOverrides;
	}

	// Apply the definition — Narrative reads SpawnParams during this call
	if (Definition)
	{
		SetNPCDefinition(Definition);
	}
}

FGuid ATerritoryGuardCharacter::GetActorGUID_Implementation() const
{
	if (SpawnInfo.SpawnAssignedSaveGUID.IsValid())
	{
		return SpawnInfo.SpawnAssignedSaveGUID;
	}

	if (!CachedFallbackGUID.IsValid())
	{
		const_cast<ATerritoryGuardCharacter*>(this)->CachedFallbackGUID = FGuid::NewGuid();
	}
	return CachedFallbackGUID;
}

void ATerritoryGuardCharacter::SetActorGUID_Implementation(const FGuid& NewGUID)
{
	SpawnInfo.SpawnAssignedSaveGUID = NewGUID;
}
