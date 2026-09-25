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
class UTerritoryDefinition;
// Forward-declared rather than included: TerritoryDefinition.h already reaches this header for the
// patrol node a guard-post row carries, so including it back would be circular. Only a reference is
// needed here; the floor claim plan's implementation is where the full type is used.
struct FTerritoryFloorTemplate;
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
 * A route may hold any number of nodes, including exactly one:
 *   - 1 node:  a single stop. The guard walks there, waits, then holds that spot.
 *   - 2+ nodes: a walk. The guard visits each in order, optionally looping back to Node0.
 *
 * A post patrols in place when it opts in via bUseSpawnTransformAsPatrolStop and authors no
 * route: the spawn point's own transform then becomes an implicit single stop, so a stationary
 * sentry needs no route authored. A post that authors neither has no patrol duty at all, which
 * is the authored way to say "this post does not patrol". See
 * ATerritoryGuardSpawnPoint::HasImplicitPatrolStop() and HasAnyPatrolDuty().
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
	 * Optional Guard.Activity tag naming the activity requested at this node
	 * (e.g. Guard.Activity.Inspect, Guard.Activity.Rest).
	 *
	 * NOT YET CONSUMED. Neither BPA_TerritoryPatrol nor BT_TerritoryPatrol reads this
	 * field, so setting it does not start an activity today. Recorded as gap 4 of
	 * Docs/GUARD_STORY_CONVERSATIONS.md.
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
 *   - A route may hold any number of nodes, including exactly one. A post with no route
 *     patrols in place only when it opts in via bUseSpawnTransformAsPatrolStop.
 *   - Guards spawned from this point access the route via ATerritoryGuardCharacter
 *     helpers (GetTerritoryPatrolRoute, HasTerritoryPatrolDuty, GetPatrolNodeCount).
 *   - Every unique spawn point contributes exactly one active combat slot.
 *   - Authored spawn points are authoritative. No random active-guard fallback or
 *     collision-driven relocation is allowed.
 *
 * Quick Blueprint Example:
 *   for spawn point in territory->GetGuardSpawnPoints():
 *     if spawn point->HasAvailableSlot() and spawn point->HasAnyPatrolDuty():
 *       spawn guard here -> configure spawn -> start patrol
 */
UCLASS(BlueprintType, Blueprintable, meta=(DisplayName="Territory Guard Spawn Point"))
class TERRITORYFRAMEWORK_API ATerritoryGuardSpawnPoint : public AActor, public INarrativeSavableActor
{
	GENERATED_BODY()
	friend class FTFBehavior_GuardSlotSaveMigration;

public:
	ATerritoryGuardSpawnPoint();
	virtual void Serialize(FArchive& Ar) override;

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

	/** Internal/editor synchronization hook. OnConstruction applies the serialized binding. */
	bool ApplyTerritoryDefinition();
	UTerritoryDefinition* GetTerritoryDefinition() const { return TerritoryDefinition; }
	FName GetGuardPostID() const { return GuardPostID; }

	/** Which authored floor this post stands on. Zero is ground; upper floors are positive. */
	int32 GetFloorIndex() const { return FloorIndex; }
	void SetDefinitionBinding(UTerritoryDefinition* NewDefinition, FName NewGuardPostID)
	{
		TerritoryDefinition = NewDefinition;
		GuardPostID = NewGuardPostID;
	}

	/**
	 * Which Place this spawn point belongs to. A Place Definition binding takes
	 * precedence, followed by this tag, then placement/patrol overlap with a Place.
	 * City and District bounds are ignored because aggregate parents own no guards.
	 * Easy example: a Blacksmith patrol post may sit outside the Blacksmith bounds when
	 * one patrol node overlaps that Place; overlapping only Market Square is invalid.
	 */
	UPROPERTY(Transient)
	FGameplayTag OwnerTerritoryTag;

	/**
	 * Which authored floor this post stands on, copied from its Definition row. Zero is
	 * ground; upper floors are positive. Ignored while the owning Place declares no floors.
	 * The Definition owns the assignment, so this stays transient.
	 */
	UPROPERTY(Transient)
	int32 FloorIndex = 0;

	/**
	 * Number of reserve guards that spawn on demand when active guards die.
	 * Set to 0 for no reserves. Typical value: 1-2.
	 */
	UPROPERTY(Transient)
	int32 ReserveSlots = 1;

	/** Automatically deploy queued reserves after a tracked guard dies. */
	UPROPERTY(Transient)
	bool bAutoSpawnReserves = true;

	/** Delay before the first automatic reserve deployment. */
	UPROPERTY(Transient)
	float ReserveSpawnDelay = 3.f;

