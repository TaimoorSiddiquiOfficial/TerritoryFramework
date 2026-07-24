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
	virtual void NativeDestruct() override;

	/** Called when territory summary text updates — override in BP for custom layout */
	UFUNCTION(BlueprintImplementableEvent, Category = "Territory|Debug")
	void OnUpdateDebugText(const FText& DebugText);

private:
	bool bDebugEnabled = false;

	/** Seconds between debug text rebuilds — avoids per-frame string allocation. */
	float UpdateInterval = 0.5f;
	float TimeSinceLastUpdate = 0.f;

	// Cached subsystem pointers — resolved once, avoids 4× GetSubsystem per rebuild.
	mutable UTerritoryRegistrySubsystem* CachedRegistry = nullptr;
	mutable UTerritoryControlSubsystem* CachedControl = nullptr;
	mutable UTerritoryEconomySubsystem* CachedEconomy = nullptr;
	mutable UTerritoryDiplomacySubsystem* CachedDiplomacy = nullptr;
	mutable bool bSubsystemsCached = false;

	void CacheSubsystems() const;

	FText BuildDebugString() const;
	FText BuildTerritorySummary() const;
	FText BuildEconomySummary() const;
	FText BuildDiplomacySummary() const;
	FText BuildCaptureSummary() const;
};
