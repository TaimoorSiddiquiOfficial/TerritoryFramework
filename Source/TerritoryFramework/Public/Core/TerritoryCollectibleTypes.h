#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TerritoryCollectibleTypes.generated.h"

class UTexture2D;

/**
 * One durable, server-recorded collectible finding.
 *
 * A finding answers a narrow question: has this campaign found this collectible, in
 * which territory, and when on the campaign clock? Everything else the Dossier shows
 * for a collectible (name, description, icon, family) belongs to the authored
 * UTerritoryCollectibleDefinition, because that is presentation data that must not be
 * duplicated per finding and must not be saved into a player's record.
 *
 * Stable identity: a finding is identified by CollectibleTag + ContextId, so a retried
 * pickup cannot record the same collectible twice.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryCollectibleFinding
{
	GENERATED_BODY()

	/** What was found, from the Territory.Collectible.* family. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Collectibles",
		meta = (Categories = "Territory.Collectible"))
	FGameplayTag CollectibleTag;

	/** Territory the finding happened in. Invalid means the finding is not territory-scoped. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Collectibles",
		meta = (Categories = "Territory"))
	FGameplayTag SourceTerritory;

	/**
	 * Player who found it, when the session exposed a stable identity. Invalid in
	 * standalone play is a real answer, not a bug: the finding is campaign-wide, and
	 * per-player completion is a later, separately approved decision.
	 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Collectibles")
	FGuid DiscoveredByPlayerId;

	/**
	 * Pickup identity. When valid, CollectibleTag + ContextId is idempotent, so a
	 * retried overlap or a reloaded save records nothing new.
	 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Collectibles")
	FGuid ContextId;

	/**
	 * Campaign-clock seconds from Narrative's accumulated time, not wall clock, so a
	 * restored save keeps the same ordering.
	 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Collectibles")
	double GameTimeSeconds = 0.0;
};

/**
 * A collectible as the UI needs it: authored presentation joined with the campaign's
 * finding, so a widget never has to load a definition asset to decide what to draw.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryCollectibleView
{
	GENERATED_BODY()

	/** Stable identity, matched against FTerritoryCollectibleFinding::CollectibleTag. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles",
		meta = (Categories = "Territory.Collectible"))
	FGameplayTag CollectibleTag;

	/** Authored family, for grouping. Invalid means the definition did not name one. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles",
		meta = (Categories = "Territory.CollectibleCategory"))
	FGameplayTag CategoryTag;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles")
	FText Description;

	/** Authored icon. Soft, because a full grid of icons must not be force-loaded. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles")
	TSoftObjectPtr<UTexture2D> Icon;

	/** True when the campaign has recorded a finding for this collectible. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles")
	bool bFound = false;

	/** Campaign seconds of the finding. 0 when it has not been found. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles")
	double FoundGameTimeSeconds = 0.0;

	/** Territory the finding happened in. Invalid when the collectible is still hidden. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles",
		meta = (Categories = "Territory"))
	FGameplayTag FoundTerritory;
};

/**
 * Structured result of a collectible mutation, mirroring the Territory
 * mutation-result convention: success, what was stored, and why nothing was stored.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryCollectibleMutationResult
{
	GENERATED_BODY()

	/** True only when a new finding was appended. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles")
	bool bSuccess = false;

	/** The appended finding on success; a default finding when nothing was stored. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles")
	FTerritoryCollectibleFinding Finding;

	/** Empty on success; the rejection reason on failure. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles")
	FText FailureReason;

	/** True when CollectibleTag + ContextId was already recorded. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles")
	bool bAlreadyCollected = false;
};

/**
 * How much of an authored collectible set the campaign has found, plus whether that
 * answer is trustworthy yet. A client whose WorldState has not published the findings
 * must show "unknown", never "0 found" (AGENTS.md §4.2).
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryCollectibleProgress
{
	GENERATED_BODY()

	/** False while the read model is unknown; counters below must then be ignored. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles")
	bool bProgressKnown = false;

	/** Collectibles in the authored set that count toward completion. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles")
	int32 TotalCount = 0;

	/** Found collectibles that count toward completion. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles")
	int32 FoundCount = 0;

	/** Findings for collectibles outside the current authored set. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Collectibles")
	int32 UnknownTagCount = 0;
};

/** Broadcast after a new finding has been committed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryCollectibleFound, const FTerritoryCollectibleFinding&, Finding);
