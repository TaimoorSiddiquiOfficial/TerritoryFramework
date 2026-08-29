#pragma once

#include "CoreMinimal.h"
#include "Weapons/NarrativeProjectile.h"
#include "TerritoryDistractionProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTerritoryDistractionComponent;

/**
 * Ready-to-duplicate Narrative projectile for stones, bottles, and other non-damaging distractions.
 * The native collision root is important: Projectile Movement sweeps it and the first blocking hit
 * becomes one Territory-tagged Narrative hearing stimulus.
 */
UCLASS(BlueprintType, Blueprintable,
	meta=(DisplayName="Territory Distraction Projectile"))
class TERRITORYFRAMEWORK_API ATerritoryDistractionProjectile
	: public ANarrativeProjectile
{
	GENERATED_BODY()

public:
	ATerritoryDistractionProjectile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Distraction")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Distraction")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Distraction")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Distraction")
	TObjectPtr<UTerritoryDistractionComponent> Distraction;
};

