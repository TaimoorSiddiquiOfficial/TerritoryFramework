#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "Economy/TerritoryProductionProfile.h"
#include "TerritoryEconomySubsystem.generated.h"

class ATerritoryVolume;
class ATerritoryProperty;
class ANarrativePlayerCharacter;
class UNarrativeInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryProductionSettled,
	const FTerritoryProductionResult&, Result);

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

	/** NoCurrencyPayout disables both income credit and upkeep debit. */
	static bool IsCurrencySettlementEnabled(ETerritoryIncomePayoutPolicy Policy)
	{
		return Policy != ETerritoryIncomePayoutPolicy::NoCurrencyPayout;
	}

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

	/**
	 * Register the Narrative inventory that stores one faction's strategic resources.
	 * This map is runtime routing only; Narrative inventory owns balances and save data.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy|Resources")
	bool RegisterFactionResourceAccount(const FGameplayTag& Faction, AActor* AccountActor);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy|Resources")
	void UnregisterFactionResourceAccount(const FGameplayTag& Faction, AActor* AccountActor);

	/** Evaluate all registered and cached Property production sites for the current campaign day. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy|Resources")
	void ProcessResourceProduction();

	/**
	 * Execute a production-shaped recipe against one exact Narrative inventory.
	 * Used by daily production and reusable by server-authoritative crafting features.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy|Resources")
	bool ExecuteResourceRecipe(AActor* RequestingActor, const FGameplayTag& Faction,
		const FTerritoryProductionRule& Recipe, int32 UpgradeLevel, int32 BatchCount,
		const FGameplayTag& SourceTerritory, FTerritoryProductionResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "Territory|Economy|Resources")
	FTerritoryFactionResourceSnapshot GetFactionResourceSnapshot(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Economy|Resources")
	TArray<FTerritoryProductionSiteRecord> GetProductionSitesForFaction(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Economy|Resources")
	FTerritoryProductionSiteRecord GetProductionSite(const FGameplayTag& TerritoryTag) const;

	/** Refresh the durable World Partition record for a loaded Property. */
	void RefreshProductionSite(ATerritoryProperty* Property);

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

	TArray<FTerritoryProductionCheckpoint> GetProductionCheckpoints() const;
	TArray<FTerritoryProductionSiteRecord> GetAllProductionSites() const;
	TArray<FTerritoryFactionResourceSnapshot> GetAllResourceSnapshots() const;
	void RestoreProductionState(const TArray<FTerritoryProductionCheckpoint>& Checkpoints,
		const TArray<FTerritoryProductionSiteRecord>& Sites,
		const TArray<FTerritoryFactionResourceSnapshot>& ResourceSnapshots);

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

	UPROPERTY(BlueprintAssignable, Category = "Territory|Economy|Resources")
	FOnTerritoryProductionSettled OnProductionSettled;

	UPROPERTY(EditDefaultsOnly, Category = "Territory|Economy")
	float TickIntervalSeconds = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Territory|Economy")
	int32 MaxTransactionHistory = 500;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Territory|Economy")
	ETerritoryIncomePayoutPolicy IncomePayoutPolicy = ETerritoryIncomePayoutPolicy::EqualSplitOnlineMembers;

	/** Narrative accumulated-time units per resource production cycle. Narrative uses 2400 per day. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Territory|Economy|Resources",
		meta=(ClampMin="1.0"))
	float ProductionCycleLength = 2400.f;

	/** Maximum missed campaign days processed during one evaluation after load or time advance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Territory|Economy|Resources",
		meta=(ClampMin="1", ClampMax="365"))
	int32 MaxProductionCatchupCycles = 7;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTerritoryProductionInventoryTransactionTest;
#endif

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
	TMap<FGameplayTag, TWeakObjectPtr<AActor>> FactionResourceAccounts;

	/** Server scheduler state and client read models hydrated by TerritoryWorldState. */
	TMap<FString, FTerritoryProductionCheckpoint> ProductionCheckpoints;
	TMap<FGuid, FTerritoryProductionSiteRecord> ProductionSites;
	TMap<FGameplayTag, FTerritoryFactionResourceSnapshot> ResourceSnapshots;

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
	AActor* ResolveFallbackFactionAccount(const FGameplayTag& Faction) const;
	AActor* ResolveRegisteredResourceAccount(const FGameplayTag& Faction) const;
	UNarrativeInventoryComponent* ResolveResourceInventory(const FGameplayTag& Faction) const;
	int64 GetCurrentProductionCycle() const;
	static FString MakeProductionCheckpointKey(const FGuid& TerritoryGUID,
		const FGameplayTag& RuleTag);
	bool ExecuteResourceRecipeOnInventory(UNarrativeInventoryComponent* Inventory,
		const FGameplayTag& Faction, const FTerritoryProductionRule& Recipe,
		int32 UpgradeLevel, int32 BatchCount, FTerritoryProductionResult& OutResult);
	void EvaluateProductionSite(FTerritoryProductionSiteRecord& Site, int64 CurrentCycle);
	void UpdateResourceSnapshot(const FGameplayTag& Faction, int64 CurrentCycle);
	void PublishProductionState() const;
	void RecordCurrencyTransaction(const FGameplayTag& Faction, int32 Amount,
		int32 BalanceAfter, const FString& Reason, ETerritoryTransactionType Type,
		const AActor* AccountActor, const FGameplayTag& SourceTerritory = FGameplayTag());
	bool TryDebitSettlementAccounts(const FGameplayTag& Faction, int32 PositiveAmount,
		ETerritoryIncomePayoutPolicy Policy, const FString& Reason,
		ETerritoryTransactionType Type);
};
