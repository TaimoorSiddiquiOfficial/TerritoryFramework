#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Spawn/TerritorySpawnPolicy.h"

#include "TerritorySpawnPolicyStatic.generated.h"

/**
 * The simplest useful policy: this slot should hold this entity, always.
 *
 * Two knobs, and the fact that there are only two is the design. "Should something be here" and
 * "put it back if it is gone" are the only decisions a static prop actually makes; everything else —
 * occupancy grace, lease vetoes, load guards, retry budget, the never-relocate rule — is enforced by
 * the framework after this policy answers.
 *
 * A respawn delay exists because the common authored value is not zero: a crate that reappears the
 * same frame it was destroyed looks like a bug to the player even though it is correct.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced,
	meta = (DisplayName = "Spawn Policy (Static)"))
class TERRITORYFRAMEWORK_API UTerritorySpawnPolicyStatic : public UTerritorySpawnPolicy
{
	GENERATED_BODY()

public:
	UTerritorySpawnPolicyStatic();

	// ─── Identity ───

	virtual FGameplayTag GetEntityTag_Implementation() const override;

	virtual TSubclassOf<UTerritorySpawnExecutor> GetExecutorClass_Implementation() const override;

	virtual TSoftClassPtr<AActor> GetDesiredActorClass_Implementation() const override;

	// ─── Decisions ───

	virtual FTerritorySpawnDecision EvaluateDecision_Implementation(const FTerritorySpawnRequest& Request) const override;

	// ─── Configuration ───

	/**
	 * What this slot produces. A child of Territory.Spawn.Entity. Left at the zero tag the anchor
	 * refuses to run rather than spawning into an unclassified bucket nobody can query.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Identity",
		meta = (DisplayName = "Entity Tag", Categories = "Territory.Spawn.Entity"))
	FGameplayTag EntityTag;

	/**
	 * The class to create. Soft, and loaded by the framework when the slot is about to spawn, so an
	 * unloaded district costs nothing but a path string.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Identity",
		meta = (DisplayName = "Actor Class"))
	TSoftClassPtr<AActor> ActorClass;

	/**
	 * What "put it back" means here.
	 *
	 * The distinction is load-bearing and is not a taste preference:
	 *   - InitialSpawn   the entity exists once and stays gone when destroyed. Use for a one-shot prop.
	 *   - RebuildLost    the slot rebuilds the entity after it disappears. Use for anything that must
	 *                    exist for the world to make sense.
	 *
	 * There is deliberately no third "respawn instantly" option. An instant respawn with no delay is
	 * RebuildLost with RespawnDelaySeconds = 0, and naming it separately would create two spellings of
	 * one state.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Recovery",
		meta = (DisplayName = "Recovery Mode"))
	ESpawnPolicyRecoveryMode RecoveryMode = ESpawnPolicyRecoveryMode::InitialSpawn;

	/**
	 * Seconds before a lost entity is rebuilt. Not a cosmetic value: it is the same number the player
	 * perceives as "it has not just teleported back", and the same number that keeps a destroy loop
	 * from retrying every sweep.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Recovery",
		meta = (DisplayName = "Respawn Delay (s)", ClampMin = "0.0",
			EditCondition = "RecoveryMode == ESpawnPolicyRecoveryMode::RebuildLost"))
	float RespawnDelaySeconds = 0.f;
};
