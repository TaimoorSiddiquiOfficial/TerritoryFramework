#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"

#include "Spawn/TerritorySpawnTypes.h"

#include "TerritorySpawnPolicy.generated.h"

class AActor;
class ATerritorySpawnAnchor;
class UTerritorySpawnExecutor;

/**
 * A policy answers one question: should this slot hold an entity right now, and in which condition?
 *
 * It owns DECISIONS ONLY. It never spawns, never destroys, never touches the save system and never
 * grants a lease. That restraint is the point: the framework's safety interlocks run after this
 * answer, so a wrong policy can waste a retry but cannot destroy a vehicle a player is driving.
 *
 * Implementations live with their consumer (a vehicle policy next to the vehicle executor), never in
 * Public/Spawn, so the framework core keeps compiling with every adapter folder deleted.
 */
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class TERRITORYFRAMEWORK_API UTerritorySpawnPolicy : public UObject
{
	GENERATED_BODY()

public:
	UTerritorySpawnPolicy();

	virtual UWorld* GetWorld() const override;

	// ─── Identity ───

	/**
	 * Stable semantic identity of the entity family this policy produces, for example
	 * Spawn.Vehicle.Personal. Used for buckets, diagnostics and editor validation; never a display
	 * string. The zero tag is inert and makes the anchor refuse to run.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Spawn|Policy")
	FGameplayTag GetEntityTag() const;
	virtual FGameplayTag GetEntityTag_Implementation() const;

	/**
	 * The executor that performs this policy's actions. Declared here rather than on the anchor so a
	 * policy and its executor ship together and cannot be mismatched by hand.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Spawn|Policy")
	TSubclassOf<UTerritorySpawnExecutor> GetExecutorClass() const;
	virtual TSubclassOf<UTerritorySpawnExecutor> GetExecutorClass_Implementation() const;

	/**
	 * Which actor class this slot wants. Left unset, the policy is treated as having nothing to say
	 * about the class and the anchor's configured class is used.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Spawn|Policy")
	TSoftClassPtr<AActor> GetDesiredActorClass() const;
	virtual TSoftClassPtr<AActor> GetDesiredActorClass_Implementation() const;

	// ─── Decisions ───

	/**
	 * The whole decision surface. Return None from any state that should be left alone; None is the
	 * zero value, so an unconfigured or mis-authored policy produces no action rather than an
	 * accidental spawn.
	 *
	 * Every result is re-checked by the framework before anything happens: a Retire answer is dropped
	 * while the entity is occupied, while a lease is live, during a save load, or inside the authored
	 * grace window.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Spawn|Policy")
	FTerritorySpawnDecision EvaluateDecision(const FTerritorySpawnRequest& Request) const;
	virtual FTerritorySpawnDecision EvaluateDecision_Implementation(const FTerritorySpawnRequest& Request) const;

	/**
	 * True when the slot should be evaluated at all. An anchor whose policy says otherwise is skipped
	 * by the sweep without touching state, which is how an ambient parking slot stays at zero cost
	 * until the district it belongs to is live.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Spawn|Policy")
	bool ShouldEvaluate(const FTerritorySpawnRequest& Request) const;
	virtual bool ShouldEvaluate_Implementation(const FTerritorySpawnRequest& Request) const;

	/**
	 * Whether this slot accepts a lease for the given purpose.
	 *
	 * The subsystem asks before it grants one, so a policy that must never be pinned can say so. This
	 * exists because the alternative — a config flag on one policy class that nothing reads — is a
	 * field that lies: a designer ticks it, the framework ignores it, and the only way to find out is
	 * to watch an entity get retired under a mission that believed it held a lease.
	 *
	 * Default is true. A slot that has no opinion is leaseable, because a lease is the mechanism that
	 * stops the framework doing something, and refusing leases by default would quietly disable the
	 * one guarantee every consumer wants.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Spawn|Policy")
	bool AllowsLease(FGameplayTag Purpose) const;
	virtual bool AllowsLease_Implementation(FGameplayTag Purpose) const;

	// ─── Ownership ───

	/** The anchor that owns this instanced policy, resolved through the outer chain. Null when detached. */
	UFUNCTION(BlueprintPure, Category = "Spawn|Policy")
	ATerritorySpawnAnchor* GetOwningAnchor() const;

	/** True when this policy is configured well enough to run. The anchor refuses to start otherwise. */
	bool IsPolicyConfigured() const;

protected:
	/**
	 * Game time helper. Every evaluation in one sweep must agree on "now", so callers pass NowGameTime
	 * inside the request; this helper only exists for policies that need a timestamp of their own.
	 */
	double GetNowGameTime() const;

	/**
	 * True when any player's pawn is within RadiusCm of Location.
	 *
	 * This is the "can the player see it" test, and it is the one rule every reclaiming policy needs.
	 * It is deliberately here rather than in each policy: two implementations of one occupancy rule is
	 * how the two drift apart, and the failure is invisible — an entity vanishes in front of a player
	 * who was standing next to it.
	 *
	 * A pawn-less controller counts as absent, because a player who is mid-respawn is not looking at
	 * anything. A spectator is not either, for the same reason.
	 */
	bool IsAnyPlayerNearby(const FVector& Location, float RadiusCm) const;
};
