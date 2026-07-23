#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"
#include "AI/Activities/NPCActivityConfiguration.h"
#include "Tales/TriggerSet.h"

ATerritoryGuardCharacter::ATerritoryGuardCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
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
	UNPCActivityConfiguration* OptionalActivityOverride,
	const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides)
{
	// Set spawn info GUIDs before SetNPCDefinition
	SpawnInfo.OwningSpawnerGUID = TerritoryGuid;
	SpawnInfo.SpawnAssignedSaveGUID = SaveGuid;

	// Override factions to exactly the territory owner — prevents
	// the NPC definition's default factions from leaking through.
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
