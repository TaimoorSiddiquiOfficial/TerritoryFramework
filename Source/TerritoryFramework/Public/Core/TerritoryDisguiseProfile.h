#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TerritoryDisguiseProfile.generated.h"

/** Why the perceived identity changed. Useful for quests, dialogue, UI, and GAS events. */
UENUM(BlueprintType)
enum class ETerritoryDisguiseChange : uint8
{
	Activated UMETA(DisplayName="Disguise Equipped"),
	Removed UMETA(DisplayName="Disguise Removed"),
	Compromised UMETA(DisplayName="Disguise Compromised"),
	Restored UMETA(DisplayName="Disguise Restored"),
	IdentityCheckPassed UMETA(DisplayName="Identity Check Passed"),
	IdentityCheckFailed UMETA(DisplayName="Identity Check Failed")
};

/** Read-only runtime information for quest conditions, dialogue, UI, and debugging. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryDisguiseSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|Disguise")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Disguise")
	bool bCompromisedForEveryone = false;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Disguise",
		meta=(Categories="Narrative.Factions"))
	FGameplayTag TrueFaction;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Disguise",
		meta=(Categories="Narrative.Factions"))
	FGameplayTag PerceivedFaction;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Disguise")
	TArray<FGameplayTag> CompromisedForFactions;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Disguise")
	float Quality = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Disguise")
	FGameplayTagContainer ClearanceTags;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Disguise")
	TObjectPtr<class UTerritoryDisguiseProfile> Profile = nullptr;
};

/**
 * Identity supplied by a uniform, badge, or other Narrative equippable item.
 *
 * This never changes the player's real Narrative faction. Territory guards use the
 * perceived faction only while deciding attitude, dialogue, and stealth exposure.
 * Ownership, diplomacy, reputation, quest allegiance, and capture credit continue
 * to use the true faction.
 */
UCLASS(BlueprintType, Const, meta=(DisplayName="Territory Disguise Profile"))
class TERRITORYFRAMEWORK_API UTerritoryDisguiseProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UTerritoryDisguiseProfile();

	/** Faction other characters should believe the wearer belongs to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Identity",
		meta=(Categories="Narrative.Factions",
			ToolTip="Faction shown to Territory guards while the disguise is intact. Easy example: a Bandit uniform makes a Heroes player look like Narrative.Factions.Bandits without changing the player's real faction."))
	FGameplayTag PerceivedFaction;

	/** Designer-controlled quality used by Territory identity checks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Identity",
		meta=(ClampMin="0.0", ClampMax="1.0",
			ToolTip="How convincing the disguise is. 1.0 means a perfect uniform for normal checks; restricted Places may still require clearance."))
	float Quality = 1.f;

	/** Badges or ranks carried by this disguise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Identity",
		meta=(ToolTip="Optional access tags. Easy example: add Territory.Disguise.Clearance.Officer so the uniform can pass an officer-only headquarters check."))
	FGameplayTagContainer ClearanceTags;

	/** A witnessed hostile action burns this identity for the observing faction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Exposure")
	bool bCompromiseWhenFiringWhileSeen = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Exposure")
	bool bCompromiseWhenDealingDamage = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Exposure")
	bool bCompromiseWhenDefenderKillIsSeen = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Exposure",
		meta=(ToolTip="A scripted Reveal Infiltrator or failed identity check burns the disguise."))
	bool bCompromiseFromScriptedReveal = true;

	/** GAS event sent to the wearer when the disguise becomes active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Gameplay Events",
		meta=(Categories="Territory.Event.Disguise"))
	FGameplayTag ActivatedEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Gameplay Events",
		meta=(Categories="Territory.Event.Disguise"))
	FGameplayTag RemovedEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Gameplay Events",
		meta=(Categories="Territory.Event.Disguise"))
	FGameplayTag CompromisedEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Gameplay Events",
		meta=(Categories="Territory.Event.Disguise"))
	FGameplayTag RestoredEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Gameplay Events",
		meta=(Categories="Territory.Event.Disguise"))
	FGameplayTag IdentityCheckPassedEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Gameplay Events",
		meta=(Categories="Territory.Event.Disguise"))
	FGameplayTag IdentityCheckFailedEventTag;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
