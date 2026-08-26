#pragma once

#include "CoreMinimal.h"
#include "Navigation/MapMarker.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryMapMarker.generated.h"

class ATerritoryVolume;
class UNarrativeNavigationComponent;

UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryMapMarker : public UMapMarker
{
	GENERATED_BODY()

public:
	UTerritoryMapMarker(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Territory Marker")
	void SetTerritoryVolume(ATerritoryVolume* InTerritory);

	UFUNCTION(BlueprintCallable, Category = "Territory Marker")
	void ClearTerritoryBinding();

	UFUNCTION(BlueprintCallable, Category = "Territory Marker")
	ATerritoryVolume* GetTerritoryVolume() const;

	UFUNCTION(BlueprintCallable, Category = "Territory Marker")
	void SetFactionColor(FGameplayTag Faction, FLinearColor Color);

	UFUNCTION(BlueprintCallable, Category = "Territory Marker")
	void ClearFactionColors();

	/** Promote this one Territory to Narrative compass/screen-space guidance. */
	UFUNCTION(BlueprintCallable, Category = "Territory Marker|Waypoint")
	void SetTracked(bool bInTracked);

	UFUNCTION(BlueprintPure, Category = "Territory Marker|Waypoint")
	bool IsTracked() const { return bTracked; }

	/** Reconcile lock/hierarchy visibility with Narrative marker registration and domains. */
	void RefreshTerritoryPresentation();

protected:
	virtual void OnMarkerAdded_Implementation(
		UNarrativeNavigationComponent* OwnerNavComp) override;
	virtual void OnMarkerRemoved_Implementation(
		UNarrativeNavigationComponent* OwnerNavComp) override;
	virtual FLinearColor GetMarkerColor_Implementation(UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType) const override;
	virtual FText GetMarkerDisplayText_Implementation(UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType, FText& OutSubtitleText) const override;
	virtual FText GetMarkerActionText_Implementation(UNarrativeNavigationComponent* Selector) const override;
	virtual bool CanInteract_Implementation(UNarrativeNavigationComponent* Selector) const override;
	virtual void OnSelect_Implementation(UNarrativeNavigationComponent* Selector) override;
	virtual void MarkerOnPaint_Implementation(FPaintContext& Context, FMarkerOnPaintData& OnPaintData) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Territory Marker")
	TMap<FGameplayTag, FLinearColor> FactionColorMap;

	UPROPERTY(EditDefaultsOnly, Category = "Territory Marker")
	FLinearColor DefaultColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.f);

	/** Color when territory is unclaimed (no owner). Default: Red. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory Marker|Colors")
	FLinearColor UnclaimedColor = FLinearColor(1.f, 0.f, 0.f, 1.f);

	/** Color when territory is being contested. Default: Yellow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory Marker|Colors")
	FLinearColor ContestedColor = FLinearColor(1.f, 1.f, 0.f, 1.f);

	/** Color when territory is owned by an enemy faction (no FactionColorMap entry).
	 *  Default: Red. Set player faction to green via FactionColorMap or SetFactionColor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory Marker|Colors")
	FLinearColor EnemyOwnedColor = FLinearColor(1.f, 0.f, 0.f, 1.f);

	/** Color when territory is locked. Default: Purple. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory Marker|Colors")
	FLinearColor LockedColor = FLinearColor(0.5f, 0.f, 0.5f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Territory Marker")
	bool bDrawTerritoryOutline = true;

	UPROPERTY(EditDefaultsOnly, Category = "Territory Marker")
	float OutlineThickness = 2.f;

	/** Selected captured Territory accent. Unselected Territories never enter the compass domain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory Marker|Waypoint")
	FLinearColor TrackedCapturedColor = FLinearColor(0.08f, 0.95f, 0.46f, 1.f);

private:
	UPROPERTY()
	TWeakObjectPtr<ATerritoryVolume> TerritoryVolume;

	bool bTracked = false;
	bool bRegisteredWithNavigation = false;
	TSet<TWeakObjectPtr<UNarrativeNavigationComponent>> BoundPOINavigationComponents;
	TSet<TWeakObjectPtr<UNarrativeNavigationComponent>> OwnedDynamicPOIData;

	void RegisterPlacePOIData(UNarrativeNavigationComponent* NavigationComponent);
	void UnregisterPlacePOIData(UNarrativeNavigationComponent* NavigationComponent);

	UFUNCTION()
	void OnNarrativePOIDiscovered(const FGameplayTag& POITag);

	UFUNCTION()
	void OnTerritoryChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void OnTerritoryStateChanged(ATerritoryVolume* Territory, ETerritoryState NewState);
};
