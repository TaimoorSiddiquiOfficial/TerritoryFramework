#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
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
	Superseded
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
