#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryAudioTypes.h"
#include "TerritoryTypes.generated.h"

class UTerritoryStealthProfile;

DECLARE_LOG_CATEGORY_EXTERN(LogTerritory, Log, All);

class ATerritoryVolume;
class UNarrativeEvent;
class UTalesComponent;

// ═══════════════════════════════════════════════════════════════════════════════
// Transition Context (P0-03)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Explicit context for territory state transitions. Replaces GetFirstPlayerController().
 * Pass the actual instigator, pawn, and faction that caused the transition.
 * Global/scripted transitions use a default-constructed (empty) context.
 */
USTRUCT(BlueprintType)
struct FTerritoryTransitionContext
{
	GENERATED_BODY()

	/** The actor that initiated the transition (e.g., capturing pawn). */
	UPROPERTY(BlueprintReadWrite, Category="Territory|Transition")
	TObjectPtr<AActor> Instigator = nullptr;

	/** The pawn involved (for condition/event evaluation). */
	UPROPERTY(BlueprintReadWrite, Category="Territory|Transition")
	TObjectPtr<APawn> TargetPawn = nullptr;

	/** The player controller (may be null on dedicated servers or AI-driven captures). */
	UPROPERTY(BlueprintReadWrite, Category="Territory|Transition")
	TObjectPtr<APlayerController> PlayerController = nullptr;

	/** Tales component for quest/dialogue event context. */
	UPROPERTY(BlueprintReadWrite, Category="Territory|Transition")
	TObjectPtr<UTalesComponent> TalesComponent = nullptr;

