#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

#include "TerritorySpawnTypes.generated.h"

class AActor;
class ATerritorySpawnAnchor;

/**
 * Generic spawn framework — shared vocabulary.
 *
 * This header is the whole contract between the framework and any consumer (vehicles today,
 * destructible props and trees later). It must never include a vehicle, tree, guard, NPC or
 * traffic header: deleting every adapter folder must leave this file and the framework core
 * compiling unchanged.
 *
 * The split that keeps the framework honest:
 *   - CORE owns identity, durable record, lifecycle states, the safety interlock, scheduling,
 *     leases and determinism. A mis-authored data asset can therefore never destroy something
 *     that is in use.
 *   - POLICY owns decisions only ("should this slot hold an entity right now?").
 *   - EXECUTOR owns actions only ("create/destroy/suspend this entity family").
 */

// ═══════════════════════════════════════════════════════════════════════════════
// Enums
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * The five reachable lifecycle states. Retired is terminal for the session; every other state can
 * be left. The entity's own words (HOME / IN USE / ABSENT / RETURNING for a car, INTACT / STUMP
 * for a tree) are StateTags and Scalars inside the record, never core states.
 */
UENUM(BlueprintType)
enum class ESpawnLifecycleState : uint8
{
	Dormant UMETA(DisplayName = "Dormant",
		ToolTip = "The slot holds no verified entity and no spawn has been requested yet. An anchor that has never run stays here."),
	PendingSpawn UMETA(DisplayName = "Pending Spawn",
		ToolTip = "An entity is wanted but does not exist yet: assets are still streaming, a placement retry is pending, or the policy said wait."),
	Live UMETA(DisplayName = "Live",
		ToolTip = "A verified entity exists and is tracked. Every despawn interlock is evaluated against this state."),
	Suspended UMETA(DisplayName = "Suspended",
		ToolTip = "A verified entity still exists but is deliberately inert (streamed out, parked out of the world). Not a destroy."),
	Retired UMETA(DisplayName = "Retired",
		ToolTip = "Terminal for this session. The slot will not produce another entity until the anchor is re-created or the session ends.")
};

/**
 * What the core should do about the slot. None is the zero value on purpose: an unset or
 * mis-authored policy produces no action rather than an accidental spawn.
 */
UENUM(BlueprintType)
enum class ESpawnAction : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "Do nothing. The zero value: an unconfigured policy can never spawn or retire anything."),
	Spawn UMETA(DisplayName = "Spawn", ToolTip = "Create the entity. Still passes every spawn-side interlock before it happens."),
	Retire UMETA(DisplayName = "Retire", ToolTip = "Remove the entity using Retire Mode. Still passes every despawn-side interlock before it happens."),
	Suspend UMETA(DisplayName = "Suspend", ToolTip = "Make the entity inert without destroying it. Used for streaming and out-of-world parking."),
	Resume UMETA(DisplayName = "Resume", ToolTip = "Bring a suspended entity back into gameplay.")
};

/**
 * How an entity is removed. Destroy is the cheapest and the most dangerous; HideThenDestroy and
 * SuspendForStreaming exist because the project already paid for the bugs they prevent.
 */
UENUM(BlueprintType)
enum class ESpawnRetireMode : uint8
{
	Destroy UMETA(DisplayName = "Destroy",
		ToolTip = "Plain destroy. The core refuses this for Narrative NPC classes, which must go through the Narrative death path."),
	HideThenDestroy UMETA(DisplayName = "Hide Then Destroy",
		ToolTip = "Disable collision, hide, then destroy after a short latent grace. Removes the visible pop while replication catches up."),
	SuspendForStreaming UMETA(DisplayName = "Suspend For Streaming",
		ToolTip = "Keep the actor alive but strip gameplay presence, matching the existing streaming-suspend component pattern."),
	ReleaseToWorld UMETA(DisplayName = "Release To World",
		ToolTip = "Stop tracking the actor and leave it in the world under another owner's control. The slot becomes free.")
};

/**
 * Why an entity left. Deliberately mirrors the guard-post removal reasons without sharing the
 * enum: sharing would drag a guard-specific header into the generic core. A test asserts the
 * shared subset still matches, so the two cannot drift apart silently.
 */
