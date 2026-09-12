#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "Tales/TerritoryGarrisonCondition.h"
#include "TerritoryProductionProfile.generated.h"

class UNarrativeItem;

/** Result of evaluating or settling one Territory production rule. */
UENUM(BlueprintType)
enum class ETerritoryProductionStatus : uint8
{
	NeverEvaluated,
	Ready,
	Produced,
	MissingInput,
	StorageUnavailable,
	StorageFull,
	Inactive,
	InvalidProfile,
	AlreadyProcessed,
	AuthorityRejected,
	/** A synchronous recipe or settlement callback attempted another transaction. */
	SettlementInProgress,
	/** A callback changed the inventory before the complete recipe was verified. */
	SettlementChanged,
	/** Compensation could not restore the affected item quantities. Never retry this cycle. */
	RollbackIncomplete,
	/** A Narrative load superseded this request; its old continuation was discarded. */
	Superseded,
	/** An authored inventory check or output cap paused this cycle. No items were consumed. */
	StockLimited UMETA(DisplayName="Stock Limit")
};

/** Pause the whole recipe when any enabled check matches the receiving inventory. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryProductionStockCondition
{
	GENERATED_BODY()

	/** Turn this check on or off without removing its settings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory Check")
	bool bEnabled = true;

	/** Count all stacks of this exact Narrative item class in the receiving faction inventory. Loaded weapon magazines and other players' inventories are not counted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory Check", meta=(EditCondition="bEnabled"))
	TSubclassOf<UNarrativeItem> ItemClass;

	/** Stop when the current count matches this comparison. At Least means >=; At Most means <=. Equal To alone can miss a count that jumps past the amount. Use an output stock cap for a strict refill target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory Check", meta=(EditCondition="bEnabled", DisplayName="Stop When"))
	ETerritoryIntegerComparison Comparison = ETerritoryIntegerComparison::AtLeast;

	/** Amount to compare with the current inventory count. Example: At Least 300 pauses at 300 or more. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory Check", meta=(EditCondition="bEnabled", ClampMin="0"))
	int32 Quantity = 300;
};

/** An upper stock limit for one output item. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryProductionStockCap
{
	GENERATED_BODY()

	/** Turn this output cap on or off without removing its settings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Output Stock Cap")
	bool bEnabled = true;

	/** An exact Narrative item class from this rule's Outputs list. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Output Stock Cap", meta=(EditCondition="bEnabled"))
	TSubclassOf<UNarrativeItem> ItemClass;

	/** Highest total stock this rule may create in its receiving inventory. Free production adds only the missing amount. Recipes with inputs wait until the whole batch fits, so they never charge for a partial batch. Existing excess stock is left alone. Zero stops this output. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Output Stock Cap", meta=(EditCondition="bEnabled", ClampMin="0"))
	int32 MaximumQuantity = 300;
};

/** Per-rule controls for the existing Territory HUD and activity feed. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryProductionNotifications
{
	GENERATED_BODY()

	/** Master switch for this rule's HUD messages and activity-feed entries. Production, quests and inventory updates still work when this is off. Global notification switches also apply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Notifications")
	bool bEnabled = true;

	/** Allow a message after this rule successfully produces items. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Notifications", meta=(EditCondition="bEnabled"))
	bool bNotifyOnSuccess = true;

	/** Allow messages when missing inputs, storage or other requirements block this rule. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Notifications", meta=(EditCondition="bEnabled"))
	bool bNotifyWhenBlocked = true;

	/** Allow a message when a stock check or cap pauses production. Off by default because a full refill target is normal. This is separate from other blocked messages. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Notifications", meta=(EditCondition="bEnabled"))
	bool bNotifyAtStockLimit = false;

	/** Also keep allowed messages in the activity feed, when global recording is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Notifications", meta=(EditCondition="bEnabled"))
	bool bRecordInFeed = true;

	/** Optional success title. Leave empty for the default. Text supports {Rule}, {Resources}, {Quantity}, {Inputs}, {Cycle}, {Territory} and {Reason}. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Notifications", meta=(EditCondition="bEnabled && bNotifyOnSuccess", MultiLine="true"))
	FText SuccessTitle;

	/** Optional success message, with the same placeholders as Success Title. Quantity and Resources describe only the items actually added. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Notifications", meta=(EditCondition="bEnabled && bNotifyOnSuccess", MultiLine="true"))
	FText SuccessMessage;

	/** Optional title for allowed blocked or stock-limit messages. Leave empty for the default. Supports the same placeholders as Success Title. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Notifications", meta=(EditCondition="bEnabled && (bNotifyWhenBlocked || bNotifyAtStockLimit)", MultiLine="true"))
	FText BlockedTitle;

	/** Optional blocked message. Use {Reason} to explain why no items were produced. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Notifications", meta=(EditCondition="bEnabled && (bNotifyWhenBlocked || bNotifyAtStockLimit)", MultiLine="true"))
	FText BlockedMessage;

	bool AllowsMessage(ETerritoryProductionStatus Status, bool bSuccess) const;
};

/** An item rate authored on a production rule. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryResourceRate
{
	GENERATED_BODY()

	/** Narrative inventory item class. Exact-class matching is used for settlement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	TSubclassOf<UNarrativeItem> ItemClass;

	/** Base quantity consumed or produced per successful recipe cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production", meta=(ClampMin="0"))
	int32 QuantityPerCycle = 0;

	/** Additional item quantity per Place upgrade level, added to the base quantity for each cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production", meta=(ClampMin="0"))
	int32 QuantityPerUpgradeLevel = 0;
};

/** Concrete item amount used by transactions and replicated UI projections. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryResourceAmount
{
	GENERATED_BODY()

	/** Narrative item class represented by this resource entry; settlement matches the exact class. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TSubclassOf<UNarrativeItem> ItemClass;

	/** Number of items represented by this resource entry. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	int32 Quantity = 0;
};

/** One deterministic input-to-output production or crafting rule. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryProductionRule
{
	GENERATED_BODY()

	/** Stable semantic identity used by save checkpoints and UI. Must be unique within the profile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag RuleTag;

	/** Readable name shown to designers or players; stable tags and IDs still identify this entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	FText DisplayName;

	/** Lower values settle first, then RuleTag provides the stable tie-break. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	int32 Priority = 0;

	/** Narrative item quantities required and consumed for one successful production cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceRate> Inputs;

	/** Narrative item quantities credited only after the complete production transaction succeeds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceRate> Outputs;

	/** Lowest Place upgrade level at which this production rule can operate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production", meta=(ClampMin="0"))
	int32 MinimumUpgradeLevel = 0;

	/** Claimed and uncontested ownership is required by default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	bool bRequiresClaimedState = true;

	/** Pause this production rule while the Place is under active capture contest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	bool bPauseWhileContested = true;

	/** Allow this recipe to run when its ownership, state, upgrade and inventory requirements pass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	bool bEnabled = true;

	/** Pause this rule if ANY enabled check matches. Checks run before inputs are consumed, using the selected faction resource inventory. Paused cycles expire; they do not build up extra production for later. An empty list adds no checks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production|Inventory Limits")
	TArray<FTerritoryProductionStockCondition> InventoryStopConditions;

	/** Limit total stock for selected output items. Free outputs refill only the missing amount. Recipes with inputs run only if every full output fits. Caps are checked on every cycle, including catch-up and restored sites. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production|Inventory Limits")
	TArray<FTerritoryProductionStockCap> OutputStockCaps;

	/** Choose which messages this recipe may show and write its text here. Turning messages off does not disable production or its gameplay events. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production|Notifications")
	FTerritoryProductionNotifications Notifications;
};

/** Saved deterministic checkpoint for one Property production rule. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryProductionCheckpoint
{
	GENERATED_BODY()

	/** Stable saved identity of the Territory represented by this record. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGuid TerritoryGUID;

	/** Stable Territory GameplayTag used by lookups, Narrative conditions and hierarchy references. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag TerritoryTag;

	/** Stable tag identifying one production rule across saves and UI queries. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag RuleTag;

	/** Narrative faction owning the Territory represented by this row. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag OwnerFaction;

	/** Last campaign cycle consumed by this rule. Storage-blocked cycles remain pending. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	int64 LastProcessedCycle = INDEX_NONE;
};

/** Durable outcome for one rule in a multi-recipe production site. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryProductionRuleState
{
	GENERATED_BODY()

	/** Stable tag identifying one production rule across saves and UI queries. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag RuleTag;

	/** Readable name shown to designers or players; stable tags and IDs still identify this entry. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FText DisplayName;

	/** Current result or blocking state of this operation. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	ETerritoryProductionStatus Status = ETerritoryProductionStatus::NeverEvaluated;

	/** Most recent campaign cycle in which this entry was evaluated. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	int64 LastEvaluatedCycle = INDEX_NONE;

	/** Readable explanation of the current status. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FText StatusReason;

	/** Input item quantities recorded by the latest production evaluation. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceAmount> LastInputs;

	/** Output item quantities recorded by the latest production evaluation. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceAmount> LastOutputs;
};

/** Durable site record used while a World Partition Property is unloaded. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryProductionSiteRecord
{
	GENERATED_BODY()

	/** Stable saved identity of the Territory represented by this record. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGuid TerritoryGUID;

	/** Stable Territory GameplayTag used by lookups, Narrative conditions and hierarchy references. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag TerritoryTag;

	/** Stable tag of this entry's parent District or City. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag ParentTerritoryTag;

	/** Readable name shown to designers or players; stable tags and IDs still identify this entry. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FText DisplayName;

	/** Narrative faction owning the Territory represented by this row. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag OwnerFaction;

	/** Reusable input/output recipes processed by the existing Territory economy and Narrative inventory systems. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TSoftObjectPtr<class UTerritoryProductionProfile> ProductionProfile;

	/** Authored state rules remain readable while the physical Place is streamed out. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TSoftObjectPtr<class UTerritoryDefinition> TerritoryDefinition;

	/** Zero is a legacy record. It must rebind to its Place before paying under new rules. */
	UPROPERTY(SaveGame)
	int32 StateRulesVersion = 0;

	/** Requested Place upgrade level used by the selected scripted operation. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	int32 UpgradeLevel = 0;

	/** Current political state, separate from story availability. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	ETerritoryState TerritoryState = ETerritoryState::Unclaimed;

	/** Story access state, such as Locked or Unlocked; separate from who owns the Territory. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	ETerritoryAvailability Availability = ETerritoryAvailability::Unlocked;

	/** Most recently recorded production outcome or blocking status. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	ETerritoryProductionStatus LastStatus = ETerritoryProductionStatus::NeverEvaluated;

	/** Stable tag of the last production rule evaluated. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag LastRuleTag;

	/** Most recent campaign cycle in which this entry was evaluated. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	int64 LastEvaluatedCycle = INDEX_NONE;

	/** Readable explanation of the current status. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FText StatusReason;

	/** Input item quantities recorded by the latest production evaluation. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceAmount> LastInputs;

	/** Output item quantities recorded by the latest production evaluation. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceAmount> LastOutputs;

	/** Per-rule state retained so multi-recipe sites never hide a blocked rule. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryProductionRuleState> RuleStates;
};

/** Read-only projection of one faction resource account. Narrative inventory remains authoritative. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryFactionResourceSnapshot
{
	GENERATED_BODY()

	/** Exact Narrative faction represented by this setting or result. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag Faction;

	/** Resource rows included in this result. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceAmount> Resources;

	/** Whether the required Narrative storage account can be resolved. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	bool bStorageAvailable = false;

	/** Several eligible accounts have the same highest priority. Production waits for a deliberate account choice. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	bool bAccountConflict = false;

	/** Campaign cycle represented by this saved or replicated snapshot. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	int64 SnapshotCycle = INDEX_NONE;
};

/** Atomic settlement result. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryProductionResult
{
	GENERATED_BODY()

	/** Whether this result reports verified success. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	bool bSuccess = false;

	/** Current result or blocking state of this operation. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	ETerritoryProductionStatus Status = ETerritoryProductionStatus::NeverEvaluated;

	/** Unique ID grouping results from the same settlement batch. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	FGuid BatchID;

	/** Stable saved identity of the Territory represented by this record. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	FGuid TerritoryGUID;

	/** Stable Territory GameplayTag used by lookups, Narrative conditions and hierarchy references. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag TerritoryTag;

	/** Exact Narrative faction represented by this setting or result. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag Faction;

	/** Stable tag identifying one production rule across saves and UI queries. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag RuleTag;

	/** Campaign production cycle represented by this record. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	int64 CycleIndex = INDEX_NONE;

	/** Item quantities actually consumed by this settlement result. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceAmount> InputsConsumed;

	/** Item quantities actually credited by this settlement result. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceAmount> OutputsProduced;

	/** Explains why the requested operation did not succeed. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	FText FailureReason;

	/** Display name copied from the evaluated rule. An empty name falls back to its friendly tag name. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	FText RuleDisplayName;

	/** Message settings copied before settlement. UI uses the exact evaluated rule, even if its Place streams out. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	FTerritoryProductionNotifications Notifications;
};

/** Reusable production definition. Actual items always live in Narrative inventory. */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryProductionProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Readable name shown to designers or players; stable tags and IDs still identify this entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	FText DisplayName;

	/** Reusable production recipes evaluated in priority and stable-tag order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryProductionRule> Rules;

	/** Check production recipes and quantity rules without consuming items or paying currency. */
	UFUNCTION(BlueprintPure, Category="Territory|Production")
	bool ValidateProfile(FText& OutFailureReason) const;

	/** Overflow-safe scale operation shared by runtime settlement and native tests. */
	static bool CalculateScaledQuantity(const FTerritoryResourceRate& Rate,
		int32 UpgradeLevel, int32 CycleCount, int32& OutQuantity);

	static bool IsRuleConfigurationValid(const FTerritoryProductionRule& Rule,
		FText& OutFailureReason);

	static bool CanRuleRunForState(const FTerritoryProductionRule& Rule,
		ETerritoryState State, int32 UpgradeLevel, FText& OutFailureReason);

	static int32 CalculatePendingCycleCount(int64 LastProcessedCycle,
		int64 CurrentCycle, int32 MaximumCatchupCycles);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
