#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Spawn/TerritorySpawnPolicy.h"

#include "TerritorySpawnPolicyPersonalVehicle.generated.h"

/**
 * The player's own car: one bay, one vehicle, and a rule about where it comes back.
 *
 * The user's original requirement is the whole class: when the player is NOT in the vehicle, a city-wide
 * check runs; possessed keeps it where it is, unpossessed sends it home. That sentence is three separate
 * concerns and this class keeps them separate:
 *
 *   - "is the player in it" is the framework's occupancy interlock, delegated to the vehicle executor.
 *     This policy never asks the question, so it cannot get it wrong.
 *   - "it is away" is a durable fact (the record says no instance) plus a policy decision (should it come
 *     back now). The fact is the framework's; the decision is here.
 *   - "where is home" is placement. The record's transform is a memory of where the car was left; the
 *     anchor's transform is the authored pad. Which one a returning car uses is a taste decision that
 *     differs per vehicle, so it is the authored enum below.
 *
 * Why this is not a component on the player: a bay is a place. Two players cannot share one place, a place
 * outlives a session, and a place can be entered when nobody is online. A component on the controller is
 * none of those things, which is why the earlier design that put the car's state there was rejected.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced,
	meta = (DisplayName = "Spawn Policy (Personal Vehicle)"))
class TERRITORYFRAMEWORK_API UTerritorySpawnPolicyPersonalVehicle : public UTerritorySpawnPolicy
{
	GENERATED_BODY()

public:
	UTerritorySpawnPolicyPersonalVehicle();

	// ─── Identity ───

	virtual FGameplayTag GetEntityTag_Implementation() const override;

	virtual TSubclassOf<UTerritorySpawnExecutor> GetExecutorClass_Implementation() const override;

	virtual TSoftClassPtr<AActor> GetDesiredActorClass_Implementation() const override;

	// ─── Decisions ───

	virtual bool ShouldEvaluate_Implementation(const FTerritorySpawnRequest& Request) const override;

	virtual FTerritorySpawnDecision EvaluateDecision_Implementation(const FTerritorySpawnRequest& Request) const override;

	// ─── Configuration: what ───

	/**
	 * Entity identity for buckets, logs and validation. A child of Territory.Spawn.Entity supplied by the
	 * project, for example Spawn.Vehicle.Personal. Left at the zero tag the anchor refuses to run.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Identity",
		meta = (DisplayName = "Entity Tag", Categories = "Territory.Spawn.Entity"))
	FGameplayTag EntityTag;

	/** The car Blueprint this bay produces. Soft so an unloaded district costs a path string. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Identity",
		meta = (DisplayName = "Vehicle Class"))
	TSoftClassPtr<AActor> VehicleClass;

	// ─── Configuration: when ───

	/**
	 * How long the car must already have been down before it is considered absent and returned.
	 *
	 * Not a cosmetic number. It is the same window in which a player who got out to open a gate can get
	 * back in, and the framework's own grace interlock is measured from the same idea. Zero means "return
	 * it on the very next sweep", which is correct for a mission car and wrong for a personal one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Return",
		meta = (DisplayName = "Return Grace Seconds", ClampMin = "0.0"))
	float ReturnGraceSeconds = 30.f;

	/** Where a returning car reappears. See ESpawnVehicleReturnPlacement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Return",
		meta = (DisplayName = "Return Placement"))
	ESpawnVehicleReturnPlacement ReturnPlacement = ESpawnVehicleReturnPlacement::AtAnchor;

	/** What condition a returning car comes back in. See ESpawnVehicleReturnCondition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Return",
		meta = (DisplayName = "Return Condition"))
	ESpawnVehicleReturnCondition ReturnCondition = ESpawnVehicleReturnCondition::PreserveDamage;

	/**
	 * How far from its bay a parked car may sit before the slot treats it as "away".
	 *
	 * This is the whole of the city-wide check. A framework car that is tracked by its anchor has not left
	 * the slot; one whose actor is gone has. Distance is not the test, because driving a car out of its bay
	 * does not change who owns the slot — and distance cannot be measured at all once the actor is gone,
	 * which is exactly the case that matters. The value is kept as a diagnostic threshold for the HUD and
	 * the debug view; it is deliberately NOT consulted to decide a retire.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Return",
		meta = (DisplayName = "Away Threshold (cm, diagnostic)", ClampMin = "0.0"))
	float AwayDiagnosticThresholdCm = 2000.f;

	/**
	 * Despawn an abandoned car and let the slot put it back.
	 *
	 * This is the literal reading of the original requirement: "if the player is not possessing it, the
	 * slot sends it home". It is OFF by default, and the reason is not caution — it is that the behaviour
	 * is only correct with a companion distance rule below. Turning it on alone would make the car vanish
	 * the instant the player steps out to open a gate, which reads as a bug even when the record is right.
	 *
	 * With ReturnPlacement = AsLastLeft the despawn is a memory/streaming optimisation that the player
	 * cannot see: the car leaves the actor list, then returns where it stood. With AtAnchor it is a real
	 * teleport home.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Return",
		meta = (DisplayName = "Despawn When Abandoned"))
	bool bDespawnWhenAbandoned = false;

	/**
	 * How far the owning player must be from the car before it may be despawned.
	 *
	 * Only consulted when bDespawnWhenAbandoned is on. Zero is refused: a zero radius would make the car
	 * disappear while the player is still standing beside it, because their own body is not a car occupant.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Return",
		meta = (DisplayName = "Abandon Distance (cm)", ClampMin = "100.0",
			EditCondition = "bDespawnWhenAbandoned"))
	float AbandonDistanceCm = 6000.f;

	/**
	 * No player may be within this radius of the car when it is despawned.
	 *
	 * The owner's own distance is not enough. A car far from its owner can still be right beside
	 * somebody else, and retiring it then removes it in front of a player who is looking at it. That
	 * failure leaves no trace in a log and is obvious to the person it happens to, which is the worst
	 * combination a bug can have.
	 *
	 * The framework's occupancy interlock does not cover this case either: it asks whether somebody is
	 * INSIDE the car, and a player standing next to it answers no.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Return",
		meta = (DisplayName = "Safe Radius (cm)", ClampMin = "0.0",
			EditCondition = "bDespawnWhenAbandoned"))
	float AbandonSafeRadiusCm = 2500.f;

	/** Retire mode used when an abandoned car is despawned. Hiding first avoids a visible pop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Return",
		meta = (DisplayName = "Abandon Retire Mode",
			EditCondition = "bDespawnWhenAbandoned"))
	ESpawnRetireMode AbandonRetireMode = ESpawnRetireMode::HideThenDestroy;

	// ─── Configuration: wreck ───

	/**
	 * What happens when the car is destroyed rather than retired.
	 *
	 * A wrecked personal car is not a consumed crate: the player expects it to be replaced, because losing
	 * the car permanently would brick every mission that needs it. On by default, which is why this policy
	 * uses RebuildLost semantics regardless of the base recovery enum.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Wreck",
		meta = (DisplayName = "Replace When Wrecked"))
	bool bReplaceVehicleWhenWrecked = true;

	/** Seconds after a wreck before the replacement appears. Long enough to read as a consequence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Wreck",
		meta = (DisplayName = "Wreck Replacement Delay (s)", ClampMin = "0.0",
			EditCondition = "bReplaceVehicleWhenWrecked"))
	float WreckReplacementDelaySeconds = 20.f;

	/** Tag written into the record when a wreck was observed, so a HUD can explain the wait. Never read by the core. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Wreck",
		meta = (DisplayName = "Wrecked State Tag", Categories = "Territory.Spawn.State"))
	FGameplayTag WreckedStateTag;

	// ─── Configuration: ownership ───

	/**
	 * Require this bay to be claimed by a player before it produces anything.
	 *
	 * On by default: a personal-car bay that spawns before its owner has logged in puts a car nobody owns
	 * into the world, and on a dedicated server that is permanent litter. A world-owned slot (an ambulance
	 * bay) uses the ambulance policy instead, which has no such concept.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Ownership",
		meta = (DisplayName = "Require Owner Claim"))
	bool bRequireOwnerClaim = true;

private:
	/** True when the request's durable record says the car was destroyed rather than simply put down. */
	bool WasWrecked(const FTerritorySpawnRequest& Request) const;
};
