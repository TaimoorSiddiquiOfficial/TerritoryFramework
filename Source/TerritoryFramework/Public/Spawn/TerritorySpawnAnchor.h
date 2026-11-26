#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NarrativeSavableActor.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

#include "Spawn/TerritorySpawnTypes.h"

#include "TerritorySpawnAnchor.generated.h"

class UBillboardComponent;
class USceneComponent;
class UTerritorySpawnExecutor;
class UTerritorySpawnPolicy;

/**
 * A placed, save-backed slot that owns exactly one spawned entity.
 *
 * The anchor is the only durable truth for its slot. It owns identity (a baked GUID), who the slot
 * belongs to (OwnerKey), the policy that decides, the executor that acts, and the record that
 * survives a save. It does NOT schedule, index, or lease: that is the subsystem's job, so this class
 * stays testable in isolation and streamable with its cell.
 *
 * One anchor is one slot, deliberately copied from the guard-post invariant: a cluster of bays is a
 * cluster of anchors, never one anchor with an array. Anything that needs two things in one place
 * is a policy variant question, not a second occupant.
 *
 * Placement guidance for level designers:
 *   - The authored transform is a request, not a promise. A consumer may project Z onto navigation
 *     or snap to the floor; it may never move the authored X/Y.
 *   - A blocked authored position defers the spawn. Nothing is silently relocated.
 *   - AnchorGuid is baked on edit and always regenerated on duplicate, so a copied anchor cannot
 *     alias its original's saved record.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Territory Spawn Anchor"))
class TERRITORYFRAMEWORK_API ATerritorySpawnAnchor : public AActor, public INarrativeSavableActor
{
	GENERATED_BODY()

public:
	ATerritorySpawnAnchor();

	virtual void Serialize(FArchive& Ar) override;

	// ─── INarrativeSavableActor ───
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& InGUID) override;
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	virtual bool ShouldRespawn_Implementation() const override;

#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#endif

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// Identity
	// ═══════════════════════════════════════════════════════════════════════════

	/** Durable slot identity. Baked on edit, never hand-editable, always regenerated on duplicate. */
	UFUNCTION(BlueprintPure, Category = "Spawn|Anchor")
	FGuid GetAnchorGuid() const { return AnchorGuid; }

	/** Generate the durable GUID if this anchor does not have one yet. Editor bake path. */
	void EnsurePersistentAnchorGUID();

	/**
	 * Who this slot belongs to, as a durable key rather than an actor reference. The project's player
	 * save key is the player name, so that is what the personal-car slot stores. Empty means a
	 * world-owned slot such as a hospital ambulance bay.
	 */
	UFUNCTION(BlueprintPure, Category = "Spawn|Anchor")
	FName GetOwnerKey() const { return OwnerKey; }

	/** Player name that has claimed this slot, or None while it is unclaimed. Saved so a claim survives a restart. */
	UFUNCTION(BlueprintPure, Category = "Spawn|Anchor")
	FName GetClaimedByPlayerName() const { return ClaimedByPlayerName; }

	/** Server-only claim. A claim is advisory to the policy; the record remains the slot's truth either way. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Spawn|Anchor")
	bool TryClaimForPlayer(FName InPlayerName);

	/** Server-only release. Never deletes the record; only the claim moves. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Spawn|Anchor")
	void ReleaseClaim();

	// ═══════════════════════════════════════════════════════════════════════════
	// Configuration
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Instanced policy: the decision layer. Null is legal and inert; the anchor logs once and does
	 * nothing, which is how a freshly placed anchor behaves before it is configured.
	 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Spawn|Configuration",
		meta = (DisplayName = "Policy"))
	TObjectPtr<UTerritorySpawnPolicy> Policy;

	/**
	 * Instanced executor: the action layer. Normally derived from the policy, so this field exists for
	 * the cases where one policy serves several creation paths, not for hand-wiring a mismatch.
	 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Spawn|Configuration",
		meta = (DisplayName = "Executor", ToolTip = "Leave empty to use the executor declared by the policy."))
	TObjectPtr<UTerritorySpawnExecutor> ExecutorOverride;

	/**
	 * The actor class this slot creates when the policy has no opinion. Soft on purpose: a record must
	 * never drag a heavy vehicle Blueprint into memory merely because the anchor is loaded.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Spawn|Configuration",
		meta = (DisplayName = "Actor Class"))
	TSoftClassPtr<AActor> ActorClass;

	/** Human-readable slot name for logs and HUD, for example Safehouse_Driveway or Hospital_Bay_01. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Configuration",
		meta = (DisplayName = "Slot ID"))
	FName SlotID;

	/*
	 * Spatial loading is deliberately NOT declared here.
	 *
	 * AActor already owns it: bIsSpatiallyLoaded (Actor.h:1330, editor-only data, defaults to true in
	 * the AActor constructor at Actor.cpp:363) and the Is Spatially Loaded checkbox under the
	 * WorldPartition category. A framework member of the same meaning would be a second authority for
	 * one flag: the designer could set the anchor resident while the framework still believed it
	 * streamed, and nothing would report the disagreement.
	 *
	 * The authoring rule therefore lives in the validator and in docs, and the runtime only READS the
	 * engine's value for diagnostics. Vehicle bays are commonly set resident because an anchor carries
	 * no art; tree chunk anchors are commonly streamed because they carry instances. Either is a level
	 * authoring decision, not a class default.
	 */

	// ═══════════════════════════════════════════════════════════════════════════
	// Runtime state (transient, never saved)
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * The executor this anchor actually uses, or null until ResolveLayers has succeeded.
	 *
	 * This is a plain accessor on purpose. The earlier version of this method built the executor by
	 * calling GetDefaultObject on the policy's declared class, which handed every anchor in the world
	 * the same class-default object. That is wrong in a way that would not crash: two anchors sharing
	 * one executor instance means any executor-side scratch state (retry counters, a pending handle, a
	 * cached class) is silently shared between unrelated slots. Construction happens in ResolveLayers,
	 * which is allowed to allocate.
	 */
	UFUNCTION(BlueprintPure, Category = "Spawn|Runtime")
	UTerritorySpawnExecutor* GetExecutor() const { return ResolvedExecutor; }

	UFUNCTION(BlueprintPure, Category = "Spawn|Runtime")
	UTerritorySpawnPolicy* GetPolicy() const { return Policy; }

