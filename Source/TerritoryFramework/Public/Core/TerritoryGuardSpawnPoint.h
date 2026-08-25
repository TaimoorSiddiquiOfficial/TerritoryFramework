#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NarrativeSavableActor.h"
#include "TerritoryGuardSpawnPoint.generated.h"

class ATerritoryVolume;
class ATerritoryGuardCharacter;
class UNPCDefinition;
class UNPCActivityConfiguration;
class UTriggerSet;
enum class ETerritoryState : uint8;

/**
 * P1-09: Policy for what happens to reserves when ownership changes.
 */
UENUM(BlueprintType)
enum class EReserveOwnershipPolicy : uint8
{
	/** Reserves persist with the post regardless of who owns it */
	PersistWithPost,
	/** Reserves refill to full when a new faction takes ownership */
	RefillOnOwnerChange,
	/** Reserves reset to the GuardPostDefinition's configured value on owner change */
	ResetToDefinitionOnOwnerChange
};

/**
 * P1-03: Why an ownership transition occurred.
 * Spawn points use this to decide whether to apply reserve ownership policy.
 */
UENUM(BlueprintType)
enum class EOwnershipTransitionReason : uint8
{
	/** First-time spawn during BeginPlay or load reconcile — no ownership change */
	InitialSpawn,
	/** Ownership changed from one faction to another */
	OwnerChanged,
	/** Territory reverted to unclaimed */
	RevertedToUnclaimed,
	/** Manual editor/admin override */
	AdminOverride
};

/**
 * P1-04: Reason a guard was removed from a spawn point.
 * Only Killed queues a reserve replacement. Manual removal does not.
 */
UENUM(BlueprintType)
enum class EGuardRemovalReason : uint8
{
	/** Guard was killed in combat — queues reserve if available */
	Killed,
	/** Guard was manually removed by player/system — no reserve queued */
	ManualRemoval,
	/** Ownership changed — reserves handled by ownership policy */
	OwnerChanged,
	/** Load reconciliation — despawning old-owner guards */
	LoadReconcile,
	/** Territory destroyed — cleanup only */
	TerritoryDestroyed
};

/**
 * A single waypoint in a guard's patrol route.
 *
 * Use these in pairs/triples inside ATerritoryGuardSpawnPoint's PatrolRoute array.
 * Guards walk Node0 -> Node1 -> Node2 ... and optionally loop back to Node0.
 */
USTRUCT(BlueprintType, meta=(DisplayName="Territory Patrol Node"))
struct FTerritoryPatrolNode
{
	GENERATED_BODY()

	/** World-space location the guard walks to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(DisplayName="Location"))
	FVector Location = FVector::ZeroVector;

	/** Rotation the guard faces when arriving at this node. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(DisplayName="Rotation"))
	FRotator Rotation = FRotator::ZeroRotator;

	/**
	 * Seconds the guard waits at this node before proceeding.
	 * Use 0 for continuous patrol with no waiting. Typical rest/inspect: 2-5s.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="30.0", DisplayName="Wait Time"))
	float WaitTime = 2.f;

	/**
	 * Optional activity tag (e.g., Guard.Activity.Inspect, Guard.Activity.Rest).
	 * If set, the guard plays this activity at the node instead of standing idle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol",
		meta=(Categories="Guard.Activity", DisplayName="Activity Tag"))
	FGameplayTag ActivityTag;
};

/**
 * Dedicated spawn point for territory guards. Place these inside a territory volume
 * to define the exact guard staging marker, patrol route, and reserve pool.
 *
 * Key design points:
 *   - Each spawn point owns its own PatrolRoute (TArray<FTerritoryPatrolNode>).
 *   - Guards spawned from this point access the route via ATerritoryGuardCharacter
 *     helpers (GetTerritoryPatrolRoute, HasTerritoryPatrolRoute, GetPatrolNodeCount).
 *   - Every unique spawn point contributes exactly one active combat slot.
 *   - Authored spawn points are authoritative. No random active-guard fallback or
 *     collision-driven relocation is allowed.
 *
 * Quick Blueprint Example:
 *   for spawn point in territory->GetGuardSpawnPoints():
 *     if spawn point->HasAvailableSlot() and spawn point->HasPatrolRoute():
 *       spawn guard here -> configure spawn -> start patrol
 */
UCLASS(BlueprintType, Blueprintable, meta=(DisplayName="Territory Guard Spawn Point"))
class TERRITORYFRAMEWORK_API ATerritoryGuardSpawnPoint : public AActor, public INarrativeSavableActor
{
	GENERATED_BODY()
	friend class FTFBehavior_GuardSlotSaveMigration;

public:
	ATerritoryGuardSpawnPoint();