UENUM(BlueprintType)
enum class ESpawnRemovalReason : uint8
{
	Killed UMETA(DisplayName = "Killed", ToolTip = "The entity was destroyed by gameplay damage."),
	ManualRemoval UMETA(DisplayName = "Manual Removal", ToolTip = "An explicit call removed it: admin, script, or authoring tool."),
	OwnerChanged UMETA(DisplayName = "Owner Changed", ToolTip = "Durable ownership moved, so the previous occupant no longer belongs here."),
	LoadReconcile UMETA(DisplayName = "Load Reconcile", ToolTip = "A save restore found the slot in a state the record no longer agrees with."),
	TerritoryDestroyed UMETA(DisplayName = "Territory Destroyed", ToolTip = "The owning Territory or Place was destroyed or reverted."),
	StreamingRetirement UMETA(DisplayName = "Streaming Retirement", ToolTip = "The owner cell streamed out. Durable occupancy is preserved, not cleared."),
	LeaseExpired UMETA(DisplayName = "Lease Expired", ToolTip = "A mission or system lease ended, so the slot returns to its own policy.")
};

/**
 * Why a lifecycle transition happened. Recorded with the transition so a save/load mismatch can be
 * diagnosed without a repro, and so the debug trace reads as a story rather than a state dump.
 */
UENUM(BlueprintType)
enum class ESpawnTransitionReason : uint8
{
	PolicyRequest UMETA(DisplayName = "Policy Request", ToolTip = "The policy asked for this transition and every interlock allowed it."),
	InitialSpawn UMETA(DisplayName = "Initial Spawn", ToolTip = "The first spawn after BeginPlay, before any player interaction."),
	SaveRestore UMETA(DisplayName = "Save Restore", ToolTip = "The transition came from a loaded record."),
	LeaseGranted UMETA(DisplayName = "Lease Granted", ToolTip = "A lease pinned the slot, so its policy can no longer retire it."),
	LeaseExpired UMETA(DisplayName = "Lease Expired", ToolTip = "The last lease ended and the slot's own policy applies again."),
	StreamingOut UMETA(DisplayName = "Streaming Out", ToolTip = "The owner cell left the world."),
	StreamingIn UMETA(DisplayName = "Streaming In", ToolTip = "The owner cell returned to the world."),
	PlacementBlocked UMETA(DisplayName = "Placement Blocked", ToolTip = "The authored transform was not clear, so the spawn was deferred instead of relocated."),
	ClassLoadFailed UMETA(DisplayName = "Class Load Failed", ToolTip = "The configured actor class never became resident."),
	RetryLimitReached UMETA(DisplayName = "Retry Limit Reached", ToolTip = "The bounded retry budget was exhausted and the slot was retired with an error."),
	HardTimeout UMETA(DisplayName = "Hard Timeout", ToolTip = "The slot stayed pending past its hard deadline and escalated once instead of spinning silently."),
	LoadReconcile UMETA(DisplayName = "Load Reconcile", ToolTip = "A load or a re-entrant sweep found the lifecycle state disagreeing with reality, so the slot was reconciled without spawning or retiring anything.")
};

/**
 * What a policy means by "recovery": what should happen after the entity the slot created stops
 * existing.
 *
 * Two values, and there is deliberately no third. An instant respawn is RebuildLost with a zero
 * delay; spelling it separately would be two names for one state, and the framework would have to
 * answer "which one wins" instead of just honouring the delay.
 */
/*
 * AUTHORING NOTE, deliberately placed OUTSIDE the enum body.
 *
 * UHT derives an enumerator's ToolTip from the doc comment immediately preceding it, and then
 * rejects an explicit ToolTip that disagrees ("Metadata key 'ToolTip' first seen with value X then
 * Y"). A comment inside the braces below therefore collides with the UMETA. Each value carries
 * exactly one source of documentation: its inline UMETA. Do not add /** *\/ blocks to these values.
 */
UENUM(BlueprintType)
enum class ESpawnPolicyRecoveryMode : uint8
{
	InitialSpawn UMETA(DisplayName = "Initial Spawn",
		ToolTip = "Create the entity once. If it is later destroyed, the slot records the absence and does not rebuild it."),

	RebuildLost UMETA(DisplayName = "Rebuild If Lost",
		ToolTip = "Rebuild the entity after it disappears, once the respawn delay has elapsed. Used by anything the world needs to keep existing.")
};

// ═══════════════════════════════════════════════════════════════════════════════
// Vehicle consumer enums
// ═══════════════════════════════════════════════════════════════════════════════

/*
 * AUTHORING NOTE, same rule as above: no /** *\/ blocks inside these braces. Each value carries its
 * own UMETA ToolTip and nothing else, because UHT compares a preceding doc comment against it and
 * fails the build when the two disagree.
 */

