#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NarrativeSavableActor.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryInterfaces.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "TerritoryVolume.generated.h"

class UNarrativeAbilitySystemComponent;
class UShapeComponent;
class UNPCDefinition;
class UNPCActivityConfiguration;
class UTriggerSet;
class ATerritoryGuardCharacter;
class ATerritoryGuardSpawnPoint;
class UTerritoryNavigationMarkerComponent;
class UTerritoryCounterAttackProfile;
class UTerritoryDefinition;
class UTerritoryStealthProfile;
/**
 * Base class for all territory volumes (City / District / Property inherit from this).
 *
 * Provides:
 *   - Ownership + state (replicated via FTerritoryOwnershipData)
 *   - Guard spawning via GuardNPCDefinition or per-faction FactionGuardDefinitions
 *   - Lock system with Narrative conditions
 *   - Blueprint events (OnOwnershipChanged, OnStateChanged, OnAllGuardsDefeated)
 *
 * Quick start:
 *   1. Create Actor in editor -> class = TerritoryProperty / District / City
 *   2. Set TerritoryTag (e.g., Territory.Marketplace)
 *   3. Set InitialOwningFaction (e.g., Narrative.Factions.Merchants)
 *   4. Optionally assign GuardNPCDefinition + set GuardSpawnCount for auto-spawn
 *
 * Blueprint usage (read-only queries are BlueprintPure):
 *   Volume->GetOwningFaction()     -> "Narrative.Factions.Merchants"
 *   Volume->GetTerritoryState()    -> Claimed / Contested / Unclaimed
 *   Volume->GetTerritoryAvailability() -> Locked / Unlocked
 *   Volume->IsOwnedByFaction(Tag)  -> true/false
 *   Volume->GetControlProgress()   -> 0.0 to 1.0
 */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryVolume : public AActor, public INarrativeSavableActor, public ITerritoryOwnershipInterface, public ITerritoryEventReceiverInterface
{
	GENERATED_BODY()
	friend class UTerritoryDataValidator;
	friend class UTerritoryDefinition;
	friend class FTFBehavior_GuardRestoreCount;
	friend class FTFBehavior_TagBoundSpawnPointRegistration;
	friend class FTFFunctional_PlayerManagedGarrisonPolicy;
	friend class FTFPatrolOverlapAndDefenceFrontRegression;
	friend class FTFCounterAttackWorldPartitionTargetRebind;
	friend class FTerritoryUIPlayerLocationDistrictTest;
	friend class FTerritoryUIDistrictWaypointResolutionTest;
	friend class FTerritoryUILockedMarkerSilenceTest;
	friend class FTerritoryUIPlaceNarrativePOIBridgeTest;
	friend class FTerritoryUIPlaceFirstDiscoveryTest;

public:
	ATerritoryVolume();

	// ─── INarrativeSavableActor ───
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& NewGUID) override;
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	virtual bool ShouldRespawn_Implementation() const override;

	// ─── ITerritoryOwnershipInterface ───
	virtual FGameplayTag GetTerritoryOwner_Implementation() const override;
	virtual float GetTerritoryControlProgress_Implementation() const override;
	virtual bool IsTerritoryContested_Implementation() const override;
	virtual FGameplayTag GetContestingFaction_Implementation() const override;

	// ─── ITerritoryEventReceiverInterface ───
	virtual void OnTerritoryControlChanged_Implementation(FGameplayTag TerritoryTag, FGameplayTag OldOwner, FGameplayTag NewOwner) override;
	virtual void OnTerritoryContested_Implementation(FGameplayTag TerritoryTag, FGameplayTag ContestingFaction) override;
	virtual void OnTerritoryUncontested_Implementation(FGameplayTag TerritoryTag) override;
	virtual void OnTerritoryStateChanged_Implementation(FGameplayTag TerritoryTag, ETerritoryState NewState) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// Query API (all BlueprintPure — no exec pin, safe on any graph)
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Returns the faction that currently owns this territory.
	 * During a contest, returns the incumbent defending faction. Use
	 * IsOwnedByFaction() when you need active, uncontested ownership.
	 *
	 * Example:
	 *   if (Volume->GetOwningFaction() == Narrative.Factions.Merchants) { ShowMerchantUI(); }
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Ownership", meta=(DisplayName="Get Owning Faction"))
	FGameplayTag GetOwningFaction() const;

	/** Returns a copy of the current ownership data struct (for building mutation candidates). */
	FTerritoryOwnershipData GetOwnershipData() const { return OwnershipData; }

	UFUNCTION(BlueprintPure, Category="Territory|Ownership", meta=(DisplayName="Get Territory State"))
	ETerritoryState GetTerritoryState() const;

	UFUNCTION(BlueprintPure, Category="Territory|Lock", meta=(DisplayName="Get Territory Availability"))
	ETerritoryAvailability GetTerritoryAvailability() const { return OwnershipData.Availability; }

	/** True only when this Territory and every authored ancestor are unlocked. */
	UFUNCTION(BlueprintPure, Category="Territory|Lock",
		meta=(DisplayName="Is Available For Gameplay",
			ToolTip="Checks this Territory plus its City/District ancestor path. Missing or cyclic hierarchy data fails closed."))
	bool IsAvailableForGameplay() const;

	UFUNCTION(BlueprintPure, Category="Territory|Ownership", meta=(DisplayName="Get Control Progress"))
	float GetControlProgress() const;

	UFUNCTION(BlueprintPure, Category="Territory|Ownership", meta=(DisplayName="Is Contested"))
	bool IsContested() const;

	/**
	 * Returns true if the given faction currently owns this territory.
	 * Example: Volume->IsOwnedByFaction(Narrative.Factions.Heroes)
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Ownership", meta=(DisplayName="Is Owned By Faction"))
	bool IsOwnedByFaction(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintPure, Category="Territory|Ownership", meta=(DisplayName="Get Territory Tag"))
	FGameplayTag GetTerritoryTag() const;

	UFUNCTION(BlueprintPure, Category="Territory|Ownership", meta=(DisplayName="Get Display Name"))
	FText GetTerritoryDisplayName() const;

	/**
	 * Command perks granted by this Territory's active State Config to its current
	 * owner. Example: Claimed -> Guard Staffing grants the control only while held.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Command",
		meta=(DisplayName="Get Active Command Capabilities"))
	FGameplayTagContainer GetActiveCommandCapabilities() const;

	/** True when any State Config on this Territory authors the exact capability. */
	UFUNCTION(BlueprintPure, Category="Territory|Command",
		meta=(DisplayName="Is Command Capability Configured"))
	bool IsCommandCapabilityConfigured(const FGameplayTag& Capability) const;

	UFUNCTION(BlueprintPure, Category="Territory|Ownership", meta=(DisplayName="Get Max Attackers"))
	int32 GetMaxConcurrentAttackers() const;

	/**
	 * Returns the count of currently-alive defenders (guard NPCs or player-assigned).
	 * Note: does NOT count reserve guards at spawn points.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Get Defender Count"))
	int32 GetDefenderCount() const;

	UFUNCTION(BlueprintPure, Category="Territory|Economy", meta=(DisplayName="Get Periodic Income"))
	int32 GetPeriodicIncome() const;

	UFUNCTION(BlueprintPure, Category="Territory|Economy", meta=(DisplayName="Get Guard Cost"))
	int32 GetGuardCost() const;

	UFUNCTION(BlueprintPure, Category="Territory|Bounds", meta=(DisplayName="Get Territory Bounds"))
	FBox GetTerritoryBounds() const;

	/**
	 * Returns true if a world-space point lies inside this territory's bounds.
	 * Useful for checking which territory a player/NPC is in.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Bounds", meta=(DisplayName="Contains Point"))
	bool ContainsPoint(const FVector& WorldPoint) const;

	/**
	 * True when this independently capturable Place uses the complete Territory bounds
	 * for its story confrontation. A living player anywhere inside the bounds can hold
	 * the Place Contested, including upper floors, but never fills automatic capture
	 * progress. The owner dialogue, quest, or Territory Capture Event completes ownership.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Capture|Story",
		meta=(DisplayName="Uses Story Capture From Bounds"))
	bool UsesStoryCaptureFromBounds() const { return bStoryCaptureFromBounds; }

	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy", meta=(DisplayName="Get Parent Territory Tag"))
	FGameplayTag GetParentTerritoryTag() const;

	UFUNCTION(BlueprintPure, Category="Territory|Ownership", meta=(DisplayName="Get Initial Owning Faction"))
	FGameplayTag GetInitialOwningFaction() const;

	/** Resolves the Definition-owned Initial State setting for a new campaign. */
	UFUNCTION(BlueprintPure, Category="Territory|Ownership",
		meta=(DisplayName="Get Resolved Initial State",
			ToolTip="Preview the state used for a new campaign. Saved games keep their saved state."))
	ETerritoryState ResolveInitialTerritoryState() const;

	/** Resolves the Definition-owned Initial Availability for a new campaign. */
	UFUNCTION(BlueprintPure, Category="Territory|Lock",
		meta=(DisplayName="Get Resolved Initial Availability",
			ToolTip="Preview whether a new campaign starts locked. Saved games keep their saved availability."))
	ETerritoryAvailability ResolveInitialTerritoryAvailability() const;

	/**
	 * True on the authority when this runtime record was restored from a campaign save.
	 * Initial Definition values are new-campaign seeds and do not overwrite that record.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Save",
		meta=(DisplayName="Was Restored From Campaign Save"))
	bool WasRestoredFromCampaignSave() const { return bLoadedFromSave; }

	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy", meta=(DisplayName="Get Control Mode"))
	ETerritoryControlMode GetControlMode() const;

	/** Single authoring source for this City, District, or Place. Runtime state remains on this actor. */
	UFUNCTION(BlueprintPure, Category="Territory|Definition",
		meta=(DisplayName="Get Territory Definition"))
	UTerritoryDefinition* GetTerritoryDefinition() const { return TerritoryDefinition; }

	/** Runtime Narrative instances cloned from the assigned Definition asset. */
	const TMap<ETerritoryState, FTerritoryStateConfig>& GetStateConfigs() const;
	const TArray<TObjectPtr<class UNarrativeEvent>>& GetDefenderDiedEvents() const;
	const TArray<TObjectPtr<class UNarrativeEvent>>& GetAllDefendersDefeatedEvents() const;

	/** State override first, then Definition default. Empty keeps legacy immediate contesting. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth",
		meta=(DisplayName="Get Active Territory Stealth Profile"))
	UTerritoryStealthProfile* GetActiveStealthProfile() const;

	/** Internal/editor synchronization hook. OnConstruction applies the assigned asset automatically. */
	bool ApplyTerritoryDefinition();

	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	UTerritoryCounterAttackProfile* GetCounterAttackProfile() const { return CounterAttackProfile; }

	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	const TArray<FTerritoryAssaultApproach>& GetCounterAttackApproaches() const { return CounterAttackApproaches; }

	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	float GetGuardQuality() const { return GuardQuality; }

	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	float GetFortificationStrength() const { return FortificationStrength; }

	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	float GetNearbyAlliedSupport() const { return NearbyAlliedSupport; }

	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	float GetStrategicValue() const { return StrategicValue; }

	FGuid GetTerritoryGUID() const { return TerritoryGUID; }

	// ═══════════════════════════════════════════════════════════════════════════
	// Mutation API (BlueprintAuthorityOnly — server-only)
	// ═══════════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Ownership",
		meta=(DisplayName="Set Owning Faction",
			ToolTip="Validated server ownership change. Uses the Territory Control Subsystem so locks, diplomacy, state conditions, guards, capture cleanup, counterattacks, save snapshots, and replication stay synchronized. Use Apply Territory Mutation when you need a result or explicit context."))
	void SetOwningFaction(const FGameplayTag& NewFaction);

	/** Internal explicit-context ownership path used by synchronous hierarchy cascades. */
	void SetOwningFactionWithContext(const FGameplayTag& NewFaction,
		const FTerritoryTransitionContext& TransitionContext);

	/** Internal hierarchy path for aggregate-only City/District ownership. */
	void SetDerivedOwningFaction(const FGameplayTag& NewFaction);
	void SetDerivedOwningFaction(const FGameplayTag& NewFaction,
		const FTerritoryTransitionContext& TransitionContext);

	/** Internal hierarchy reducer path. Aggregate parents never change their children. */
	void SetDerivedControl(const FGameplayTag& SecuredOwner, ETerritoryState DerivedState,
		const FTerritoryTransitionContext& TransitionContext = FTerritoryTransitionContext());

	/** Valid only while an atomic transition is broadcasting its synchronous event bundle. */
	const FTerritoryTransitionContext& GetActiveTransitionContext() const { return ActiveTransitionContext; }

	/**
	 * Returns the owner that existed immediately before the active atomic transition.
	 * The result is intentionally empty outside the synchronous transition event bundle,
	 * so a story event cannot accidentally reuse stale ownership from an older capture.
	 *
	 * Easy example: Bandits own a Place, Rebels finish capturing it, and the Claimed
	 * row runs. Current Owner is Rebels while Transition Previous Owner is Bandits.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Ownership|Transition",
		meta=(DisplayName="Get Transition Previous Owner"))
	FGameplayTag GetTransitionPreviousOwningFaction() const
	{
		return bTransitionInProgress ? PreviousOwningFaction : FGameplayTag();
	}

	/** True only while state/ownership entry and exit events are executing. */
	UFUNCTION(BlueprintPure, Category="Territory|Ownership|Transition",
		meta=(DisplayName="Is Ownership Transition Active"))
	bool IsOwnershipTransitionActive() const { return bTransitionInProgress; }

	/** Internal authority path for explicit quest/script overrides and save restore. */
	void ForceSetOwningFaction(const FGameplayTag& NewFaction);
	void ForceSetOwningFactionWithContext(const FGameplayTag& NewFaction,
		const FTerritoryTransitionContext& TransitionContext);

	/** Internal authority path for explicit quest/script overrides and save restore. */
	void ForceSetTerritoryState(ETerritoryState NewState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Defenders", meta=(DisplayName="Register Defender"))
	void RegisterDefender(AActor* Defender);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Defenders", meta=(DisplayName="Unregister Defender"))
	void UnregisterDefender(AActor* Defender);

	/** Returns a list of currently-registered defenders (guards + player-assigned). */
	UFUNCTION(BlueprintPure, Category="Territory|Defenders", meta=(DisplayName="Get Registered Defenders"))
	TArray<AActor*> GetRegisteredDefenders() const;

	/** Set contesting faction through the atomic ownership commit path. */
	void SetContestingFaction(const FGameplayTag& Faction);

	/**
	 * P0-05: Atomically commit a new FTerritoryOwnershipData struct.
	 * Writes the entire struct in one operation, then fires ONE ordered event bundle:
	 *   1. Despawn old guards
	 *   2. Spawn new guards (if applicable)
	 *   3. BP virtual OnOwnershipChanged
	 *   4. OnTerritoryOwnershipChanged delegate
	 *   5. BP virtual OnStateChanged (if state changed)
	 *   6. OnTerritoryStateChangedDelegate (if state changed)
	 *
	 * No intermediate state is visible to listeners between steps.
	 * Returns true if the commit was applied (old != new).
	 */
	bool CommitOwnershipData(const FTerritoryOwnershipData& NewData, const FTerritoryTransitionContext& TransitionContext = FTerritoryTransitionContext());

	// ═══════════════════════════════════════════════════════════════════════════
	// Blueprint Events (BlueprintNativeEvent — subclasses can override)
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Fires whenever this territory changes ownership. Override in Blueprint to
	 * run side effects (UI updates, economy notifications, quest triggers).
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Territory", meta=(DisplayName="On Ownership Changed"))
	void OnOwnershipChanged(FGameplayTag OldOwner, FGameplayTag NewOwner);
	virtual void OnOwnershipChanged_Implementation(FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION(BlueprintNativeEvent, Category="Territory", meta=(DisplayName="On State Changed"))
	void OnStateChanged(ETerritoryState OldState, ETerritoryState NewState);
	virtual void OnStateChanged_Implementation(ETerritoryState OldState, ETerritoryState NewState);

	/**
	 * Fires when all defenders have been defeated. The default keeps the current owner and
	 * makes the garrison vulnerable; ownership can change only through the capture flow.
	 * Override in Blueprint for presentation or story hooks, not direct ownership rolls.
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Territory|Guards", meta=(DisplayName="On All Guards Defeated"))
	void OnAllGuardsDefeated();
	virtual void OnAllGuardsDefeated_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category="Territory", meta=(DisplayName="On Territory Initialized"))
	void OnTerritoryInitialized();
	virtual void OnTerritoryInitialized_Implementation();

	// ═══════════════════════════════════════════════════════════════════════════
	// Blueprint Delegates (bind from Blueprint)
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintAssignable, Category="Territory", meta=(DisplayName="On Ownership Changed"))
	FOnTerritoryControlChanged OnTerritoryOwnershipChanged;

	UPROPERTY(BlueprintAssignable, Category="Territory", meta=(DisplayName="On State Changed"))
	FOnTerritoryStateChanged OnTerritoryStateChangedDelegate;

	UPROPERTY(BlueprintAssignable, Category="Territory|Lock", meta=(DisplayName="On Availability Changed"))
	FOnTerritoryAvailabilityChanged OnTerritoryAvailabilityChanged;

	UPROPERTY(BlueprintAssignable, Category="Territory|Guards", meta=(DisplayName="On All Guards Defeated"))
	FOnAllGuardsDefeated OnAllGuardsDefeatedDelegate;

	UPROPERTY(BlueprintAssignable, Category="Territory|Guards", meta=(DisplayName="On Garrison Changed"))
	FOnTerritoryGarrisonChanged OnGarrisonChanged;

	// ═══════════════════════════════════════════════════════════════════════════
	// Lock System API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Returns true if the territory is currently locked (can't be captured).
	 * Locked territories preserve saved political ownership but remain unavailable for
	 * capture, production, economy settlement, garrison command, and counterattacks.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Lock", meta=(DisplayName="Is Locked"))
	bool IsLocked() const;

	/**
	 * Lock this territory. Server-only. Optional reason shown in UI.
	 * Unlock with TryUnlock() once the Definition's Locked Exit Conditions pass.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Lock", meta=(DisplayName="Lock Territory"))
	void LockTerritory(const FText& Reason = FText());

	/** Lock using the exact pawn/controller/Tales context that caused the transition. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Lock", meta=(DisplayName="Lock Territory With Context"))
	bool LockTerritoryWithContext(const FText& Reason, const FTerritoryTransitionContext& TransitionContext);

	/**
	 * Attempts to unlock. Returns true if successful.
	 * Checks:
	 *   - If bForce=true, unlocks unconditionally.
	 *   - Otherwise, all Locked-state Exit Conditions in the Definition must pass.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Lock", meta=(DisplayName="Try Unlock"))
	bool TryUnlock(bool bForce = false);

	/** Unlock using the exact pawn/controller/Tales context that must satisfy lock conditions. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Lock", meta=(DisplayName="Try Unlock With Context"))
	bool TryUnlockWithContext(const FTerritoryTransitionContext& TransitionContext, bool bForce = false);

	/**
	 * Read-only check: would TryUnlock() succeed right now?
	 * True if not locked OR all Definition-owned Locked-state Exit Conditions pass.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Lock", meta=(DisplayName="Can Unlock"))
	bool CanUnlock() const;

	UFUNCTION(BlueprintPure, Category="Territory|Lock", meta=(DisplayName="Can Unlock With Context"))
	bool CanUnlockWithContext(const FTerritoryTransitionContext& TransitionContext) const;

	/** Returns the reason this territory was locked. Empty if not locked. */
	UFUNCTION(BlueprintPure, Category="Territory|Lock", meta=(DisplayName="Get Lock Reason"))
	FText GetLockReason() const { return OwnershipData.LockReason; }

	// ═══════════════════════════════════════════════════════════════════════════
	// Guard Spawning API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Spawns guards for the current owner. Uses FactionGuardDefinitions[first match]
	 * or falls back to GuardNPCDefinition. Respects GuardSpawnCount.
	 * Server-only. Usually called in BeginPlay or after SetOwningFaction.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Guards", meta=(DisplayName="Spawn Guards"))
	void SpawnGuards();

	/**
	 * Spawns exactly one guard at the given spawn point (used for reserve replacement).
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Guards", meta=(DisplayName="Spawn Single Guard"))
	void SpawnSingleGuard(class ATerritoryGuardSpawnPoint* SpawnPoint);

	/** Attempts one guard spawn and reports whether it completed successfully. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Guards", meta=(DisplayName="Try Spawn Single Guard"))
	bool TrySpawnSingleGuard(class ATerritoryGuardSpawnPoint* SpawnPoint, bool bRequireConcealment = false);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Guards", meta=(DisplayName="Despawn Guards"))
	void DespawnGuards();

	/**
	 * Returns the count of currently-alive guards spawned from this territory.
	 * Does not count reserves, dead guards, or registered player-defenders.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Get Spawned Guard Count"))
	int32 GetSpawnedGuardCount() const;

	/**
	 * Returns the number of unique authored/resolved guard spawn points. Each point is
	 * exactly one active combat slot. This differs from GetSpawnedGuardCount, the live count.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Get Configured Guard Count"))
	int32 GetConfiguredGuardCount() const { return GuardSpawnCount; }

	/** Persistent garrison target, including guards purchased after capture. */
	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Get Desired Guard Count"))
	int32 GetDesiredGuardCount() const
	{
		return FMath::Max(0, HasAuthority()
			? OwnershipData.DesiredGuardCount : GarrisonSnapshot.DesiredGuards);
	}

	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Get Maximum Guard Count"))
	int32 GetMaxGuardCount() const;

	/** Resolves the initial target for a new owner without mutating territory state. */
	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Get Post Capture Guard Count"))
	int32 GetPostCaptureGuardCount(const FTerritoryTransitionContext& TransitionContext) const;

	/** Owner-aware form used by atomic capture paths. */
	int32 GetPostCaptureGuardCountForOwner(const FTerritoryTransitionContext& TransitionContext,
		const FGameplayTag& NewOwner) const;

	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Get Garrison Snapshot"))
	FTerritoryGarrisonSnapshot GetGarrisonSnapshot() const { return GarrisonSnapshot; }

	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Get Guard Recruitment Cost"))
	int32 GetGuardRecruitmentCost(int32 Count = 1) const;

	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Get Guard Purchase Cost"))
	int32 GetGuardPurchaseCost(int32 Count = 1) const;

	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Can Purchase Guards"))
	bool CanPurchaseGuards(const AActor* Requester, int32 Count, FText& OutFailureReason) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Guards", meta=(DisplayName="Try Purchase Guards"))
	bool TryPurchaseGuards(AActor* Requester, int32 Count, FText& OutResult);

	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Can Remove Guards"))
	bool CanRemoveGuards(const AActor* Requester, int32 Count, FText& OutFailureReason) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Guards", meta=(DisplayName="Try Remove Guards"))
	bool TryRemoveGuards(AActor* Requester, int32 Count, FText& OutResult);

	/** Validate an absolute staffing target. Lower targets do not require live guards. */
	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Can Set Desired Guard Count"))
	bool CanSetDesiredGuardCount(const AActor* Requester, int32 NewDesiredGuardCount,
		FText& OutFailureReason, int32& OutRecruitmentCost) const;

	/** Atomically apply an absolute target, its Narrative currency debit, and physical withdrawals/deployments. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Guards",
		meta=(DisplayName="Try Set Desired Guard Count"))
	FTerritoryGarrisonMutationResult TrySetDesiredGuardCount(AActor* Requester, int32 NewDesiredGuardCount);

	/**
	 * Checks whether one or more existing reserves can be deployed now. Unlike
	 * Add Guard, reinforcement does not raise the saved staffing target or charge
	 * recruitment currency; it fills an existing active-versus-assigned shortfall.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guards",
		meta=(DisplayName="Can Send Reinforcements"))
	bool CanSendReinforcements(const AActor* Requester, int32 Count,
		FText& OutFailureReason) const;

	/** Server-authoritative reserve deployment for a player command. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Guards",
		meta=(DisplayName="Try Send Reinforcements"))
	FTerritoryGarrisonMutationResult TrySendReinforcements(AActor* Requester, int32 Count = 1);

	/** Returns true if at least one guard is alive and spawned from this territory. */
	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Has Guards Alive"))
	bool HasGuardsAlive() const;

	/**
	 * Returns all spawn points assigned to this territory. Empty array if using
	 * random spawning (no explicit spawn points in-level).
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Get Guard Spawn Points"))
	TArray<ATerritoryGuardSpawnPoint*> GetGuardSpawnPoints() const;

	/**
	 * Returns the resolved guard definition for the given faction.
	 * Lookup order:
	 *   1. FactionGuardDefinitions (first matching entry)
	 *   2. GuardNPCDefinition (fallback)
	 *   3. nullptr (no definition at all — territory spawns nothing)
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Resolve Guard Definition"))
	UNPCDefinition* GetResolvedGuardDefinition(const FGameplayTag& Faction) const;

	/**
	 * Returns the auto-created navigation marker component (if map marker enabled).
	 * Use to configure marker colors, faction mappings, or subscribe to state changes.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Visual", meta=(DisplayName="Get Map Marker Component"))
	UTerritoryNavigationMarkerComponent* GetMapMarkerComponent() const;

	/** Returns a human-readable debug string: owner, state, progress, guard count. */
	UFUNCTION(BlueprintPure, Category="Territory|Debug", meta=(DisplayName="Get Debug String", CompactNodeTitle="Debug"))
	FString GetDebugString() const;

	// ═══════════════════════════════════════════════════════════════════════════
	// Internal — public for subsystem access only
	// ═══════════════════════════════════════════════════════════════════════════

	void CheckBoundsForReindex();
	UNPCDefinition* ResolveGuardDefinition(const FGameplayTag& Faction) const;

	/** P0-07: Unified narrative override resolver — inline > GuardPostDefinition > territory default. */
	void ResolveSpawnPointNarrativeOverrides(
		class ATerritoryGuardSpawnPoint* SpawnPoint,
		UNPCDefinition*& OutDef,
		UClass*& OutNPCClass,
		UNPCActivityConfiguration*& OutActivityConfig,
		const TArray<TSoftObjectPtr<UTriggerSet>>*& OutTriggerSets,
		const TArray<TSoftObjectPtr<UTriggerSet>>& DefaultTriggerSets);

	void ReconcileGuardsAfterLoad();
	void SpawnGuardsToCount(int32 TargetGuardCount);
	void RefreshGarrisonSnapshot();

public:
	/** Check if all Entry Conditions for the given state pass. Public for atomic mutation validation. */
	bool CheckStateConditions(ETerritoryState State, FText& OutFailureReason, const FTerritoryTransitionContext& TransitionContext = FTerritoryTransitionContext()) const;
	/** Check if all Definition-owned Exit Conditions for the given state pass. */
	bool CheckStateExitConditions(ETerritoryState State, FText& OutFailureReason, const FTerritoryTransitionContext& TransitionContext = FTerritoryTransitionContext()) const;
	/** Validate the complete old-state exit and new-state entry rule set. */
	bool CheckStateTransitionConditions(ETerritoryState OldState, ETerritoryState NewState, FText& OutFailureReason, const FTerritoryTransitionContext& TransitionContext = FTerritoryTransitionContext()) const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostActorCreated() override;
	virtual void PostEditImport() override;
#endif

	void EnsurePersistentTerritoryGUID();

	UFUNCTION()
	void OnRep_OwnershipData();

	UFUNCTION()
	void OnRep_GarrisonSnapshot();

	// ─── Editable Properties ───

	/**
	 * Required authoring source. This asset supplies identity, hierarchy, state rules,
	 * Narrative events, guards, economy, and assault settings. The actor has no
	 * Blueprint or level-side policy fallback.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Territory|Definition",
		meta=(DisplayName="Territory Definition"))
	TObjectPtr<UTerritoryDefinition> TerritoryDefinition;

	UPROPERTY(Transient)
	FGameplayTag TerritoryTag;

	UPROPERTY(Transient)
	FText TerritoryDisplayName;

	UPROPERTY(Transient)
	FGameplayTag InitialOwningFaction;

	UPROPERTY(Transient)
	ETerritoryAvailability InitialAvailability = ETerritoryAvailability::Unlocked;

	UPROPERTY(Transient)
	ETerritoryInitialState InitialState = ETerritoryInitialState::Automatic;

	UPROPERTY(Transient)
	ETerritoryControlMode ControlMode = ETerritoryControlMode::Independent;

	UPROPERTY(Transient)
	int32 InitialMaxConcurrentAttackers = 3;

	UPROPERTY(Transient)
	int32 InitialPeriodicIncome = 100;

	UPROPERTY(Transient)
	int32 InitialGuardCost = 50;

	UPROPERTY(Transient)
	int32 InitialGuardRecruitmentCost = 50;

	// ─── Strategic counterattack configuration ───

	UPROPERTY(Transient)
	TObjectPtr<UTerritoryCounterAttackProfile> CounterAttackProfile;

	UPROPERTY(Transient)
	TArray<FTerritoryAssaultApproach> CounterAttackApproaches;

	UPROPERTY(Transient)
	float GuardQuality = 1.f;

	UPROPERTY(Transient)
	float FortificationStrength = 0.f;

	UPROPERTY(Transient)
	float NearbyAlliedSupport = 0.f;

	UPROPERTY(Transient)
	float StrategicValue = 1.f;

	/** Fire EntryEvents (bEntering=true) or ExitEvents (bEntering=false) for the given state. Uses TransitionContext for instigator. */
	void FireStateEvents(ETerritoryState State, bool bEntering, const FTerritoryTransitionContext& TransitionContext = FTerritoryTransitionContext());

	UPROPERTY(Transient)
	FGameplayTag ParentTerritoryTag;

	UPROPERTY(SaveGame, ReplicatedUsing=OnRep_OwnershipData)
	FTerritoryOwnershipData OwnershipData;

	UPROPERTY(ReplicatedUsing=OnRep_GarrisonSnapshot)
	FTerritoryGarrisonSnapshot GarrisonSnapshot;

	UPROPERTY(SaveGame)
	FGuid TerritoryGUID;

	UPROPERTY()
	TObjectPtr<UShapeComponent> BoundsShape;

	/**
	 * Story capture mode for multi-floor Places. When enabled, the server checks the
	 * player's position against the complete Bounds Shape instead of requiring a flag
	 * or Capture Point. Entering with an eligible faction starts/holds Contested but
	 * adds no automatic progress. Any Capture Point targeting this Territory is
	 * automatically disabled and hidden, so story and multiplayer capture cannot run
	 * together.
	 *
	 * Easy example: make the Bounds Shape tall enough to include a shop's ground floor,
	 * stairs, and second floor. The player can clear every defender on any floor, then
	 * speak to the protected owner to accept the handover.
	 */
	UPROPERTY(Transient)
	bool bStoryCaptureFromBounds = false;

	/** Navigation marker component — manages map marker, auto-refreshes on state changes. */
	UPROPERTY()
	TObjectPtr<UTerritoryNavigationMarkerComponent> MapMarkerComponent;

	// ─── Guard Configuration ───

	/**
	 * Default guard definition — used when no per-faction entry matches.
	 * Set this if you want all guards (regardless of owner faction) to use the same NPC class.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UNPCDefinition> GuardNPCDefinition;

	/**
	 * Per-faction guard definitions. When territory owner changes, guards are re-spawned
	 * using the definition for the new owner's faction (first match). Falls back to
	 * GuardNPCDefinition if no matching entry exists.
	 */
	UPROPERTY(Transient)
	TArray<FTerritoryFactionGuardDefinition> FactionGuardDefinitions;

	UPROPERTY(Transient)
	int32 GuardSpawnCount = 3;

	/** Player captures start unstaffed by default so the player explicitly controls profit and loss. */
	UPROPERTY(Transient)
	ETerritoryPostCaptureGarrisonPolicy PostCaptureGarrisonPolicy =
		ETerritoryPostCaptureGarrisonPolicy::PlayerChooses;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ATerritoryGuardSpawnPoint>> GuardSpawnPoints;

	UPROPERTY(BlueprintAssignable, Category="Territory|Guards", meta=(DisplayName="On Guard Killed"))
	FOnGuardKilled OnGuardKilled;

private:
	friend class ATerritoryGuardSpawnPoint;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTFDefenderNarrativeEventConditions;
	friend class FTFTerritoryDefinitionRuntimeNarrative;
#endif

	static int32 CalculateGuardRestoreCount(bool bLoadedFromSave, int32 DesiredGuards,
		int32 SavedActiveGuards, int32 LegacyDefenderCount);

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RegisteredDefenders;

	UPROPERTY()
	TArray<TWeakObjectPtr<ATerritoryGuardCharacter>> SpawnedGuards;

	/** Runtime union members resolved by stable OwnerTerritoryTag or proximity. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ATerritoryGuardSpawnPoint>> ResolvedGuardSpawnPoints;

	/** Per-actor clones. DataAsset Narrative objects are immutable templates and never execute. */
	UPROPERTY(Transient)
	TMap<ETerritoryState, FTerritoryStateConfig> RuntimeStateConfigs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UNarrativeEvent>> RuntimeDefenderDiedEvents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UNarrativeEvent>> RuntimeAllDefendersDefeatedEvents;

	FGameplayTag PreviousOwningFaction;
	ETerritoryState PreviousState = ETerritoryState::Unclaimed;
	ETerritoryAvailability PreviousAvailability = ETerritoryAvailability::Unlocked;
	FBox LastKnownBounds;
	bool bLoadedFromSave = false;
	bool bGuardsReconciled = false;
	bool bTransitionInProgress = false;
	/** True after OnRep_OwnershipData has fired at least once. Suppresses synthetic
	 *  ownership/state change events on initial replication (late join). */
	bool bReplicationInitialized = false;
	UPROPERTY(Transient)
	FTerritoryTransitionContext ActiveTransitionContext;
	bool bApplyingDerivedOwnership = false;
	bool bBypassTransitionConditions = false;
	bool bSpawningGuards = false;
	bool bGarrisonMutationInProgress = false;

	/** Narrative may create its ASC after the pawn is registered as a defender. */
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<UNarrativeAbilitySystemComponent>> BoundDefenderASCs;
	TMap<TWeakObjectPtr<AActor>, int32> PendingDefenderDeathBindAttempts;
	FTimerHandle DefenderDeathBindRetryTimer;

	/** Server-only players currently registered from the full story Territory bounds. */
	TMap<TWeakObjectPtr<AActor>, FGameplayTag> StoryBoundsContesters;

	/** Server-only physical occupants whose stealth profile has deferred contest admission. */
	TMap<TWeakObjectPtr<AActor>, FGameplayTag> StoryBoundsInfiltrators;

	UFUNCTION()
	void OnDefenderDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC,
		const bool bIsDead);

	bool BindDefenderDeath(AActor* Defender);
	void UnbindDefenderDeath(AActor* Defender);
	void ScheduleDefenderDeathBindingRetry(AActor* Defender);
	void RetryPendingDefenderDeathBindings();
	void CleanupInvalidDefenders();
	void TryCompleteDefenderDefeat(const FTerritoryTransitionContext& EventContext);
	void ReconcileStoryBoundsContesters();
	void ReleaseStoryBoundsContesters();
	void ReconcileAvailabilityDependentSystems();
	void ApplyInitialStateDiplomacyPolicies();
	void RebuildRuntimeNarrativeConfiguration(const UTerritoryDefinition& Definition);
	void PublishCaptureSummary();
	bool HasPendingReserveDeployments() const;
	void CancelPendingReserveDeployments();
	void RemoveGuardWithoutReplacement(ATerritoryGuardCharacter* Guard);
	void RegisterResolvedGuardSpawnPoint(ATerritoryGuardSpawnPoint* SpawnPoint);
	void UnregisterResolvedGuardSpawnPoint(ATerritoryGuardSpawnPoint* SpawnPoint);
	void EnsureCounterAttackApproachIDs();
	bool FindGuardSpawnTransform(ATerritoryGuardSpawnPoint* SpawnPoint, UClass* GuardClass,
		bool bRequireConcealment, FTransform& OutTransform) const;
	bool IsGuardSpawnLocationClear(UClass* GuardClass, const FVector& Location) const;

};
