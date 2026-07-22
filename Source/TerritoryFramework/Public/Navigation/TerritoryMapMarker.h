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

protected:
	virtual FLinearColor GetMarkerColor_Implementation(UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType) const override;
	virtual FText GetMarkerDisplayText_Implementation(UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType, FText& OutSubtitleText) const override;
	virtual void MarkerOnPaint_Implementation(FPaintContext& Context, FMarkerOnPaintData& OnPaintData) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Territory Marker")
	TMap<FGameplayTag, FLinearColor> FactionColorMap;

	UPROPERTY(EditDefaultsOnly, Category = "Territory Marker")
	FLinearColor DefaultColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Territory Marker")
	FLinearColor ContestedColor = FLinearColor(1.f, 0.5f, 0.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Territory Marker")
	FLinearColor LockedColor = FLinearColor(0.3f, 0.3f, 0.3f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Territory Marker")
	bool bDrawTerritoryOutline = true;

	UPROPERTY(EditDefaultsOnly, Category = "Territory Marker")
	float OutlineThickness = 2.f;

private:
	UPROPERTY()
	TWeakObjectPtr<ATerritoryVolume> TerritoryVolume;

	UFUNCTION()
	void OnTerritoryChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void OnTerritoryStateChanged(ATerritoryVolume* Territory, ETerritoryState NewState);
};
