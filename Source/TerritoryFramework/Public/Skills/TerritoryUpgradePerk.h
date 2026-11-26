#pragma once

#include "CoreMinimal.h"
#include "SkillTrees/TreePerk.h"

#include "TerritoryUpgradePerk.generated.h"

/**
 * Territory skill-tree perk.
 *
 * Buying it unlocks a faction-scoped Place upgrade tier. The perk itself is per-player (vendor
 * USkillTreeComponent), but the tier it writes is shared by the whole faction through
 * UTerritoryUpgradeSubsystem, so one team member unlocks it for everyone.
 */
UCLASS(BlueprintType, EditInlineNew, meta = (DisplayName = "Territory Upgrade Perk"))
class TERRITORYFRAMEWORK_API UTerritoryUpgradePerk : public UTreePerk
{
	GENERATED_BODY()

public:
	UTerritoryUpgradePerk(const FObjectInitializer& ObjectInitializer);

	/** Faction tier this perk unlocks (1 = Upgrade I, 2 = II, 3 = III). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Territory|Upgrade",
		meta = (ClampMin = "1", ClampMax = "3"))
	int32 UnlockedTier = 1;

	/** Previous tier perk that must be purchased first. The vendor prerequisite map is not used
	 *  for the runtime-injected Territory skill, so the chain is validated explicitly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Territory|Upgrade")
	TSubclassOf<UTreePerk> PrerequisitePerk;

	virtual void SetPerkLevel_Implementation(const int32 NewPerkLevel) override;
};

/** Upgrade I — unlocks faction Place tier 1. */
UCLASS(BlueprintType, EditInlineNew, meta = (DisplayName = "Territory Upgrade I — Unlock"))
class TERRITORYFRAMEWORK_API UTerritoryUpgradePerk_I : public UTerritoryUpgradePerk
{
	GENERATED_BODY()
public:
	UTerritoryUpgradePerk_I(const FObjectInitializer& ObjectInitializer);
};

/** Upgrade II — requires Upgrade I. */
UCLASS(BlueprintType, EditInlineNew, meta = (DisplayName = "Territory Upgrade II"))
class TERRITORYFRAMEWORK_API UTerritoryUpgradePerk_II : public UTerritoryUpgradePerk
{
	GENERATED_BODY()
public:
	UTerritoryUpgradePerk_II(const FObjectInitializer& ObjectInitializer);
};

/** Upgrade III — requires Upgrade II. No story quest is hard-wired (decision D4). */
UCLASS(BlueprintType, EditInlineNew, meta = (DisplayName = "Territory Upgrade III"))
class TERRITORYFRAMEWORK_API UTerritoryUpgradePerk_III : public UTerritoryUpgradePerk
{
	GENERATED_BODY()
public:
	UTerritoryUpgradePerk_III(const FObjectInitializer& ObjectInitializer);
};
