#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TerritoryTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTerritory, Log, All);

class ATerritoryVolume;
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
	Claimed UMETA(DisplayName="Claimed", ToolTip="One faction owns this place. Example: Faction.Heroes controls the market."),
	Contested UMETA(DisplayName="Contested", ToolTip="A physical capture is in progress or child territories disagree. This is runtime state, not a recommended starting state."),
	Locked UMETA(DisplayName="Locked", ToolTip="Ownership cannot change until this state's Exit Conditions pass. Example: finish the 'Open the City Gates' quest before leaving Locked.")
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
	Claimed UMETA(DisplayName="Claimed", ToolTip="Start owned by Initial Owning Faction. If that faction is empty, the safe result is Unclaimed."),
	Locked UMETA(DisplayName="Locked", ToolTip="Start locked. The place may still have an Initial Owning Faction, but ownership cannot change until Locked Exit Conditions pass.")
};

UENUM(BlueprintType)
enum class ETerritoryControlMode : uint8
{
	Independent UMETA(DisplayName="Independent", ToolTip="Capture this actor directly. Example: a single farm with its own physical capture point."),
	AggregateOnly UMETA(DisplayName="Aggregate Only", ToolTip="Never capture this actor directly; child ownership decides it. Example: a City becomes owned when its Districts agree."),
	Cascading UMETA(DisplayName="Cascading", ToolTip="This actor may be captured directly and may also change child ownership. Use only when your game intentionally wants a parent capture to cascade.")
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
	InvalidTerritory
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

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	FGameplayTag OwningFaction;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	ETerritoryState State = ETerritoryState::Unclaimed;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	float ControlProgress = 0.f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	FGameplayTag ContestingFaction;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	int32 DefenderCount = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	int32 MaxConcurrentAttackers = 3;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory")
	int32 PeriodicIncome = 0;

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

/** Exact replicated read model for guard UI; live pawn pointers remain server-owned. */
USTRUCT(BlueprintType)
struct FTerritoryGarrisonSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 ActiveGuards = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 DesiredGuards = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 MaximumGuards = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 ReserveGuards = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 PendingDeployments = 0;

	bool operator==(const FTerritoryGarrisonSnapshot& Other) const
	{
		return ActiveGuards == Other.ActiveGuards
			&& DesiredGuards == Other.DesiredGuards
			&& MaximumGuards == Other.MaximumGuards
			&& ReserveGuards == Other.ReserveGuards
			&& PendingDeployments == Other.PendingDeployments;
	}

	bool operator!=(const FTerritoryGarrisonSnapshot& Other) const { return !(*this == Other); }
};

/** Structured result for an absolute garrison staffing mutation. */
USTRUCT(BlueprintType)
struct FTerritoryGarrisonMutationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 OldDesiredGuards = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 NewDesiredGuards = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 OldActiveGuards = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 NewActiveGuards = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 RecruitmentCost = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 GuardsDeployed = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	int32 GuardsWithdrawn = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Guards")
	FText Message;
};

USTRUCT(BlueprintType)
struct FTerritoryEconomySnapshot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Economy")
	int32 Treasury = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Economy")
	int32 TotalIncome = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Economy")
	int32 TotalCosts = 0;

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

/**
 * Transaction ledger entry — immutable audit trail for every economy mutations.
 * Records who, what, when, why, and how much.
 */
USTRUCT(BlueprintType)
struct FTerritoryTransaction
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction")
	FGuid TransactionID;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction")
	FGameplayTag Faction;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction")
	ETerritoryTransactionType Type = ETerritoryTransactionType::ManualCredit;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction")
	int32 Amount = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction")
	int32 BalanceAfter = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction")
	double GameTime = 0.0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Transaction")
	FString Reason;

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

	UPROPERTY(BlueprintReadOnly, Category = "Territory")
	TWeakObjectPtr<ATerritoryVolume> Territory;

	UPROPERTY(BlueprintReadOnly, Category = "Territory")
	FGameplayTag AttackingFaction;

	UPROPERTY(BlueprintReadOnly, Category = "Territory")
	FGameplayTag DefendingFaction;

	UPROPERTY(BlueprintReadOnly, Category = "Territory")
	ECaptureResult Result = ECaptureResult::InvalidTerritory;

	UPROPERTY(BlueprintReadOnly, Category = "Territory")
	int32 AttackersPresent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Territory")
	int32 DefendersPresent = 0;
};

/**
 * Per-state configuration for Narrative conditions and events.
 * Entry conditions must pass before entering. Exit conditions must pass before leaving.
 * Example: put a quest-complete condition in Locked -> Exit Conditions to unlock a city.
 */
USTRUCT(BlueprintType)
struct FTerritoryStateConfig
{
	GENERATED_BODY()

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
			ToolTip="Every condition must pass before the enum state can begin. Example: entering Locked may require a story event to be active. A direct Claimed owner swap does not enter Claimed again; normal physical capture passes through Contested first."))
	TArray<TObjectPtr<class UNarrativeCondition>> EntryConditions;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Conditions",
		meta=(DisplayName="Exit Conditions",
			ToolTip="Every condition must pass before this state can end. Example: Locked exits only after the player completes the gate quest."))
	TArray<TObjectPtr<class UNarrativeCondition>> ExitConditions;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Events",
		meta=(DisplayName="Entry Events",
			ToolTip="Narrative events fired after the enum state is committed. Each event runs only when all conditions inside that event pass, including Narrative's Not option. Example: when Contested becomes Claimed, schedule an enemy wave only if the two factions are at War. Use the control-changed delegate for trusted direct Claimed-to-Claimed owner swaps."))
	TArray<TObjectPtr<class UNarrativeEvent>> EntryEvents;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Events",
		meta=(DisplayName="Exit Events",
			ToolTip="Narrative events fired after this state ends. Every inherited condition inside each event must pass. Example: advance the quest when the District unlocks, but only while reputation is at least 50."))
	TArray<TObjectPtr<class UNarrativeEvent>> ExitEvents;
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