	// ─── INarrativeSavableActor (P0-06) ───
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& InGUID) override;
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	virtual bool ShouldRespawn_Implementation() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#endif

	// ─── Configuration ───

	/**
	 * Which territory this spawn point belongs to. A territory's authored GuardSpawnPoints
	 * reference takes precedence, followed by this tag, then placement/patrol overlap.
	 * An untagged post whose actor origin is outside a District can still belong to it
	 * when at least one world-space patrol node overlaps that District.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|GuardSpawn",
		meta=(Categories="Territory", DisplayName="Owner Territory"))
	FGameplayTag OwnerTerritoryTag;

	/** Legacy serialized value. Active capacity is always one guard per spawn point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|GuardSpawn",
		meta=(DeprecatedProperty,
			DeprecationMessage="Ignored. Each spawn point is one active guard slot; add more spawn point actors to increase capacity.",
			ClampMin="1", UIMin="1", UIMax="1", DisplayName="Legacy Max Guards (Ignored)"))
	int32 MaxGuards = 1;

	/**
	 * Number of reserve guards that spawn on demand when active guards die.
	 * Set to 0 for no reserves. Typical value: 1-2.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|GuardSpawn",
		meta=(ClampMin="0", UIMin="0", UIMax="10", DisplayName="Reserve Slots"))
	int32 ReserveSlots = 1;

	/** Automatically deploy queued reserves after a tracked guard dies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|GuardSpawn|Reserve",
		meta=(DisplayName="Auto Spawn Reserves"))
	bool bAutoSpawnReserves = true;

	/** Delay before the first automatic reserve deployment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|GuardSpawn|Reserve",
		meta=(ClampMin="0.1", UIMin="0.1", UIMax="30.0", Units="s", DisplayName="Reserve Spawn Delay"))
	float ReserveSpawnDelay = 3.f;

	/** Retry interval while no camera-frustum-avoided, collision-free spawn location is available. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|GuardSpawn|Reserve",
		meta=(ClampMin="0.1", UIMin="0.1", UIMax="30.0", Units="s", DisplayName="Reserve Retry Interval"))
	float ReserveSpawnRetryInterval = 2.f;

	/** Radius around this actor used to randomize automatic reserve placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|GuardSpawn|Reserve",
		meta=(ClampMin="100.0", UIMin="100.0", UIMax="3000.0", Units="cm", DisplayName="Reserve Spawn Radius"))
	float ReserveSpawnRadius = 600.f;

	/** Minimum distance automatic reserve spawns keep from every player camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|GuardSpawn|Reserve",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="3000.0", Units="cm", DisplayName="Reserve Player Clearance"))
	float ReserveMinimumPlayerDistance = 500.f;

	/** Number of random navmesh candidates considered per automatic deployment attempt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|GuardSpawn|Reserve",
		meta=(ClampMin="1", ClampMax="64", UIMin="1", UIMax="32", DisplayName="Reserve Spawn Attempts"))
	int32 ReserveSpawnCandidateCount = 12;

	/**
	 * The patrol route this spawn point's guards walk through.
	 * Empty = guard stands idle at spawn. Minimum useful route: 2 nodes.
	 * Guarded access via GetPatrolRoute() or HasPatrolRoute().
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|GuardSpawn|Patrol",
		meta=(DisplayName="Patrol Route"))
	TArray<FTerritoryPatrolNode> PatrolRoute;

	/** If true, the patrol loop returns to Node0 after the last node. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|GuardSpawn|Patrol",
		meta=(DisplayName="Loop Patrol"))
	bool bLoopPatrol = true;

	/**
	 * Faction override. If invalid, the guard uses the territory owner's faction.
	 * Useful for "neutral" garrisons or captured territories.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|GuardSpawn",
		meta=(Categories="Narrative.Factions", DisplayName="Faction Override"))
	FGameplayTag FactionOverride;

	/** Higher-priority spawn points fill first when territory guards spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|GuardSpawn",
		meta=(ClampMin="0", UIMin="0", UIMax="100", DisplayName="Priority"))
	int32 Priority = 50;

	/** P1-09: What happens to reserves when territory ownership changes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|GuardSpawn|Reserve",
		meta=(DisplayName="Reserve Ownership Policy"))
	EReserveOwnershipPolicy ReserveOwnershipPolicy = EReserveOwnershipPolicy::RefillOnOwnerChange;

	// ─── Guard Post Definition (Data Asset) ───

	/**
	 * Optional data-driven guard post configuration. When assigned, provides NPC definition,
	 * activity config, trigger sets, patrol route, and reserve settings from a
	 * reusable UPrimaryDataAsset. Inline properties below act as overrides when this is set.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|GuardSpawn",
		meta=(DisplayName="Guard Post Definition"))
	TObjectPtr<class UTerritoryGuardPostDefinition> GuardPostDefinition;

	// ─── Narrative Overrides ───

	/** Optional NPC definition override for guards spawned from this point. Uses territory default if null. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|GuardSpawn|Narrative",
		meta=(DisplayName="NPC Definition Override"))
	TObjectPtr<UNPCDefinition> NPCDefinitionOverride;

	/** Optional activity configuration override. Uses territory default if null. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|GuardSpawn|Narrative",
		meta=(DisplayName="Activity Configuration Override"))
	TObjectPtr<UNPCActivityConfiguration> ActivityConfigurationOverride;

	/** Optional trigger set overrides. Uses territory default if empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|GuardSpawn|Narrative",
		meta=(DisplayName="Trigger Set Overrides"))
	TArray<TSoftObjectPtr<UTriggerSet>> TriggerSetOverrides;

	// ─── Slot Queries (BlueprintPure) ───

	/**
	 * Returns true if this spawn point has at least one free active slot.
	 * Use before calling SpawnSingleGuard() to confirm this point's one active slot is free.
	 *
	 * Example: if spawn point->HasAvailableSlot() -> spawn guard
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Slot",
		meta=(DisplayName="Has Available Slot"))
	bool HasAvailableSlot() const;

	/**
	 * Returns true if this spawn point has reserve guards that can be deployed.
	 * Reserves spawn only after an active guard is killed/despawned.
	 *
	 * Example: if (ActiveGuard->Died) { if spawn point->HasReserveAvailable() -> SpawnReserve(); }
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Slot",
		meta=(DisplayName="Has Reserve Available"))
	bool HasReserveAvailable() const;

	/**
	 * Returns the number of currently-alive guards spawned from this point.
	 * 1 - GetActiveGuardCount() == available active slots.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Slot",
		meta=(DisplayName="Get Active Guard Count"))
	int32 GetActiveGuardCount() const;

	/** Number of active guards persisted by the most recent Narrative save. */
	int32 GetSavedActiveGuardCount() const { return FMath::Max(0, SavedActiveGuardCount); }

