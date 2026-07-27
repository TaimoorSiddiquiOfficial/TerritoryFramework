#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryEconomySubsystem.generated.h"

class ATerritoryVolume;

USTRUCT(BlueprintType)
struct FTerritoryTreasury
{
	GENERATED_BODY()

	/** Income/cost snapshot for a faction. This struct is not a wallet. */

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Economy")
	int32 IncomePerTick = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Economy")
	int32 CostsPerTick = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Economy")
	int32 TerritoryCount = 0;
};

UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryEconomySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Territory|Economy",
		meta=(DeprecatedFunction, DeprecationMessage="TerritoryFramework has no faction wallet; use GetActorCurrency(Requester)."))
	int32 GetTreasury(const FGameplayTag& Faction) const;

	/** Return the Narrative currency of the requesting actor's inventory account. */
	UFUNCTION(BlueprintPure, Category = "Territory|Economy")
	int32 GetActorCurrency(const AActor* RequestingActor) const;

	/** Check the requesting actor's Narrative inventory account. */
	UFUNCTION(BlueprintPure, Category = "Territory|Economy")
	bool CanActorAfford(const AActor* RequestingActor, int32 Cost) const;

	/** Debit exactly the requesting actor's Narrative inventory account. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy")
	bool TryDebitCurrency(AActor* RequestingActor, int32 PositiveAmount,
		const FGameplayTag& Faction, const FString& Reason = TEXT(""),
		ETerritoryTransactionType Type = ETerritoryTransactionType::ManualDebit);

	/** Credit exactly the beneficiary actor's Narrative inventory account. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy")
	bool CreditCurrency(AActor* Beneficiary, int32 PositiveAmount,
		const FGameplayTag& Faction, const FString& Reason = TEXT(""),
		ETerritoryTransactionType Type = ETerritoryTransactionType::ManualCredit);

	/** Apply an explicit payout policy to territory-generated currency. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy")
	int32 CreditCurrencyToFaction(const FGameplayTag& Faction, int32 PositiveAmount,
		ETerritoryIncomePayoutPolicy Policy, const FString& Reason = TEXT(""),
		ETerritoryTransactionType Type = ETerritoryTransactionType::Income,
		AActor* PreferredBeneficiary = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Territory|Economy")
	int32 GetIncome(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintCallable, Category = "Territory|Economy")
	int32 GetCosts(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintCallable, Category = "Territory|Economy",
		meta=(DeprecatedFunction, DeprecationMessage="Use CanActorAfford(Requester, Cost)."))
	bool CanAfford(const FGameplayTag& Faction, int32 Cost) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy",
		meta=(DeprecatedFunction, DeprecationMessage="Use CreditCurrency(Beneficiary, Amount, Faction, Reason, Type)."))
	void AddToTreasury(const FGameplayTag& Faction, int32 PositiveAmount, const FString& Reason = TEXT(""), ETerritoryTransactionType Type = ETerritoryTransactionType::ManualCredit);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy",
		meta=(DeprecatedFunction, DeprecationMessage="Use TryDebitCurrency(Requester, Amount, Faction, Reason, Type)."))
	bool TryDebitTreasury(const FGameplayTag& Faction, int32 PositiveAmount, const FString& Reason = TEXT(""), ETerritoryTransactionType Type = ETerritoryTransactionType::ManualDebit);

	/** Direct treasury state assignment (used by WorldState actor during load/restore) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy")
	void SetFactionTreasury(const FGameplayTag& Faction, const FTerritoryTreasury& Treasury);

	UFUNCTION(BlueprintPure, Category = "Territory|Economy")
	FTerritoryTreasury GetFactionEconomy(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Economy")
	TArray<FGameplayTag> GetAllFactionsWithTreasury() const;

	UFUNCTION(BlueprintCallable, Category = "Territory|Economy")
	TArray<FTerritoryTransaction> GetTransactionHistory(const FGameplayTag& Faction, int32 MaxEntries = 50) const;

	/** Native persistence bridge. Returns the ledger in chronological order. */
	TArray<FTerritoryTransaction> GetAllTransactionHistory() const { return TransactionLedger; }

	/** Native persistence bridge. Replaces the ledger without emitting gameplay delegates. */
	void RestoreTransactionHistory(const TArray<FTerritoryTransaction>& Transactions);

	/** Native persistence bridge. Replaces all tracked faction economy parameters. */
	void RestoreTreasuryState(const TMap<FGameplayTag, FTerritoryTreasury>& Treasuries);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy")
	void RecalculateIncome(const FGameplayTag& Faction);

	/** Mark a faction for deferred income recalculation (processed on next economy tick). */
	void MarkFactionDirty(const FGameplayTag& Faction) { if (Faction.IsValid()) DirtyFactions.Add(Faction); }

	UPROPERTY(BlueprintAssignable, Category = "Territory|Economy")
	FOnEconomyTick OnEconomyTickFired;

	UPROPERTY(BlueprintAssignable, Category = "Territory|Economy")
	FOnTransactionRecorded OnTransactionRecorded;

	UPROPERTY(EditDefaultsOnly, Category = "Territory|Economy")
	float TickIntervalSeconds = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Territory|Economy")
	int32 MaxTransactionHistory = 500;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Territory|Economy")
	ETerritoryIncomePayoutPolicy IncomePayoutPolicy = ETerritoryIncomePayoutPolicy::EqualSplitOnlineMembers;

	private:
	UPROPERTY(SaveGame)
	TMap<FGameplayTag, FTerritoryTreasury> FactionTreasuries;

	UPROPERTY(SaveGame)
	TArray<FTerritoryTransaction> TransactionLedger;

	FTimerHandle EconomyTickTimerHandle;

	/** Factions whose income needs recalculation — processed once per economy tick. */
	TSet<FGameplayTag> DirtyFactions;

	UFUNCTION()
	void OnEconomyTick();

	UFUNCTION()
	void OnTerritoryControlChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	UFUNCTION()
	void OnTerritoryUnregistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	/** Get all online faction member characters with valid inventory components. */
	TArray<class ANarrativeCharacter*> GetFactionMembers(const FGameplayTag& Faction) const;
	class UNarrativeInventoryComponent* ResolveCurrencyAccount(const AActor* RequestingActor) const;
	void RecordCurrencyTransaction(const FGameplayTag& Faction, int32 Amount,
		int32 BalanceAfter, const FString& Reason, ETerritoryTransactionType Type,
		const AActor* AccountActor);
	bool TryDebitFactionMembers(const FGameplayTag& Faction, int32 PositiveAmount,
		const FString& Reason, ETerritoryTransactionType Type);
};