#if WITH_DEV_AUTOMATION_TESTS
	/**
	 * Replace the instanced policy from native code.
	 *
	 * Exists because Policy is Instanced and only assignable in the editor or through a Blueprint
	 * out-of-line setter, and a behavioural test has to build a configured slot at runtime. Guarded by
	 * WITH_DEV_AUTOMATION_TESTS so it is compiled out of a shipping build and cannot become a second
	 * way for gameplay code to swap a slot's purpose behind the anchor's back.
	 *
	 * Resolves the layers immediately so a test does not have to simulate BeginPlay ordering.
	 */
	void SetPolicyForTesting(UTerritorySpawnPolicy* InPolicy);

	/**
	 * Set the durable owner key from native code.
	 *
	 * OwnerKey is EditAnywhere but private, so a runtime test cannot assign it. Without this the ownership
	 * claim path — the one that decides which player a personal-car bay belongs to — could only ever be
	 * exercised with an empty key, which is the case that proves nothing.
	 */
	void SetOwnerKeyForTesting(FName InOwnerKey);

	/**
	 * Write a record into the slot and run the real load path over it.
	 *
	 * Exists because the load path is the one place where "the record promises an entity" and "the entity
	 * was destroyed" arrive in the same shape, and forcing SlotRecord from a test would prove the test's
	 * own write rather than the anchor's reading of it. This drives Load_Implementation, so the state the
	 * sweep later sees is the state a real save would have produced.
	 */
	void SimulateLoadedRecordForTesting(bool bHadInstance, bool bWasDestroyed);
