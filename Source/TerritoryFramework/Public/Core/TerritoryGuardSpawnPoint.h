#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TerritoryGuardSpawnPoint.generated.h"

class ATerritoryVolume;
class ATerritoryGuardCharacter;

/**
 * Patrol waypoint — a single node in a guard's patrol route.
 */
USTRUCT(BlueprintType)
struct FTerritoryPatrolNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol")
	FRotator Rotation = FRotator::ZeroRotator;

	/** Time in seconds the guard waits at this node before moving to next */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "30.0"))
	float WaitTime = 2.f;

	/** Optional activity tag (e.g., Guard.Activity.Inspect, Guard.Activity.Rest) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol",
		meta = (Categories = "Guard.Activity"))
	FGameplayTag ActivityTag;
};

/**
 * Dedicated guard spawn point actor for territory districts.
 *
 * Place these inside a territory volume to define:
 * - Where guards spawn (instead of random within bounds)
 * - Patrol routes (ordered list of waypoints)
 * - Reserve slots (guards that spawn only when needed)
 * - Activity assignments (what guards do at each patrol node)
 *
 * The owning ATerritoryVolume references these spawn points
 * instead of using random positions within its BoundsShape.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Territory Guard Spawn Point"))
class TERRITORYFRAMEWORK_API ATerritoryGuardSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ATerritoryGuardSpawnPoint();

	// ─── Configuration ───

	/** Which territory this spawn point belongs to (auto-resolved by proximity at BeginPlay) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|GuardSpawn")
	FGameplayTag OwnerTerritoryTag;

	/** Maximum guards that can spawn at this point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|GuardSpawn",
		meta = (ClampMin = "1", UIMin = "1", UIMax = "20"))
	int32 MaxGuards = 3;

	/** Number of reserve slots — guards that only spawn when active guards die */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|GuardSpawn",
		meta = (ClampMin = "0", UIMin = "0", UIMax = "10"))
	int32 ReserveSlots = 1;

	/** Patrol route waypoints (ordered). Empty = guard stays at spawn point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|GuardSpawn|Patrol")
	TArray<FTerritoryPatrolNode> PatrolRoute;

	/** If true, patrol route loops back to start after last node */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|GuardSpawn|Patrol")
	bool bLoopPatrol = true;

	/** Faction override — if invalid, uses territory owner's faction */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|GuardSpawn",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag FactionOverride;

	/** Priority — higher priority spawn points fill first */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|GuardSpawn",
		meta = (ClampMin = "0", UIMin = "0", UIMax = "100"))
	int32 Priority = 50;

	// ─── Runtime API ───

	/** Get the spawn transform for the next available slot */
	UFUNCTION(BlueprintCallable, Category = "Territory|GuardSpawn")
	FTransform GetSpawnTransform() const;

	/** Check if this spawn point has available slots */
	UFUNCTION(BlueprintPure, Category = "Territory|GuardSpawn")
	bool HasAvailableSlot() const;

	/** Check if this spawn point has reserve guards available */
	UFUNCTION(BlueprintPure, Category = "Territory|GuardSpawn")
	bool HasReserveAvailable() const;

	/** Get current active guard count */
	UFUNCTION(BlueprintPure, Category = "Territory|GuardSpawn")
	int32 GetActiveGuardCount() const;

	/** Get current reserve count */
	UFUNCTION(BlueprintPure, Category = "Territory|GuardSpawn")
	int32 GetReserveCount() const;

	/** Register a guard spawned from this point */
	UFUNCTION(BlueprintCallable, Category = "Territory|GuardSpawn")
	void RegisterSpawnedGuard(ATerritoryGuardCharacter* Guard);

	/** Unregister a guard (death, despawn) — frees slot or uses reserve */
	UFUNCTION(BlueprintCallable, Category = "Territory|GuardSpawn")
	void UnregisterGuard(ATerritoryGuardCharacter* Guard);

	/** Get the patrol route for a guard */
	UFUNCTION(BlueprintPure, Category = "Territory|GuardSpawn")
	const TArray<FTerritoryPatrolNode>& GetPatrolRoute() const;

	/** Whether the patrol route loops back to start after the last node */
	UFUNCTION(BlueprintPure, Category = "Territory|GuardSpawn")
	bool GetLoopPatrol() const { return bLoopPatrol; }

	/** Get the owning territory volume (resolved at BeginPlay) */
	UFUNCTION(BlueprintPure, Category = "Territory|GuardSpawn")
	ATerritoryVolume* GetOwningTerritory() const;

	/** Check if patrol route is valid (has at least 2 nodes for meaningful patrol) */
	UFUNCTION(BlueprintPure, Category = "Territory|GuardSpawn")
	bool HasPatrolRoute() const;

	/**
	 * Get patrol route as transforms — for populating a Narrative Goal_Patrol's
	 * PatrolPoints array from Blueprint. Each node becomes an FTransform with
	 * WaitTime stored separately via GetPatrolWaitTimes().
	 *
	 * Usage in BP:
	 *   1. Spawn guard with Territory ConfigureTerritorySpawn
	 *   2. Get the spawn point's PatrolRouteAsTransforms
	 *   3. Set them on a Goal_Patrol asset or BPA_Patrol activity
	 */
	UFUNCTION(BlueprintPure, Category = "Territory|GuardSpawn|Patrol")
	TArray<FTransform> GetPatrolRouteAsTransforms() const;

	/** Get wait times for each patrol node — parallel to GetPatrolRouteAsTransforms. */
	UFUNCTION(BlueprintPure, Category = "Territory|GuardSpawn|Patrol")
	TArray<float> GetPatrolWaitTimes() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Late-binding handler for when a territory registers after this spawn point. */
	UFUNCTION()
	void OnTerritoryRegistered(class ATerritoryVolume* Territory, bool bIsNew);

#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
#endif

	UPROPERTY()
	TWeakObjectPtr<ATerritoryVolume> CachedTerritory;

	UPROPERTY()
	TArray<TWeakObjectPtr<ATerritoryGuardCharacter>> ActiveGuards;

	UPROPERTY()
	int32 CurrentReserveCount = 0;

	/** Visualize spawn point and patrol route in editor */
	UPROPERTY(EditDefaultsOnly, Category = "Territory|GuardSpawn|Visual")
	bool bShowPatrolRouteInEditor = true;

	UPROPERTY(EditDefaultsOnly, Category = "Territory|GuardSpawn|Visual")
	FLinearColor SpawnPointColor = FLinearColor(0.f, 1.f, 0.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Territory|GuardSpawn|Visual")
	FLinearColor PatrolRouteColor = FLinearColor(1.f, 1.f, 0.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Territory|GuardSpawn|Visual")
	FLinearColor ReserveColor = FLinearColor(0.f, 0.5f, 1.f, 1.f);

private:
	void ResolveOwningTerritory();
	void InitializeReserves();
};
