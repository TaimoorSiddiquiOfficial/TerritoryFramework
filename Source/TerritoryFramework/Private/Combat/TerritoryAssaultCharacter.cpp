#include "Combat/TerritoryAssaultCharacter.h"

#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "AI/NPCDefinition.h"
#include "AI/Activities/NPCActivityConfiguration.h"
#include "Tales/TriggerSet.h"

ATerritoryAssaultCharacter::ATerritoryAssaultCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AssaultParticipant = CreateDefaultSubobject<UTerritoryAssaultParticipantComponent>(TEXT("TerritoryAssaultParticipant"));
}

void ATerritoryAssaultCharacter::ConfigureAssaultSpawn(
	UNPCDefinition* Definition, const FGameplayTag& ExactFaction,
	const FGuid& TerritoryGuid, const FGuid& SpawnGuid, const FTransform& InSpawnTransform,
	FName InSpawnName, UNPCActivityConfiguration* OptionalActivityOverride,
	const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides,
	const FGuid& AssaultID, const FGameplayTag& TargetTerritory)
{
	SpawnInfo.OwningSpawnerGUID = TerritoryGuid;
	SpawnInfo.SpawnAssignedSaveGUID = SpawnGuid;
	SpawnInfo.SpawnTransform = InSpawnTransform;
	SpawnInfo.SpawnName = InSpawnName;
	SpawnInfo.SpawnParams.bOverride_DefaultFactions = true;
	SpawnInfo.SpawnParams.DefaultFactions.Reset();
	SpawnInfo.SpawnParams.DefaultFactions.AddTag(ExactFaction);

	if (OptionalActivityOverride)
	{
		SpawnInfo.SpawnParams.bOverride_ActivityConfiguration = true;
		SpawnInfo.SpawnParams.ActivityConfiguration = FSoftObjectPath(OptionalActivityOverride);
	}
	if (!OptionalTriggerOverrides.IsEmpty())
	{
		SpawnInfo.SpawnParams.bOverride_TriggerSets = true;
		SpawnInfo.SpawnParams.TriggerSets = OptionalTriggerOverrides;
	}

	if (AssaultParticipant)
	{
		AssaultParticipant->Configure(AssaultID, TargetTerritory, ExactFaction);
	}
	if (Definition)
	{
		SetNPCDefinition(Definition);
	}
}

FGuid ATerritoryAssaultCharacter::GetActorGUID_Implementation() const
{
	return SpawnInfo.SpawnAssignedSaveGUID;
}

void ATerritoryAssaultCharacter::SetActorGUID_Implementation(const FGuid& NewGUID)
{
	SpawnInfo.SpawnAssignedSaveGUID = NewGUID;
}

bool ATerritoryAssaultCharacter::ShouldRespawn_Implementation() const
{
	// The durable assault record reconstructs surviving finite force after load.
	// Saving individual live pawn pointers/records would create a second authority.
	return false;
}
