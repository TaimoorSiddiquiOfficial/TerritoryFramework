#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryAssetPreload.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"

#include "Spawn/TerritorySpawnTypes.h"

#include "TerritorySpawnSubsystem.generated.h"

class ATerritorySpawnAnchor;
class APlayerController;

/**
 * Fires whenever the registry gains or loses an anchor. BlueprintAssignable needs a DYNAMIC multicast
 * delegate: a plain C++ FSimpleMulticastDelegate is not a UHT-known type and cannot carry a UPROPERTY
 * at all. Same shape as UTerritoryRegistrySubsystem's OnTerritoryRegistered.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSpawnRegistryChangedSignature);

/**
 * Scheduling, indexing and leasing for every spawn anchor in the world. It owns NO entity state and
 * NO save data: the anchor's record is the only truth, so this subsystem can be thrown away and
 * rebuilt at any moment without losing anything durable.
 *
 * Cost model, deliberately matching the rest of the plugin:
 *   - Zero Tick. One repeating timer drives the sweep and is cleared in Deinitialize.
 *   - Zero world scans. Anchors register in BeginPlay and unregister in EndPlay, so there is no
 *     TActorIterator and no GetAllActorsOfClass anywhere in this file.
 *   - Bounded per-sweep work. Anchors are split into hash buckets by their durable GUID and each
 *     sweep evaluates one bucket, so adding anchors lengthens the per-anchor period rather than
 *     raising the per-frame cost.
 *
 * Determinism: evaluation order is AnchorGuid-ascending inside a bucket, never map order, so the
 * same world produces the same decisions in the same sequence on every run.
 */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritorySpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// ═══════════════════════════════════════════════════════════════════════════
	// Registration
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Register a placed anchor. Called from ATerritorySpawnAnchor::BeginPlay. Rejects an anchor with no
	 * baked GUID, and rejects a second anchor claiming a GUID that is already registered: a duplicate
	 * identity would make the saved record ambiguous.
	 */
	bool RegisterAnchor(ATerritorySpawnAnchor* Anchor);

	/** Unregister a placed anchor. Safe on null and on an anchor that was never registered. */
	void UnregisterAnchor(ATerritorySpawnAnchor* Anchor);

	UFUNCTION(BlueprintPure, Category = "Spawn|Subsystem")
	int32 GetAnchorCount() const { return AnchorsByGuid.Num(); }

	UFUNCTION(BlueprintPure, Category = "Spawn|Subsystem")
	ATerritorySpawnAnchor* FindAnchor(const FGuid& AnchorGuid) const;

	/** Every registered anchor, ordered by AnchorGuid. Never map order. */
	UFUNCTION(BlueprintPure, Category = "Spawn|Subsystem")
	TArray<ATerritorySpawnAnchor*> GetAllAnchors() const;

	/** Every registered anchor whose policy reports this entity tag. */
	UFUNCTION(BlueprintPure, Category = "Spawn|Subsystem")
	TArray<ATerritorySpawnAnchor*> GetAnchorsByEntityTag(FGameplayTag EntityTag) const;

	/** Anchors whose durable OwnerKey matches this player name. Used by the post-login claim pass. */
	UFUNCTION(BlueprintPure, Category = "Spawn|Subsystem")
	TArray<ATerritorySpawnAnchor*> GetAnchorsOwnedBy(FName PlayerName) const;

	/**
	 * The anchor that currently tracks this actor, or null when no slot owns it.
	 *
	 * A linear scan over the registry. Kept as a Blueprint-callable diagnostic: "which slot thinks it
	 * owns this car" is the first question worth asking when two anchors disagree about a level. The
	 * alternative — a reverse map kept in sync with every SetTrackedActor — would be a second copy of a
	 * fact the anchors already hold, and a stale copy of it silently grants two slots one actor.
	 *
	 * A linear scan rather than a cached map is acceptable because the question is asked by a human
	 * debugging a level or by an editor validator, never per frame.
	 */
	UFUNCTION(BlueprintPure, Category = "Spawn|Subsystem")
	ATerritorySpawnAnchor* FindAnchorOwningActor(const AActor* Actor) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// Leases
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Pin a slot so its own policy cannot retire the occupant. Server-only. A lease always carries an
	 * expiry, because a lease that outlives its holder is a permanently blocked slot.
	 *
	 * Returns an invalid GUID on failure (no world, bad arguments).
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Spawn|Lease")
	FGuid AcquireLease(FGuid AnchorGuid, UObject* Holder, FGameplayTag Purpose, float DurationSeconds);

	/** Release exactly the lease that was granted. A stale or unknown id is a no-op. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Spawn|Lease")
	bool ReleaseLease(FGuid LeaseId);

	/**
	 * Release every lease on one slot, whatever their ids.
	 *
	 * This exists because the common intent is "unpin this slot", not "unpin this lease". A caller that
	 * had to remember a LeaseId would leak the pin on any early-out, and an un-released lease blocks the
	 * slot until it expires. Returns how many leases were released.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Spawn|Lease")
	int32 ReleaseLeasesForAnchor(FGuid AnchorGuid);

	/** Release every lease a holder still has. Called when a mission ends or its owner goes away. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Spawn|Lease")
	int32 ReleaseLeasesForHolder(UObject* Holder);

	/** True while at least one live lease pins this slot. */
	UFUNCTION(BlueprintPure, Category = "Spawn|Lease")
	bool HasLiveLease(FGuid AnchorGuid, double NowGameTime) const;

	/** Live leases on one slot, for diagnostics. */
	UFUNCTION(BlueprintPure, Category = "Spawn|Lease")
	TArray<FTerritorySpawnLease> GetLeasesForAnchor(FGuid AnchorGuid, double NowGameTime) const;

	/**
	 * Evaluate one anchor right now instead of waiting for its bucket turn. Server-only. The same
	 * interlocks run, so this cannot be used to force a transition past a check.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Spawn|Subsystem")
	void RequestImmediateEvaluation(ATerritorySpawnAnchor* Anchor);

	/** Total evaluations performed, for tests and cost instrumentation. Monotonic. */
	uint64 GetEvaluationCount() const { return EvaluationCount; }

	/** Number of buckets the sweep is split into. */
	UFUNCTION(BlueprintPure, Category = "Spawn|Subsystem")
	int32 GetBucketCount() const { return SweepBuckets; }

	UPROPERTY(BlueprintAssignable, Category = "Spawn|Subsystem")
	FOnSpawnRegistryChangedSignature OnSpawnRegistryChanged;

