#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "TerritoryDeveloperSettings.generated.h"

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
	// Spatial Index
	// ═══════════════════════════════════════════════════════════════════════════

	/** Grid cell size (in Unreal units) for the spatial index. Smaller = more precise but more cells. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Spatial",
		meta = (ClampMin = "500.0", UIMin = "500.0", UIMax = "10000.0"))
	float SpatialCellSize = 2000.f;

	// ═══════════════════════════════════════════════════════════════════════════
	// Tags
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Tags",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag DefaultPlayerFaction;

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
};
