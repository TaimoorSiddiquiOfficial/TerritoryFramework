// Copyright TerritoryFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "TerritoryGuardPostDefinition.generated.h"

class UNPCDefinition;
class UNPCActivityConfiguration;
class UTriggerSet;

/**
 * Data-oriented guard post configuration — consolidates all guard authoring into one
 * reusable UPrimaryDataAsset. Assign to ATerritoryGuardSpawnPoint::GuardPostDefinition
 * to configure guard type, patrol, reserves, and Narrative overrides in a single asset.
 *
 * Design rationale (AGENTS.md 5.2):
 *   - Spawn-point actors contain transforms + optional definition override reference.
 *   - They do NOT duplicate all policy data inline.
 *   - Multiple spawn points can share one GuardPostDefinition for consistent behavior.
 *   - Territory-level FactionGuardDefinitions remain the per-faction fallback when no
 *     spawn-point-level definition is assigned.
 *
 * Usage:
 *   1. Create a UTerritoryGuardPostDefinition asset in Content Browser.
 *   2. Set NPCDefinition, patrol route, reserve count, etc.
 *   3. Assign it to one or more ATerritoryGuardSpawnPoint actors via GuardPostDefinition.
 *   4. SpawnGuards/TrySpawnSingleGuard reads from the data asset when present.
 */
UCLASS(BlueprintType, Const)
class TERRITORYFRAMEWORK_API UTerritoryGuardPostDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ─── Identity ───

	/** Display name for this guard post type (e.g., "Elite Guard Post", "Militia Post"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Post|Identity")
	FText DisplayName;

	/** Optional faction override. If invalid, the territory owner's faction is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Post|Identity",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag FactionOverride;

	// ─── NPC Configuration ───

	/** The NPC definition that drives guard spawning (abilities, appearance, class). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Post|NPC")
	TObjectPtr<UNPCDefinition> NPCDefinition;

	/** Optional activity configuration override (patrol behavior, goals, trigger sets). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Post|NPC")
	TObjectPtr<UNPCActivityConfiguration> ActivityConfiguration;

	/** Optional trigger set overrides applied to spawned guards. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Post|NPC")
	TArray<TSoftObjectPtr<UTriggerSet>> TriggerSetOverrides;

	// ─── Patrol Route ───

	/**
	 * The patrol route guards walk through.
	 * Empty = guard stands idle at spawn. Minimum useful route: 2 nodes.
	 * A Place Definition Guard Post row can override this with its own relative route.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Post|Patrol")
	TArray<FTerritoryPatrolNode> PatrolRoute;

	/** If true, the patrol loop returns to Node0 after the last node. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Post|Patrol")
	bool bLoopPatrol = true;

	// ─── Capacity & Reserves ───

	/** Number of reserve guards that spawn on demand when active guards die. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Post|Capacity",
		meta = (ClampMin = "0", UIMin = "0", UIMax = "10"))
	int32 ReserveSlots = 1;

	/** Delay before the first automatic reserve deployment (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Post|Capacity",
		meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "30.0"))
	float ReserveSpawnDelay = 3.f;

	/** Retry interval while no camera-frustum-avoided spawn location is available. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Post|Capacity",
		meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "30.0"))
	float ReserveSpawnRetryInterval = 2.f;

	/** Radius around spawn point used to randomize reserve placement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Post|Capacity",
		meta = (ClampMin = "100.0", UIMin = "100.0", UIMax = "3000.0"))
	float ReserveSpawnRadius = 600.f;

	/** Minimum distance reserves keep from player cameras. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Post|Capacity",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3000.0"))
	float ReserveMinimumPlayerDistance = 500.f;

	/** Number of navmesh candidates considered per reserve deployment attempt. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard Post|Capacity",
		meta = (ClampMin = "1", ClampMax = "64", UIMin = "1", UIMax = "32"))
	int32 ReserveSpawnCandidateCount = 12;

	// ─── Data Asset Identity ───

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("TerritoryGuardPost"), GetFName());
	}
};
