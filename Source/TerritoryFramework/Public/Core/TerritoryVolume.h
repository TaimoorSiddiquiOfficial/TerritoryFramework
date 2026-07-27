#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NarrativeSavableActor.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryInterfaces.h"
#include "TerritoryVolume.generated.h"

class UNarrativeAbilitySystemComponent;
class UShapeComponent;
class UNPCDefinition;
class ATerritoryGuardCharacter;
class ATerritoryGuardSpawnPoint;
class UTerritoryNavigationMarkerComponent;

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
 *   Volume->GetTerritoryState()    -> Claimed / Contested / Locked / Unclaimed
 *   Volume->IsOwnedByFaction(Tag)  -> true/false
 *   Volume->GetControlProgress()   -> 0.0 to 1.0
 */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryVolume : public AActor, public INarrativeSavableActor, public ITerritoryOwnershipInterface, public ITerritoryEventReceiverInterface
{
	GENERATED_BODY()
	friend class UTerritoryDataValidator;

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

	UFUNCTION(BlueprintPure, Category="Territory|Ownership", meta=(DisplayName="Get Territory State"))
	ETerritoryState GetTerritoryState() const;

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

	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy", meta=(DisplayName="Get Parent Territory Tag"))
	FGameplayTag GetParentTerritoryTag() const;

	UFUNCTION(BlueprintPure, Category="Territory|Ownership", meta=(DisplayName="Get Initial Owning Faction"))
	FGameplayTag GetInitialOwningFaction() const;

	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy", meta=(DisplayName="Get Control Mode"))
	ETerritoryControlMode GetControlMode() const;

	// ═══════════════════════════════════════════════════════════════════════════
	// Mutation API (BlueprintAuthorityOnly — server-only)
	// ═══════════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Ownership", meta=(DisplayName="Set Owning Faction"))
	void SetOwningFaction(const FGameplayTag& NewFaction);

	/** Internal hierarchy path for aggregate-only City/District ownership. */
	void SetDerivedOwningFaction(const FGameplayTag& NewFaction);

	/** Internal authority path for explicit quest/script overrides and save restore. */
	void ForceSetOwningFaction(const FGameplayTag& NewFaction);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Ownership", meta=(DisplayName="Set Control Progress"))
	void SetControlProgress(float Progress);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Ownership", meta=(DisplayName="Set Territory State"))
	void SetTerritoryState(ETerritoryState NewState);

	/** Internal authority path for explicit quest/script overrides and save restore. */
	void ForceSetTerritoryState(ETerritoryState NewState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Defenders", meta=(DisplayName="Register Defender"))
	void RegisterDefender(AActor* Defender);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Defenders", meta=(DisplayName="Unregister Defender"))
	void UnregisterDefender(AActor* Defender);

	/** Returns a list of currently-registered defenders (guards + player-assigned). */
	UFUNCTION(BlueprintPure, Category="Territory|Defenders", meta=(DisplayName="Get Registered Defenders"))
	TArray<AActor*> GetRegisteredDefenders() const;

	void SetContestingFaction(const FGameplayTag& Faction)
	{
		if (HasAuthority())
		{
			OwnershipData.ContestingFaction = Faction;
		}
	}

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
	 * Fires when all defenders have been defeated. Default behavior: marks territory Unclaimed.
	 * Override in Blueprint to change outcome (e.g., auto-spawn reserves, trigger quest).
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

	UPROPERTY(BlueprintAssignable, Category="Territory|Guards", meta=(DisplayName="On All Guards Defeated"))
	FOnAllGuardsDefeated OnAllGuardsDefeatedDelegate;

	// ═══════════════════════════════════════════════════════════════════════════
	// Lock System API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Returns true if the territory is currently locked (can't be captured).
	 * Locked territories still generate income and pay upkeep, just can't change ownership.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Lock", meta=(DisplayName="Is Locked"))
	bool IsLocked() const;

	/**
	 * Lock this territory. Server-only. Optional reason shown in UI.
	 * Unlock with TryUnlock() once all LockConditions pass.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Lock", meta=(DisplayName="Lock Territory"))
	void LockTerritory(const FText& Reason = FText());

	/**
	 * Attempts to unlock. Returns true if successful.
	 * Checks:
	 *   - If bForce=true, unlocks unconditionally.
	 *   - Otherwise, all LockConditions must pass (each condition's CheckCondition() true).
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Lock", meta=(DisplayName="Try Unlock"))
	bool TryUnlock(bool bForce = false);

	/**
	 * Read-only check: would TryUnlock() succeed right now?
	 * True if not locked OR all LockConditions pass.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Lock", meta=(DisplayName="Can Unlock"))
	bool CanUnlock() const;

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
	 * Returns the configured maximum number of guards (GuardSpawnCount property).
	 * This is different from GetSpawnedGuardCount — which is the current live count.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Get Configured Guard Count"))
	int32 GetConfiguredGuardCount() const { return GuardSpawnCount; }

	/** Persistent garrison target, including guards purchased after capture. */
	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Get Desired Guard Count"))
	int32 GetDesiredGuardCount() const { return FMath::Max(0, OwnershipData.DesiredGuardCount); }

	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Get Maximum Guard Count"))
	int32 GetMaxGuardCount() const;

	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Get Guard Purchase Cost"))
	int32 GetGuardPurchaseCost(int32 Count = 1) const;

	UFUNCTION(BlueprintPure, Category="Territory|Guards", meta=(DisplayName="Can Purchase Guards"))
	bool CanPurchaseGuards(const AActor* Requester, int32 Count, FText& OutFailureReason) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Guards", meta=(DisplayName="Try Purchase Guards"))
	bool TryPurchaseGuards(AActor* Requester, int32 Count, FText& OutResult);

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
	void ReconcileGuardsAfterLoad();

protected:
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

	// ─── Editable Properties ───

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory",
		meta=(Categories="Territory", DisplayName="Territory Tag"))
	FGameplayTag TerritoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory",
		meta=(DisplayName="Display Name"))
	FText TerritoryDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory",
		meta=(Categories="Narrative.Factions", DisplayName="Initial Owning Faction"))
	FGameplayTag InitialOwningFaction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Hierarchy",
		meta=(DisplayName="Control Mode"))
	ETerritoryControlMode ControlMode = ETerritoryControlMode::Independent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory",
		meta=(ClampMin="1", UIMin="1", UIMax="20", DisplayName="Max Concurrent Attackers"))
	int32 InitialMaxConcurrentAttackers = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Economy",
		meta=(ClampMin="0", DisplayName="Periodic Income"))
	int32 InitialPeriodicIncome = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Economy",
		meta=(ClampMin="0", DisplayName="Guard Cost"))
	int32 InitialGuardCost = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory",
		meta=(DisplayName="Starts Locked"))
	bool bStartsLocked = false;

	// ─── State Configuration (Conditions & Events) ───

	/**
	 * State configuration map — assign conditions and events per territory state.
	 * EntryConditions must all pass to enter that state; EntryEvents fire on entry;
	 * ExitEvents fire on exit. Evaluated in SetTerritoryState before/after transition.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|States",
		meta=(DisplayName="State Configs"))
	TMap<ETerritoryState, FTerritoryStateConfig> StateConfigs;

	/** Check if all EntryConditions for the given state pass. */
	bool CheckStateConditions(ETerritoryState State, FText& OutFailureReason) const;

	/** Fire EntryEvents (bEntering=true) or ExitEvents (bEntering=false) for the given state. */
	void FireStateEvents(ETerritoryState State, bool bEntering);

	// ─── Lock System ───

	/**
	 * Narrative conditions that must ALL pass for TryUnlock() to succeed.
	 * If empty, territory can always be unlocked. EditCondition: bStartsLocked.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category="Territory|Lock",
		meta=(EditCondition="bStartsLocked", DisplayName="Lock Conditions"))
	TArray<TObjectPtr<class UNarrativeCondition>> LockConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Hierarchy",
		meta=(Categories="Territory", DisplayName="Parent Territory Tag"))
	FGameplayTag ParentTerritoryTag;

	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_OwnershipData, BlueprintReadWrite,
		Category="Territory|Ownership")
	FTerritoryOwnershipData OwnershipData;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category="Territory|Identity",
		meta=(DisplayName="Territory GUID"))
	FGuid TerritoryGUID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Visual",
		meta=(DisplayName="Bounds Shape"))
	TObjectPtr<UShapeComponent> BoundsShape;

	/** Navigation marker component — manages map marker, auto-refreshes on state changes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Visual",
		meta=(DisplayName="Map Marker Component"))
	TObjectPtr<UTerritoryNavigationMarkerComponent> MapMarkerComponent;

	// ─── Guard Configuration ───

	/**
	 * Default guard definition — used when no per-faction entry matches.
	 * Set this if you want all guards (regardless of owner faction) to use the same NPC class.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Guards",
		meta=(AllowedClasses="/Script/NarrativeArsenal.NPCDefinition", DisplayName="Default Guard Definition"))
	TObjectPtr<UNPCDefinition> GuardNPCDefinition;

	/**
	 * Per-faction guard definitions. When territory owner changes, guards are re-spawned
	 * using the definition for the new owner's faction (first match). Falls back to
	 * GuardNPCDefinition if no matching entry exists.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Guards",
		meta=(TitleProperty="{Faction}", DisplayName="Per-Faction Guard Definitions"))
	TArray<FTerritoryFactionGuardDefinition> FactionGuardDefinitions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Guards",
		meta=(ClampMin="0", DisplayName="Guard Spawn Count"))
	int32 GuardSpawnCount = 3;

	/**
	 * Maximum live garrison after player purchases. When authored spawn points exist,
	 * this is the sum of their MaxGuards values; otherwise the configured fallback
	 * maximum is used.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Guards",
		meta=(ClampMin="0", DisplayName="Maximum Guard Count"))
	int32 MaxGuardCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Guards",
		meta=(ClampMin="0.0", DisplayName="Guard Spawn Radius"))
	float GuardSpawnRadius = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Guards",
		meta=(AllowedClasses="/Script/TerritoryFramework.TerritoryGuardSpawnPoint", DisplayName="Guard Spawn Points"))
	TArray<TObjectPtr<AActor>> GuardSpawnPoints;

	// ─── Guard Events ───

	UPROPERTY(BlueprintAssignable, Category="Territory|Guards", meta=(DisplayName="On Guard Killed"))
	FOnGuardKilled OnGuardKilled;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RegisteredDefenders;

	UPROPERTY()
	TArray<TWeakObjectPtr<ATerritoryGuardCharacter>> SpawnedGuards;

	FGameplayTag PreviousOwningFaction;
	ETerritoryState PreviousState = ETerritoryState::Unclaimed;
	FBox LastKnownBounds;
	bool bLoadedFromSave = false;
	bool bGuardsReconciled = false;
	bool bTransitionInProgress = false;
	bool bApplyingDerivedOwnership = false;
	bool bBypassTransitionConditions = false;

	UFUNCTION()
	void OnDefenderDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC);

	void BindDefenderDeath(AActor* Defender);
	void UnbindDefenderDeath(AActor* Defender);
	void CleanupInvalidDefenders();
	bool HasPendingReserveDeployments() const;
	bool FindGuardSpawnTransform(ATerritoryGuardSpawnPoint* SpawnPoint, UClass* GuardClass,
		bool bRequireConcealment, FTransform& OutTransform) const;
	bool IsGuardSpawnLocationClear(UClass* GuardClass, const FVector& Location) const;

	FVector GetRandomSpawnPoint() const;
};
