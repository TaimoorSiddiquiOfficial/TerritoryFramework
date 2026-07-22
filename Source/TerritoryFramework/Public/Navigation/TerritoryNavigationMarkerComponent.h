#pragma once

#include "CoreMinimal.h"
#include "Navigation/NavigationMarkerComponent.h"
#include "TerritoryNavigationMarkerComponent.generated.h"

class ATerritoryVolume;
class UTerritoryMapMarker;

/**
 * Navigation marker component for territory volumes.
 * Creates and manages a UTerritoryMapMarker instance, subscribes to
 * territory ownership/state changes, and auto-refreshes the marker.
 */
UCLASS(BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class TERRITORYFRAMEWORK_API UTerritoryNavigationMarkerComponent : public UNavigationMarkerComponent
{
	GENERATED_BODY()

public:
	UTerritoryNavigationMarkerComponent(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Get the territory map marker instance */
	UFUNCTION(BlueprintPure, Category = "Territory|Marker")
	UTerritoryMapMarker* GetTerritoryMapMarker() const;

	/** Get the owning territory volume */
	UFUNCTION(BlueprintPure, Category = "Territory|Marker")
	ATerritoryVolume* GetOwningTerritory() const;

	/** Force refresh the marker display */
	UFUNCTION(BlueprintCallable, Category = "Territory|Marker")
	void RefreshTerritoryMarker();

protected:
	UPROPERTY()
	TObjectPtr<UTerritoryMapMarker> TerritoryMapMarker;

	UPROPERTY()
	TWeakObjectPtr<ATerritoryVolume> CachedTerritory;

private:
	UFUNCTION()
	void OnTerritoryControlChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void OnTerritoryStateChanged(ATerritoryVolume* Territory, ETerritoryState NewState);
};
