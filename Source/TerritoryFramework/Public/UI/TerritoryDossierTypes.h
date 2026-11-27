#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TerritoryDossierTypes.generated.h"

/**
 * One page of the Dossier screen.
 *
 * Batch 1 ships History, which is a read-only projection of the evidence ledger. The other
 * three pages exist so the authoring contract is visible, and they say what they do not track
 * yet instead of showing invented rows.
 */
UENUM(BlueprintType)
enum class ETerritoryDossierDrawer : uint8
{
	/** Committed facts: the evidence ledger projection. */
	History UMETA(DisplayName="History"),
	/** Player-authored notes. Not tracked yet. */
	Notes UMETA(DisplayName="Notes"),
	/** Collected items and lore finds. Not tracked yet. */
	Collectibles UMETA(DisplayName="Collectibles"),
	/** Per-NPC standing. Not tracked yet. */
	Relationships UMETA(DisplayName="Relationships")
};

/**
 * Filter applied to the History drawer.
 *
 * The Dossier is presentation only: every row it shows is a read-only snapshot of one
 * committed fact from UTerritoryEvidenceLedgerSubsystem. Nothing here decides gameplay
 * state, and no category is stored anywhere - a category is derived from the record's
 * Territory.Evidence.* tag at read time so the ledger keeps its single authority
 * (AGENTS.md §4.1).
 */
UENUM(BlueprintType)
enum class ETerritoryDossierCategory : uint8
{
	/** Every retained fact, newest first. */
	All UMETA(DisplayName="All Evidence"),
	/** Captures that committed and territories that were lost. */
	Control UMETA(DisplayName="Control"),
	/** Guards and attackers that died in a physical assault. */
	Conflict UMETA(DisplayName="Conflict"),
	/** Exposures and compromised disguises. */
	Stealth UMETA(DisplayName="Stealth"),
	/** Treaties signed and treaties broken. */
	Diplomacy UMETA(DisplayName="Diplomacy"),
	/** Quests and dialogues that completed. */
	Tales UMETA(DisplayName="Tales")
};

/**
 * How much history the ledger can honestly show right now.
 *
 * A client replica is unready until ATerritoryWorldState has hydrated it, so an empty
 * row list on an unready ledger means "unknown", never "no evidence" (AGENTS.md §4.2).
 * Retention is bounded, so a full window is reported as truncated rather than passed off
 * as the campaign's complete history.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryDossierLedgerState
{
	GENERATED_BODY()

	/** True when the ledger answering this query is authoritative for this machine. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Dossier")
	bool bLedgerReady = false;

	/** True when rows were evicted, so this list is a window and not the full history. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Dossier")
	bool bHistoryTruncated = false;

	/** Rows currently retained by the ledger. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Dossier")
	int32 RetainedCount = 0;

	/** Rows the ledger dropped because retention is bounded. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Dossier")
	int32 EvictedCount = 0;

	/** Rows the campaign ever recorded, including evicted ones. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Dossier")
	int32 TotalRecordedCount = 0;

	/** Rows matching the active drawer after the filter was applied. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Dossier")
	int32 FilteredCount = 0;
};

/**
 * Read-only presentation snapshot of one ledger record.
 *
 * Field names intentionally match FTerritoryEvidenceRecord so a reader can trace every
 * value back to the authoritative row; the widget never mutates the ledger.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryDossierRecordView
{
	GENERATED_BODY()

	/**
	 * Grouping identity the ledger stored for this fact, when the producer supplied one.
	 * The ledger identifies a row by EvidenceTag + SubjectId + ContextId, so this is the
	 * closest thing a row has to an id; an invalid ContextId means the producer opted out
	 * of deduplication and the row is identified by its position in campaign order only.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Dossier")
	FGuid ContextId;

	/** The Territory.Evidence.* fact this row describes. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Dossier")
	FGameplayTag EvidenceTag;

	/** Drawer this tag belongs to; derived from the tag, never authored per row. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Dossier")
	ETerritoryDossierCategory Category = ETerritoryDossierCategory::All;

	/** Durable subject the fact is about (guard, attacker, territory, or faction pair). */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Dossier")
	FGuid SubjectId;

	/** Faction that caused the fact, when one is known. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Dossier")
	FGameplayTag InstigatorFaction;

	/** Territory the fact happened in, when one is known. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Dossier")
	FGameplayTag SourceTerritory;

	/** Bounded importance the ledger stored, 1-1000. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Dossier")
	int32 Weight = 1;

	/** Campaign clock the ledger stamped the fact with. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Dossier")
	double GameTimeSeconds = 0.0;
};
