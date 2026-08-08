#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryEconomySubsystem.generated.h"

class ATerritoryVolume;
class ANarrativePlayerCharacter;

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

	/**
	 * Return the online Narrative player characters that automatically receive
	 * territory income and pay upkeep for the faction. Guards and other NPCs are
	 * never included; NPC-backed faction accounts must be registered explicitly.
	 */
	UFUNCTION(BlueprintPure, Category = "Territory|Economy")
	TArray<ANarrativePlayerCharacter*> GetOnlineFactionPlayers(
		const FGameplayTag& Faction) const;

	/** Native regression predicate shared by routing and behavioural tests. */
	static bool IsAutomaticPlayerCurrencyAccount(const AActor* AccountActor);

	/**
	 * Explicit-account policies accept no beneficiary override unless it is the
	 * exact currently registered account. Kept pure for native regression tests.
	 */
	static bool IsExplicitAccountSelectionValid(const AActor* RegisteredAccount,
		const AActor* PreferredBeneficiary);

	/**
	 * Register a live Narrative inventory account for a policy that requires one
	 * explicit faction account. Narrative remains the currency/save authority.
	 * Re-register the actor after it streams or respawns; UObject pointers are not
	 * stored as campaign state.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy")
	bool RegisterFactionCurrencyAccount(const FGameplayTag& Faction,
		ETerritoryIncomePayoutPolicy AccountRole, AActor* AccountActor);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy")
	void UnregisterFactionCurrencyAccount(const FGameplayTag& Faction, AActor* AccountActor);

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

	/** Broadcast when a faction cannot pay full guard upkeep. Deficit = required - paid. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFactionUpkeepDeficit, FGameplayTag, Faction, int32, Deficit);
	UPROPERTY(BlueprintAssignable, Category = "Territory|Economy")
	FOnFactionUpkeepDeficit OnFactionUpkeepDeficit;

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

	/** Runtime routing only. Narrative inventories own and persist the actual balances. */
	TMap<FGameplayTag, TWeakObjectPtr<AActor>> SharedFactionAccounts;
	TMap<FGameplayTag, TWeakObjectPtr<AActor>> FactionLeaderAccounts;

	UFUNCTION()
	void OnEconomyTick();

	UFUNCTION()
	void OnTerritoryControlChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	UFUNCTION()
	void OnTerritoryUnregistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	/** Resolve the exact accounts used for one periodic settlement policy. */
	TArray<AActor*> ResolvePeriodicSettlementAccounts(const FGameplayTag& Faction,
		ETerritoryIncomePayoutPolicy Policy) const;
	class UNarrativeInventoryComponent* ResolveCurrencyAccount(const AActor* RequestingActor) const;
	bool DoesAccountBelongToFaction(const AActor* AccountActor, const FGameplayTag& Faction) const;
	AActor* ResolveRegisteredCurrencyAccount(const FGameplayTag& Faction,
		ETerritoryIncomePayoutPolicy AccountRole) const;
	void RecordCurrencyTransaction(const FGameplayTag& Faction, int32 Amount,
		int32 BalanceAfter, const FString& Reason, ETerritoryTransactionType Type,
		const AActor* AccountActor, const FGameplayTag& SourceTerritory = FGameplayTag());
	bool TryDebitSettlementAccounts(const FGameplayTag& Faction, int32 PositiveAmount,
		ETerritoryIncomePayoutPolicy Policy, const FString& Reason,
		ETerritoryTransactionType Type);
};
