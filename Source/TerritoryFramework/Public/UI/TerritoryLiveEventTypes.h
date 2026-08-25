#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TerritoryLiveEventTypes.generated.h"

/** Player-facing, session-local summary of an authoritative Territory transition. */
UENUM(BlueprintType)
enum class ETerritoryLiveEventType : uint8
{
	Unlocked UMETA(DisplayName="Territory Unlocked"),
	Captured UMETA(DisplayName="Territory Captured"),
	Lost UMETA(DisplayName="Territory Lost"),
	Contested UMETA(DisplayName="Territory Contested"),
	Secured UMETA(DisplayName="Territory Secured"),
	CounterAttackWarning UMETA(DisplayName="Counterattack Warning"),
	CounterAttackActive UMETA(DisplayName="Counterattack Active"),
	CounterAttackDefeated UMETA(DisplayName="Counterattack Defeated"),
	CounterAttackSucceeded UMETA(DisplayName="Counterattack Succeeded"),
	CounterAttackCancelled UMETA(DisplayName="Counterattack Cancelled")
};

/**
 * Read-only UI history entry. Gameplay state remains owned by ATerritoryVolume and
 * the counterattack subsystem; this is a local presentation snapshot only.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryLiveEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|Live Events")
	FGuid EventID;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Live Events")
	ETerritoryLiveEventType Type = ETerritoryLiveEventType::Contested;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Live Events", meta=(Categories="Territory"))
	FGameplayTag TerritoryTag;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Live Events")
	FText TerritoryName;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Live Events")
	FText Headline;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Live Events")
	FText Detail;

	/** Local real-time seconds, deliberately not campaign/save authority. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Live Events")
	double CreatedRealTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Live Events")
	float ActiveDuration = 30.f;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Live Events")
	bool bCanSetWaypoint = false;

	/** Calculated when the feed is queried; expired rows remain readable but muted. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Live Events")
	bool bExpired = false;

	bool IsExpiredAt(double NowRealTime) const
	{
		return ActiveDuration >= 0.f && NowRealTime >= CreatedRealTime + ActiveDuration;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnTerritoryLiveEventAdded, const FTerritoryLiveEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTerritoryLiveEventsChanged);
