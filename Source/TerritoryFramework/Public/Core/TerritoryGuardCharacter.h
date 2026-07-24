#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "TerritoryGuardSpawnPoint.h"
#include "TerritoryGuardCharacter.generated.h"

class UNPCDefinition;
class UNPCActivityConfiguration;
class UTriggerSet;
class ATerritoryVolume;

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

	/**
	 * Single entrypoint for deterministic territory guard configuration.
	 * Fills ALL SpawnInfo fields that Narrative activities need, including
	 * SpawnTransform (critical for BPA_ReturnToSpawn) and faction overrides.
	 *
	 * Call this during deferred spawn (between BeginDeferredActorSpawnFromClass
	 * and FinishSpawningActor), NOT after FinishSpawningActor.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Guard", meta=(DisplayName="Configure Territory Spawn"))
	void ConfigureTerritorySpawn(
		UNPCDefinition* Definition,
		const FGameplayTag& ExactFaction,
		const FGuid& TerritoryGuid,
		const FGuid& SaveGuid,
		const FTransform& InSpawnTransform,
		FName SpawnPointName,
		UNPCActivityConfiguration* OptionalActivityOverride,
		const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides);

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

protected:
	virtual void BeginPlay() override;

	// Prevent Narrative save system from restoring stale guards on load.
	virtual bool ShouldRespawn_Implementation() const override;

private:
	FGuid CachedFallbackGUID;
};