/**
 * Where a returning vehicle is placed.
 *
 * This is POLICY, not core: the framework only knows "the slot wants an entity again". Whether that
 * entity reappears at the pad or where the player abandoned it is a taste decision that differs
 * between a personal car and a duty ambulance, so it lives in the policy asset and never in the
 * framework's state machine.
 */
UENUM(BlueprintType)
enum class ESpawnVehicleReturnPlacement : uint8
{
	AtAnchor UMETA(DisplayName = "At Anchor",
		ToolTip = "Always reappear at the anchor's authored transform. Predictable, and the only option that cannot strand a car inside geometry the player parked next to."),

	AsLastLeft UMETA(DisplayName = "As Last Left",
		ToolTip = "Reappear where the vehicle was when it was last put down. The player's mental map of 'my car is over there' survives, at the cost of trusting a saved transform.")
};

/**
 * What condition a returning vehicle comes back in.
 *
 * Deliberately separate from placement rather than a combined list of four outcomes: a designer who
 * wants "same spot, repaired" would otherwise need a fourth enumerator, and every later combination
 * would need a fifth.
 */
UENUM(BlueprintType)
enum class ESpawnVehicleReturnCondition : uint8
{
	PreserveDamage UMETA(DisplayName = "Preserve Damage",
		ToolTip = "Restore the health fraction the vehicle had when it was put away. A car the player nearly wrecked is still nearly wrecked."),

	FreshHealth UMETA(DisplayName = "Fresh Health",
		ToolTip = "Restore full health regardless of what was saved. Use when the fiction is that a depot or station serviced the vehicle.")
};

// ═══════════════════════════════════════════════════════════════════════════════
// Framework limits — one definition, both translation units
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * The core's numeric limits. Deliberately declared once, in the shared header, rather than as a
 * file-local constant in each .cpp: unity builds paste several .cpp files into one translation unit,
 * so two file-local constants with the same name and different values become an ambiguous-symbol
 * compile error or, worse, silently pick one. The retry budget also has exactly one authoritative
 * home, so the anchor and the subsystem can never drift apart on how many failures is "too many".
 *
 * These are core rules, not tuning. A designer tunes grace periods and return modes through the
 * policy asset; they never get to change how many times a slot may fail before it stops retrying.
 */
namespace TerritorySpawnLimits
{
	/** A spawn failure past this many consecutive attempts retires the slot with one error, never retries forever. */
	constexpr int32 MaxConsecutiveSpawnFailures = 5;

	/** How long a slot may sit in PendingSpawn before it escalates once instead of spinning silently. */
	constexpr double HardReturnTimeoutSeconds = 120.0;
}

/**
 * Whether spawn-framework diagnostics should log. Declared here and defined once in
 * TerritorySpawnTypes.cpp for the same unity-build reason as the limits above: a file-local copy in
 * each .cpp would become an ambiguous overload the moment two of them share a translation unit.
 */
TERRITORYFRAMEWORK_API bool ShouldLogTerritorySpawn();

// ═══════════════════════════════════════════════════════════════════════════════
// Durable record — the only truth the save system sees
// ═══════════════════════════════════════════════════════════════════════════════

