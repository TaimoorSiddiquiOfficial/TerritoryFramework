#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TerritoryTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTerritory, Log, All);

class ATerritoryVolume;

// ═══════════════════════════════════════════════════════════════════════════════
// Enums
// ═══════════════════════════════════════════════════════════════════════════════

UENUM(BlueprintType)
enum class ETerritoryState : uint8
{
	Unclaimed,
	Claimed,
	Contested,
	Locked
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

/**
 * Transaction ledger entry — immutable audit trail for every economy mutation.
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