	/**
	 * Returns the number of reserve guards currently held at this point.
	 * Reserves start at ReserveSlots and decrease as they deploy.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Slot",
		meta=(DisplayName="Get Reserve Count"))
	int32 GetReserveCount() const;

	/** Returns true while a dead active slot is waiting for reserve deployment. */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Reserve",
		meta=(DisplayName="Has Pending Reserve Spawn"))
	bool HasPendingReserveSpawn() const;

	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Reserve",
		meta=(DisplayName="Get Pending Reserve Count"))
	int32 GetPendingReserveCount() const { return FMath::Max(0, PendingReserveSpawns); }

	/**
	 * Deploys one reserve into a free active slot. This is the manual path when
	 * Auto Spawn Reserves is disabled and intentionally does not require camera-frustum avoidance.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|GuardSpawn|Reserve",
		meta=(DisplayName="Spawn Reserve Guard"))
	bool SpawnReserveGuard();

	// ─── Transform Query ───

	/**
	 * Returns the exact authored marker transform. The marker represents the guard's
	 * foot position and facing; it is never horizontally projected or randomized.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn",
		meta=(DisplayName="Get Spawn Transform"))
	virtual FTransform GetSpawnTransform() const;

	/**
	 * Resolves the actual character-capsule transform for this marker. X/Y and facing
	 * remain exact; only Z is aligned to nearby navigation ground and raised by the
	 * guard capsule half-height.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn",
		meta=(DisplayName="Resolve Guard Deployment Transform"))
	bool ResolveGuardDeploymentTransform(TSubclassOf<ATerritoryGuardCharacter> GuardClass,
		FTransform& OutTransform) const;

	// ─── Guard Registration ───

	/**
	 * Registers a guard as spawned from this point. Decrements available slots.
	 * Called automatically by TerritoryVolume after spawn; you usually don't call this directly.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|GuardSpawn",
		meta=(DisplayName="Register Spawned Guard"))
	void RegisterSpawnedGuard(ATerritoryGuardCharacter* Guard);

	/**
	 * Unregisters a guard (death or despawn). Frees the active slot and may spawn a reserve.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|GuardSpawn",
		meta=(DisplayName="Unregister Guard"))
	void UnregisterGuard(ATerritoryGuardCharacter* Guard, EGuardRemovalReason Reason = EGuardRemovalReason::Killed);

	// ─── Patrol Route Access ───

	/**
	 * Returns the patrol route stored on this spawn point.
	 *
	 * For patrol AI, prefer the ATerritoryGuardCharacter helpers (GetTerritoryPatrolRoute)
	 * which read this array via the guard's bound spawn point.
	 *
	 * Returns every configured node. Use HasPatrolRoute() when AI requires at least two nodes.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Patrol",
		meta=(DisplayName="Get Patrol Route", CompactNodeTitle="Patrol Route"))
	TArray<FTerritoryPatrolNode> GetPatrolRoute() const;

	/**
	 * Returns true if this spawn point has a meaningful patrol route (>= 2 nodes).
	 *
	 * Example: if (spawn point->HasPatrolRoute()) { RunPatrolActivity(); } else { StandIdle(); }
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Patrol",
		meta=(DisplayName="Has Patrol Route"))
	bool HasPatrolRoute() const;

	/**
	 * Returns whether the patrol route loops back to the first node after the last.
	 * Used by patrol AI to decide whether to keep walking or stop.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Patrol",
		meta=(DisplayName="Is Looping Patrol"))
	bool GetLoopPatrol() const { return GetEffectiveLoopPatrol(); }

	/**
	 * Returns the patrol route as an array of FTransforms — convenient for Narrative
	 * GoalItem's PatrolPoints input. Each node's Location+Rotation becomes a transform.
	 *
	 * Parallel to GetPatrolWaitTimes() for duration per node.
	 *
	 * Example:
	 *   Transforms = spawn point->GetPatrolRouteAsTransforms();
	 *   WaitTimes  = spawn point->GetPatrolWaitTimes();
	 *   for i in range(Transforms.Num()): SetBlackboardValue(Transforms[i], WaitTimes[i]);
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Patrol",
		meta=(DisplayName="Get Patrol Route As Transforms"))
	TArray<FTransform> GetPatrolRouteAsTransforms() const;

	/**
	 * Returns an array of wait times (seconds), parallel to GetPatrolRouteAsTransforms().
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Patrol",
		meta=(DisplayName="Get Patrol Wait Times"))
	TArray<float> GetPatrolWaitTimes() const;

	/**
	 * Returns the owning territory volume resolved from an authored reference, tag, or proximity.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn",
		meta=(DisplayName="Get Owning Territory"))
	ATerritoryVolume* GetOwningTerritory() const;

	// ─── P1-07: Effective Configuration Getters ───
	// GuardPostDefinition overrides inline values when assigned, except active capacity:
	// each spawn point is always exactly one active combat slot.

	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	int32 GetEffectiveMaxGuards() const;

	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	int32 GetEffectiveReserveSlots() const;

	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	float GetEffectiveReserveSpawnDelay() const;

	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	float GetEffectiveReserveRetryInterval() const;

	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	float GetEffectiveReserveRadius() const;

	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	float GetEffectiveMinimumPlayerDistance() const;

	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	int32 GetEffectiveCandidateCount() const;

	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	FGameplayTag GetEffectiveFactionOverride() const;

	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	const TArray<FTerritoryPatrolNode>& GetEffectivePatrolRoute() const;

	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	bool GetEffectiveLoopPatrol() const;

	/**
	 * Owner reserves may deploy while securely claimed or while physically defending
	 * an opposing faction's active contest. Locked/unclaimed/self-contested states reject it.
	 */
	static bool IsOwnerReserveDeploymentStateValid(ETerritoryState State,
		const FGameplayTag& OwningFaction, const FGameplayTag& ContestingFaction);

	/** Deterministically selects the most specific Territory from placement/patrol hits. */
	static ATerritoryVolume* ChooseMostSpecificTerritory(
		TConstArrayView<ATerritoryVolume*> Candidates);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Late-binding handler for when a territory registers after this spawn point. */
	UFUNCTION()
	void OnTerritoryRegistered(class ATerritoryVolume* Territory, bool bIsNew);

