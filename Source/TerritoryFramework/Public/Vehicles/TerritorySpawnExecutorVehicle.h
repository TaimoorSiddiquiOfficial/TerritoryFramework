#pragma once

#include "CoreMinimal.h"

#include "Spawn/TerritorySpawnExecutorActor.h"

#include "TerritorySpawnExecutorVehicle.generated.h"

class ANarrativeVehicleBase;
class ATerritorySpawnAnchor;

/**
 * The executor for Narrative Pro vehicles.
 *
 * DELIBERATELY MINIMAL. This class exists so the spawn framework can create and destroy a vehicle, and
 * for nothing else. It does not touch the vehicle's physics, its replication rate, its inputs, its
 * suspension, or its position beyond the transform it was asked to spawn at.
 *
 * That restraint is the result of a long and expensive defect. An earlier version of this file tried to
 * be helpful: it snapped the car to the ground, hid it while the suspension settled, forced physics on so
 * the settle could be measured, applied a parking brake so it would not roll, and overrode the net update
 * rate. Every one of those was individually defensible and collectively they changed the car into
 * something a hand-placed ambulance is not. The symptom the user reported was precise and correct —
 * "spawner sy spawn hoi ambulance ko run nhi kr rhi" — the framework's car could not be driven the way the
 * level's own car could.
 *
 * The rule that replaced all of it: A FRAMEWORK VEHICLE MUST BEHAVE EXACTLY LIKE ONE DRAGGED OUT OF THE
 * CONTENT BROWSER. A designer places a car by eye at the height it looks right and leaves the Blueprint's
 * own physics, replication and input configuration alone. The framework does the same — it spawns the
 * authored Blueprint at the authored transform and stops.
 *
 * What is left, and why each line is still here:
 *
 *   1. RF_Transient. A framework car is remembered by its anchor and recreated by the sweep. It must not
 *      additionally be picked up as a net-startup actor, because UNarrativeSaveSubsystem::RemoveSingleActor
 *      marks such a record bDestroyed and every later load destroys the actor before the anchor exists.
 *
 *   2. Clearing the vendor save GUID. ANarrativeVehicleBase::GetActorGUID_Implementation returns
 *      VehicleSaveGUID while unoccupied, so a framework car left with an authored GUID gets TWO durable
 *      records: one on the anchor and one on the car. The vendor ships it zero, so today this is a no-op;
 *      it is kept as the one line that stops a future author from creating duplicates by accident.
 *
 *   3. Occupancy. "Can I remove this car" is not "does it have a controller" — a player can be mounted as a
 *      passenger whose pawn is momentarily unpossessed during a Native mount. That predicate already
 *      exists in the project, so it is delegated to rather than re-derived.
 *
 *   4. Health as a fraction, not a pointer. This is the "as last left, damaged or fresh" feature the spawn
 *      system was asked for. It only runs when the POLICY asks for it (Decision.bRestoreCheckpoint), so a
 *      policy that wants a serviced vehicle simply does not ask.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced,
	meta = (DisplayName = "Spawn Executor (Vehicle)"))
class TERRITORYFRAMEWORK_API UTerritorySpawnExecutorVehicle : public UTerritorySpawnExecutorActor
{
	GENERATED_BODY()

public:
	UTerritorySpawnExecutorVehicle();

	/** Vehicles only. A policy that points this executor at a crate is an authoring mistake worth failing. */
	virtual bool SupportsActorClass_Implementation(TSubclassOf<AActor> ActorClass) const override;

	virtual AActor* CreateSpawned_Implementation(ATerritorySpawnAnchor* Anchor, TSubclassOf<AActor> ActorClass,
		const FTransform& Transform) override;

	virtual void RetireSpawned_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor,
		ESpawnRetireMode Mode) override;

	// ─── Checkpoint ───

	virtual void CaptureCheckpoint_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor,
		FTerritorySpawnRecord& InOutRecord) override;

	virtual void ApplyCheckpoint_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor,
		const FTerritorySpawnRecord& Record, bool bRestoreCondition) override;

	// ─── External ownership ───

	/**
	 * Delegates to the project's existing occupancy predicate, which knows about mounted passengers as
	 * well as a possessing controller. This is the interlock that makes "never despawn a car somebody is
	 * using" a framework guarantee rather than a policy author's good intentions.
	 */
	virtual bool IsClaimedByExternalOwner_Implementation(AActor* Actor) const override;

public:
	/**
	 * Destroy a vehicle whose ASC cannot accept a health write.
	 *
	 * On by default, and it matches the assault subsystem's existing decision: an unusable ASC must not
	 * silently turn a nearly-wrecked car into a fresh one, because that is a free repair the economy never
	 * authorised. Off exists only for a project that tracks vehicle condition somewhere else entirely.
	 *
	 * This is the ONE place the executor is allowed to act on a car beyond creating and destroying it, and
	 * it only runs when the policy asked for the condition to be restored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Vehicle|Condition",
		meta = (DisplayName = "Destroy If Health Cannot Be Applied"))
	bool bDestroyIfHealthUnusable = true;

protected:
	/** Adds RF_Transient so a framework car never becomes a second durable record. */
	virtual void ConfigureSpawnParameters(FActorSpawnParameters& Params) const override;

	/**
	 * Clears the vendor save GUID, and does nothing else.
	 *
	 * Everything a previous version did here — net update rate, parking brake, possession watch — has been
	 * removed. Those are the vehicle's own business, and overriding them is what made a framework car
	 * behave differently from a hand-placed one.
	 */
	virtual void ConfigureSpawnedActor(AActor* Actor) const override;
};