/** One named number inside the durable record. An array, not a TMap: a TMap's iteration order is not a save-stability guarantee. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritorySpawnScalar
{
	GENERATED_BODY()

	FTerritorySpawnScalar() = default;
	FTerritorySpawnScalar(FName InKey, float InValue) : Key(InKey), Value(InValue) {}

	/** Stable machine name such as HealthFraction or RemainingYield. Never a display string. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Spawn|Record")
	FName Key;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Spawn|Record")
	float Value = 0.f;
};

/**
 * Durable per-anchor spawn state. Saved on the anchor, never on the spawned actor: the vehicle's
 * own save record is structurally unable to hold this because a controlled vehicle returns an
 * invalid durable GUID while a player drives it.
 *
 * Contains no hard object pointer by design. The class is a soft reference so a record never drags
 * a 30 MB vehicle Blueprint into memory, and the transform is stored so load cannot reroll a
 * placement decision.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritorySpawnRecord
{
	GENERATED_BODY()

	/** Bump when a field's meaning changes. A record written by newer code is refused, never reinterpreted. */
	static constexpr int32 CurrentSchemaVersion = 1;

	/** Oldest version this code can still read. Raise only with a real migration path. */
	static constexpr int32 MinimumSupportedSchemaVersion = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Spawn|Record")
	int32 SchemaVersion = CurrentSchemaVersion;

	/** Durable identity of the spawned actor, when that actor family has one. Invalid is legitimate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Spawn|Record")
	FGuid SpawnedGuid;

	/** Soft on purpose. A record must never keep a hard class reference alive. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Spawn|Record")
	TSoftClassPtr<AActor> ActorClass;

	/** Where the entity was actually placed. Written on every realized spawn so load cannot choose a different answer. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Spawn|Record")
	FTransform Transform = FTransform::Identity;

	/** True when this slot is supposed to hold an entity. False means nothing was spawned and nothing should be restored. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Spawn|Record")
	bool bHasInstance = false;

	/** True when the entity was destroyed by gameplay rather than retired by policy. Drives "wrecked" style recovery. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Spawn|Record")
	bool bWasDestroyed = false;

	/** The consumer's own words for the current condition. The core never interprets these tags. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Spawn|Record")
	FGameplayTagContainer StateTags;

	/** The consumer's own numbers. Sorted and de-duplicated before save; the core never interprets these either. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Spawn|Record")
	TArray<FTerritorySpawnScalar> Scalars;

	/** World game time of the last committed transition, or -1 before the first one. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Spawn|Record")
	double LastTransitionGameTime = -1.0;

	/**
	 * Monotonic count of realized spawns. A client actor is destroyed by relevancy loss, so
	 * "the car came back" and "I came back" look identical; the generation is how the return is
	 * told apart from the walk-back. int32, because a uint16 would wrap inside a long campaign.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Spawn|Record")
	int32 Generation = 0;

	// ─── Scalar helpers (the consumer's numbers, never interpreted by the core) ───

	void SetScalar(FName Key, float Value);
	bool GetScalar(FName Key, float& OutValue) const;
	float GetScalarOrDefault(FName Key, float DefaultValue) const;
	bool RemoveScalar(FName Key);

	/** Sort by key and collapse duplicates keeping the newest value, so the saved bytes are stable across runs. */
	void PrepareScalarsForSave();

	// ─── Integrity ───

	/**
	 * True when the stored transform is usable. A NaN coordinate or anything past a billion
	 * centimetres is a corrupt record, and fabricating a position for it is worse than refusing.
	 */
	bool IsTransformUsable() const;

	/** True when this code understands the record. A newer schema is refused rather than guessed at. */
	bool IsSchemaReadable() const;

	/** Clear the per-instance half while keeping identity. Used when a consumer deliberately discards condition. */
	void ResetInstanceState();
};