	/** The faction requesting or causing the transition. */
	UPROPERTY(BlueprintReadWrite, Category="Territory|Transition")
	FGameplayTag RequestingFaction;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Enums
// ═══════════════════════════════════════════════════════════════════════════════

UENUM(BlueprintType)
enum class ETerritoryState : uint8
{
	Unclaimed UMETA(DisplayName="Unclaimed", ToolTip="No faction owns this place. Example: an abandoned farm that any hostile faction may capture."),
	Claimed UMETA(DisplayName="Claimed", ToolTip="One faction securely owns this Territory. Capture is the action; Claimed is the stable result. A real Faction A to Faction B handover runs the old owner's Claimed Exit Events and the new owner's Claimed Entry Events. Same-owner resets do not refire."),
	Contested UMETA(DisplayName="Contested", ToolTip="A real physical capture has started, or an aggregate City/District has children owned by different factions. Entry Events run once when the state changes into Contested; they do not repeat every capture tick. Merely walking through a Place triggers this only when its story-bounds rules actually register the player as a valid attacker."),
	Locked UMETA(DisplayName="Locked (Legacy)", ToolTip="Legacy serialized value. Runtime lock availability is stored separately so a locked enemy Place can still contribute political power.")
};

/** Capture availability is independent from political control. */
UENUM(BlueprintType)
enum class ETerritoryAvailability : uint8
{
	Unlocked UMETA(DisplayName="Unlocked", ToolTip="This Territory may participate in gameplay, subject to normal capture and diplomacy rules."),
	Locked UMETA(DisplayName="Locked", ToolTip="This Territory is unavailable until its Narrative unlock conditions pass. Existing ownership and power are preserved.")
};

/** Scope used by the Narrative Unlock Territory event. */
UENUM(BlueprintType)
enum class ETerritoryUnlockScope : uint8
{
	AutomaticHierarchy UMETA(DisplayName="Automatic Hierarchy (Recommended)", ToolTip="Place: open its ancestor path and only that Place. District/City: open the target and eligible descendants while respecting every local lock condition."),
	ExactOnly UMETA(DisplayName="Exact Target Only", ToolTip="Attempt only the selected Territory and do not open ancestors, siblings, or descendants."),
	ForceExact UMETA(DisplayName="Force Exact Target", ToolTip="Trusted story override that bypasses lock conditions only for the exact target."),
	ForceHierarchy UMETA(DisplayName="Force Complete Hierarchy", ToolTip="Trusted story override that bypasses lock conditions for the target path and all descendants.")
};

UENUM(BlueprintType)
enum class ETerritoryUnlockOutcome : uint8
{
	Unlocked,
	AlreadyUnlocked,
	BlockedByCondition,
	MissingRuntimeTerritory,
	SkippedBlockedParent,
	InvalidTarget
};

/**
 * Designer-facing new-campaign state. Automatic is the migration-safe default.
 * Contested is deliberately excluded because a real contest needs physical participants
 * or a hierarchy disagreement at runtime.
 */
UENUM(BlueprintType)
enum class ETerritoryInitialState : uint8
{
	Automatic UMETA(DisplayName="Automatic (Recommended)", ToolTip="Start Claimed when Initial Owning Faction is set; otherwise start Unclaimed. Existing Starts Locked assets remain locked until migrated."),
	Unclaimed UMETA(DisplayName="Unclaimed", ToolTip="Start with no owner, even if Initial Owning Faction is filled."),
	Claimed UMETA(DisplayName="Claimed", ToolTip="Start securely owned by Initial Owning Faction. If that faction is empty, the safe result is Unclaimed."),
	Locked UMETA(Hidden, DisplayName="Locked (Legacy)", ToolTip="Serialized compatibility value. Use Initial Availability instead.")
};

/**
 * The single place that answers: "what Availability does a NEW CAMPAIGN start this Territory with?"
 *
 * Combine the two properties like this:
 *   - The legacy `Initial State` wins ONLY when it is the old `Locked` value. Assets authored long
 *     ago stored "starts locked" there, before `Initial Availability` existed, so those assets must
 *     keep starting locked. That value is hidden in the editor and is never chosen by hand.
 *   - In every other case the modern `Initial Availability` is passed straight through, including
 *     when it is `Unlocked`.
 *
 * Why this is a function and not four copies of one line: this rule was written out identically at
 * every site that needed it (the placed actor, the replicated world state, Definition application, and
 * the Story outcome analyzer). Four copies of a compatibility rule is four chances for one of them to
 * drift, and a Territory that starts locked in the analyzer but unlocked at runtime is the kind of bug
 * that only shows up in a save game. There is now exactly one answer.
 *
 * Example: an asset with Initial State = Locked and Initial Availability = Unlocked resolves to
 * Locked (the legacy value wins). The same asset with Initial State = Automatic resolves to whatever
 * Initial Availability says, i.e. Unlocked.
 */
TERRITORYFRAMEWORK_API ETerritoryAvailability TerritoryResolveInitialAvailability(
	ETerritoryInitialState InitialState,
	ETerritoryAvailability InitialAvailability);

/**
 * The single place that answers: "does a NEW CAMPAIGN start this Territory owned or unowned?"
 *
 * The rule, in full:
 *   - `Unclaimed` means unclaimed. It wins even when an Initial Owning Faction is filled, because
 *     that is literally what the option promises.
 *   - Every other value — `Automatic`, `Claimed`, and the legacy `Locked` — asks the same question:
 *     is an Initial Owning Faction set? Yes starts owned, no starts unowned. They differ in
 *     *availability*, not in who holds the ground at the start.
 *   - So "Claimed" with an empty faction resolves to Unclaimed. The game never allows the
 *     contradictory state "owned by nobody".
 *
 * `bHasInitialOwningFaction` is passed in rather than the tag itself, so the same function serves a
 * Definition asset, a placed actor, and the replicated world state without any of them needing the
 * other's type.
 *
 * Example: Initial State = Automatic with no Initial Owning Faction starts Unclaimed; fill the
 * faction in and the same Territory starts Claimed. Set Initial State = Unclaimed and it starts
 * Unclaimed even with the faction filled.
 */
TERRITORYFRAMEWORK_API ETerritoryState TerritoryResolveInitialPoliticalState(
	ETerritoryInitialState InitialState,
	bool bHasInitialOwningFaction);

UENUM(BlueprintType)
enum class ETerritoryControlMode : uint8
{
	Independent UMETA(DisplayName="Independent", ToolTip="Capture this actor directly. Example: a single farm with its own physical capture point."),
	AggregateOnly UMETA(DisplayName="Aggregate Only", ToolTip="Never capture this actor directly; child ownership decides it. Example: a City becomes owned when its Districts agree."),
	Cascading UMETA(Hidden, DisplayName="Cascading (Legacy)", ToolTip="Serialized compatibility value. Definition assets now enforce Place=Independent and City/District=Aggregate Only; parent capture never rewrites children.")
};

/** Stable hierarchy identity used by runtime actors, replicated directory rows, and UI. */
UENUM(BlueprintType)
enum class ETerritoryHierarchyLevel : uint8
{
	City UMETA(ToolTip="A City groups Districts and derives control from them."),
	District UMETA(ToolTip="A District belongs to a City and derives control from Places."),
	Place UMETA(ToolTip="An independently capturable Property/Place inside a District.")
};

/** Determines the desired garrison assigned when ownership changes. */
UENUM(BlueprintType)
enum class ETerritoryPostCaptureGarrisonPolicy : uint8
{
	ConfiguredForEveryOwner UMETA(DisplayName="Configured for Every Owner", ToolTip="Every capture assigns Guard Spawn Count. Example: both Bandits and Heroes automatically receive 3 assigned guards."),
	PlayerChooses UMETA(DisplayName="Player Chooses (Recommended)", ToolTip="A player-faction capture starts with 0 assigned guards; AI and world-script captures use Guard Spawn Count. Example: the player decides whether profit is worth guard upkeep."),
	AlwaysUnstaffed UMETA(DisplayName="Always Unstaffed", ToolTip="Every new owner starts with 0 assigned guards. Example: all factions must recruit after every capture.")
};

UENUM(BlueprintType)
enum class ECaptureResult : uint8
{
	Success,
	AlreadyOwned,
	Locked,
	DefendersRemain,
	DiplomaticallyBlocked,
	InvalidTerritory,
	QuestOverrideActive UMETA(DisplayName="Quest Override Active")
};

/**
 * Result of a territory registration attempt.
 * P1-04: Registration must return a result so rejected actors can abort gameplay activation.
 */
UENUM(BlueprintType)
enum class ETerritoryRegistrationResult : uint8
{
	Success,
	DuplicateTag,
	DuplicateGUID,
	InvalidTerritory
};

UENUM(BlueprintType)
enum class ETerritoryTransactionType : uint8
{
	Income,
	GuardUpkeep,
	UpgradeCost,
	Purchase,
	Reward,
	Scripted,
	ManualCredit,
	ManualDebit
};

/** Explicit policy for distributing territory-generated currency. */
UENUM(BlueprintType)
enum class ETerritoryIncomePayoutPolicy : uint8
{
	CapturingPlayer UMETA(DisplayName="Capturing Player", ToolTip="Pay the Narrative inventory/account of the player who captured the place."),
	FactionLeader UMETA(DisplayName="Faction Leader", ToolTip="Pay the configured online faction leader's Narrative account."),
	EqualSplitOnlineMembers UMETA(DisplayName="Split Between Online Members", ToolTip="Split the payout between eligible online members of the owning faction."),
	SharedNarrativeAccount UMETA(DisplayName="Shared Narrative Account", ToolTip="Pay one Narrative-owned shared faction account. Example: all Heroes fund the same treasury."),
	NoCurrencyPayout UMETA(DisplayName="Rates Only (No Currency)", ToolTip="Calculate income and upkeep rates for UI/strategy, but move no real currency. Useful when Tales events own rewards.")
};

// ═══════════════════════════════════════════════════════════════════════════════
// Structs
// ═══════════════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct FTerritoryOwnershipData
{
	GENERATED_BODY()

	/** Narrative faction that owns this Territory or account. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	FGameplayTag OwningFaction;

	/** Verified former owners, recorded only by successful ownership commits. Old saves start empty. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|History")
	FGameplayTagContainer FormerOwningFactions;

	/**
	 * The faction that physically took this Place. Usually the player's faction, but any
	 * faction that completes a capture writes here. Empty while Unclaimed.
	 *
	 * Easy example: you fight through a Bandit outpost and capture it. CapturedBy becomes
	 * Faction.Heroes — whoever did the work.
	 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|History")
	FGameplayTag CapturedBy;

	/**
	 * The faction the capture was done FOR — normally the new owner. It differs from
	 * CapturedBy exactly when someone captures a Place on another faction's behalf.
	 *
	 * Easy example: the Regime sends you to take a Bandit outpost. CapturedBy is
	 * Faction.Heroes, CapturedFor is Faction.Regime. Later the Regime turns on you, and a
	 * quest can ask "which Places did I win for the faction that betrayed me?" by checking
	 * CapturedFor == Faction.Regime && CapturedBy == Faction.Heroes. That comparison is
	 * what makes the betrayal beat authorable instead of hard-coded.
	 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|History")
	FGameplayTag CapturedFor;

	/** State represented by this record; use the field's enum choices to interpret it. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	ETerritoryState State = ETerritoryState::Unclaimed;

	/** Saved and replicated independently from control. Old State=Locked saves migrate on load. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	ETerritoryAvailability Availability = ETerritoryAvailability::Unlocked;

	/** Control progress represented by this result; read with its current state and owner. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	float ControlProgress = 0.f;

	/** Faction currently represented by this capture contest. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	FGameplayTag ContestingFaction;

	/** Defenders counted by this query or evaluation. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	int32 DefenderCount = 0;

	/** Strategic attacker slots available for this Territory. Narrative's per-target combat tokens remain separate. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	int32 MaxConcurrentAttackers = 3;

	/** Base currency income per economy cycle before state, faction, upgrade and hierarchy modifiers. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	int32 PeriodicIncome = 0;

	/** Currency cost associated with the reported guard operation. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	int32 GuardCost = 0;

	/** One-time Narrative inventory debit for each newly-authorized garrison slot. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	int32 GuardRecruitmentCost = 0;

	/** Persistent target garrison size, including guards purchased after capture. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	int32 DesiredGuardCount = INDEX_NONE;

	/** Why the territory is locked. Empty when not locked. Replicated + saved. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	FText LockReason;
};

USTRUCT(BlueprintType)
struct FTerritoryUnlockResultRow
{
	GENERATED_BODY()

	/** Stable Territory GameplayTag used by lookups, Narrative conditions and hierarchy references. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Unlock")
	FGameplayTag TerritoryTag;

	/** Verified outcome of the reported operation. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Unlock")
	ETerritoryUnlockOutcome Outcome = ETerritoryUnlockOutcome::InvalidTarget;

	/** Explanation of the reported decision or result. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Unlock")
	FText Reason;
};

USTRUCT(BlueprintType)
struct FTerritoryUnlockCascadeResult
{
	GENERATED_BODY()

	/** Whether the target operation completed successfully in this result. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Unlock")
	bool bTargetSucceeded = false;

	/** Number of entries whose story availability is Unlocked. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Unlock")
	int32 UnlockedCount = 0;

	/** Number of entries currently blocked by their rules. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Unlock")
	int32 BlockedCount = 0;

	/** Individual outcomes returned by this batch operation. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Unlock")
	TArray<FTerritoryUnlockResultRow> Results;
};

/**
 * Exact replicated read model for one authored floor of a Territory; live pawn pointers
 * remain server-owned. One entry exists per declared FTerritoryFloorTemplate row, in
 * declaration order, so the list is stable across streaming and iteration order.
 *
 * These numbers are *derived*: they are regrouped from the guard posts that already own
 * the real state, never stored, so no save record and no migration are involved.
 */
USTRUCT(BlueprintType)
struct FTerritoryFloorSnapshot
{
	GENERATED_BODY()

	/** Authored floor identity. Zero is ground; upper floors are positive. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards|Floor")
	int32 FloorIndex = 0;

	/** Guards currently alive and assigned to posts on this floor. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards|Floor")
	int32 ActiveGuards = 0;

	/**
	 * Authored quota for this floor. A row quota of zero reports the floor's physical
	 * capacity instead, so a floor may be declared without also restating its post count.
	 * This is staging and objective data; the Territory's runtime DesiredGuards still
	 * governs how many guards the owner actually wants.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards|Floor")
	int32 DesiredGuards = 0;

	/** Physical guard slots on this floor: its authored posts plus any unidentified loaded post. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards|Floor")
	int32 MaximumGuards = 0;

	/** Finite replacement guards still available to this floor's posts. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards|Floor")
	int32 ReserveGuards = 0;

	/**
	 * This floor's replacements still waiting for their allowed physical deployment. A floor
	 * with no living guard but a pending deployment is mid-fight, not cleared, so this is
	 * what stops a reserve gap from reading as an empty floor.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards|Floor")
	int32 PendingDeployments = 0;

	/**
	 * Whether these counts are complete: every guard post the Definition authors for this floor was
	 * observed standing. False means **unknown, not zero** - the same vocabulary
	 * FTerritoryGarrisonOperationsView::bReserveCountKnown already establishes.
	 *
	 * A post actor streams out with its cell, and the counts above are accumulated from live post
	 * actors, so an unloaded post contributes nothing while its authored capacity stays in
	 * MaximumGuards. Without this flag that reads as an emptied floor, and IsCleared() would report a
	 * floor nobody has fought as cleared. Defaults to false so a producer that forgets to set it
	 * fails closed: a missing flag must never be read as "counts are fine".
	 */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	bool bCountsKnown = false;

	/**
	 * Whether this floor has nothing left to send: a post stands on it, nobody is alive,
	 * nobody is mid-deployment, and no reserve is left to walk in.
	 *
	 * Requiring the reserves is what stops a reinforcement gap from reading as an empty
	 * floor. Requiring a post stops a floor that never held defenders from reading as
	 * cleared, so a fresh Territory does not announce a fight nobody fought.
	 *
	 * Requiring the counts to be known is what stops a streamed-out post from clearing its own
	 * floor: the counts would all read zero because the post that owns them is not loaded, so the
	 * floor would satisfy its objective and fire its cleared beat while a defender is still
	 * physically standing there.
	 *
	 * The per-floor objective, the floor-cleared event and any Blueprint widget all read this
	 * one rule, so a floor can never satisfy the quest while it is still being defended.
	 * Blueprint reaches it through UTerritoryBlueprintLibrary::IsTerritoryFloorCleared, because
	 * UHT does not reflect UFUNCTIONs declared inside a USTRUCT.
	 */
	bool IsCleared() const
	{
		return bCountsKnown
			&& MaximumGuards > 0
			&& ActiveGuards == 0
			&& PendingDeployments == 0
			&& ReserveGuards == 0;
	}

	bool operator==(const FTerritoryFloorSnapshot& Other) const
	{
		return FloorIndex == Other.FloorIndex
			&& ActiveGuards == Other.ActiveGuards
			&& DesiredGuards == Other.DesiredGuards
			&& MaximumGuards == Other.MaximumGuards
			&& ReserveGuards == Other.ReserveGuards
			&& PendingDeployments == Other.PendingDeployments
			&& bCountsKnown == Other.bCountsKnown;
	}

	bool operator!=(const FTerritoryFloorSnapshot& Other) const { return !(*this == Other); }
};

/**
 * One Territory's runtime clones of a single floor's authored cleared events.
 *
 * This exists only because UHT forbids a container as a TMap value. A floor carries a list of
 * events, and the runtime store is keyed by floor index, so the list needs a name before it can be
 * a value. Keeping the key on the outside preserves the "one row per floor, no duplicates" shape
 * the map gives us; folding the index into the struct instead would allow a repeated row.
 *
 * Runtime-only: not Blueprint-visible, not replicated and not saved. The events it holds are
 * Transient duplicates owned by the Territory actor, never the Definition's shared templates.
 */
USTRUCT()
struct FTerritoryFloorRuntimeEvents
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNarrativeEvent>> Events;
};

/** Exact replicated read model for guard UI; live pawn pointers remain server-owned. */
USTRUCT(BlueprintType)
struct FTerritoryGarrisonSnapshot
{
	GENERATED_BODY()

