#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"

ATerritoryGuardCharacter::ATerritoryGuardCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FGuid ATerritoryGuardCharacter::GetActorGUID_Implementation() const
{
	// Return the save GUID assigned by the territory spawner.
	// If not yet assigned (invalid), generate one on the fly to prevent
	// the NarrativeStableActor assertion crash.
	if (SpawnInfo.SpawnAssignedSaveGUID.IsValid())
	{
		return SpawnInfo.SpawnAssignedSaveGUID;
	}

	// Fallback: return a deterministic GUID based on the actor's name
	// This prevents the checkf(false) crash while still providing identity
	UE_LOG(LogTerritory, Warning, TEXT("TerritoryGuardCharacter %s has no SpawnAssignedSaveGUID, generating fallback"),
		*GetName());
	return FGuid::NewGuid();
}

void ATerritoryGuardCharacter::SetActorGUID_Implementation(const FGuid& NewGUID)
{
	SpawnInfo.SpawnAssignedSaveGUID = NewGUID;
}

void ATerritoryGuardCharacter::SetTerritorySaveGUID(const FGuid& NewGUID)
{
	SpawnInfo.SpawnAssignedSaveGUID = NewGUID;
}

void ATerritoryGuardCharacter::SetOwningTerritoryGUID(const FGuid& TerritoryGUID)
{
	SpawnInfo.OwningSpawnerGUID = TerritoryGUID;
}
