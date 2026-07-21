#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TerritoryTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTerritory, Log, All);

class ATerritoryVolume;

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
	InvalidTerritory
};

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