// ═══════════════════════════════════════════════════════════════════════════════
// Transient request / decision — never saved
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Everything a policy is allowed to consider when it decides. Built fresh each evaluation, so a
 * policy cannot cache a stale view and cannot mutate framework state. Never saved.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritorySpawnRequest
{
	GENERATED_BODY()

	/** The anchor's baked durable identity. Deterministic ordering and every log line key off this. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Request")
	FGuid AnchorGuid;

	/** Who this slot belongs to, when it has an owner. Empty for world-owned slots such as a hospital bay. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Request")
	FName OwnerKey;

	/** The policy's own identity tag, copied so a policy can branch without reading itself. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Request")
	FGameplayTag EntityTag;

	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Request")
	ESpawnLifecycleState CurrentState = ESpawnLifecycleState::Dormant;

	/** Current world game time. Passed in rather than read in the policy so every evaluation in one sweep agrees. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Request")
	double NowGameTime = 0.0;

	/** The anchor's live transform. The policy decides whether the record transform or this one is current. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Request")
	FTransform AnchorTransform = FTransform::Identity;

	/** How many consecutive create attempts have failed. Policy authors use this to stop retrying gracefully. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Request")
	int32 ConsecutiveFailures = 0;

	/** Game time the slot entered PendingSpawn, or -1 when it is not pending. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Request")
	double PendingSinceGameTime = -1.0;

	/** True while at least one lease pins this slot. A leased slot cannot be retired by its own policy. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Request")
	bool bHasLiveLease = false;

	/**
	 * Whether a live entity is actually tracked right now.
	 *
	 * CurrentState alone cannot answer this: a slot can be Live with its actor already destroyed by
	 * gameplay (a wrecked car, a felled tree) and the reconcile has not run yet. A policy that needs
	 * "is something physically here" must read this flag rather than inferring it from the state.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Request")
	bool bHasEntity = false;

	/**
	 * Where the live entity IS, not where it was placed.
	 *
	 * The record's transform is a placement memory; a vehicle driven across the district has a record
	 * that still says "at home". A policy that wants to know whether the entity has MOVED since it was
	 * placed needs the entity's current transform, and only the framework can read that. Meaningless
	 * when bHasEntity is false.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Request")
	FTransform EntityTransform = FTransform::Identity;

	/**
	 * True when the record describes an entity that has not been materialised yet.
	 *
	 * This is the load case, and it is NOT the same fact as "the entity was destroyed". A slot restored
	 * from a save arrives with bHasInstance set and nothing in the world, because the record is a
	 * promise that an entity exists, not the entity itself. A slot whose car was wrecked also arrives
	 * with nothing in the world, but the record says so: bWasDestroyed is set and the recovery policy
	 * applies.
	 *
	 * A policy that reads only bHasInstance cannot tell these apart, and will either rebuild a wreck
	 * that was authored to stay gone, or refuse to restore a car that was authored to come back. This
	 * flag is the framework's own answer to "do I owe this slot an entity that I have not delivered yet",
	 * so the two cases stop sharing one shape.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Request")
	bool bAwaitingRestore = false;

	/**
	 * A read-only copy of the durable record. Copied in rather than referenced so a policy cannot
	 * mutate saved state through its input, and so "as last left" style decisions can read the stored
	 * transform and the consumer's own scalars without the policy needing a second accessor.
	 *
	 * This is the record as it stood BEFORE this evaluation. Nothing the policy returns here is
	 * written back: the framework commits the record itself, at its own commit points.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Request")
	FTerritorySpawnRecord Record;
};

/**
 * The policy's answer. The core may only downgrade it, never upgrade it: a decision that asks for
 * a retire is re-checked against every interlock before anything is touched.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritorySpawnDecision
{
	GENERATED_BODY()

	/** None is the zero value. An unset policy therefore produces no action rather than an accidental spawn. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Decision")
	ESpawnAction Action = ESpawnAction::None;

	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Decision")
	ESpawnRetireMode RetireMode = ESpawnRetireMode::Destroy;

	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Decision")
	ESpawnTransitionReason Reason = ESpawnTransitionReason::PolicyRequest;

	/** How long the entity must already have been live before this retire may proceed. The framework owns the rule; the policy owns the number. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Decision")
	float MinimumLiveSeconds = 0.f;

	/** How long to wait before retrying a failed create. Zero retries on the next sweep. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Decision")
	float RetryDelaySeconds = 0.f;

	/** Where the entity should appear. Ignored unless Action says Spawn or Resume. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Decision")
	FTransform DesiredTransform = FTransform::Identity;

	/** Which class to instantiate. Left unset, the anchor's configured class is used. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Decision")
	TSoftClassPtr<AActor> DesiredClass;

	/**
	 * Whether a freshly spawned entity should be handed the condition the record remembers.
	 *
	 * Default true: a checkpoint is the whole reason the record keeps scalars, and a consumer that
	 * captures damage expects it back. A policy that wants the entity serviced instead sets this false,
	 * and that is the ONLY way to turn restoration off — the executor no longer carries a flag of its
	 * own, because two switches for one rule meant one of them silently won.
	 *
	 * The record is still captured on the way out. This gates restoration, not observation: the slot's
	 * own log of what was there must not depend on a design preference.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Decision")
	bool bRestoreCheckpoint = true;

	/** True when this decision actually asks the core to do something. */
	bool IsActionable() const { return Action != ESpawnAction::None; }
};

/**
 * A transient hold on a slot. A lease stops the owning policy from retiring the occupant: it is how
 * a mission borrows an ambulance without the bay's own restock rule destroying it mid-job.
 *
 * Never saved. A saved lease whose holder does not exist after load is a permanent lock, so the
 * holder re-acquires from its own durable state instead.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritorySpawnLease
{
	GENERATED_BODY()

	/** Server-issued identity. Used to release exactly this lease, never "whatever is held". */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Lease")
	FGuid LeaseId;

	/** The anchor this lease pins. A lease is granted against a specific slot, not against an entity. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Lease")
	FGuid AnchorGuid;

	/** Weak because a lease must never keep a finished mission object alive. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Lease")
	TWeakObjectPtr<UObject> Holder;

	/** Why the slot is pinned, as a stable semantic tag. Never a display string. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Lease")
	FGameplayTag Purpose;

	/** World game time past which the lease stops counting, or a negative value for no expiry. A lease always expires, otherwise a crash is a permanent lock. */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn|Lease")
	double ExpiryGameTime = -1.0;

	bool IsValidLease(double NowGameTime) const
	{
		if (!LeaseId.IsValid() || !Holder.IsValid()) return false;
		return ExpiryGameTime < 0.0 || NowGameTime < ExpiryGameTime;
	}
};
