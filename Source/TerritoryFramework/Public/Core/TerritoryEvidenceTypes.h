#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TerritoryEvidenceTypes.generated.h"

/**
 * One durable, server-recorded physical fact about the campaign.
 *
 * This is an observation, not an opinion. There is deliberately no trust, attitude,
 * friendship, or threat field here: the story bible keeps relationship variables
 * with Narrative Pro and gives Territory the physical consequences
 * (Docs/TDA_STORY_BIBLE_AND_VERTICAL_SLICE.md:162, :176). Derived relationship
 * values are a later, separately approved decision, and they must be computed from
 * these facts rather than stored beside them.
 *
 * Stable identity: a fact is identified by EvidenceTag + SubjectId + ContextId. A
 * producer that passes a valid ContextId gets idempotency for free, so a retried
 * commit cannot record the same assault or capture twice.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryEvidenceRecord
{
	GENERATED_BODY()

	/** What happened, from the Territory.Evidence.* family. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Evidence",
		meta = (Categories = "Territory.Evidence"))
	FGameplayTag EvidenceTag;

	/**
	 * Durable identity of the actor, place, or record the fact is about. Never a live
	 * pointer. Invalid means the fact is attributed by faction alone.
	 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Evidence")
	FGuid SubjectId;

	/** Faction the fact is attributed to. Invalid means the fact is subject-scoped. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Evidence",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag InstigatorFaction;

	/** Territory the fact happened in. Invalid means the fact is not territory-scoped. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Evidence",
		meta = (Categories = "Territory"))
	FGameplayTag SourceTerritory;

	/**
	 * Optional grouping identity (one assault, one capture attempt, one dialogue).
	 * When valid, EvidenceTag + SubjectId + ContextId is idempotent, so a repeated
	 * commit records nothing new.
	 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Evidence")
	FGuid ContextId;

	/**
	 * Relative size of this single fact. Bounded per record so one producer cannot
	 * dominate a query with a single row. Never interpreted as a relationship value.
	 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Evidence",
		meta = (ClampMin = "1", ClampMax = "1000"))
	int32 Weight = 1;

	/**
	 * Campaign-clock seconds from Narrative's accumulated time, not wall clock, so a
	 * restored save keeps the same ordering and intervals.
	 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Evidence")
	double GameTimeSeconds = 0.0;
};

/**
 * Structured result of a ledger mutation, mirroring the Territory mutation-result
 * convention: success, what was stored, and why nothing was stored.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryEvidenceMutationResult
{
	GENERATED_BODY()

	/** True only when a new record was appended to the ledger. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Evidence")
	bool bSuccess = false;

	/** The appended row on success; a default row when nothing was stored. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Evidence")
	FTerritoryEvidenceRecord Record;

	/** Empty on success; the rejection reason on failure. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Evidence")
	FText FailureReason;

	/** True when an identical EvidenceTag + SubjectId + ContextId already existed. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Evidence")
	bool bAlreadyRecorded = false;
};

/** Broadcast after a new fact has been committed to the ledger. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryEvidenceRecorded, const FTerritoryEvidenceRecord&, Record);
