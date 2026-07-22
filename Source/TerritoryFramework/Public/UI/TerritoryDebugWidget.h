#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "TerritoryDebugWidget.generated.h"

class ATerritoryVolume;
class UTerritoryRegistrySubsystem;
class UTerritoryControlSubsystem;
class UTerritoryEconomySubsystem;
class UTerritoryDiplomacySubsystem;

/**
 * Debug overlay widget that displays live territory state when added to the viewport.
 * Reads from DeveloperSettings to show/hide sections.
 * Place in any UMG layout — it polls once per tick (lightweight).
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryDebugWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Enable/disable the entire debug overlay */
	UFUNCTION(BlueprintCallable, Category = "Territory|Debug")
	void SetDebugEnabled(bool bEnabled);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Called when territory summary text updates — override in BP for custom layout */
	UFUNCTION(BlueprintImplementableEvent, Category = "Territory|Debug")
	void OnUpdateDebugText(const FText& DebugText);

private:
	bool bDebugEnabled = false;

	FText BuildDebugString() const;
	FText BuildTerritorySummary() const;
	FText BuildEconomySummary() const;
	FText BuildDiplomacySummary() const;
	FText BuildCaptureSummary() const;
};