	/** Guards currently alive and assigned to this garrison. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 ActiveGuards = 0;

	/** Requested garrison size; it may exceed the currently living guards while replacements are pending. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 DesiredGuards = 0;

	/** Maximum guard capacity supported by the assigned posts and current rules. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 MaximumGuards = 0;

	/** Finite replacement guards still available beyond the active garrison. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 ReserveGuards = 0;

	/** Troops or groups still waiting for their allowed physical deployment. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 PendingDeployments = 0;

	/**
	 * Per-floor breakdown, one entry per declared floor row and in declaration order.
	 * Empty when the Definition declares no floors, which keeps the legacy whole-Territory
	 * comparison byte-for-byte unchanged for every existing asset.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	TArray<FTerritoryFloorSnapshot> Floors;

	/**
	 * Whether the whole-Territory counts above are complete: every guard post the Definition
	 * authors for this Territory was observed standing. False means **unknown, not zero**.
	 *
	 * The totals have the same stream-out hole the per-floor counts have, and the same failure:
	 * a fully streamed-out Place reports no defenders and no pending deployments, so the
	 * whole-Place AllDefendersDefeated objective satisfies with every guard still standing in the
	 * unloaded cell. Defaults to false so a producer that forgets to set it fails closed.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	bool bCountsKnown = false;

	bool operator==(const FTerritoryGarrisonSnapshot& Other) const
	{
		return ActiveGuards == Other.ActiveGuards
			&& DesiredGuards == Other.DesiredGuards
			&& MaximumGuards == Other.MaximumGuards
			&& ReserveGuards == Other.ReserveGuards
			&& PendingDeployments == Other.PendingDeployments
			&& Floors == Other.Floors
			&& bCountsKnown == Other.bCountsKnown;
	}

	bool operator!=(const FTerritoryGarrisonSnapshot& Other) const { return !(*this == Other); }
};

/** Structured result for an absolute garrison staffing mutation. */
USTRUCT(BlueprintType)
struct FTerritoryGarrisonMutationResult
{
	GENERATED_BODY()