	/** Retry interval while no camera-frustum-avoided, collision-free spawn location is available. */
	UPROPERTY(Transient)
	float ReserveSpawnRetryInterval = 2.f;

	/** Radius around this actor used to randomize automatic reserve placement. */
	UPROPERTY(Transient)
	float ReserveSpawnRadius = 600.f;

	/** Minimum distance automatic reserve spawns keep from every player camera. */
	UPROPERTY(Transient)
	float ReserveMinimumPlayerDistance = 500.f;

	/** Number of random navmesh candidates considered per automatic deployment attempt. */
	UPROPERTY(Transient)
	int32 ReserveSpawnCandidateCount = 12;

	/** Camera-safe attempts made before using the same authored post without camera avoidance. */
	UPROPERTY(Transient)
	int32 ReserveCameraAvoidanceRetryLimit = 3;

	/** Total failures before abandoning this queued deployment without consuming its reserve. */
	UPROPERTY(Transient)
	int32 ReserveTotalRetryLimit = 10;

	/**
	 * The patrol route this spawn point's guards walk through. Rebuilt at BeginPlay from the
	 * bound Place Definition row or GuardPostDefinition, which is why it is transient.
	 *
	 * One node is a valid single-stop route. An empty route means this post has no authored
	 * patrol duty unless bUseSpawnTransformAsPatrolStop opts in to the implicit stop.
	 * Guarded access via GetPatrolRoute() or HasPatrolRoute().
	 */
	UPROPERTY(Transient)
	TArray<FTerritoryPatrolNode> PatrolRoute;

	/** If true, the patrol loop returns to Node0 after the last node. */
	UPROPERTY(Transient)
	bool bLoopPatrol = true;

	/**
	 * Let this post patrol with no authored route: its own spawn transform becomes the guard's
	 * single patrol stop, so a stationary sentry needs nothing authored to get a patrol goal.
	 *
	 * Authored on the Place Definition guard-post row or on the nested Guard Post Definition,
	 * then copied here by ApplyTerritoryDefinition - the placed actor has no authoring surface
	 * of its own. Defaults false, so a post that never opted in keeps its previous behaviour of
	 * having no patrol goal at all. Ignored once any route node is authored: a route always wins.
	 */
	UPROPERTY(Transient)
	bool bUseSpawnTransformAsPatrolStop = false;

	/**
	 * Faction override. If invalid, the guard uses the territory owner's faction.
	 * Useful for "neutral" garrisons or captured territories.
	 */
	UPROPERTY(Transient)
	FGameplayTag FactionOverride;

	/** Higher-priority spawn points fill first when territory guards spawn. */
	UPROPERTY(Transient)
	int32 Priority = 50;

	/** P1-09: What happens to reserves when territory ownership changes. */
	UPROPERTY(Transient)
	EReserveOwnershipPolicy ReserveOwnershipPolicy = EReserveOwnershipPolicy::RefillOnOwnerChange;

	// ─── Guard Post Definition (Data Asset) ───

	/**
	 * Optional nested Guard Post Data Asset selected by the Place Definition row. The
	 * placed Blueprint cannot override it.
	 */
	UPROPERTY(Transient)
	TObjectPtr<class UTerritoryGuardPostDefinition> GuardPostDefinition;

	// ─── Narrative Overrides ───

	/** Optional NPC definition override for guards spawned from this point. Uses territory default if null. */
	UPROPERTY(Transient)
	TObjectPtr<UNPCDefinition> NPCDefinitionOverride;

	/** Optional activity configuration override. Uses territory default if null. */
	UPROPERTY(Transient)
	TObjectPtr<UNPCActivityConfiguration> ActivityConfigurationOverride;

	/** Optional trigger set overrides. Uses territory default if empty. */
	UPROPERTY(Transient)
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

