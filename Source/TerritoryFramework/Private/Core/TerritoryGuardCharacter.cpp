#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"

ATerritoryGuardCharacter::ATerritoryGuardCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ATerritoryGuardCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Bind to our own damage event to prevent floating on hit
	OnTakeAnyDamage.AddDynamic(this, &ATerritoryGuardCharacter::OnGuardTakeAnyDamage);
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

void ATerritoryGuardCharacter::OnGuardTakeAnyDamage(AActor* DamagedActor, float Damage,
	const UDamageType* DamageType, AController* EventInstigator, AActor* DamageCauser)
{
	// Prevent floating on hit impact — aggressively force character to ground
	if (Damage > 0.f && !bIsRagdoll)
	{
		UCharacterMovementComponent* CMC = GetCharacterMovement();
		if (!CMC) return;

		// Immediately force walking mode — suppress any physics/falling mode from hit reactions
		CMC->SetMovementMode(MOVE_Walking);

		// Force the character to stay on the ground — snap to current ground position
		if (!CMC->IsMovingOnGround())
		{
			CMC->SetMovementMode(MOVE_Walking);
		}

		// Disable any physics simulation that might have been triggered by the hit
		GetMesh()->SetSimulatePhysics(false);
	}
}
