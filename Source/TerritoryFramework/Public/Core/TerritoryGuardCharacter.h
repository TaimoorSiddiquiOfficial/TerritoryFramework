#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "TerritoryGuardSpawnPoint.h"
#include "TerritoryGuardCharacter.generated.h"

class UNPCDefinition;
class UNPCActivityConfiguration;
class UNarrativeCharacterSubsystem;
class UTriggerSet;
class ATerritoryVolume;
class UTerritoryPatrolGoal;

/**
 * Territory guard NPC character. Bridges NarrativePro's NPC framework with
 * TerritoryFramework's spawn-point patrol and ownership data.
 *
 * Use this (or a Blueprint child) as the NPCClassPath in GuardNPCDefinition
 * so guards inherit stable GUIDs and access to their patrol routes.
 *
 * Typical Blueprint usage:
 *   - Get patrol:   Guard->GetTerritoryPatrolRoute() -> [Node0, Node1, ...]
 *   - Has route:    Guard->HasTerritoryPatrolRoute()  -> true/false
 *   - Home:         Guard->TerritoryHomeTransform     -> FTransform
 */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryGuardCharacter : public ANarrativeNPCCharacter
{
	GENERATED_BODY()

public:
	ATerritoryGuardCharacter(const FObjectInitializer& ObjectInitializer);

	// INarrativeStableActor — return SpawnAssignedSaveGUID instead of crashing
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& NewGUID) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Validates the Narrative controller, spawned-possession, class, and instance policy. */
	static bool ValidateNarrativeSpawnDefinition(const UNPCDefinition* Definition,
		int32 RequiredInstances, FText& OutFailureReason);

	/**
	 * Territory-owned adapter into Narrative's authoritative SpawnNPC registry.
	 * All SpawnInfo and Territory context exists before SetNPCDefinition is applied.
	 */
	static ATerritoryGuardCharacter* SpawnThroughNarrative(
		UNarrativeCharacterSubsystem* CharacterSubsystem, UNPCDefinition* Definition,
		const FGameplayTag& ExactFaction, const FGuid& TerritoryGuid,
		const FGuid& SaveGuid, const FTransform& InSpawnTransform, FName SpawnPointName,
		UNPCActivityConfiguration* OptionalActivityOverride,
		const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides,
		ATerritoryVolume* OwningTerritory,
		ATerritoryGuardSpawnPoint* OwningSpawnPoint);

	virtual void SetNPCDefinition(UNPCDefinition* Definition) override;

	/**
	 * Contextual Narrative attitude for a stationary Territory defender.
	 * A target is Hostile only while this guard's Territory is Contested AND
	 * at least one exact faction pair is explicitly at War. Seeing a neutral
	 * player walking through a Claimed Place therefore never starts combat.
	 */
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	/**
	 * Readable version of the Territory guard combat gate for Blueprint/debug UI.
	 * Easy example: Claimed + War = false; Contested + Neutral = false;
	 * Contested + War = true.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guard|Combat",
		meta=(DisplayName="Can Engage Territory Target"))
	bool CanEngageTerritoryTarget(const AActor* Target) const;

	/**
	 * Reconciles Narrative Pro's reported event value with its authoritative ASC state,
	 * then stops AI movement and enters Narrative's replicated ragdoll state when dead.
	 */
	void ReconcileNarrativeDeathState(UNarrativeAbilitySystemComponent* KilledActorASC,
		bool bReportedIsDead);

	/** NPC ragdoll mutations are authored by the server and projected by Narrative replication. */
	virtual void SetRagdoll(bool bWantsRagdoll) override;

	/**
	 * Single entrypoint for deterministic territory guard configuration.
	 * Fills ALL SpawnInfo fields that Narrative activities need, including
	 * SpawnTransform (critical for BPA_ReturnToSpawn) and faction overrides.
	 *
	 * Call this during deferred spawn (between BeginDeferredActorSpawnFromClass
	 * and FinishSpawningActor), NOT after FinishSpawningActor.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Guard",
		meta=(DisplayName="Configure Territory Spawn (Legacy)", DeprecatedFunction,
			DeprecationMessage="Use Configure Territory Spawn With Context so ownership is valid before Narrative applies the definition."))
	void ConfigureTerritorySpawn(
		UNPCDefinition* Definition,
		const FGameplayTag& ExactFaction,
		const FGuid& TerritoryGuid,
		const FGuid& SaveGuid,
		const FTransform& InSpawnTransform,
		FName SpawnPointName,
		UNPCActivityConfiguration* OptionalActivityOverride,
		const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides);

	/**
	 * Blueprint deferred-spawn adapter with the complete typed Territory context.
	 * Returns false without applying the Narrative definition when any identity or
	 * ownership invariant is invalid.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Guard",
		meta=(DisplayName="Configure Territory Spawn With Context"))
	bool ConfigureTerritorySpawnWithContext(
		UNPCDefinition* Definition,
		const FGameplayTag& ExactFaction,
		const FGuid& TerritoryGuid,
		const FGuid& SaveGuid,
		const FTransform& InSpawnTransform,
		FName SpawnPointName,
		UNPCActivityConfiguration* OptionalActivityOverride,
		const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides,
		ATerritoryVolume* InOwningTerritory,
		ATerritoryGuardSpawnPoint* InOwningSpawnPoint);

	// ─── Territory AI context ───

	/** Home position for ReturnToTerritory activity — the spawn point transform. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Territory|AI", meta=(DisplayName="Territory Home Transform"))
	FTransform TerritoryHomeTransform;

	/** Owning territory volume. May be null for unowned guards. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Territory|AI", meta=(DisplayName="Owning Territory"))
	TObjectPtr<ATerritoryVolume> OwningTerritory;

	/** Spawn point this guard was spawned from. May be null for random-spawned guards. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Territory|AI", meta=(DisplayName="Owning Spawn Point"))
	TObjectPtr<ATerritoryGuardSpawnPoint> OwningTerritorySpawnPoint;

	/** Goal class added after Narrative's activity configuration is ready. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|AI|Patrol")
	TSubclassOf<UTerritoryPatrolGoal> PatrolGoalClass;

	/** Live per-guard goal populated from the assigned spawn point. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Territory|AI|Patrol")
	TObjectPtr<UTerritoryPatrolGoal> TerritoryPatrolGoal;

	// ─── Patrol Route Helpers ───

	/**
	 * Returns the guard's patrol route from their spawn point. Empty array if no spawn point
	 * is assigned (random-spawned guards have no route).
	 *
	 * Use in BPA_TerritoryPatrol or any patrol AI to get the waypoints this guard should visit.
	 * Always pair with HasTerritoryPatrolRoute() to guard the empty-route case.
	 *
	 * Example:
	 *   Route = Guard->GetTerritoryPatrolRoute();
	 *   Length = Route.Num();
	 *   if (Length > 0) { SafeIndex = CurrentIndex % Length; Node = Route[SafeIndex]; }
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guard|Patrol",
		meta=(DisplayName="Get Patrol Route", CompactNodeTitle="Patrol"))
	TArray<FTerritoryPatrolNode> GetTerritoryPatrolRoute() const;

	/**
	 * Returns true if this guard has a valid patrol route (spawn point assigned AND has >= 2 nodes).
	 *
	 * Example:
	 *   if (Guard->HasTerritoryPatrolRoute()) { StartPatrol(); } else { StandGuard(); }
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guard|Patrol",
		meta=(DisplayName="Has Patrol Route"))
	bool HasTerritoryPatrolRoute() const;

	/**
	 * Returns the number of patrol nodes this guard's route contains.
	 * Zero if no spawn point is assigned.
	 *
	 * Shorthand for GetTerritoryPatrolRoute().Num() without copying the array.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guard|Patrol",
		meta=(DisplayName="Get Patrol Node Count"))
	int32 GetPatrolNodeCount() const;

	/**
	 * Safely fetches a single patrol node by index. Returns false if the index is out of range
	 * or no route exists. Never crashes.
	 *
	 * Example (safe modulo indexing):
	 *   Index = CurrentPatrolIndex % Guard->GetPatrolNodeCount();
	 *   Guard->GetSafePatrolNode(Index, OutNode);
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guard|Patrol",
		meta=(DisplayName="Get Safe Patrol Node"))
	bool GetSafePatrolNode(int32 Index, FTerritoryPatrolNode& OutNode) const;

	/**
	 * Returns the spawn-point transform this guard was spawned from.
	 * Equivalent to TerritoryHomeTransform but makes ownership semantics explicit.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Guard",
		meta=(DisplayName="Get Spawn Transform"))
	FTransform GetSpawnTransform() const;

	/** Returns the owning territory this guard belongs to. Null if unassigned. */
	UFUNCTION(BlueprintPure, Category="Territory|Guard",
		meta=(DisplayName="Get Owning Territory"))
	ATerritoryVolume* GetOwningTerritory() const;

	/** Returns the guard's replicated Narrative faction, including spawn-point overrides. */
	UFUNCTION(BlueprintPure, Category="Territory|Guard",
		meta=(DisplayName="Get Guard Faction"))
	FGameplayTag GetGuardFaction() const;

	/** Returns true if this guard was spawned from a spawn point (not randomly). */
	UFUNCTION(BlueprintPure, Category="Territory|Guard",
		meta=(DisplayName="Is Spawn Point Guard"))
	bool IsSpawnPointGuard() const;

	/**
	 * Creates or refreshes this guard's per-instance Narrative patrol goal.
	 *
	 * P1-12 PIE verification required:
	 *   1. Spawn NPC from UNPCDefinition — verify definition data is ready
	 *   2. Verify activity configuration is active on UNPCActivityComponent
	 *   3. Add Territory patrol goal via this function
	 *   4. Assert the expected Narrative activity becomes selected
	 *   5. Assert the NPC reaches at least two patrol nodes
	 *   6. Assert combat interruption and patrol resumption work
	 * Until a PIE test proves this, patrol is integrated but unverified.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Guard|Patrol")
	bool InitializeTerritoryPatrolGoal();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnCharacterVisualInitialized() override;
	virtual void ApplyActivityConfig_Implementation(UNPCActivityConfiguration* NPCActivityConfig) override;
	virtual void HandleDeath_Implementation(AActor* KilledActor,
		UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead) override;

	// Prevent Narrative save system from restoring stale guards on load.
	virtual bool ShouldRespawn_Implementation() const override;

	virtual float TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

public:
	/** Most recent actor that dealt damage to this guard. Used by OnGuardKilled to provide the killer. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Guard")
	TWeakObjectPtr<AActor> LastDamagingInstigator;

private:
	void TryWieldDefaultWeapon();

	FTimerHandle DefaultWeaponWieldTimer;
	int32 DefaultWeaponWieldAttempts = 0;
	int32 DefaultWeaponPostInitializationAttempts = 0;
	bool bNarrativeInitializationCompleted = false;

	FGuid CachedFallbackGUID;
};
