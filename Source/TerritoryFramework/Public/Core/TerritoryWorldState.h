#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NarrativeSavableActor.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "TerritoryWorldState.generated.h"

class ATerritoryVolume;
class UTerritoryCityDefinition;
class UTerritoryDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTerritoryProductionStateChanged);

/** Replicated snapshot of a faction's economy parameters. */
USTRUCT(BlueprintType)
struct FReplicatedFactionEconomy
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Economy")
	FGameplayTag Faction;

	/** Reserved for compatibility; currency is stored in Narrative character inventories. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Economy")
	int32 Treasury = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Economy")
	int32 IncomePerTick = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Economy")
	int32 CostsPerTick = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Economy")
	int32 TerritoryCount = 0;
};

/**
 * Replicated transaction record for audit trail.
 */
USTRUCT(BlueprintType)
struct FReplicatedTransaction
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Transaction")
	FGuid TransactionID;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Transaction")
	FGameplayTag Faction;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Transaction")
	ETerritoryTransactionType Type = ETerritoryTransactionType::Income;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Transaction")
	int32 Amount = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Transaction")
	int32 BalanceAfter = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Transaction")
	double GameTime = 0.0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Transaction")
	FString Reason;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Transaction")
	FGameplayTag SourceTerritory;
};

/**
 * Replicated treaty record with full metadata.
 */
USTRUCT(BlueprintType)
struct FReplicatedTreaty
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Diplomacy")
	FGuid TreatyID;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Diplomacy")
	FGameplayTag FactionA;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Diplomacy")
	FGameplayTag FactionB;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Diplomacy")
	EDiplomacyState State = EDiplomacyState::None;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Diplomacy")
	double SignedGameTime = 0.0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Diplomacy")
	double ExpiryGameTime = -1.0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Diplomacy")
	bool bPermanent = true;
};

/**
 * Replicated active capture summary for a territory.
 */
USTRUCT(BlueprintType)
struct FReplicatedCaptureSummary
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Capture")
	FGameplayTag TerritoryTag;

	/** Stable actor identity used when the territory is streamed or its tag changes. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Capture")
	FGuid TerritoryGUID;

	/** Exact authored parent used to evaluate availability while World Partition unloads it. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Capture")
	FGameplayTag ParentTerritoryTag;

	/** Stable presentation survives actor streaming; it is copied from the Definition. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Capture|Directory")
	FText DisplayName;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Capture|Directory")
	ETerritoryHierarchyLevel HierarchyLevel = ETerritoryHierarchyLevel::Place;

	/** Authored direct-child count. Child identities remain hidden while locked. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Capture|Directory")
	int32 TotalChildren = 0;

	/** True when the row was reconciled with a Territory Definition asset. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Capture|Directory")
	bool bDefinitionBacked = false;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Capture")
	FGameplayTag CurrentOwner;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Capture")
	FGameplayTag ContestingFaction;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Capture")
	float ControlProgress = 0.f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Capture")
	ETerritoryState State = ETerritoryState::Unclaimed;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Capture")
	ETerritoryAvailability Availability = ETerritoryAvailability::Unlocked;
};

/**
 * Replicated faction reputation entry.
 */
USTRUCT(BlueprintType)
struct FReplicatedFactionReputation
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Diplomacy")
	FGameplayTag Faction;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Diplomacy")
	int32 Reputation = 0;
};

