#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Spawn/TerritorySpawnPolicy.h"

#include "TerritorySpawnPolicyVehicle.generated.h"

/**
 * A managed vehicle bay: one slot, one vehicle, kept where every consumer looks for it.
 *
 * This is the GENERAL vehicle policy. A police car, a taxi, a fire engine and an ambulance are the
 * same rule with different numbers, so they are one class configured per anchor rather than one class
 * per noun. The previous shape had an ambulance-specific class, which meant a designer who wanted a
 * second managed vehicle type had to ask for C++ — that is the wrong dependency for a data problem.
 *
 * What it owns:
 *
 *   - Rebuild. A vehicle that is destroyed is replaced after ReplacementDelaySeconds. A district with
 *     no vehicle where the HUD says there is one is a bug the player can see from the street.
 *
 *   - Reclaim. A vehicle driven away and abandoned comes home, so the next consumer that walks up to
 *     the bay finds one. This is the case the whole spawn system exists for; without it the bay
 *     believes it still holds a vehicle forever and the next call for help finds an empty pad.
 *
 *   - Condition. Whether damage survives a rebuild is a design decision, and it belongs here rather
 *     than on the executor. bPersistDamage is read by EvaluateDecision and travels to the framework on
 *     the decision itself (bRestoreCheckpoint), so the switch a designer ticks is the switch the code
 *     obeys. Before this was wired, the details-panel checkbox changed nothing.
 *
 * What it does NOT own: who the slot belongs to. Ownership is the personal-vehicle policy's job, and a
 * bay that waits for a claim is a different product decision, not a flag on this one.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced,
	meta = (DisplayName = "Spawn Policy (Vehicle Bay)"))
class TERRITORYFRAMEWORK_API UTerritorySpawnPolicyVehicle : public UTerritorySpawnPolicy
{
	GENERATED_BODY()

public:
	UTerritorySpawnPolicyVehicle();

	// ─── Identity ───

	virtual FGameplayTag GetEntityTag_Implementation() const override;

	virtual TSubclassOf<UTerritorySpawnExecutor> GetExecutorClass_Implementation() const override;

	virtual TSoftClassPtr<AActor> GetDesiredActorClass_Implementation() const override;

	/**
	 * Whether this bay accepts a pin of the given purpose.
	 *
	 * Overridden rather than left at the framework default because bAllowMissionLeases is a switch a
	 * designer actually sets, and a switch nothing reads is a lie in the details panel.
	 */
	virtual bool AllowsLease_Implementation(FGameplayTag Purpose) const override;

	// ─── Decisions ───

	virtual FTerritorySpawnDecision EvaluateDecision_Implementation(const FTerritorySpawnRequest& Request) const override;

	// ─── Configuration ───

	/** Entity identity for buckets, logs and validation. A child of Territory.Spawn.Entity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Identity",
		meta = (DisplayName = "Entity Tag", Categories = "Territory.Spawn.Entity"))
	FGameplayTag EntityTag;

	/** The vehicle Blueprint this bay manages. Soft so an unloaded district costs a path string. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Identity",
		meta = (DisplayName = "Vehicle Class"))
	TSoftClassPtr<AActor> VehicleClass;

	/**
	 * Seconds before a lost vehicle is rebuilt.
	 *
	 * Not zero by default: a vehicle destroyed in front of the player reappearing the next frame reads
	 * as a bug, and a district that gets a new one while the wreck is still smoking has no consequence.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Return",
		meta = (DisplayName = "Replacement Delay (s)", ClampMin = "0.0"))
	float ReplacementDelaySeconds = 45.f;

	/**
	 * Where a rebuilt vehicle appears. Always the authored bay by default.
	 *
	 * "As last left" is offered because a job may drive the vehicle somewhere and the district may want
	 * it to stay there; but a bay is what a depot actually has, so the default is the pad.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Return",
		meta = (DisplayName = "Replacement Placement"))
	ESpawnVehicleReturnPlacement ReplacementPlacement = ESpawnVehicleReturnPlacement::AtAnchor;

	/**
	 * Carry damage across a rebuild.
	 *
	 * Off by default, and the off case is a real design decision rather than laziness: a depot services
	 * its vehicles, so a player who wrecks the one vehicle and then needs a second job expects a working
	 * one. Turning this on makes damage a lasting consequence across jobs, which is a different game.
	 *
	 * This is honoured through FTerritorySpawnDecision::bRestoreCheckpoint. The record still captures
	 * the condition it observed — the record is a log of reality — but a rebuild of this bay starts
	 * fresh unless a designer says otherwise.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Condition",
		meta = (DisplayName = "Persist Damage Between Rebuilds"))
	bool bPersistDamage = false;

	/**
	 * Retire mode used when this policy needs the bay emptied.
	 *
	 * HideThenDestroy rather than Destroy because the reclaim is a relocation, not a death: the vehicle
	 * is serviced and re-dispatched, so nothing about it should read as destroyed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Return",
		meta = (DisplayName = "Retire Mode"))
	ESpawnRetireMode RetireMode = ESpawnRetireMode::HideThenDestroy;

	/**
	 * Bring the vehicle home once it has been abandoned somewhere in the city.
	 *
	 * This is the case the spawn system exists for: a player takes the district's vehicle on a job,
	 * parks it outside a shop, and walks away. Without a reclaim, the bay believes it still holds a
	 * vehicle forever, so the NEXT consumer finds an empty pad and no way to get one.
	 *
	 * Off by default would be the timid choice, and it is the wrong one here: an abandoned managed
	 * vehicle is not a design preference, it is a leak. What the guard rails below protect is not the
	 * feature but the player — see ReclaimSafeRadiusCm, which is what stops a vehicle vanishing while
	 * its driver is still standing next to it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Reclaim",
		meta = (DisplayName = "Reclaim When Abandoned"))
	bool bReclaimWhenAbandoned = true;

	/**
	 * How far the vehicle must be from its bay before a reclaim is considered.
	 *
	 * A vehicle parked ON its bay is already where the reclaim would put it, so retiring it would be a
	 * destroy-and-respawn with nothing to show for it. 10 m is roughly a forecourt.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Reclaim",
		meta = (DisplayName = "Reclaim Distance From Bay (cm)", ClampMin = "0.0",
			EditCondition = "bReclaimWhenAbandoned"))
	float ReclaimDistanceCm = 1000.f;

	/**
	 * How close a player may be, in centimetres, for a reclaim to proceed.
	 *
	 * The distance from the bay alone is not enough: a player who parked the vehicle and is now loading
	 * a patient into the back is far from the bay and very close to the vehicle. Retiring it would
	 * delete it in front of them. So a reclaim also requires that nobody is standing near it.
	 *
	 * 25 m is deliberately generous. The cost of being too wide is that a reclaim is delayed until the
	 * player walks away; the cost of being too narrow is a vehicle disappearing beside its driver.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Reclaim",
		meta = (DisplayName = "Player Safe Radius (cm)", ClampMin = "0.0",
			EditCondition = "bReclaimWhenAbandoned"))
	float ReclaimSafeRadiusCm = 2500.f;

	/**
	 * How long the vehicle must have been left alone before a reclaim.
	 *
	 * Measured from the vehicle's own live time, not from a policy timer: a policy that kept a stopwatch
	 * would need state, and state on an instanced data-template object is the kind of thing that
	 * survives a respawn and then behaves differently for reasons nobody can see.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Reclaim",
		meta = (DisplayName = "Reclaim Delay (s)", ClampMin = "0.0",
			EditCondition = "bReclaimWhenAbandoned"))
	float ReclaimDelaySeconds = 60.f;

	/**
	 * Whether a mission lease may pin this bay.
	 *
	 * On by default: this is the case the lease system exists for. A mission borrows the vehicle, the
	 * bay's own restock rule must not pull it out from under the objective, and the lease is released
	 * when the mission ends.
	 *
	 * The order of these two rules matters, and it is the framework's, not this policy's: a live lease
	 * vetoes a retire before the reclaim above is even considered. That is why the reclaim cannot cancel
	 * a mission in progress, no matter how far the vehicle has been driven.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Lease",
		meta = (DisplayName = "Allow Mission Leases"))
	bool bAllowMissionLeases = true;
};
