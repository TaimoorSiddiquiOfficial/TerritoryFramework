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
	AuthorityRejected
};

/** An item rate authored on a production rule. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryResourceRate
{
	GENERATED_BODY()

	/** Narrative inventory item class. Exact-class matching is used for settlement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	TSubclassOf<UNarrativeItem> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production", meta=(ClampMin="0"))
	int32 QuantityPerCycle = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production", meta=(ClampMin="0"))
	int32 QuantityPerUpgradeLevel = 0;
};

/** Concrete item amount used by transactions and replicated UI projections. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryResourceAmount
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TSubclassOf<UNarrativeItem> ItemClass;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	FText DisplayName;

	/** Lower values settle first, then RuleTag provides the stable tie-break. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceRate> Inputs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceRate> Outputs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production", meta=(ClampMin="0"))
	int32 MinimumUpgradeLevel = 0;

	/** Claimed and uncontested ownership is required by default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	bool bRequiresClaimedState = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	bool bPauseWhileContested = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	bool bEnabled = true;
};

/** Saved deterministic checkpoint for one Property production rule. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryProductionCheckpoint
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGuid TerritoryGUID;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag TerritoryTag;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag RuleTag;

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

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag RuleTag;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FText DisplayName;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	ETerritoryProductionStatus Status = ETerritoryProductionStatus::NeverEvaluated;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	int64 LastEvaluatedCycle = INDEX_NONE;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FText StatusReason;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceAmount> LastInputs;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceAmount> LastOutputs;
};

/** Durable site record used while a World Partition Property is unloaded. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryProductionSiteRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGuid TerritoryGUID;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag TerritoryTag;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag ParentTerritoryTag;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FText DisplayName;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag OwnerFaction;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TSoftObjectPtr<class UTerritoryProductionProfile> ProductionProfile;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	int32 UpgradeLevel = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	ETerritoryState TerritoryState = ETerritoryState::Unclaimed;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	ETerritoryAvailability Availability = ETerritoryAvailability::Unlocked;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	ETerritoryProductionStatus LastStatus = ETerritoryProductionStatus::NeverEvaluated;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag LastRuleTag;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	int64 LastEvaluatedCycle = INDEX_NONE;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FText StatusReason;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceAmount> LastInputs;

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

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag Faction;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceAmount> Resources;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	bool bStorageAvailable = false;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Production")
	int64 SnapshotCycle = INDEX_NONE;
};

/** Atomic settlement result. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryProductionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	ETerritoryProductionStatus Status = ETerritoryProductionStatus::NeverEvaluated;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	FGuid BatchID;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	FGuid TerritoryGUID;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag TerritoryTag;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag Faction;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	FGameplayTag RuleTag;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	int64 CycleIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceAmount> InputsConsumed;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryResourceAmount> OutputsProduced;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Production")
	FText FailureReason;
};

/** Reusable production definition. Actual items always live in Narrative inventory. */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryProductionProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Production")
	TArray<FTerritoryProductionRule> Rules;

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
