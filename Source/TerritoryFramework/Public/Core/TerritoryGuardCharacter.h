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
 * Territory-specific NPC character that properly implements GetActorGUID
 * by returning SpawnInfo.SpawnAssignedSaveGUID. This prevents the
 * NarrativeStableActor assertion crash when spawned outside of
 * Narrative's UNPCSpawnComponent framework.
 *
 * Assign this class (or a Blueprint child of it) as the NPCClassPath
 * in the GuardNPCDefinition asset.
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

	/**
	 * Single entrypoint for deterministic territory guard configuration.
	 * Fills ALL SpawnInfo fields that Narrative activities need, including
	 * SpawnTransform (critical for BPA_ReturnToSpawn) and faction overrides.
	 *
	 * Call this during deferred spawn (between BeginDeferredActorSpawnFromClass
	 * and FinishSpawningActor), NOT after FinishSpawningActor.
	 */
	void ConfigureTerritorySpawn(
		UNPCDefinition* Definition,
		const FGameplayTag& ExactFaction,
		const FGuid& TerritoryGuid,
		const FGuid& SaveGuid,
		const FTransform& InSpawnTransform,
		FName SpawnPointName = NAME_None,
		UNPCActivityConfiguration* OptionalActivityOverride = nullptr,
		const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides = {});

	// ─── Territory AI context ───

	/** Home position for ReturnToTerritory activity — the spawn point transform. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|AI")
	FTransform TerritoryHomeTransform;

	/** Owning territory volume. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|AI")
	TWeakObjectPtr<ATerritoryVolume> OwningTerritory;

	/** Spawn point this guard was spawned from (may be null for random spawns). */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|AI")
	TWeakObjectPtr<ATerritoryGuardSpawnPoint> OwningTerritorySpawnPoint;

	/**
	 * Get the patrol route for this guard's spawn point.
	 * Returns an empty array if no spawn point is assigned (random fallback spawn).
	 *
	 * Preferred Blueprint access path:
	 *   Guard->GetTerritoryPatrolRoute() → Length check → index node safely
	 * Safer than chaining through OwningTerritorySpawnPoint directly in BP.
	 */
	UFUNCTION(BlueprintPure, Category = "Territory|AI")
	TArray<FTerritoryPatrolNode> GetTerritoryPatrolRoute() const;

	/** Whether this guard has a valid patrol route attached via its spawn point. */
	UFUNCTION(BlueprintPure, Category = "Territory|AI")
	bool HasTerritoryPatrolRoute() const;

protected:
	virtual void BeginPlay() override;

	// Prevent Narrative save system from restoring stale guards on load.
	virtual bool ShouldRespawn_Implementation() const override;

private:
	FGuid CachedFallbackGUID;
};
