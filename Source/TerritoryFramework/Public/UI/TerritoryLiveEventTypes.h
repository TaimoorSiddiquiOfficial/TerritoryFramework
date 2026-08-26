#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TerritoryLiveEventTypes.generated.h"

/** Player-facing summary of an authoritative Territory transition. */
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
	CounterAttackCancelled UMETA(DisplayName="Counterattack Cancelled"),
	CommandCapabilityGained UMETA(DisplayName="Territory Perk Gained"),
	CommandCapabilityLost UMETA(DisplayName="Territory Perk Lost"),
	IncomeRecorded UMETA(DisplayName="Territory Income Recorded"),
	ExpenseRecorded UMETA(DisplayName="Territory Expense Recorded"),
	UpkeepDeficit UMETA(DisplayName="Guard Upkeep Deficit"),
	ProductionCompleted UMETA(DisplayName="Production Completed"),
	ProductionBlocked UMETA(DisplayName="Production Blocked"),
	GarrisonChanged UMETA(DisplayName="Garrison Changed"),
	ReinforcementDeployed UMETA(DisplayName="Reinforcement Deployed"),
	DiplomacyChanged UMETA(DisplayName="Diplomacy Changed"),
	EspionageSucceeded UMETA(DisplayName="Espionage Succeeded"),
	EspionageFailed UMETA(DisplayName="Espionage Failed")
};

/** High-level drawer used to filter the Command Center intelligence databank. */
UENUM(BlueprintType)
enum class ETerritoryIntelligenceCategory : uint8
{
	Control UMETA(DisplayName="Control"),
	Conflict UMETA(DisplayName="Conflict"),
	Economy UMETA(DisplayName="Economy"),
	Command UMETA(DisplayName="Command"),
	Production UMETA(DisplayName="Production"),
	Diplomacy UMETA(DisplayName="Diplomacy")
};

/** Visual and gameplay importance of an intelligence record. */
UENUM(BlueprintType)
enum class ETerritoryIntelligenceSeverity : uint8
{
	Information UMETA(DisplayName="Information"),
	Positive UMETA(DisplayName="Positive"),
	Warning UMETA(DisplayName="Warning"),
	Critical UMETA(DisplayName="Critical")
};

/** Command Center filter. All is UI-only; records always use a concrete category. */
UENUM(BlueprintType)
enum class ETerritoryIntelligenceFilter : uint8
{
	All UMETA(DisplayName="All Reports"),
	Conflict UMETA(DisplayName="Conflict"),
	Control UMETA(DisplayName="Control"),
	Economy UMETA(DisplayName="Economy"),
	Command UMETA(DisplayName="Command"),
	Production UMETA(DisplayName="Production"),
	Diplomacy UMETA(DisplayName="Diplomacy")
};

/**
 * Read-only UI history entry. Gameplay state remains owned by ATerritoryVolume and
 * the counterattack subsystem; this is a local presentation snapshot only.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryLiveEvent
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events")
	FGuid EventID;

	/** Optional ID of the durable economy/assault record represented by this UI report. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events")
	FGuid SourceRecordID;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events")
	ETerritoryLiveEventType Type = ETerritoryLiveEventType::Contested;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events")
	ETerritoryIntelligenceCategory Category = ETerritoryIntelligenceCategory::Control;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events")
	ETerritoryIntelligenceSeverity Severity = ETerritoryIntelligenceSeverity::Information;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events", meta=(Categories="Territory"))
	FGameplayTag TerritoryTag;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events")
	FText TerritoryName;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events")
	FText Headline;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events")
	FText Detail;

	/** Faction responsible for the event, such as the attacking or earning faction. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events",
		meta=(Categories="Narrative.Factions"))
	FGameplayTag SourceFaction;

	/** Other faction affected by the event, when known. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events",
		meta=(Categories="Narrative.Factions"))
	FGameplayTag TargetFaction;

	/** Perks granted or revoked by this exact report. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events",
		meta=(Categories="Territory.Capability"))
	FGameplayTagContainer CommandCapabilities;

	/** Signed recurring income change caused by this report. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events")
	int64 IncomeDelta = 0;

	/** Signed recurring guard-cost change. Positive means higher cost; negative means cost removed. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events")
	int64 UpkeepDelta = 0;

	/** Signed currency transaction recorded by the economy authority. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events")
	int64 CurrencyDelta = 0;

	/** Sequence is stable within this player's session and makes reports easy to reference. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events")
	int32 Sequence = 0;

	/** Local real-time seconds, deliberately not campaign/save authority. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Live Events")
	double CreatedRealTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Live Events")
	float ActiveDuration = 30.f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Live Events")
	bool bCanSetWaypoint = false;

	/** Calculated when the feed is queried; expired rows remain readable but muted. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Live Events")
	bool bExpired = false;

	/** Economy/production audit rows normally stay in the databank without interrupting the HUD. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Live Events")
	bool bWasHUDNotification = false;

	bool IsExpiredAt(double NowRealTime) const
	{
		return ActiveDuration >= 0.f && NowRealTime >= CreatedRealTime + ActiveDuration;
	}

	/**
	 * Stable hash of fields that change how this report is drawn. EventID and
	 * CreatedRealTime are intentionally excluded because archive adapters may
	 * create a fresh presentation copy every time the Command Center queries them.
	 */
	uint32 GetPresentationRevision() const
	{
		uint32 Hash = GetTypeHash(SourceRecordID);
		Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Type)));
		Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Category)));
		Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Severity)));
		Hash = HashCombineFast(Hash, GetTypeHash(TerritoryTag));
		Hash = HashCombineFast(Hash, GetTypeHash(SourceFaction));
		Hash = HashCombineFast(Hash, GetTypeHash(TargetFaction));
		Hash = HashCombineFast(Hash, GetTypeHash(Headline.ToString()));
		Hash = HashCombineFast(Hash, GetTypeHash(Detail.ToString()));
		Hash = HashCombineFast(Hash, GetTypeHash(IncomeDelta));
		Hash = HashCombineFast(Hash, GetTypeHash(UpkeepDelta));
		Hash = HashCombineFast(Hash, GetTypeHash(CurrencyDelta));
		Hash = HashCombineFast(Hash, GetTypeHash(Sequence));
		Hash = HashCombineFast(Hash, GetTypeHash(bCanSetWaypoint));
		Hash = HashCombineFast(Hash, GetTypeHash(bExpired));
		TArray<FGameplayTag> Capabilities;
		CommandCapabilities.GetGameplayTagArray(Capabilities);
		for (const FGameplayTag& Capability : Capabilities)
		{
			Hash = HashCombineFast(Hash, GetTypeHash(Capability));
		}
		return Hash;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnTerritoryLiveEventAdded, const FTerritoryLiveEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTerritoryLiveEventsChanged);
