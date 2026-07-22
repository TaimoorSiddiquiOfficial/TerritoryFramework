#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NarrativeSavableActor.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "TerritoryWorldState.generated.h"

class ATerritoryVolume;

/**
 * Replicated snapshot of a faction's economy state.
 */
USTRUCT(BlueprintType)
struct FReplicatedFactionEconomy
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Economy")
	FGameplayTag Faction;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Economy")
	int32 Treasury = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Economy")
	int32 IncomePerTick = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Economy")
	int32 CostsPerTick = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Economy")
	int32 TerritoryCount = 0;
};

/**
 * Replicated transaction record for audit trail.
 */
USTRUCT(BlueprintType)
struct FReplicatedTransaction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Transaction")
	FGuid TransactionID;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Transaction")
	FGameplayTag Faction;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Transaction")
	ETerritoryTransactionType Type = ETerritoryTransactionType::Income;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Transaction")
	int32 Amount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Transaction")
	int32 BalanceAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Transaction")
	double GameTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Transaction")
	FString Reason;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Transaction")
	FGameplayTag SourceTerritory;
};

/**
 * Replicated treaty record with full metadata.
 */
USTRUCT(BlueprintType)
struct FReplicatedTreaty
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Diplomacy")
	FGuid TreatyID;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Diplomacy")
	FGameplayTag FactionA;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Diplomacy")
	FGameplayTag FactionB;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Diplomacy")
	EDiplomacyState State = EDiplomacyState::None;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Diplomacy")
	double SignedGameTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Diplomacy")
	double ExpiryGameTime = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Diplomacy")
	bool bPermanent = true;
};

/**
 * Replicated active capture summary for a territory.
 */
USTRUCT(BlueprintType)
struct FReplicatedCaptureSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Capture")
	FGameplayTag TerritoryTag;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Capture")
	FGameplayTag CurrentOwner;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Capture")
	FGameplayTag ContestingFaction;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Capture")
	float ControlProgress = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Capture")
	ETerritoryState State = ETerritoryState::Unclaimed;
};

/**
 * Replicated faction reputation entry.
 */
USTRUCT(BlueprintType)
struct FReplicatedFactionReputation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Diplomacy")
	FGameplayTag Faction;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Diplomacy")
	int32 Reputation = 0;
};

/**
 * Replicated, Narrative-savable actor containing all persistent,
 * multiplayer-visible territory state. This is the single authoritative
 * source for economy, diplomacy, reputation, and capture summaries.
 *
 * Place one instance in the level (or auto-spawn from GameMode).
 * Subsystems read from and write to this actor for all persistent state.
 */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryWorldState : public AActor, public INarrativeSavableActor
{
	GENERATED_BODY()

public:
	ATerritoryWorldState();

	// INarrativeSavableActor
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& NewGUID) override;
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	virtual bool ShouldRespawn_Implementation() const override;

	// ─── Economy API (server-authoritative) ───

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy")
	void SetFactionTreasury(const FGameplayTag& Faction, const FTerritoryTreasury& Treasury);

	UFUNCTION(BlueprintPure, Category = "Territory|Economy")
	FTerritoryTreasury GetFactionTreasury(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Economy")
	TArray<FGameplayTag> GetAllFactionsWithEconomy() const;

	// ─── Transaction API (server-authoritative) ───

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Transaction")
	void RecordTransaction(const FReplicatedTransaction& Transaction);

	UFUNCTION(BlueprintPure, Category = "Territory|Transaction")
	TArray<FReplicatedTransaction> GetTransactionHistory(const FGameplayTag& Faction, int32 MaxEntries = 50) const;

	// ─── Treaty API (server-authoritative) ───

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Diplomacy")
	void SetTreaty(const FReplicatedTreaty& Treaty);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Diplomacy")
	void RemoveTreaty(const FGuid& TreatyID);

	UFUNCTION(BlueprintPure, Category = "Territory|Diplomacy")
	TArray<FReplicatedTreaty> GetAllTreaties() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Diplomacy")
	FReplicatedTreaty GetTreatyBetween(const FGameplayTag& FactionA, const FGameplayTag& FactionB) const;

	// ─── Reputation API (server-authoritative) ───

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Diplomacy")
	void SetReputation(const FGameplayTag& Faction, int32 Value);

	UFUNCTION(BlueprintPure, Category = "Territory|Diplomacy")
	int32 GetReputation(const FGameplayTag& Faction) const;

	// ─── Capture Summary API (server-authoritative) ───

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	void SetCaptureSummary(const FReplicatedCaptureSummary& Summary);

	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	FReplicatedCaptureSummary GetCaptureSummary(const FGameplayTag& TerritoryTag) const;

	// ─── State Export/Import for Save System ───

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Save")
	void ExportPersistentState();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Save")
	void ImportPersistentState();

	// ─── Delegates ───

	UPROPERTY(BlueprintAssignable, Category = "Territory|Economy")
	FOnEconomyTick OnEconomyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Territory|Diplomacy")
	FOnDiplomacyStateChanged OnDiplomacyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Territory|Transaction")
	FOnTransactionRecorded OnTransactionRecorded;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#endif

	// ─── Replicated State (TArray-based — TMap cannot be replicated in UE5) ───

	UPROPERTY(Replicated)
	TArray<FReplicatedFactionEconomy> ReplicatedTreasuries;

	UPROPERTY(Replicated)
	TArray<FReplicatedTransaction> ReplicatedTransactions;

	UPROPERTY(Replicated)
	TArray<FReplicatedTreaty> ReplicatedTreaties;

	UPROPERTY(Replicated)
	TArray<FReplicatedFactionReputation> ReplicatedReputation;

	UPROPERTY(Replicated)
	TArray<FReplicatedCaptureSummary> ReplicatedCaptureSummaries;

	// ─── Save Data (mirrors replicated state) ───

	UPROPERTY(SaveGame)
	TArray<FReplicatedFactionEconomy> SavedTreasuries;

	UPROPERTY(SaveGame)
	TArray<FReplicatedTransaction> SavedTransactions;

	UPROPERTY(SaveGame)
	TArray<FReplicatedTreaty> SavedTreaties;

	UPROPERTY(SaveGame)
	TArray<FReplicatedFactionReputation> SavedReputation;

	UPROPERTY(SaveGame)
	TArray<FReplicatedCaptureSummary> SavedCaptureSummaries;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly, Category = "Territory|Identity",
		meta = (DisplayName = "World State GUID (auto-generated)"))
	FGuid WorldStateGUID;

private:
	void SyncSubsystemsFromReplicatedState();
};
