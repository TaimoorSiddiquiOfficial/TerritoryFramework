#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryCollectibleTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "TerritoryCollectibleSubsystem.generated.h"

class UTerritoryCollectibleDefinition;
class UTerritoryCollectibleSet;

/**
 * The campaign's collectible findings: which authored collectibles have been found,
 * where, and when on the campaign clock.
 *
 * What this subsystem is:
 *   - The single authority for "has the campaign found this collectible".
 *   - Server-authoritative and save-backed, mirroring UTerritoryEvidenceLedgerSubsystem:
 *     writes are BlueprintAuthorityOnly, and a client's read model is hydrated from
 *     ATerritoryWorldState rather than trusted locally.
 *
 * What this subsystem is deliberately not:
 *   - Not an inventory. Finding a collectible never grants an item; if a collectible
 *     should be sellable or usable, the pickup awards it through Narrative's inventory.
 *   - Not a per-player progression track. Findings are campaign-wide, because the
 *     campaign save is campaign-wide. A per-player split needs an agreed player
 *     identity authority first, which is a separate decision.
 *   - Not an achievement authority. Achievements must read these findings, not the
 *     other way round.
 *
 * Readiness: on a server (and in standalone) "no findings" is a real answer. On a
 * client, an empty read means the WorldState has not published yet, so callers must
 * check IsCollectibleLedgerReady() before treating the list as fact (AGENTS.md §4.2).
 */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryCollectibleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/**
	 * Deterministic context id for a pickup, so a retried overlap or a reloaded save
	 * records nothing new. Namespaced so a producer's key cannot collide with an
	 * unrelated deterministic GUID recipe.
	 */
	UFUNCTION(BlueprintPure, Category = "Territory|Collectibles")
	static FGuid MakeCollectibleContextId(const FString& Key);

	/**
	 * Records that the campaign found a collectible. Server only. Idempotent when
	 * ContextId is valid: a repeat is reported as bAlreadyCollected, not as an error.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Collectibles")
	FTerritoryCollectibleMutationResult RecordCollectibleFound(
		FGameplayTag CollectibleTag,
		FGameplayTag SourceTerritory,
		FGuid DiscoveredByPlayerId,
		FGuid ContextId);

	/** Records a fully formed finding (save import, tests, tools). Server only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Collectibles")
	FTerritoryCollectibleMutationResult RecordCollectibleFinding(
		const FTerritoryCollectibleFinding& Finding);

	/** True when an empty finding list is an answer rather than "not loaded yet". */
	UFUNCTION(BlueprintPure, Category = "Territory|Collectibles")
	bool IsCollectibleLedgerReady() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Collectibles")
	bool HasCollectibleBeenFound(FGameplayTag CollectibleTag) const;

	/** Reads the finding for one collectible. False when it has not been found. */
	UFUNCTION(BlueprintPure, Category = "Territory|Collectibles")
	bool GetCollectibleFinding(FGameplayTag CollectibleTag, FTerritoryCollectibleFinding& OutFinding) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Collectibles")
	TArray<FTerritoryCollectibleFinding> GetCollectibleFindings() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Collectibles")
	int32 GetFindingCount() const;

	/** Findings dropped by the retention limit. Non-zero means the window is not complete. */
	UFUNCTION(BlueprintPure, Category = "Territory|Collectibles")
	int32 GetEvictedFindingCount() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Collectibles")
	int32 GetTotalRecordedFindingCount() const;

	/** Found x of y, plus whether the answer is known at all. */
	UFUNCTION(BlueprintPure, Category = "Territory|Collectibles")
	FTerritoryCollectibleProgress GetCollectibleProgress() const;

	/**
	 * The authored set joined with the campaign's findings, in authored order. Loads
	 * the set's definitions, so call it when the player opens the screen, not per tick.
	 */
	UFUNCTION(BlueprintCallable, Category = "Territory|Collectibles")
	TArray<FTerritoryCollectibleView> BuildCollectibleViews() const;

	/** The authored set this campaign counts, or null when none is configured. */
	UFUNCTION(BlueprintPure, Category = "Territory|Collectibles")
	UTerritoryCollectibleSet* GetCollectibleSet() const;

	/** Broadcast after a new finding has been committed. */
	UPROPERTY(BlueprintAssignable, Category = "Territory|Collectibles")
	FOnTerritoryCollectibleFound OnCollectibleFound;

	/**
	 * Bounded window. Findings are one row per collectible, so this only evicts when a
	 * project ships more collectibles than the window, or when an unknown tag floods
	 * the ledger. Eviction is counted and reported rather than hidden.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Territory|Collectibles", meta = (ClampMin = "1"))
	int32 MaxCollectibleFindings = 512;

	/** Save boundary: the rows to persist. */
	const TArray<FTerritoryCollectibleFinding>& GetPersistentState() const;

	/** Save import and client hydration. Marks the read model known. */
	void RestorePersistentState(const TArray<FTerritoryCollectibleFinding>& InFindings);

private:
	/** Resolves the authored set from developer settings, for one campaign. */
	const UTerritoryCollectibleSet* ResolveCollectibleSet() const;

	double GetCampaignGameTime() const;
	int32 FindExistingFindingIndex(const FTerritoryCollectibleFinding& Finding) const;
	void EnforceRetention();

	TArray<FTerritoryCollectibleFinding> Findings;
	bool bFindingsKnown = false;
	int32 TotalRecordedFindings = 0;
	int32 EvictedFindings = 0;

	/** Resolved once; the set is authored content, not per-session state. */
	mutable TWeakObjectPtr<const UTerritoryCollectibleSet> CachedCollectibleSet;
	mutable bool bCollectibleSetResolved = false;

	friend class FTFCollectiblePersistence;
	friend class FTFCollectibleContract;
};
