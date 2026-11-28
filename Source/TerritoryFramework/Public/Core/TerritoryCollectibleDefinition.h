#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TerritoryCollectibleDefinition.generated.h"

class UTexture2D;

/**
 * One authored collectible.
 *
 * This asset is presentation plus identity: the stable tag the campaign records, the
 * name and description the Dossier prints, the icon it draws, and the family it is
 * grouped under. It holds no campaign state, because "has this been found" belongs to
 * UTerritoryCollectibleSubsystem and is saved with the campaign, not with content.
 *
 * One entry = one collectible. Never author two definitions with the same tag: the
 * finding is keyed by tag, so duplicates would make two assets share one finding.
 */
UCLASS(BlueprintType, Const, meta = (DisplayName = "Territory Collectible"))
class TERRITORYFRAMEWORK_API UTerritoryCollectibleDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable identity recorded in the campaign's findings. Required. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible",
		meta = (Categories = "Territory.Collectible"))
	FGameplayTag CollectibleTag;

	/** Name the Dossier prints for this collectible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	FText DisplayName;

	/** Body copy shown when the entry is inspected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible", meta = (MultiLine = "true"))
	FText Description;

	/** Family this collectible is grouped under. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible",
		meta = (Categories = "Territory.CollectibleCategory"))
	FGameplayTag CategoryTag;

	/**
	 * Grid icon. Soft on purpose: a full Dossier grid must be able to list a hundred
	 * entries without force-loading a hundred textures, and a hidden collectible's
	 * icon must not leak through a hard reference.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	TSoftObjectPtr<UTexture2D> Icon;

	/**
	 * Count toward the set's completion counter. Turn this off for bonus, debug, or
	 * story-critical entries that must not inflate the "found x of y" total.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	bool bCountsTowardCompletion = true;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/**
 * The authored list of collectibles the campaign counts.
 *
 * A set exists so the "x of y" denominator is authored data instead of something the
 * runtime discovers by scanning the world. Scanning would make the total depend on
 * World Partition streaming, so a player who has not visited a district yet would see
 * a smaller total and the Dossier would quietly lie.
 *
 * Referenced from UTerritoryDeveloperSettings so the campaign has exactly one set.
 */
UCLASS(BlueprintType, Const, meta = (DisplayName = "Territory Collectible Set"))
class TERRITORYFRAMEWORK_API UTerritoryCollectibleSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Optional name for the set, for UI that wants to title the grid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible Set")
	FText SetDisplayName;

	/** Every authored collectible in this set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible Set",
		meta = (TitleProperty = "CollectibleTag"))
	TArray<TSoftObjectPtr<UTerritoryCollectibleDefinition>> Collectibles;

	/**
	 * Loads every definition in the set and appends it to OutDefinitions, skipping
	 * null entries. Returns the number appended. Synchronous by design: the Dossier is
	 * an explicit player action, and the alternative (a partially loaded grid whose
	 * totals move while the player watches) is worse than a single load.
	 */
	UFUNCTION(BlueprintCallable, Category = "Territory|Collectibles")
	int32 LoadDefinitions(TArray<UTerritoryCollectibleDefinition*>& OutDefinitions) const;

	/** How many entries count toward completion, ignoring unset references. */
	UFUNCTION(BlueprintPure, Category = "Territory|Collectibles")
	int32 GetCompletionCount() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
