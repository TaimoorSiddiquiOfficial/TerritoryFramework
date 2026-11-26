#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"

#include "TerritoryHidingSpotComponent.generated.h"

class ATerritoryVolume;

/**
 * A hiding spot a designer places on any prop (cart, haystack, wardrobe, bush).
 *
 * While a player pawn overlaps the sphere the component registers it as hidden in the Place's
 * stealth authority. The stealth observer then reports zero effective sight for that pawn, so guards
 * keep patrolling instead of seeing through the cover; evidence from noise, damage or a broken
 * disguise still works normally, and point-blank combat permission is unchanged.
 *
 * Server-only. Registers with the Territory that owns the component's world location, so it needs no
 * manual territory reference and works with streamed World Partition Places.
 */
UCLASS(ClassGroup=(Territory), meta=(BlueprintSpawnableComponent, DisplayName="Territory Hiding Spot"))
class TERRITORYFRAMEWORK_API UTerritoryHidingSpotComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	UTerritoryHidingSpotComponent();

	/** Overlap radius in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hiding",
		meta=(ClampMin="10.0", Units="cm"))
	float EnterRadius = 120.f;

	/** Only a crouched pawn is hidden; disable for a wardrobe or a crawlspace. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hiding")
	bool bRequiresCrouch = true;

	/** Ignore the spot in Places whose stealth profile does not allow infiltration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hiding")
	bool bRequiresStealthPlace = true;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RefreshHiddenState();

	/** Pawn -> Territory it was registered in, so teardown clears the right record. */
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<ATerritoryVolume>> HiddenActors;
	FTimerHandle RefreshTimer;
};
