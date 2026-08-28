#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "TerritoryDeveloperSettings.generated.h"

class UNarrativeCommonButtonBase;
class UCommonButtonStyle;
class UCommonTextStyle;
class UTexture2D;

UCLASS(BlueprintType, config = Engine, defaultconfig, meta = (DisplayName = "Territory Framework"))
class TERRITORYFRAMEWORK_API UTerritoryDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTerritoryDeveloperSettings();

	// ═══════════════════════════════════════════════════════════════════════════
	// Economy
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Economy",
		meta = (ClampMin = "10.0", UIMin = "10.0", UIMax = "3600.0"))
	float EconomyTickIntervalSeconds = 300.f;

	/**
	 * How often the server checks Narrative Pro's campaign clock for a new
	 * production cycle. This does not change the production cycle length.
	 * Easy example: when sleeping advances the story by one day, a value of 1
	 * delivers Farm or Blacksmith items within about one real second instead of
	 * waiting for the five-minute currency/upkeep tick.
	 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Economy|Resources",
		meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "10.0",
			DisplayName = "Campaign Production Check Interval (Seconds)"))
	float ProductionCycleObservationIntervalSeconds = 1.f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Economy",
		meta = (ClampMin = "0"))
	int32 DefaultTerritoryIncome = 100;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Economy",
		meta = (ClampMin = "0"))
	int32 DefaultGuardCost = 50;

	// ═══════════════════════════════════════════════════════════════════════════
	// Capture
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Capture",
		meta = (ClampMin = "0.01", UIMin = "0.01", UIMax = "1.0"))
	float CaptureProgressPerSecond = 0.1f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Capture",
		meta = (ClampMin = "0.01", UIMin = "0.01", UIMax = "0.5"))
	float CaptureProgressDecayPerSecond = 0.05f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Capture",
		meta = (ClampMin = "1", UIMin = "1", UIMax = "20"))
	int32 DefaultMaxConcurrentAttackers = 3;

	/** Capture tick interval in seconds. Controls how often capture progression is evaluated. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Capture",
		meta = (ClampMin = "0.05", UIMin = "0.05", UIMax = "1.0"))
	float CaptureTickInterval = 0.1f;

	/** Treaty expiration check interval in seconds. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Capture",
		meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "60.0"))
	float TreatyExpirationCheckInterval = 10.f;

	// ═══════════════════════════════════════════════════════════════════════════
	// Counterattacks
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|Counter Attack")
	int32 CounterAttackCampaignSeed = 1337;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ClampMin="0.25", ClampMax="60.0"))
	float CounterAttackUpdateInterval = 2.f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ClampMin="1"))
	int32 MaxConcurrentScheduledAssaults = 8;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ClampMin="1"))
	int32 MaxConcurrentAssaultsPerFaction = 2;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ClampMin="1"))
	int32 MaxLiveCounterAttackNPCs = 24;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ClampMin="1"))
	int32 MaxRetainedAssaultRecords = 100;

	// ═══════════════════════════════════════════════════════════════════════════
	// Spatial Index
	// ═══════════════════════════════════════════════════════════════════════════

	/** Grid cell size (in Unreal units) for the spatial index. Smaller = more precise but more cells. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Spatial",
		meta = (ClampMin = "500.0", UIMin = "500.0", UIMax = "10000.0"))
	float SpatialCellSize = 2000.f;

	// ═══════════════════════════════════════════════════════════════════════════
	// Guard / Patrol
	// ═══════════════════════════════════════════════════════════════════════════

	/** Default arrival threshold for patrol route advancement (uu) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Guards",
		meta = (ClampMin = "50.0", UIMin = "50.0", UIMax = "500.0"))
	float DefaultPatrolArrivalThreshold = 100.f;

	/** Default acceptance radius for patrol move task (uu) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Guards",
		meta = (ClampMin = "10.0", UIMin = "10.0", UIMax = "200.0"))
	float DefaultPatrolAcceptanceRadius = 50.f;

	/** Default wait time at patrol nodes (seconds) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Guards",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "30.0"))
	float DefaultPatrolWaitTime = 2.f;

	/** Max patrol route nodes per spawn point (sanity cap) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Guards",
		meta = (ClampMin = "0", UIMin = "0", UIMax = "100"))
	int32 MaxPatrolRouteNodes = 32;

	/** @deprecated Unused — factions start with zero gold. Remove in v0.3.0. */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Unused. Remove in v0.3.0."))
	int32 EconomyStartingGold = 0;

	/** @deprecated Unused — no capture history system exists. Remove in v0.3.0. */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Unused. Remove in v0.3.0."))
	int32 MaxCaptureHistory = 50;

	// ═══════════════════════════════════════════════════════════════════════════
	// Tags
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Tags",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag DefaultPlayerFaction;

	/** Configurable Narrative CommonUI button used by native fallback Territory rows. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|UI")
	TSoftClassPtr<UNarrativeCommonButtonBase> DefaultNarrativeButtonClass;

	/** Territory-owned visual style applied to Narrative buttons created by native fallback layouts. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|UI")
	TSoftClassPtr<UCommonButtonStyle> DefaultTerritoryButtonStyle;

	/**
	 * Compact selector style for Territory navigation tabs.
	 * Easy example: Overview, Places, Garrison, Economy, and Diplomacy use this
	 * style so the selected page is easy to see without looking like a command.
	 * When empty, Default Territory Button Style is used.
	 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|UI|Theme",
		meta=(DisplayName="Territory Tab Button Style"))
	TSoftClassPtr<UCommonButtonStyle> TerritoryTabButtonStyle;

	/**
	 * Strong action style for buttons that perform a Territory command.
	 * Easy example: Set Waypoint, Espionage, Add Guard, Reinforce, and Close use
	 * this style. When empty, Default Territory Button Style is used.
	 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|UI|Theme",
		meta=(DisplayName="Territory Action Button Style"))
	TSoftClassPtr<UCommonButtonStyle> TerritoryActionButtonStyle;

	/** Body style for runtime-created Narrative CommonText rows. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|UI|Theme")
	TSoftClassPtr<UCommonTextStyle> DefaultTerritoryTextStyle;

	/** Large title style used by City/District command headings. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|UI|Theme")
	TSoftClassPtr<UCommonTextStyle> TerritoryTitleTextStyle;

	/** Section/header style used for City grouping and selected-control tabs. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|UI|Theme")
	TSoftClassPtr<UCommonTextStyle> TerritoryHeadingTextStyle;

	/** Quiet explanatory/empty-state style. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|UI|Theme")
	TSoftClassPtr<UCommonTextStyle> TerritoryMutedTextStyle;

	/**
	 * Multiplies Territory text sizes after the Narrative CommonUI style is applied.
	 * Easy example: keep 1.0 for the compact Command Center, or use 1.25 when the
	 * game is played from a television and the same text needs to be easier to read.
	 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|UI|Theme",
		meta=(ClampMin="0.75", ClampMax="2.0", UIMin="0.75", UIMax="2.0",
			DisplayName="Territory Text Scale"))
	float TerritoryTextScale = 1.f;

	/** Optional nine-slice texture for Command Center, District, intelligence, and compact HUD cards. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|UI|Theme")
	TSoftObjectPtr<UTexture2D> TerritoryPanelTexture;

	/**
	 * Optional full-screen image behind the Command Center content.
	 * Easy example: import a 1920 x 1080 dark menu background. Leave this empty to
	 * keep the owning Narrative Pro menu background fully visible.
	 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|UI|Theme",
		meta=(DisplayName="Command Center Background Texture"))
	TSoftObjectPtr<UTexture2D> TerritoryScreenBackgroundTexture;

	/** Optional frame texture applied behind capture and garrison progress bars. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|UI|Theme")
	TSoftObjectPtr<UTexture2D> TerritoryProgressFrameTexture;

	/** Optional positive fill texture used by the garrison staffing planner. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|UI|Theme")
	TSoftObjectPtr<UTexture2D> TerritoryProgressFillTexture;

	// ═══════════════════════════════════════════════════════════════════════════
	// Debug — Toggle individual debug categories
	// ═══════════════════════════════════════════════════════════════════════════

	/** Master debug toggle — enables all debug output when true */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug",
		meta = (DisplayName = "Enable All Debug Output"))
	bool bEnableDebug = false;

	/** Log territory registration/unregistration events */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Registry",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugRegistry = false;

	/** Log capture progression (tick-by-tick progress, faction changes) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Capture",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugCapture = false;

	/** Log capture attempts with full details (attacker, defender, result) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Capture",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugCaptureAttempts = false;

	/** Log ownership changes with before/after state */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Ownership",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugOwnershipChanges = false;

	/** Log state transitions (Unclaimed→Claimed→Contested→etc) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Ownership",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugStateTransitions = false;

	/** Log economy ticks with full treasury snapshots */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Economy",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugEconomyTicks = false;

	/** Log every transaction (credit/debit) with reason and balance */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Economy",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugTransactions = false;

	/** Log guard spawning/despawning events */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Guards",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugGuardSpawning = false;

	/** Log guard death events and reserve usage */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Guards",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugGuardDeaths = false;

	/** Log diplomacy changes (treaties, wars, peace) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Diplomacy",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugDiplomacy = false;

	/** Log faction attitude checks and their results */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Diplomacy",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugFactionAttitudes = false;

	/** Log save/load events for territory data */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|SaveLoad",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugSaveLoad = false;

	/** Log spatial index queries */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Spatial",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugSpatialIndex = false;

	/** Log map marker refresh events */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Markers",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugMapMarkers = false;

	/** Log Tales integration (capture tasks, events, conditions) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Tales",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugTales = false;

	/** Log behavior tree patrol AI (move tasks, route advancement) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|AI",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugBT = false;

	/** Log combat director events (attack permissions, budget) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Combat",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugCombat = false;

	// ═══════════════════════════════════════════════════════════════════════════
	// Debug — Visual
	// ═══════════════════════════════════════════════════════════════════════════

	/** Draw territory bounds in PIE */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Visual",
		meta = (EditCondition = "bEnableDebug"))
	bool bDrawTerritoryBounds = false;

	/** Draw territory ownership color overlay in PIE */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Visual",
		meta = (EditCondition = "bEnableDebug"))
	bool bDrawOwnershipOverlay = false;

	/** Draw capture progress bars above contested territories */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Visual",
		meta = (EditCondition = "bEnableDebug"))
	bool bDrawCaptureProgress = false;

	/** Draw guard spawn points and patrol routes in PIE */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Visual",
		meta = (EditCondition = "bEnableDebug"))
	bool bDrawGuardSpawnPoints = false;

	/** Draw spatial index grid cells */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Visual",
		meta = (EditCondition = "bEnableDebug"))
	bool bDrawSpatialGrid = false;

	// ═══════════════════════════════════════════════════════════════════════════
	// Debug — Verbosity
	// ═══════════════════════════════════════════════════════════════════════════

	/** Debug log verbosity: 0=NoLogging, 1=Fatal, 2=Error, 3=Warning, 4=Display, 5=Log, 6=Verbose */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Verbosity",
		meta = (EditCondition = "bEnableDebug", ClampMin = "0", ClampMax = "6", UIMin = "0", UIMax = "6"))
	int32 DebugVerbosityLevel = 5;

	/** Helper: check if a specific debug category is enabled */
	bool IsDebugEnabled() const { return bEnableDebug; }
	bool ShouldDebugRegistry() const { return bEnableDebug && bDebugRegistry; }
	bool ShouldDebugCapture() const { return bEnableDebug && bDebugCapture; }
	bool ShouldDebugCaptureAttempts() const { return bEnableDebug && bDebugCaptureAttempts; }
	bool ShouldDebugOwnership() const { return bEnableDebug && bDebugOwnershipChanges; }
	bool ShouldDebugStateTransitions() const { return bEnableDebug && bDebugStateTransitions; }
	bool ShouldDebugEconomy() const { return bEnableDebug && bDebugEconomyTicks; }
	bool ShouldDebugTransactions() const { return bEnableDebug && bDebugTransactions; }
	bool ShouldDebugGuards() const { return bEnableDebug && bDebugGuardSpawning; }
	bool ShouldDebugGuardDeaths() const { return bEnableDebug && bDebugGuardDeaths; }
	bool ShouldDebugDiplomacy() const { return bEnableDebug && bDebugDiplomacy; }
	bool ShouldDebugAttitudes() const { return bEnableDebug && bDebugFactionAttitudes; }
	bool ShouldDebugSaveLoad() const { return bEnableDebug && bDebugSaveLoad; }
	bool ShouldDebugSpatial() const { return bEnableDebug && bDebugSpatialIndex; }
	bool ShouldDebugMarkers() const { return bEnableDebug && bDebugMapMarkers; }
	bool ShouldDebugTales() const { return bEnableDebug && bDebugTales; }
	UFUNCTION(BlueprintPure, Category = "Territory|Debug")
	bool ShouldDebugBT() const { return bEnableDebug && bDebugBT; }
	UFUNCTION(BlueprintPure, Category = "Territory|Debug")
	bool ShouldDebugCombat() const { return bEnableDebug && bDebugCombat; }
};
