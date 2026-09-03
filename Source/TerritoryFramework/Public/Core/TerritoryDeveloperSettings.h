#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "TerritoryDeveloperSettings.generated.h"

class UNarrativeCommonButtonBase;
class UCommonButtonStyle;
class UCommonTextStyle;
class UTexture2D;
class ATerritoryRoadGuide;
class ATerritoryRoadTrafficControls;
class AMassVehicleSpawner;
class UMassEntityConfigAsset;

/**
 * Player-facing Territory notification policy.
 *
 * The Command Center log and the short Narrative HUD toast are separate choices:
 * a studio can keep a complete audit trail without interrupting the player every
 * production cycle. Currency and item storage remain owned by Narrative Pro.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryNotificationSettings
{
	GENERATED_BODY()

	/** Keep successful and failed money transactions in Command Center intelligence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Money",
		meta=(DisplayName="Record Money Transactions In Command Center"))
	bool bRecordMoneyTransactions = true;

	/** Show a short Narrative HUD toast after money was actually credited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Money",
		meta=(DisplayName="Show Money Earnings On HUD"))
	bool bShowMoneyEarningsOnHUD = true;

	/** Expenses stay in the log by default without repeatedly interrupting play. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Money",
		meta=(DisplayName="Show Money Expenses On HUD"))
	bool bShowMoneyExpensesOnHUD = false;

	/** Smaller transactions are still authoritative, but do not create a HUD toast. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Money",
		meta=(ClampMin="1", DisplayName="Minimum Money For HUD Notification"))
	int32 MinimumMoneyForHUDNotification = 1;

	/** Keep each settled production result in Command Center intelligence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resources",
		meta=(DisplayName="Record Resource Production In Command Center"))
	bool bRecordResourceProduction = true;

	/** Show the exact Narrative item names and quantities after a successful cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resources",
		meta=(DisplayName="Show Resource Earnings On HUD"))
	bool bShowResourceEarningsOnHUD = true;

	/** A blocked cycle is important by default because it needs player action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resources",
		meta=(DisplayName="Show Blocked Production On HUD"))
	bool bShowBlockedProductionOnHUD = true;

	/** Smaller successful item batches remain in the log but do not create a HUD toast. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resources",
		meta=(ClampMin="1", DisplayName="Minimum Resource Units For HUD Notification"))
	int32 MinimumResourceUnitsForHUDNotification = 1;

	/** Duration requested from Narrative Pro for money and resource toasts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation",
		meta=(ClampMin="1.0", ClampMax="15.0", Units="s"))
	float EconomyHUDNotificationDuration = 5.f;
};

UCLASS(BlueprintType, config = Engine, defaultconfig, meta = (DisplayName = "Territory Framework"))
class TERRITORYFRAMEWORK_API UTerritoryDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTerritoryDeveloperSettings();

	/** Global defaults for Territory intelligence and Narrative HUD economy toasts. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Territory|Notifications",
		meta=(ShowOnlyInnerProperties))
	FTerritoryNotificationSettings Notifications;

	// ═══════════════════════════════════════════════════════════════════════════
	// Narrative cinematic presentation
	// ═══════════════════════════════════════════════════════════════════════════

	/** Keep Territory's passive capture card out of Narrative dialogue shots. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly,
		Category="Territory|Presentation|Narrative Cinematics",
		meta=(DisplayName="Hide Territory HUD During Narrative Dialogue"))
	bool bHideTerritoryHUDDuringNarrativeDialogue = true;

	/**
	 * Temporarily requests LOD 0 for Narrative dialogue participants, including
	 * MetaHuman LODSync and Groom components. Exact pre-dialogue values are restored.
	 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly,
		Category="Territory|Presentation|Narrative Cinematics",
		meta=(DisplayName="Use Cinematic Participant LOD During Dialogue"))
	bool bUseCinematicParticipantLODDuringDialogue = true;

	/** Include attached visual actors when applying temporary dialogue LOD quality. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly,
		Category="Territory|Presentation|Narrative Cinematics")
	bool bIncludeAttachedActorsInCinematicLOD = true;

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

	/**
	 * Blueprint class placed by the straight-road editor helper. Keep project-specific
	 * visuals and defaults in this Blueprint instead of placing the native C++ class.
	 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly,
		Category="Territory|Counter Attack|Road Missions",
		meta=(DisplayName="Road Guide Blueprint Class"))
	TSoftClassPtr<ATerritoryRoadGuide> RoadGuideBlueprintClass;

	/** Blueprint placed once per traffic-enabled world; it owns the shared road bounds. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly,
		Category="Territory|Counter Attack|Road Missions")
	TSoftClassPtr<ATerritoryRoadTrafficControls> RoadTrafficControlsBlueprintClass;

	/** Blueprint placed once per traffic-enabled world; it owns Narrative Mass traffic entities. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly,
		Category="Territory|Counter Attack|Road Missions")
	TSoftClassPtr<AMassVehicleSpawner> RoadTrafficSpawnerBlueprintClass;

	/** Narrative Mass Entity Config used for ordinary mission traffic. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly,
		Category="Territory|Counter Attack|Road Missions")
	TSoftObjectPtr<UMassEntityConfigAsset> DefaultRoadTrafficEntityConfig;

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

	/**
	 * Optional fallback used only when a player pawn/controller has no faction from
	 * Narrative Pro's team interface. Leave empty when the game always assigns the
	 * player's faction through Narrative. A real Narrative faction always wins, so
	 * a betrayal or allegiance choice can change the player without this setting
	 * forcing them back to an old test faction.
	 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Tags",
		meta = (Categories = "Narrative.Factions",
			DisplayName = "Player Faction Fallback (Optional)",
			ToolTip = "Optional fallback for a player with no Narrative faction. Easy example: choose your project's Player faction while prototyping, then leave this empty once Narrative assigns factions. This is never used for NPCs and never overrides a player's live Narrative faction."))
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

	/** Master gate for the debug system. Enable the individual categories you need below. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug",
		meta = (DisplayName = "Enable Debug System (Master Gate)"))
	bool bEnableDebug = false;

	/** Log Definition seed versus saved/runtime availability and hierarchy lock decisions. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Availability",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugAvailabilityHierarchy = false;

	/** Log strategic scheduling, physical activation, waves, withdrawal, and resolution. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Counter Attacks",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugCounterAttacks = false;

	/** Log production rule selection, inventory inputs, outputs, and block reasons. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Production",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugProduction = false;

	/** Log Territory UI binding, read-model refreshes, visibility, and displayed status. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|UI",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugUI = false;

	/** Log stealth exposure, suspicion, detection, shots, and distraction decisions. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Stealth",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugStealth = false;

	/** Log replicated WorldState/directory publication and runtime actor hydration. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|World State",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugWorldState = false;

	/** Log capture-point, story-owner, management-point, and player interaction decisions. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Debug|Interaction",
		meta = (EditCondition = "bEnableDebug"))
	bool bDebugInteraction = false;

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

	/** Helper: master gate plus the configured log threshold. */
	bool IsDebugLevelEnabled(int32 RequiredLevel = 5) const
	{
		return bEnableDebug && DebugVerbosityLevel >= RequiredLevel;
	}

	/** Helpers: a category checkbox never bypasses the master gate or verbosity. */
	bool IsDebugEnabled() const { return bEnableDebug; }
	bool ShouldDebugAvailability() const { return IsDebugLevelEnabled() && bDebugAvailabilityHierarchy; }
	bool ShouldDebugCounterAttacks() const { return IsDebugLevelEnabled() && bDebugCounterAttacks; }
	bool ShouldDebugProduction() const { return IsDebugLevelEnabled() && bDebugProduction; }
	bool ShouldDebugUI() const { return IsDebugLevelEnabled() && bDebugUI; }
	bool ShouldDebugStealth() const { return IsDebugLevelEnabled() && bDebugStealth; }
	bool ShouldDebugWorldState() const { return IsDebugLevelEnabled() && bDebugWorldState; }
	bool ShouldDebugInteraction() const { return IsDebugLevelEnabled() && bDebugInteraction; }
	bool ShouldDebugRegistry() const { return IsDebugLevelEnabled() && bDebugRegistry; }
	bool ShouldDebugCapture() const { return IsDebugLevelEnabled() && bDebugCapture; }
	bool ShouldDebugCaptureAttempts() const { return IsDebugLevelEnabled() && bDebugCaptureAttempts; }
	bool ShouldDebugOwnership() const { return IsDebugLevelEnabled() && bDebugOwnershipChanges; }
	bool ShouldDebugStateTransitions() const { return IsDebugLevelEnabled() && bDebugStateTransitions; }
	bool ShouldDebugEconomy() const { return IsDebugLevelEnabled() && bDebugEconomyTicks; }
	bool ShouldDebugTransactions() const { return IsDebugLevelEnabled() && bDebugTransactions; }
	bool ShouldDebugGuards() const { return IsDebugLevelEnabled() && bDebugGuardSpawning; }
	bool ShouldDebugGuardDeaths() const { return IsDebugLevelEnabled() && bDebugGuardDeaths; }
	bool ShouldDebugDiplomacy() const { return IsDebugLevelEnabled() && bDebugDiplomacy; }
	bool ShouldDebugAttitudes() const { return IsDebugLevelEnabled() && bDebugFactionAttitudes; }
	bool ShouldDebugSaveLoad() const { return IsDebugLevelEnabled() && bDebugSaveLoad; }
	bool ShouldDebugSpatial() const { return IsDebugLevelEnabled() && bDebugSpatialIndex; }
	bool ShouldDebugMarkers() const { return IsDebugLevelEnabled() && bDebugMapMarkers; }
	bool ShouldDebugTales() const { return IsDebugLevelEnabled() && bDebugTales; }
	UFUNCTION(BlueprintPure, Category = "Territory|Debug")
	bool ShouldDebugBT() const { return IsDebugLevelEnabled() && bDebugBT; }
	UFUNCTION(BlueprintPure, Category = "Territory|Debug")
	bool ShouldDebugCombat() const { return IsDebugLevelEnabled() && bDebugCombat; }
};
