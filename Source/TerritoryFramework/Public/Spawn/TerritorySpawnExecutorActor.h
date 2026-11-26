#pragma once

#include "CoreMinimal.h"

#include "Spawn/TerritorySpawnExecutor.h"

#include "TerritorySpawnExecutorActor.generated.h"

class ATerritorySpawnAnchor;
class UNavigationSystemV1;

/**
 * The default executor for one addressable actor: a prop, a pickup, a unique world object, or any
 * future family whose "spawn" really is SpawnActor and whose "despawn" really is a teardown.
 *
 * This is the executor that makes the framework usable without writing C++ per entity family. A
 * designer points a policy at a class and gets spawn, retire, suspend and checkpoint for free. A
 * family that needs different behaviour (a vehicle restoring damage, a chunk actor flipping instance
 * bits) subclasses this and overrides only the parts that differ.
 *
 * What it deliberately does NOT do:
 *   - It never relocates. A blocked placement is a failed attempt, which the framework turns into a
 *     bounded retry. Moving a prop to make it fit would silently change an authored level.
 *   - It never spawns a Narrative NPC. Those must be removed through the Narrative death path, and a
 *     plain Destroy on a controlled character corrupts the activity and save state around it.
 *   - It never sets SpawnParameters.Owner. A parked actor with an owner gets a net connection, and a
 *     net connection changes which RPCs and replicated properties reach which client.
 *   - IT NEVER TOUCHES THE ACTOR'S PHYSICS, HEIGHT, VISIBILITY OR REPLICATION.
 *
 * That last rule is the most important line in this file and it was learned expensively.
 *
 * An earlier version of this class snapped a spawned actor to the ground, re-snapped it one tick later,
 * hid it while its physics settled, forced physics on so the settle could be measured, and revealed it
 * afterwards. Every step was individually defensible and each one was a bug in combination: the snap
 * measured degenerate spawn-frame bounds, the re-snap measured a pose that changed per frame, the settle
 * watcher measured a body that was not simulating, and the reveal put the actor back at whatever height
 * the previous step had left it at. The observed result was a spawned ambulance that could not be driven
 * the way the level's own ambulance could.
 *
 * A designer places an actor by eye at the transform it belongs at, and the Blueprint's own physics and
 * replication settings are what that actor uses. The framework does the same thing: it spawns the
 * authored class at the authored transform and then leaves it alone. If an authored transform is wrong,
 * that is an authoring problem with a visible fix, not something a spawn system should paper over at
 * runtime.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced,
	meta = (DisplayName = "Spawn Executor (Actor)"))
class TERRITORYFRAMEWORK_API UTerritorySpawnExecutorActor : public UTerritorySpawnExecutor
{
	GENERATED_BODY()

public:
	UTerritorySpawnExecutorActor();

	// ─── Capability ───

	virtual bool SupportsActorClass_Implementation(TSubclassOf<AActor> ActorClass) const override;

	// ─── Creation and removal ───

	virtual AActor* CreateSpawned_Implementation(ATerritorySpawnAnchor* Anchor, TSubclassOf<AActor> ActorClass,
		const FTransform& Transform) override;

	virtual void RetireSpawned_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor,
		ESpawnRetireMode Mode) override;

	virtual bool SuspendSpawned_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor) override;

	virtual bool ResumeSpawned_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor) override;

	// ─── External ownership ───

	virtual bool IsClaimedByExternalOwner_Implementation(AActor* Actor) const override;

	// ─── Configuration ───

	/**
	 * Whether a blocked placement is a hard failure or an acceptable one.
	 *
	 * Off, which is the framework default and the opposite of the vendor spawner's bNoFail=true. Two
	 * props intersecting is a level bug that must be reported and retried, not silently accepted; a
	 * designer who genuinely wants an overlapping spawn has to say so here, on this anchor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Placement",
		meta = (DisplayName = "Spawn Even If Blocked"))
	bool bSpawnEvenIfBlocked = false;

	/**
	 * Whether to project the authored transform down onto the navmesh before spawning.
	 *
	 * Z only. The authored X/Y is preserved exactly, because moving a designer's prop sideways to
	 * reach walkable ground is the same silent relocation the framework forbids everywhere else.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Placement",
		meta = (DisplayName = "Project To Navigation"))
	bool bProjectToNavigation = false;

	/** How far to search for navigation below the slot. Ignored when projection is off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Placement",
		meta = (DisplayName = "Projection Extent (cm)", ClampMin = "10.0", EditCondition = "bProjectToNavigation"))
	float NavigationProjectionExtent = 500.f;

	/** How hard the spawned actor is culled when a player is near. Zero leaves the class default alone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Replication",
		meta = (DisplayName = "Net Cull Distance Override (cm)", ClampMin = "0.0"))
	float NetCullDistanceOverrideCm = 0.f;

protected:
	/**
	 * Fill in the spawn parameters before the actor is created.
	 *
	 * Exists so a family can declare its own creation flags without copying the whole spawn routine:
	 * the vehicle executor adds RF_Transient here, because a framework-spawned car is remembered by its
	 * anchor's record and must never acquire a second durable record of its own.
	 */
	virtual void ConfigureSpawnParameters(FActorSpawnParameters& Params) const;

	/**
	 * Apply the executor's own policy to a freshly created actor.
	 *
	 * Virtual so a family can finish configuring its own type. The vehicle override clears the vendor
	 * save GUID, which is the one line that stops two records existing for one car. Nothing here may
	 * move the actor, hide it, or change its physics or replication: the spawned actor must behave
	 * exactly like the same Blueprint placed by hand.
	 */
	virtual void ConfigureSpawnedActor(AActor* Actor) const;

private:
	/**
	 * The transform the entity is actually created at: the wanted transform, optionally projected down
	 * onto navigation. Returns false only when projection was requested and found no ground, which is
	 * a failed attempt rather than a fallback to the unprojected spot.
	 */
	bool ResolveSpawnTransform(const FTransform& Wanted, FTransform& OutTransform) const;
};