	/** Whether this result reports verified success. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	bool bSuccess = false;

	/** Requested garrison size before the reported operation. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 OldDesiredGuards = 0;

	/** Requested garrison size after the reported operation. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 NewDesiredGuards = 0;

	/** Living guard count before the reported operation. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 OldActiveGuards = 0;

	/** Living guard count after the reported operation. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 NewActiveGuards = 0;

	/** Currency cost used for guard recruitment in this result. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 RecruitmentCost = 0;

	/** Guards successfully deployed by this operation. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 GuardsDeployed = 0;

	/** Guards withdrawn by this operation. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 GuardsWithdrawn = 0;

	/** Readable information about the operation or event. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	FText Message;
};

USTRUCT(BlueprintType)
struct FTerritoryEconomySnapshot
{
	GENERATED_BODY()

	/** Currency read from the relevant Narrative account; Territory does not own a separate balance. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Economy")
	int32 Treasury = 0;

	/** Combined currency income included in this summary. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Economy")
	int32 TotalIncome = 0;

	/** Combined currency costs included in this summary. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Economy")
	int32 TotalCosts = 0;

	/** Number of Territories included in this summary. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Economy")
	int32 TerritoryCount = 0;
};

/** Maps a faction tag to a specific NPC definition for guard spawning. */
USTRUCT(BlueprintType)
struct FTerritoryFactionGuardDefinition
{
	GENERATED_BODY()

