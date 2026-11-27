#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeEvent.h"

#include "Tales/TerritoryNarrativeContext.h"
#include "TerritoryUpgradeEvents.generated.h"

class ATerritoryVolume;

/**
 * Fired after a verified ownership change, carrying the captured Place, previous owner, new owner
 * and the old upgrade level. Author it on a UTerritoryPlaceDefinition's Capture Events.
 *
 * The C++ base is a notification; a Blueprint child can read the payload and start/advance a quest.
 */
UCLASS(BlueprintType, EditInlineNew, meta = (DisplayName = "Territory Place Captured"))
class TERRITORYFRAMEWORK_API UTerritoryPlaceCapturedEvent : public UTerritoryNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryPlaceCapturedEvent(const FObjectInitializer& ObjectInitializer);

	/** Optional filter: only fire for this Place tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capture")
	FGameplayTag PlaceTag;

	/** Optional filter: only fire when the new owner is this faction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capture",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag NewOwnerFilter;

	/** Optional filter: only fire when the Place had at least this upgrade level before capture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "0"))
	int32 MinOldLevel = 0;

	/** Transient payload, set immediately before this event executes on the server. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Capture")
	TObjectPtr<ATerritoryVolume> CapturedPlace;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Capture")
	FGameplayTag PreviousOwner;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Capture")
	FGameplayTag NewOwner;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Capture")
	int32 OldUpgradeLevel = 0;

	void SetPayload(ATerritoryVolume* Place, FGameplayTag InPreviousOwner, FGameplayTag InNewOwner,
		int32 InOldUpgradeLevel);
	void ClearPayload();
	bool Matches(const ATerritoryVolume* Place) const;

  virtual void ExecuteEventInContext(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent) override;
};

/**
 * Quest/dialogue event that upgrades a Place through the same server-authoritative mutation.
 * When bAllowForeignOwnership is true a quest may upgrade a Place its own faction does not own.
 */
UCLASS(BlueprintType, EditInlineNew, meta = (DisplayName = "Territory Upgrade Place"))
class TERRITORYFRAMEWORK_API UTerritoryUpgradePlaceEvent : public UTerritoryNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryUpgradePlaceEvent(const FObjectInitializer& ObjectInitializer);

	/** Optional filter: only upgrade this Place. Leave empty to upgrade the one the quest names. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade",
		meta = (Categories = "Territory"))
	FGameplayTag PlaceTag;

	/** Target Place level (one step above the current level). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "1"))
	int32 TargetLevel = 1;

	/**
	 * Allow the upgrade to run on a Place the acting faction does not own. Off by default so a
	 * mis-authored quest cannot spend another faction's upgrade budget.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	bool bAllowForeignOwnership = false;

	/** Skip the authored cost; used by tutorial/story grants and AI catch-up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	bool bFreeUpgrade = false;

  virtual void ExecuteEventInContext(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent) override;
};