	/** Return the number of reserve guards still waiting to deploy. */
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
	 * Returns every configured node, including a single-node route. An empty result means
	 * this post patrols its own spawn transform; see HasImplicitPatrolStop().
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Patrol",
		meta=(DisplayName="Get Patrol Route", CompactNodeTitle="Patrol Route"))
	TArray<FTerritoryPatrolNode> GetPatrolRoute() const;

	/**
	 * Returns true if this spawn point has an authored patrol route (1 or more nodes).
	 *
	 * A single node is a valid single-stop route. Use HasMultiStopPatrolRoute() when the
	 * caller needs a guard that actually covers ground, and HasAnyPatrolDuty() when it needs
	 * to know whether the guard has any patrol duty at all, authored or implicit.
	 *
	 * Example: if (spawn point->HasAnyPatrolDuty()) { RunPatrolActivity(); } else { StandIdle(); }
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Patrol",
		meta=(DisplayName="Has Patrol Route"))
	bool HasPatrolRoute() const;

	/**
	 * Returns true if this spawn point has a route that walks between stops (2 or more nodes).
	 *
	 * This is the deployment tie-break predicate: a post whose guard covers ground wins an
	 * equal-priority tie over one that holds a single spot. It is deliberately narrower than
	 * HasPatrolRoute() so that adding a single stop never reorders existing deployment.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Patrol",
		meta=(DisplayName="Has Multi-Stop Patrol Route"))
	bool HasMultiStopPatrolRoute() const;

	/**
	 * Returns true if this post opted in to patrolling in place and authored no route, so its
	 * own transform becomes the guard's single patrol stop.
	 *
	 * Requires GetEffectiveUseSpawnTransformAsPatrolStop(), so a post that simply has no route
	 * still reports false and gets no patrol goal. False once any node is authored: an authored
	 * route always takes precedence, so a designer who wants control over the stop's wait time
	 * or activity authors one node instead of relying on this.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Patrol",
		meta=(DisplayName="Has Implicit Patrol Stop"))
	bool HasImplicitPatrolStop() const;

	/**
	 * Returns the single stop used when this post authored no route: this actor's own
	 * transform, with the FTerritoryPatrolNode defaults for wait time and activity.
	 *
	 * Callers must check HasImplicitPatrolStop() first; when a route is authored this still
	 * returns a node, but it is not the route the guard walks.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Patrol",
		meta=(DisplayName="Get Implicit Patrol Stop"))
	FTerritoryPatrolNode GetImplicitPatrolStop() const;

	/**
	 * Returns true if this post's guards have any patrol duty: an authored route, or the
	 * implicit single stop on a post that opted in to it.
	 *
	 * This is the gate patrol AI should use. It is false for a post with neither, which is a
	 * deliberate static post and not an idle-guard bug.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Patrol",
		meta=(DisplayName="Has Any Patrol Duty"))
	bool HasAnyPatrolDuty() const;

	/**
	 * Returns whether this post opted in to using its spawn transform as a patrol stop.
	 *
	 * Prefers the post's own flag, then its GuardPostDefinition's, mirroring how the route
	 * itself resolves between the two authorities.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Patrol",
		meta=(DisplayName="Get Effective Use Spawn Transform As Patrol Stop"))
	bool GetEffectiveUseSpawnTransformAsPatrolStop() const;

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

	// ─── Deployment Order ───

	/**
	 * Sorts posts into the one order their Territory fills them in: higher Priority first, a post
	 * with a patrol route before one without, then by actor path name so the order is identical on
	 * every machine and every run.
	 *
	 * This is the single authority for "which post gets a guard first". SpawnGuardsToCount, both
	 * reinforcement paths and the editor's deployment-reachability check all call it, so a change
	 * here moves them together. It replaced three byte-identical comparators.
	 */
	static void SortForDeployment(TArray<ATerritoryGuardSpawnPoint*>& Posts);

	/**
	 * Whether a territory staffing itself toward TargetGuardCount reaches the post standing at
	 * PostIndex in the order SortForDeployment produces.
	 *
	 * SpawnGuardsToCount fills front to back, one guard per post (GetEffectiveMaxGuards is 1), and
	 * each pass restarts from the front - so guard k stands on the k-th post the fill accepts. A
	 * post below the target is therefore always reached; a post at or above it is reached only if
	 * enough earlier posts were refused, which is why the editor check passes the *accepted* count
	 * rather than the raw index.
	 *
	 * If a post ever holds more than one guard, this predicate and that fill loop change together.
	 */
	static bool IsReachedByDeploymentTarget(int32 PostIndex, int32 TargetGuardCount);

	/** One floor's share of the staffing target, reserved before the flat order is consulted. */
	struct FTerritoryFloorClaim
	{
		int32 FloorIndex = INDEX_NONE;
		int32 Guards = 0;
	};

	/**
	 * How the authored target is split between floors that claim a quota and the flat surplus.
	 *
	 * A floor row authoring DesiredGuards > 0 claims that many guards ahead of SortForDeployment's
	 * order, so a low-priority upper floor is staffed instead of losing every post to a
	 * high-priority ground floor. Floors are served in the order of their best-placed post, which is
	 * SortForDeployment's own comparison, so Priority still decides between floors.
	 *
	 * Shared, like the two helpers above it, because SpawnGuardsToCount spends this plan and the
	 * editor's deployment-reachability check predicts it. A second implementation of the budget walk
	 * is exactly how the rule that says "this floor can never be staffed" drifts away from the fill
	 * that staffs it.
	 *
	 * @param ResolvedFloorByRank  each deployable post's floor, in SortForDeployment order
	 * @param Floors               the Place's authored floor rows
	 * @param TargetGuardCount     the authored, clamped staffing target
	 * @param OutClaims            floors served ahead of the flat order, in service order
	 * @return                     the budget left for the flat surplus pass
	 */
	static int32 PlanFloorClaims(
		const TArray<int32>& ResolvedFloorByRank,
		const TArray<FTerritoryFloorTemplate>& Floors,
		int32 TargetGuardCount,
		TArray<FTerritoryFloorClaim>& OutClaims);

	// ─── P1-07: Effective Configuration Getters ───
	// The optional nested GuardPostDefinition supplies reusable defaults behind the
	// owning Place Definition row. Each post is always one active combat slot.

	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	int32 GetEffectiveMaxGuards() const;

	/** Return reserve capacity after applying the active post configuration. */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	int32 GetEffectiveReserveSlots() const;

	/** Return seconds before the first reserve placement attempt after applying overrides. */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	float GetEffectiveReserveSpawnDelay() const;

	/** Return seconds between reserve placement retries after applying overrides. */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	float GetEffectiveReserveRetryInterval() const;

	/** Return the effective reserve placement search radius in centimetres. */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	float GetEffectiveReserveRadius() const;

	/** Return the effective reserve-to-player-camera minimum distance in centimetres. */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	float GetEffectiveMinimumPlayerDistance() const;

	/** Return reserve placement candidates per attempt after applying the active post configuration. */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	int32 GetEffectiveCandidateCount() const;

	/** Return the effective guard faction override; an empty tag means use the Territory owner. */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	FGameplayTag GetEffectiveFactionOverride() const;

	/** Return the patrol stops after applying the assigned post and definition overrides. */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	const TArray<FTerritoryPatrolNode>& GetEffectivePatrolRoute() const;

	/** Return whether the effective patrol route loops back to its first stop. */
	UFUNCTION(BlueprintPure, Category="Territory|GuardSpawn|Effective")
	bool GetEffectiveLoopPatrol() const;

	/**
	 * Owner reserves may deploy while securely claimed or while physically defending
	 * an opposing faction's active contest. Locked/unclaimed/self-contested states reject it.
	 */
	static bool IsOwnerReserveDeploymentStateValid(ETerritoryState State,
		const FGameplayTag& OwningFaction, const FGameplayTag& ContestingFaction);

	/** Deterministically selects the smallest physical Place from placement/patrol hits. Aggregate City/District hits are ignored. */
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

	UPROPERTY(Transient)
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
	int32 AutomaticReserveSpawnFailures = 0;

	// ─── P0-06: Persistence ───

	/** Baked GUID for save/load. Set at editor placement time, not at runtime. */
	UPROPERTY(SaveGame)
	FGuid SpawnPointGUID;

	/** Set by initial provisioning or Narrative load; zero then means exhausted. */
	bool bReserveStateInitialized = false;

	/** Native loaded a durable slot record; an empty saved slot is not a fresh hire. */
	bool bLoadedFromSave = false;

	/** Ensure GUID is baked at editor time. */
	void EnsurePersistentSpawnPointGUID();