	/** Faction that triggers this definition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|Guards",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag Faction;

	/** NPC definition to use when this faction owns the territory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|Guards")
	TObjectPtr<class UNPCDefinition> NPCDefinition;
};

/** Result of one payment through the existing Narrative inventory. */
UENUM(BlueprintType)
enum class ETerritoryCurrencyMutationStatus : uint8
{
	/** No payment was made. Unpaid purchase staging can be cancelled. */
	Rejected,
	/** Narrative applied the payment and no load interrupted its callbacks. */
	Applied,
	/** A load replaced the operation. Keep loaded state; do not automatically refund or retry. */
	Superseded UMETA(DisplayName="Interrupted By Load")
};

/** Temporary payment receipt. Narrative owns and saves the actual balance. */
USTRUCT(BlueprintType)
struct FTerritoryCurrencyMutationResult
{
	GENERATED_BODY()

	/** Applied means the payment completed. Interrupted By Load requires checking restored state. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Economy")
	ETerritoryCurrencyMutationStatus Status = ETerritoryCurrencyMutationStatus::Rejected;

	/** Signed requested amount: positive adds money, negative pays money. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Economy")
	int32 Amount = 0;

	/** Balance before this payment, when an eligible account was found. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Economy")
	int32 BalanceBefore = 0;

	/** Balance at the Native write, before callbacks. A later expense or load can change it. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Economy")
	int32 BalanceAfter = 0;

	/** Why the payment was rejected or interrupted. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Economy")
	FText FailureReason;
};

/**
 * Transaction ledger entry — immutable audit trail for every economy mutations.
 * Records who, what, when, why, and how much.
 */
USTRUCT(BlueprintType)
struct FTerritoryTransaction
{
	GENERATED_BODY()