/**
 * Replicated, Narrative-savable snapshot of multiplayer-visible territory state.
 * ExportPersistentState pulls current subsystem data at save boundaries. Live
 * subsystem delegates and explicit production publication keep replicated read
 * models current between saves.
 *
 * Place at most one instance in the level (or auto-spawn from GameMode).
 */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryWorldState : public AActor, public INarrativeSavableActor
{
	GENERATED_BODY()

public:
	ATerritoryWorldState();
	virtual void Serialize(FArchive& Ar) override;

	/** Resolve the single strategic read-model actor for this world. */
	UFUNCTION(BlueprintPure, Category="Territory|World State",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory World State"))
	static ATerritoryWorldState* FindTerritoryWorldState(
		const UObject* WorldContextObject);

	// INarrativeSavableActor
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& NewGUID) override;
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	virtual bool ShouldRespawn_Implementation() const override;

	// ─── Economy API (server-authoritative) ───

	/** Projection writer used by the economy authority. Blueprint must mutate the economy subsystem. */
	void SetFactionTreasury(const FGameplayTag& Faction, const FTerritoryTreasury& Treasury);

	UFUNCTION(BlueprintPure, Category = "Territory|Economy")
	FTerritoryTreasury GetFactionTreasury(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Economy")
	TArray<FGameplayTag> GetAllFactionsWithEconomy() const;

	/** Projection writer used by the economy authority; not a gameplay mutation API. */
	void SetProductionState(
		const TArray<FTerritoryProductionCheckpoint>& Checkpoints,
		const TArray<FTerritoryProductionSiteRecord>& Sites,
		const TArray<FTerritoryFactionResourceSnapshot>& ResourceSnapshots);

	UFUNCTION(BlueprintPure, Category = "Territory|Economy|Resources")
	TArray<FTerritoryProductionSiteRecord> GetProductionSites() const { return ReplicatedProductionSites; }

	UFUNCTION(BlueprintPure, Category = "Territory|Economy|Resources")
	TArray<FTerritoryProductionSiteRecord> GetProductionSitesForFaction(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Economy|Resources")
	FTerritoryFactionResourceSnapshot GetFactionResourceSnapshot(const FGameplayTag& Faction) const;

	// ─── Transaction API (server-authoritative) ───

	/** Projection writer used by the economy authority; not a gameplay mutation API. */
	void RecordTransaction(const FReplicatedTransaction& Transaction);

	UFUNCTION(BlueprintPure, Category = "Territory|Transaction")
	TArray<FReplicatedTransaction> GetTransactionHistory(const FGameplayTag& Faction, int32 MaxEntries = 50) const;

	// ─── Treaty API (server-authoritative) ───

	/** Projection writer used by the diplomacy authority. Blueprint must mutate that subsystem. */
	void SetTreaty(const FReplicatedTreaty& Treaty);

	/** Projection writer used by the diplomacy authority. Blueprint must mutate that subsystem. */
	void RemoveTreaty(const FGuid& TreatyID);

	UFUNCTION(BlueprintPure, Category = "Territory|Diplomacy")
	TArray<FReplicatedTreaty> GetAllTreaties() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Diplomacy")
	FReplicatedTreaty GetTreatyBetween(const FGameplayTag& FactionA, const FGameplayTag& FactionB) const;

	// ─── Reputation API (server-authoritative) ───

	/** Projection writer used by the diplomacy authority. Blueprint must mutate that subsystem. */
	void SetReputation(const FGameplayTag& Faction, int32 Value);

	UFUNCTION(BlueprintPure, Category = "Territory|Diplomacy")
	int32 GetReputation(const FGameplayTag& Faction) const;

	// ─── Capture Summary API (server-authoritative) ───

	/**
	 * Root campaign assets used to seed City -> District -> Place directory rows
	 * before World Partition loads their actors. One City asset brings its complete
	 * hierarchy; designers do not repeat every District and Place here.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Directory",
		meta=(DisplayName="Campaign City Definitions"))
	TArray<TObjectPtr<UTerritoryCityDefinition>> CampaignCities;

	/** Projection writer used by Territory actors/subsystems; not a gameplay mutation API. */
	void SetCaptureSummary(const FReplicatedCaptureSummary& Summary);

	/** Native projection writer for one loaded actor. */
	void PublishTerritorySummary(const ATerritoryVolume* Territory);

	/** Native projection writer for Definition rows; never overwrites live ownership. */
	void RegisterDefinitionHierarchy(const UTerritoryDefinition* Definition);

	/** Native projection reconciliation for configured and loaded Definitions. */
	void RefreshStrategicDirectory();

	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	FReplicatedCaptureSummary GetCaptureSummary(const FGameplayTag& TerritoryTag) const;

	/** Complete replicated directory, including Definition rows whose actors are unloaded. */
	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	TArray<FReplicatedCaptureSummary> GetAllCaptureSummaries() const
	{
		return ReplicatedCaptureSummaries;
	}

	/**
	 * Counts the faction's currently unlocked, stable Claimed Districts from the
	 * complete replicated strategic directory. Definition-backed rows keep this
	 * correct while a District actor is unloaded by World Partition.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Capture|Hierarchy",
		meta=(DisplayName="Get Claimed District Count For Faction"))
	int32 GetClaimedDistrictCountForFaction(const FGameplayTag& Faction) const;

	/** Pure summary reducer shared by runtime queries and regression tests. */
	static int32 CountClaimedDistrictsForFaction(
		TConstArrayView<FReplicatedCaptureSummary> Summaries,
		const FGameplayTag& Faction);

	/** Replicated physical assault read model used by strategic UI for unloaded cells. */
	UFUNCTION(BlueprintPure, Category = "Territory|Assault")
	TArray<FTerritoryAssaultRecord> GetAllAssaultSummaries() const
	{
		return ReplicatedAssaults;
	}

	/**
	 * Checks the replicated capture read model, including summaries left behind by
	 * World Partition territories that are currently unloaded.
	 *
	 * Easy example: Market Place has just been secured, but Farm Place is still
	 * contested by the same two factions in another streamed cell. A Claimed-state
	 * peace event can use this query to avoid cancelling the Farm battle.
	 */
	UFUNCTION(BlueprintPure, Category = "Territory|Capture|Conflict",
		meta=(DisplayName="Has Contested Territory Between Factions"))
	bool HasContestedTerritoryBetweenFactions(const FGameplayTag& FactionA,
		const FGameplayTag& FactionB,
		const FGameplayTag& ExcludedTerritoryTag = FGameplayTag()) const;

	// ─── State Export/Import for Save System ───

	/** Native Narrative save bridge. Blueprint treats WorldState as read-only. */
	void ExportPersistentState();

	/** Native Narrative load bridge. Blueprint treats WorldState as read-only. */
	void ImportPersistentState();

	// ─── Delegates ───
	// NOTE: Economy and diplomacy change events are broadcast by the subsystems
	// (UTerritoryEconomySubsystem::OnEconomyTickFired, UTerritoryDiplomacySubsystem::OnDiplomacyStateChanged).
	// The WorldState actor is for save/load persistence and client-visible replicated state.
	// Subscribe to subsystem delegates for reactive gameplay, not to WorldState.

	UPROPERTY(BlueprintAssignable, Category = "Territory|Transaction")
	FOnTransactionRecorded OnTransactionRecorded;

	UPROPERTY(BlueprintAssignable, Category = "Territory|Economy|Resources")
	FOnTerritoryProductionStateChanged OnProductionStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#endif

	// ─── Replicated State (TArray-based — TMap cannot be replicated in UE5) ───
	// P2-13 migration path: These arrays replicate the full state on every change.
	// For production scale (large transaction/diplomacy histories), migrate to
	// FFastArraySerializer with per-row delta replication:
	//   1. Create FFastArraySerializer subclass per data type (e.g., FFactionEconomyArray)
	//   2. Each entry becomes a FFastArraySerializerEntry with stable ID
	//   3. Replace UPROPERTY(Replicated) TArray with UPROPERTY FFastArraySerializer member
	//   4. Bind OnRep callbacks to update client read models
	//   5. ExportPersistentState reads live FastArray data, not a second representation

	UPROPERTY(ReplicatedUsing=OnRep_EconomyState)
	TArray<FReplicatedFactionEconomy> ReplicatedTreasuries;

	UPROPERTY(ReplicatedUsing=OnRep_EconomyState)
	TArray<FReplicatedTransaction> ReplicatedTransactions;

	UPROPERTY(ReplicatedUsing=OnRep_ProductionState)
	TArray<FTerritoryProductionSiteRecord> ReplicatedProductionSites;

	UPROPERTY(ReplicatedUsing=OnRep_ProductionState)
	TArray<FTerritoryFactionResourceSnapshot> ReplicatedResourceSnapshots;

	UPROPERTY(ReplicatedUsing=OnRep_DiplomacyState)
	TArray<FReplicatedTreaty> ReplicatedTreaties;

	UPROPERTY(ReplicatedUsing=OnRep_DiplomacyState)
	TArray<FReplicatedFactionReputation> ReplicatedReputation;

	UPROPERTY(ReplicatedUsing=OnRep_DiplomacyState)
	TArray<FDiplomacyEvent> ReplicatedDiplomacyHistory;

	UPROPERTY(Replicated)
	TArray<FReplicatedCaptureSummary> ReplicatedCaptureSummaries;

	UPROPERTY(ReplicatedUsing=OnRep_AssaultState)
	TArray<FTerritoryAssaultRecord> ReplicatedAssaults;

	// ─── Save Data (mirrors replicated state) ───

	UPROPERTY(SaveGame)
	TArray<FReplicatedFactionEconomy> SavedTreasuries;

	UPROPERTY(SaveGame)
	TArray<FReplicatedTransaction> SavedTransactions;

	/** Server-only deterministic production scheduler state. */
	UPROPERTY(SaveGame)
	TArray<FTerritoryProductionCheckpoint> SavedProductionCheckpoints;

	UPROPERTY(SaveGame)
	TArray<FTerritoryProductionSiteRecord> SavedProductionSites;

	/** Read-only cache; Narrative inventories remain the saved quantity authority. */
	UPROPERTY(SaveGame)
	TArray<FTerritoryFactionResourceSnapshot> SavedResourceSnapshots;

	UPROPERTY(SaveGame)
	TArray<FReplicatedTreaty> SavedTreaties;

	UPROPERTY(SaveGame)
	TArray<FReplicatedFactionReputation> SavedReputation;

	UPROPERTY(SaveGame)
	TArray<FDiplomacyEvent> SavedDiplomacyHistory;

	UPROPERTY(SaveGame)
	TArray<FTerritoryAssaultRecord> SavedAssaults;

	/** Server-only deterministic cycle ledger; clients need assault read models, not scheduler history. */
	UPROPERTY(SaveGame)
	TArray<FTerritoryAssaultCycleRecord> SavedAssaultCycles;

	/**
	 * Read-only strategic directory cache for actors that may still be unloaded when
	 * a campaign resumes. This array is never applied to ATerritoryVolume; each
	 * Volume's SaveGame OwnershipData remains the sole durable capture authority.
	 * A loaded Volume publishes over the cached row as soon as it registers.
	 */
	UPROPERTY(SaveGame)
	TArray<FReplicatedCaptureSummary> SavedStrategicDirectory;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly, Category = "Territory|Identity",
		meta = (DisplayName = "World State GUID (auto-generated)"))
	FGuid WorldStateGUID;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTFDiplomacyWorldStateLiveBridge;
	friend class FTFWorldStateAssaultPersistenceRoundTrip;
	friend class FTFSaveDefaultReload;
	friend class FTFSavedStrategicDirectoryProjectionRoundTrip;
#endif
	void SyncSubsystemsFromReplicatedState();
	void SyncEconomySubsystemFromReplicatedState();
	void SyncDiplomacySubsystemFromReplicatedState();
	void SyncCounterAttackSubsystemFromReplicatedState();

	/** Hydrate the client-side economy query model after authoritative snapshots replicate. */
	UFUNCTION()
	void OnRep_EconomyState();

	UFUNCTION()
	void OnRep_ProductionState();

	/** Hydrate the client-side diplomacy query model after authoritative snapshots replicate. */
	UFUNCTION()
	void OnRep_DiplomacyState();

	UFUNCTION()
	void OnRep_AssaultState();

	// ─── P0-02: Live replication handlers ───
	// Subscribe to subsystem delegates so replicated arrays stay current between saves.

	// P0-03: OnTerritoryRegistered removed — Volume is sole ownership authority

	UFUNCTION()
	void OnEconomyTickLive(FGameplayTag Faction, FTerritoryEconomySnapshot Snapshot);

	UFUNCTION()
	void OnTransactionRecordedLive(const FTerritoryTransaction& Transaction);

	UFUNCTION()
	void OnDiplomacyChangedLive(FGameplayTag FactionA, FGameplayTag FactionB, EDiplomacyState NewState);

	UFUNCTION()
	void OnDiplomacyEventLive(const FDiplomacyEvent& Event);

	UFUNCTION()
	void OnReputationChangedLive(FGameplayTag Faction, int32 NewReputation);

	UFUNCTION()
	void OnTerritoryControlChangedLive(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void OnAssaultChangedLive(const FTerritoryAssaultRecord& Assault);

	/** Subscribe to all subsystem delegates for live replication. */
	void SubscribeToLiveUpdates();

	/** Unsubscribe from all subsystem delegates. */
	void UnsubscribeFromLiveUpdates();

	// P0-03: PendingCaptureSummaries retry machinery removed — TerritoryVolume
	// is sole authority for ownership persistence. No cross-actor sync needed.
};