private:
	friend class ATerritoryVolume;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTFGuardReserveReconciliation;
	friend class FTFGuardRetirementCallbacks;
	friend class FTFGuardSpawnAdmissionCallbacks;
	friend class FTFGarrisonPurchaseCallbacks;
	friend class FTFGuardReserveTotals;
	friend class FTFIndependentGuardPostStreaming;
	// Floor staging tests share one fixture, so the reserve seam lives in one class.
	friend class FTFTerritoryFloorTestAccess;
	// The floor-quota deployment test deploys through the production binding (BindToTerritory),
	// which is the private call BeginPlay makes for each authored post.
	friend class FTFFloorQuotaDeployment;
#endif

	/** Hidden serialized binding maintained by the Definition synchronizer. */
	UPROPERTY()
	TObjectPtr<UTerritoryDefinition> TerritoryDefinition;

	UPROPERTY()
	FName GuardPostID;

	/** Bind from a territory's authored GuardSpawnPoints array, which overrides proximity. */
	void BindToTerritory(ATerritoryVolume* Territory);
	void SetResolvedTerritory(ATerritoryVolume* Territory);
	ATerritoryVolume* FindPlacementOrPatrolTerritory(
		class UTerritoryRegistrySubsystem* Registry) const;
	void ResolveOwningTerritory();
	/** Registry absence pauses a tagged post without erasing its saved occupants. */
	bool IsWaitingForOwningTerritory() const;
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

	/** Reconcile registrations without refilling an already initialized reserve. */
	void ResetReserveState();
};