	/** Unique ID identifying this verified economy transaction. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction")
	FGuid TransactionID;

	/** Exact Narrative faction represented by this setting or result. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction")
	FGameplayTag Faction;

	/** Kind of operation or record represented by this entry. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction")
	ETerritoryTransactionType Type = ETerritoryTransactionType::ManualCredit;

	/** Amount involved in this resource or currency operation. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction")
	int32 Amount = 0;

	/** Narrative balance at this transaction's write, before independent callback expenses. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction")
	int32 BalanceAfter = 0;

	/** Campaign-clock timestamp for this record, rather than wall-clock time. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction")
	double GameTime = 0.0;

	/** Explanation of the reported decision or result. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction")
	FString Reason;

	/** Territory from which this event, effect or transfer originates. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction",
		meta = (Categories = "Territory"))
	FGameplayTag SourceTerritory;

	bool IsCredit() const { return Amount > 0; }
	bool IsDebit() const { return Amount < 0; }
};

USTRUCT(BlueprintType)
struct FCaptureAttempt
{
	GENERATED_BODY()

	/** Loaded Territory actor represented by this entry; may be empty while streamed out. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory")
	TWeakObjectPtr<ATerritoryVolume> Territory;

	/** Narrative faction launching or participating in this assault. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory")
	FGameplayTag AttackingFaction;

	/** Narrative faction defending the target Territory. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory")
	FGameplayTag DefendingFaction;

	/** Structured outcome; inspect its success and reason fields before treating the operation as complete. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory")
	ECaptureResult Result = ECaptureResult::InvalidTerritory;

	/** Attackers counted at this point in the capture evaluation. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory")
	int32 AttackersPresent = 0;

	/** Defenders included in the current capture evaluation. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory")
	int32 DefendersPresent = 0;
};

/** Admission policy; none of these modes bypass diplomacy or physical capture. */
UENUM(BlueprintType)
enum class ETerritoryStateCounterAttackPolicy : uint8
{
	CaptureTriggered UMETA(DisplayName="After Capture (Profile Schedule)", ToolTip="Preserves existing ownership-change and recurring profile behavior. Explicit Narrative Waves are also allowed."),
	WhileAtWar UMETA(DisplayName="Automatic While At War", ToolTip="May begin a finite schedule against an already-owned Place while at War. Quest gates, grace, profile repeat limits, cooldowns and all strategic calculations still apply."),
	QuestOnly UMETA(DisplayName="Quest / Explicit Waves Only", ToolTip="Automatic scheduling is disabled. Explicit Narrative Wave events may launch after normal hard validation."),
	Disabled UMETA(DisplayName="No New Assaults", ToolTip="Reject automatic and explicit new waves. An already physical battle continues; peace still cancels it.")
};

/** Gameplay rules selected by exact territory-owner faction. Narrative owns their execution. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryStateGameplayRules
{
	GENERATED_BODY()

	/** State-specific permission for automatic counters and explicit story waves; diplomacy and finite-force rules still apply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Counterattacks")
	ETerritoryStateCounterAttackPolicy CounterAttackPolicy = ETerritoryStateCounterAttackPolicy::CaptureTriggered;

	/** Empty permits any otherwise eligible hostile faction; tags match exactly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Counterattacks", meta=(Categories="Narrative.Factions"))
	FGameplayTagContainer AllowedAttackingFactions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Economy", meta=(ToolTip="Allows this state's current owner to earn periodic currency from this Place. Guard upkeep remains payable."))
	bool bAllowPeriodicIncome = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Economy", meta=(ToolTip="Allows this state's current owner to run Property resource recipes. Blocked cycles expire without later backpay."))
	bool bAllowResourceProduction = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Economy", meta=(ToolTip="Allows the authored capital capture bonus. Other rewards belong in this faction's Narrative Entry Events."))
	bool bAllowCapitalCaptureReward = true;

	bool AllowsAssault(const FGameplayTag& AttackingFaction, bool bExplicitNarrativeRequest) const
	{
		const bool bModeAllows = CounterAttackPolicy == ETerritoryStateCounterAttackPolicy::CaptureTriggered
			|| CounterAttackPolicy == ETerritoryStateCounterAttackPolicy::WhileAtWar
			|| (CounterAttackPolicy == ETerritoryStateCounterAttackPolicy::QuestOnly && bExplicitNarrativeRequest);
		return bModeAllows && AttackingFaction.IsValid()
			&& (AllowedAttackingFactions.IsEmpty() || AllowedAttackingFactions.HasTagExact(AttackingFaction));
	}

	/**
	 * Strategic controls supplied to the current owning faction while this state
	 * is active. The grant is derived from live ownership, so it needs no separate
	 * save data and is removed as soon as the Territory is lost or leaves this state.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Command Capabilities",
		meta=(DisplayName="Granted Command Capabilities",
			Categories="Territory.Capability",
			ToolTip="Controls this Territory gives its current owner while this state is active. Easy example: in a District's Claimed row add Territory.Capability.GuardStaffing. The player's faction can add guards while it holds that District; losing the District removes the control immediately. Leave this empty when the state gives no strategic perk."))
	FGameplayTagContainer GrantedCommandCapabilities;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Conditions",
		meta=(DisplayName="Entry Conditions",
			ToolTip="Every condition must pass before the state can begin. Claimed means capture completed. Contested begins only after gameplay registers a valid contest; walking through a Place is not enough unless Story Capture From Bounds intentionally makes that player an attacker. A real Faction A to Faction B capture evaluates the Claimed row even if the political enum was already Claimed; a same-owner reset does not."))
	TArray<TObjectPtr<class UNarrativeCondition>> EntryConditions;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Conditions",
		meta=(DisplayName="Exit Conditions",
			ToolTip="Every condition must pass before this state can end, and something must still request the change. These conditions are a gate, not a trigger: nothing polls them, so the state stays put until a quest event or TryUnlock asks for the transition. Example: a Locked Territory opens when a quest event requests the unlock after the gate quest is complete."))
	TArray<TObjectPtr<class UNarrativeCondition>> ExitConditions;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Events",
		meta=(DisplayName="Entry Events",
			ToolTip="Narrative events fired once after the state is committed. The Claimed row also fires for a real Faction A to Faction B handover even when the political enum remains Claimed. The same handover first runs this row's Exit Events for the old owner. Same-owner resets do not refire. Contested fires once when a valid contest begins, not every progress tick. Each event runs only when all inherited conditions pass, including Narrative's Not option."))
	TArray<TObjectPtr<class UNarrativeEvent>> EntryEvents;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Events",
		meta=(DisplayName="Exit Events",
			ToolTip="Narrative events fired after this state ends. Every inherited condition inside each event must pass. Example: advance the quest when the District unlocks, but only while reputation is at least 50."))
	TArray<TObjectPtr<class UNarrativeEvent>> ExitEvents;
};

/** Common rules retain existing property names. An exact owner override replaces gameplay rules only. */
USTRUCT(BlueprintType)
struct FTerritoryStateConfig : public FTerritoryStateGameplayRules
{
	GENERATED_BODY()