private:
	/** One sweep pass: evaluate the next bucket. */
	void RunSweep();

	/** Evaluate a single anchor through the policy, then the interlocks. */
	void EvaluateAnchor(ATerritorySpawnAnchor* Anchor, double NowGameTime);

	/** Drop leases whose holder is gone or whose expiry has passed, before any decision reads them. */
	void ExpireStaleLeases(double NowGameTime);

	/**
	 * Bind unclaimed owner-keyed slots to the players who are connected right now.
	 *
	 * Deliberately driven by the sweep rather than by the game mode's PostLogin. An anchor registers in its
	 * own BeginPlay, and a World Partition anchor registers again every time its cell streams back in —
	 * neither of which is ordered against a login. A PostLogin handler would have to replay itself for
	 * every anchor that appeared later, and that replay is a second code path with its own bugs. One sweep
	 * pass sees the truth as it is now, for the same reason the interlock lives here.
	 *
	 * Only anchors with a non-empty OwnerKey are visited, so this costs one pass over the personal bays
	 * rather than one pass over the whole registry.
	 */
	void ReconcilePlayerClaims();

#if WITH_DEV_AUTOMATION_TESTS
public:
	/**
	 * Run one ownership reconcile pass outside the timer.
	 *
	 * The sweep is a timer, and a test cannot wait half a second per pass without becoming flaky on a
	 * loaded machine. Exposing the pass itself keeps the test measuring the shipping implementation
	 * rather than a duplicate of it.
	 */
	void ReconcilePlayerClaimsForTesting() { ReconcilePlayerClaims(); }

private:
#endif

	/** Registry keyed by durable GUID. Weak so a streamed-out anchor cannot be kept alive by the index. */
	TMap<FGuid, TWeakObjectPtr<ATerritorySpawnAnchor>> AnchorsByGuid;

	/** Entity tag to the anchors that produce it, refreshed on registration. */
	TMap<FGameplayTag, TArray<TWeakObjectPtr<ATerritorySpawnAnchor>>> AnchorsByEntityTag;

	/** Bucketed weak anchors. Assignment is a hash of the durable GUID, so it survives save/load. */
	TArray<TArray<TWeakObjectPtr<ATerritorySpawnAnchor>>> SweepBucketsStorage;

	/** Live leases keyed by lease id. Transient by definition and by design. */
	TMap<FGuid, FTerritorySpawnLease> Leases;

	FTimerHandle SweepTimerHandle;

	int32 CurrentBucket = 0;

	/** Ten buckets at a 0.5 s tick gives every anchor a 5 s period. */
	int32 SweepBuckets = 10;

	uint64 EvaluationCount = 0;

	/** Guards re-entrancy: an executor may synchronously destroy an actor that calls back into here. */
	bool bSweeping = false;

	/**
	 * Async class residency for spawns that would otherwise stall the frame.
	 *
	 * Polled from the sweep, never driven by a load delegate: a delegate fires at a moment nobody
	 * controls, and committing a spawn from inside one would run gameplay in the middle of a streaming
	 * update. Same contract and same helper as the rest of the plugin.
	 */
	FTerritoryAssetPreload ClassPreload;
};