#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
#endif

	UPROPERTY(BlueprintReadOnly, Category="Territory|GuardSpawn", meta=(DisplayName="Cached Territory"))
	TWeakObjectPtr<ATerritoryVolume> CachedTerritory;

	UPROPERTY()
	TArray<TWeakObjectPtr<ATerritoryGuardCharacter>> ActiveGuards;

	/** Saved across sessions so depleted reserves don't reset to full on load. */
	UPROPERTY(SaveGame)
	int32 CurrentReserveCount = 0;

	/** Saved across sessions so pending reserve requests survive save/load. */
	UPROPERTY(SaveGame)
	int32 PendingReserveSpawns = 0;

	/** Count of active guards at save time — restored on load to avoid recounting. */
	UPROPERTY(SaveGame)
	int32 SavedActiveGuardCount = 0;

	FTimerHandle ReserveSpawnTimer;

	// ─── Editor Visualization ───

	UPROPERTY(EditDefaultsOnly, Category="Territory|GuardSpawn|Visual",
		meta=(DisplayName="Show Patrol Route In Editor"))
	bool bShowPatrolRouteInEditor = true;

	UPROPERTY(EditDefaultsOnly, Category="Territory|GuardSpawn|Visual",
		meta=(DisplayName="Spawn Point Color"))
	FLinearColor SpawnPointColor = FLinearColor(0.f, 1.f, 0.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category="Territory|GuardSpawn|Visual",
		meta=(DisplayName="Patrol Route Color"))
	FLinearColor PatrolRouteColor = FLinearColor(1.f, 1.f, 0.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category="Territory|GuardSpawn|Visual",
		meta=(DisplayName="Reserve Color"))
	FLinearColor ReserveColor = FLinearColor(0.f, 0.5f, 1.f, 1.f);

	// ─── P0-06: Persistence ───

	/** Baked GUID for save/load. Set at editor placement time, not at runtime. */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Territory|GuardSpawn|Persistence",
		meta=(DisplayName="Spawn Point GUID (auto-generated)"))
	FGuid SpawnPointGUID;

	/** Whether reserve state was loaded from save (prevents reset on reconcile). */
	bool bLoadedFromSave = false;

	/** Ensure GUID is baked at editor time. */
	void EnsurePersistentSpawnPointGUID();

private:
	friend class ATerritoryVolume;

	/** Bind from a territory's authored GuardSpawnPoints array, which overrides proximity. */
	void BindToTerritory(ATerritoryVolume* Territory);
	void SetResolvedTerritory(ATerritoryVolume* Territory);
	ATerritoryVolume* FindPlacementOrPatrolTerritory(
		class UTerritoryRegistrySubsystem* Registry) const;
	void ResolveOwningTerritory();
	void InitializeReserves();
	void QueueReserveSpawn();
	void ScheduleAutomaticReserveSpawn(float Delay);
	void TryAutomaticReserveSpawn();
	/** @param bRequireCameraAvoidance If true, spawn location must be outside player camera frustums. */
	bool TrySpawnReserveGuard(bool bRequireCameraAvoidance);
	void CancelPendingReserveSpawns();

	/**
	 * P1-03: Called when the owning territory changes ownership.
	 * Applies reserve ownership policy only for OwnerChanged/RevertedToUnclaimed reasons.
	 */
	void HandleOwnershipTransition(EOwnershipTransitionReason Reason);

	/** Reset reserves to initial state — called during BeginPlay/load reconcile only. */
	void ResetReserveState();
};