	/**
	 * Local music and one-shot effects for this state. Narrative Music owns the
	 * soundtrack; this row only selects its tagged set/theme for a player inside.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio",
		meta=(DisplayName="Narrative Music And State Effects",
			ToolTip="Optional local audio for this state. Easy example: Contested selects Music.Combat and plays an alarm; Claimed selects Music.Ambient and plays a short victory cue. Empty keeps the parent Territory or current world music."))
	FTerritoryStateAudioConfig Audio;

	/**
	 * Optional stealth policy for this state. Empty uses the Territory Definition's
	 * default profile. This keeps quest infiltration beside the other modular state rules.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth",
		meta=(DisplayName="Stealth Profile Override",
			ToolTip="Optional stealth rules while this state is active. Easy example: assign Rescue Mission Stealth to the Claimed row so entering the enemy Place does not start War until a guard confirms the player."))
	TObjectPtr<UTerritoryStealthProfile> StealthProfileOverride;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Faction Rules", meta=(ForceInlineRow, ToolTip="Exact Narrative owner faction overrides. A matching row replaces common conditions, events, capabilities, income and counterattack policy. Entry uses the incoming owner; exit uses the outgoing owner. Unlisted factions use common rules. Audio and stealth stay on the state row."))
	TMap<FGameplayTag, FTerritoryStateGameplayRules> FactionOverrides;

	const FTerritoryStateGameplayRules& ForFaction(const FGameplayTag& OwnerFaction) const
	{
		if (const FTerritoryStateGameplayRules* Override = FactionOverrides.Find(OwnerFaction)) return *Override;
		return *this;
	}
};

// ═══════════════════════════════════════════════════════════════════════════════
// Delegates
// ═══════════════════════════════════════════════════════════════════════════════

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnTerritoryControlChanged,
	ATerritoryVolume*, Territory,
	FGameplayTag, OldOwner,
	FGameplayTag, NewOwner);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnTerritoryStateChanged,
	ATerritoryVolume*, Territory,
	ETerritoryState, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnTerritoryAvailabilityChanged,
	ATerritoryVolume*, Territory,
	ETerritoryAvailability, NewAvailability);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnTerritoryRegistered,
	ATerritoryVolume*, Territory,
	bool, bWasUnregistered);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnEconomyTick,
	FGameplayTag, Faction,
	FTerritoryEconomySnapshot, Snapshot);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnCaptureAttempted,
	const FCaptureAttempt&, Attempt);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnTransactionRecorded,
	const FTerritoryTransaction&, Transaction);

// ─── Hierarchy Delegates ───

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnCityCaptured,
	class ATerritoryCity*, City,
	FGameplayTag, CapturingFaction);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnCityLost,
	class ATerritoryCity*, City,
	FGameplayTag, PreviousFaction);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnDistrictCaptured,
	class ATerritoryDistrict*, District,
	FGameplayTag, OldOwner,
	FGameplayTag, NewOwner);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnPropertyCaptured,
	class ATerritoryProperty*, Property,
	FGameplayTag, NewOwner);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnAllGuardsDefeated,
	class ATerritoryVolume*, Territory);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnTerritoryGarrisonChanged,
	class ATerritoryVolume*, Territory,
	FTerritoryGarrisonSnapshot, Snapshot);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnGuardKilled,
	class ATerritoryVolume*, Territory,
	AActor*, Guard,
	AActor*, Killer,
	int32, RemainingDefenders);

/**
 * One authored floor of a Territory lost its last defender. Fired at each fight conclusion,
 * once per floor, only on an observed transition, and never for a floor first seen empty.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnTerritoryFloorCleared,
	class ATerritoryVolume*, Territory,
	int32, FloorIndex);

/**
 * A guard finished deploying and is fully configured, so story may address it. FloorIndex is
 * the authored floor of the post it deployed from, or INDEX_NONE when it had no post.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnTerritoryDefenderSpawned,
	class ATerritoryVolume*, Territory,
	AActor*, Guard,
	int32, FloorIndex);
