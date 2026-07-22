#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"

ATerritoryGuardCharacter::ATerritoryGuardCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ATerritoryGuardCharacter::BeginPlay()
{
	Super::BeginPlay();
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

void ATerritoryGuardCharacter::SetTerritorySaveGUID(const FGuid& NewGUID)
{
	SpawnInfo.SpawnAssignedSaveGUID = NewGUID;
}

void ATerritoryGuardCharacter::SetOwningTerritoryGUID(const FGuid& TerritoryGUID)
{
	SpawnInfo.OwningSpawnerGUID = TerritoryGUID;
}
