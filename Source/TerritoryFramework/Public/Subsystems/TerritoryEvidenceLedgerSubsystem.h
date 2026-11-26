#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/TerritoryEvidenceTypes.h"
#include "TerritoryEvidenceLedgerSubsystem.generated.h"

class ANarrativeGameState;

/**
 * Server-authoritative ledger of physical campaign evidence.
 *
 * What this subsystem is
 * ----------------------
 * It stores durable, attributable facts: a capture that committed, a guard that died,
 * a disguise that was compromised, a treaty that was signed or broken, a story beat
 * that completed. Every fact carries a Territory.Evidence.* tag, a durable subject
 * identity, an optional faction, an optional Territory, an optional context identity,
 * a bounded weight, and campaign time. Records are server-only, save-backed, and
 * projected to clients through ATerritoryWorldState, never through a live pointer.
 *
 * What this subsystem is NOT
 * --------------------------
 * It is not a relationship, trust, friendship, or reputation store, and it does not
 * own anyone's opinion of anyone. The story bible assigns relationship variables,
 * quest history, and dialogue state to Narrative Pro and the physical consequences to
 * Territory (Docs/TDA_STORY_BIBLE_AND_VERTICAL_SLICE.md:162, :176, :503). A derived
 * per-NPC relationship value is a separate decision; if it is ever added, it must be
 * computed from these facts rather than stored beside them, so the campaign keeps one
 * authority for one kind of state (AGENTS.md §4.1).
 *
 * Why the name says Evidence and not Relationship: class names are promises
 * (AGENTS.md §9). This class observes. It does not judge.
 *
 * Readiness
 * ---------
 * A client replica stays unready until ATerritoryWorldState has hydrated it, so an
 * empty read on a client is reported as unknown instead of "no evidence"
 * (AGENTS.md §4.2). Servers become ready when the world begins play, because on a
 * server "no facts yet" is a real answer.
 *
 * Retention
 * ---------
 * Retention is a bounded window (MaxEvidenceRecords). Evicted rows are counted and
 * logged, never dropped silently, so a consumer can tell a full history from a window.
 */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryEvidenceLedgerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ─── Lifecycle ───

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// ─── Mutations (server only) ───

	/**
	 * Records one physical fact. Server only.
	 *
	 * Call this after the world change it describes has committed, never before and
	 * never instead of it (AGENTS.md §4.3): evidence follows the commit, it does not
	 * replace it.
	 *
	 * A fact must be attributable: either SubjectId or InstigatorFaction has to be
	 * valid. Passing a valid ContextId makes the call idempotent for
	 * EvidenceTag + SubjectId + ContextId, so a retried commit cannot record the same
	 * assault, capture, or dialogue twice.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Evidence")
	FTerritoryEvidenceMutationResult RecordEvidence(
		FGameplayTag EvidenceTag,
		FGuid SubjectId,
		FGameplayTag InstigatorFaction,
		FGameplayTag SourceTerritory,
		FGuid ContextId,
		int32 Weight = 1);

	/**
	 * C++ entry point for producers that already hold a full record, including ones
	 * replaying a historical fact with an explicit GameTimeSeconds. Same validation
	 * and idempotency rules as RecordEvidence.
	 */
	FTerritoryEvidenceMutationResult RecordEvidenceRecord(const FTerritoryEvidenceRecord& Evidence);

	// ─── Queries ───

	UFUNCTION(BlueprintPure, Category = "Territory|Evidence")
	bool IsEvidenceLedgerReady() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Evidence")
	TArray<FTerritoryEvidenceRecord> GetEvidenceRecords() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Evidence")
	TArray<FTerritoryEvidenceRecord> GetEvidenceForSubject(FGuid SubjectId) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Evidence")
	TArray<FTerritoryEvidenceRecord> GetEvidenceForTag(FGameplayTag EvidenceTag) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Evidence")
	TArray<FTerritoryEvidenceRecord> GetEvidenceForTerritory(FGameplayTag SourceTerritory) const;

	/** Sum of recorded weights for this exact tag across the whole ledger. */
	UFUNCTION(BlueprintPure, Category = "Territory|Evidence")
	int32 GetEvidenceWeight(FGameplayTag EvidenceTag) const;

	/** Sum of recorded weights for this exact tag, narrowed to one durable subject. */
	UFUNCTION(BlueprintPure, Category = "Territory|Evidence")
	int32 GetEvidenceWeightForSubject(FGameplayTag EvidenceTag, FGuid SubjectId) const;

	/** Rows currently retained. Reports a window, not the campaign's full history. */
	UFUNCTION(BlueprintPure, Category = "Territory|Evidence")
	int32 GetRetainedEvidenceCount() const;

	/** Rows this instance dropped because retention is bounded. */
	UFUNCTION(BlueprintPure, Category = "Territory|Evidence")
	int32 GetEvictedEvidenceCount() const;

	/** Rows this instance ever accepted, including evicted ones. */
	UFUNCTION(BlueprintPure, Category = "Territory|Evidence")
	int32 GetTotalRecordedEvidenceCount() const;

	// ─── Persistence and replication support (C++) ───

	/**
	 * Deterministic context id for producers that need a replay-safe key.
	 *
	 * The same key string always yields the same GUID, in every process and every
	 * session, so a producer that rebuilds the key from durable state (a captured
	 * territory, a faction pair, the campaign time) can call this and let the ledger's
	 * idempotency rule absorb a replayed or duplicated commit instead of record it twice.
	 * Producers remain free to leave ContextId invalid, which means "never deduplicate".
	 */
	static FGuid MakeEvidenceContextId(const FString& Key);

	/**
	 * The campaign clock the ledger stamps rows with, for producers that live on actors.
	 *
	 * Actor-side producers need the same time base the ledger uses internally: a
	 * producer that folded a different clock into its ContextId would build a key that
	 * never matches the row it is supposed to de-duplicate against. Returns 0 when the
	 * world or Narrative game state is unavailable, which the ledger treats as
	 * "stamp it yourself at commit time".
	 */
	static double ResolveCampaignGameTime(const UObject* WorldContextObject);

	/** Committed facts in recording order. ATerritoryWorldState mirrors this array. */
	const TArray<FTerritoryEvidenceRecord>& GetPersistentState() const { return EvidenceRecords; }

	/**
	 * Replaces the ledger with an authoritative or replicated window. Called by
	 * ATerritoryWorldState on save import (server) and on replication (client); a
	 * hydrated client may then treat an empty result as a real answer.
	 */
	void RestorePersistentState(const TArray<FTerritoryEvidenceRecord>& InRecords);

	/** Broadcast after a fact committed, so the projection can push it immediately. */
	UPROPERTY(BlueprintAssignable, Category = "Territory|Evidence")
	FOnTerritoryEvidenceRecorded OnEvidenceRecorded;

	/** Retained facts before the oldest rows are evicted. */
	UPROPERTY(EditDefaultsOnly, Category = "Territory|Evidence", meta = (ClampMin = "1"))
	int32 MaxEvidenceRecords = 512;

	/** Largest weight a single record may carry. */
	UPROPERTY(EditDefaultsOnly, Category = "Territory|Evidence", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 MaxEvidenceWeight = 100;

private:
	ANarrativeGameState* GetNarrativeGameState() const;
	double GetCampaignGameTime() const;

	/** Index of an identical fact when ContextId makes the call idempotent. */
	int32 FindExistingRecordIndex(const FTerritoryEvidenceRecord& Evidence) const;

	/** Drops the oldest rows once the retention window is exceeded. */
	void EnforceRetention();

	UPROPERTY()
	TArray<FTerritoryEvidenceRecord> EvidenceRecords;

	/** True once a server owns its state, or a replica has been hydrated. */
	bool bEvidenceLedgerReady = false;

	/** Monotonic counters so a bounded window is never mistaken for full history. */
	int32 TotalRecordedEvidence = 0;
	int32 EvictedEvidence = 0;
};