#endif

	UFUNCTION(BlueprintPure, Category = "Spawn|Runtime")
	ESpawnLifecycleState GetLifecycleState() const { return LifecycleState; }

	UFUNCTION(BlueprintPure, Category = "Spawn|Runtime")
	AActor* GetSpawnedActor() const { return SpawnedActor.Get(); }

	/** True when this anchor is configured well enough to run. */
	UFUNCTION(BlueprintPure, Category = "Spawn|Runtime")
	bool IsAnchorConfigured() const;

	/** True while the anchor must not be touched: not begun play, tearing down, or loading a save. */
	bool IsEvaluationBlocked() const;

	/**
	 * Resolve the executor once, at BeginPlay and whenever the policy is changed in the editor.
	 * Returns false, with an error, when the policy and executor do not agree.
	 */
	bool ResolveLayers();

	/**
	 * True while this anchor is still being filled in by a designer rather than running.
	 *
	 * Exists so ResolveLayers can report an incomplete policy at a severity that matches the moment.
	 * An unconfigured anchor sitting in the editor is a workflow state the engine itself creates,
	 * not a fault; the same incomplete policy at BeginPlay is a real authoring error and is loud.
	 * Without this distinction every intermediate details-panel edit wrote a red line into the log,
	 * which is a release gate — noise there hides the failures that matter.
	 */
	bool IsInAuthoringDraft() const;

	// ═══════════════════════════════════════════════════════════════════════════
	// Durable record
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * The only durable truth this anchor holds. Mutated only by framework commit points.
	 *
	 * Named SlotRecord rather than Record because UHT's generated structured-archive serializer for this
	 * class declares a local called Record, and a same-named member makes that generated file fail to
	 * compile with C4458. The Blueprint-facing name is unaffected: it is a UPROPERTY on GetRecord().
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Spawn|Record")
	FTerritorySpawnRecord SlotRecord;

	/** Read-only view for diagnostics and HUD. */
	UFUNCTION(BlueprintPure, Category = "Spawn|Record")
	const FTerritorySpawnRecord& GetRecord() const { return SlotRecord; }

	/**
	 * Commit a lifecycle transition. Framework-internal: every caller has already passed the safety
	 * interlocks. Kept public so the subsystem and the tests can drive the same path the policy does,
	 * never so Blueprint can bypass a check.
	 */
	void CommitTransition(ESpawnLifecycleState NewState, ESpawnTransitionReason Reason, double NowGameTime);

	/** Point the anchor at its tracked actor. Framework-internal. */
	void SetTrackedActor(AActor* InActor);

	/**
	 * Force the slot back to dormant and clear the record's per-instance half. Used by load reconcile and admin resets.
	 *
	 * bTreatAsDestroyed exists because the two ways a slot empties are NOT the same fact. A framework
	 * retire already recorded `bWasDestroyed` itself before calling in. An entity that vanished on its
	 * own — a car wrecked by gameplay, a tree felled by a player, an actor removed by a level stream —
	 * arrives here with a record that still claims an instance, and wiping that silently would make
	 * `InitialSpawn` ("create once, stay gone") indistinguishable from "never spawned", so the prop
	 * would quietly come back. The caller that OBSERVED the disappearance passes true.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Spawn|Record")
	void ResetToDormant(ESpawnRemovalReason Reason, bool bTreatAsDestroyed = false);

	/** Game time the slot entered PendingSpawn, or -1 when it is not pending. */
	double GetPendingSinceGameTime() const { return PendingSinceGameTime; }

	/** Consecutive failed create attempts, reset on a successful spawn. */
	int32 GetConsecutiveFailures() const { return ConsecutiveFailures; }

	/** Record one failed create attempt and return the new count. Framework-internal. */
	int32 RegisterSpawnFailure();

	/** Record one successful create and reset the retry budget. Framework-internal. */
	void RegisterSpawnSuccess();

	// ═══════════════════════════════════════════════════════════════════════════
	// Owner binding (post-login claim)
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Bind this slot to a player by durable name. The framework calls this after PostLogin; a policy
	 * can also call it from a custom claim flow. Returns false when the slot already belongs to
	 * somebody else, and never steals a live claim.
	 */
	bool BindOwnerKeyToPlayer(FName InPlayerName);

	/** True when OwnerKey is set and matches this player name. */
	UFUNCTION(BlueprintPure, Category = "Spawn|Anchor")
	bool IsOwnedByPlayer(FName InPlayerName) const;

private:
	// ─── Identity ───
	//
	// These three are private on purpose, and therefore carry no Blueprint specifier: UHT rejects
	// BlueprintReadOnly on a private member. Blueprint reaches them through the BlueprintPure accessors
	// above (GetAnchorGuid / GetOwnerKey / GetClaimedByPlayerName), which is the only supported path
	// and keeps the write side out of a designer's reach.

	/** Baked durable slot GUID. Visible so it can be read and cited, never editable so it cannot be hand-typed. */
	UPROPERTY(VisibleAnywhere, SaveGame, Category = "Spawn|Anchor")
	FGuid AnchorGuid;

	/** Who this slot belongs to. Empty for a world-owned slot. Not SaveGame: the claim below is the durable half. */
	UPROPERTY(EditAnywhere, Category = "Spawn|Anchor")
	FName OwnerKey;

	/** Durable player claim, set on login and cleared on release. Name-keyed because that is the project's existing player save key. */
	UPROPERTY(VisibleAnywhere, SaveGame, Category = "Spawn|Anchor")
	FName ClaimedByPlayerName;

	// ─── Runtime ───

	UPROPERTY(Transient)
	TObjectPtr<UTerritorySpawnExecutor> ResolvedExecutor;

	UPROPERTY(Transient)
	ESpawnLifecycleState LifecycleState = ESpawnLifecycleState::Dormant;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> SpawnedActor;

	double PendingSinceGameTime = -1.0;
	int32 ConsecutiveFailures = 0;

	/** Set while this anchor is being removed by streaming, so EndPlay saves instead of clearing. */
	bool bRetiringForStreaming = false;

	/** True once BeginPlay has run and the layers resolved. Guards every cross-frame access. */
	bool bAnchorReady = false;

	void RegisterWithSubsystem();
	void UnregisterFromSubsystem();
};
