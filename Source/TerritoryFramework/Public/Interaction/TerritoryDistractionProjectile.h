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
	virtual void BeginPlay() override;

	/** Projectile collision component used to detect impact. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Distraction")
	TObjectPtr<USphereComponent> Collision;

	/** Optional visible mesh for this distraction projectile. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Distraction")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Movement component controlling the distraction projectile's flight. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Distraction")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Component that reports anonymous hearing evidence when the projectile triggers its distraction. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Distraction")
	TObjectPtr<UTerritoryDistractionComponent> Distraction;
};

