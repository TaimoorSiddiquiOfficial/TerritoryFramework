#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"

#include "Spawn/TerritorySpawnTypes.h"

#include "TerritorySpawnExecutor.generated.h"

class AActor;
class ATerritorySpawnAnchor;

/**
 * An executor performs the actions for exactly one entity family: how to create a vehicle, how to
 * create a prop, how to suspend a streamed actor.
 *
 * It owns ACTIONS ONLY. It never decides when or why: no timers, no grace windows, no "should this
 * exist" logic. The framework hands it an already-approved instruction, so the one place that knows
 * how a car differs from a tree is also the one place that cannot schedule anything.
 *
 * Implementations ship with their consumer, never in Public/Spawn.
 */
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class TERRITORYFRAMEWORK_API UTerritorySpawnExecutor : public UObject
{
	GENERATED_BODY()

public:
	UTerritorySpawnExecutor();

	virtual UWorld* GetWorld() const override;

	// ─── Capability ───

	/**
	 * Whether this executor can create this actor class at all. Checked once while configuring the
	 * anchor, and again in the editor validator, so a policy wired to the wrong executor fails
	 * loudly at authoring time instead of silently at runtime.
	 *
	 * This is also where the framework's hard exclusions live in practice: the default actor executor
	 * refuses Narrative NPC classes, because their removal must route through the Narrative death
	 * path rather than a plain Destroy.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Spawn|Executor")
	bool SupportsActorClass(TSubclassOf<AActor> ActorClass) const;
	virtual bool SupportsActorClass_Implementation(TSubclassOf<AActor> ActorClass) const;

	// ─── Creation and removal ───

	/**
	 * Create the entity at the given transform. Return null when placement was blocked or the class
	 * was not resident; the framework turns that into a bounded retry, never into a relocated spawn.
	 *
	 * Must be server-only. Implementations assert authority rather than trusting the caller.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Spawn|Executor")
	AActor* CreateSpawned(ATerritorySpawnAnchor* Anchor, TSubclassOf<AActor> ActorClass, const FTransform& Transform);
	virtual AActor* CreateSpawned_Implementation(ATerritorySpawnAnchor* Anchor, TSubclassOf<AActor> ActorClass, const FTransform& Transform);

	/**
	 * Remove the entity in the approved way. The framework has already cleared every interlock, so an
	 * implementation may act directly; it should still use the project's existing teardown helpers for
	 * its family rather than a bare Destroy.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Spawn|Executor")
	void RetireSpawned(ATerritorySpawnAnchor* Anchor, AActor* Actor, ESpawnRetireMode Mode);
	virtual void RetireSpawned_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor, ESpawnRetireMode Mode);

	/** Make the entity inert without destroying it. Used for streaming and out-of-world parking. */
	UFUNCTION(BlueprintNativeEvent, Category = "Spawn|Executor")
	bool SuspendSpawned(ATerritorySpawnAnchor* Anchor, AActor* Actor);
	virtual bool SuspendSpawned_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor);

	/** Return a suspended entity to gameplay. Return false when it cannot be restored, which retires the slot. */
	UFUNCTION(BlueprintNativeEvent, Category = "Spawn|Executor")
	bool ResumeSpawned(ATerritorySpawnAnchor* Anchor, AActor* Actor);
	virtual bool ResumeSpawned_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor);

	// ─── Checkpoint ───

	/**
	 * Copy the entity's condition into the durable record: StateTags and Scalars only. The core never
	 * interprets what is written here, which is what lets one record type hold a car's damage and a
	 * tree's harvest state without either concept entering the framework.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Spawn|Executor")
	void CaptureCheckpoint(ATerritorySpawnAnchor* Anchor, AActor* Actor, FTerritorySpawnRecord& InOutRecord);
	virtual void CaptureCheckpoint_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor, FTerritorySpawnRecord& InOutRecord);

	/**
	 * Push a saved condition back onto a freshly created entity. Called once, immediately after
	 * CreateSpawned, so the consumer controls what "restore" means for its family.
	 *
	 * bRestoreCondition comes from the policy's own decision, not from a flag on the executor. That is
	 * deliberate: whether a serviced vehicle comes back repaired is a design decision about the GAME,
	 * and the object that decides when the vehicle exists is the object that should decide what
	 * condition it arrives in. An executor that carried its own copy of the switch could disagree with
	 * the policy that produced the spawn, and the losing side would be invisible.
	 *
	 * False means "create it fresh". It does not mean "forget what was there": CaptureCheckpoint has
	 * already written the observed condition into the record on the way out, and a slot's log of what
	 * it held must not depend on a design preference.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Spawn|Executor")
	void ApplyCheckpoint(ATerritorySpawnAnchor* Anchor, AActor* Actor, const FTerritorySpawnRecord& Record,
		bool bRestoreCondition);
	virtual void ApplyCheckpoint_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor,
		const FTerritorySpawnRecord& Record, bool bRestoreCondition);

	// ─── External ownership ───

	/**
	 * True when something outside the framework currently controls this actor, so the framework must
	 * not touch it. The vehicle executor delegates to the existing occupancy predicate instead of
	 * re-implementing "is a player driving this", which is how the framework inherits that guarantee
	 * without knowing what a vehicle is.
	 *
	 * Unknown must answer TRUE. A wrong false destroys a player's car; a wrong true only delays a spawn.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Spawn|Executor")
	bool IsClaimedByExternalOwner(AActor* Actor) const;
	virtual bool IsClaimedByExternalOwner_Implementation(AActor* Actor) const;

	/** The anchor that owns this instanced executor, resolved through the outer chain. Null when detached. */
	UFUNCTION(BlueprintPure, Category = "Spawn|Executor")
	ATerritorySpawnAnchor* GetOwningAnchor() const;
};
