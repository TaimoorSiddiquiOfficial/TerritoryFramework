#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "TerritoryUpgradeDefinition.generated.h"

class AActor;
class APlayerController;
class APawn;
class UNarrativeCondition;
class UNarrativeEvent;
class UNarrativeItem;
class UTalesComponent;

/** How one upgrade cost line is paid. */
UENUM(BlueprintType)
enum class ETerritoryUpgradePayment : uint8
{
	/** Debit the requester's Narrative inventory account. */
	Currency,
	/** Consume items from the requester's Narrative inventory. */
	Item,
	/** No material cost; the attached Narrative conditions are the payment (quest, reputation, flag). */
	ConditionOnly,
	/** No cost and no condition; tutorial/story grant. */
	Free
};

/**
 * One authored cost line for an upgrade tier.
 *
 * Required lines must all pass. Optional lines are alternative routes: at least one of them must
 * pass when any optional line exists, and the first satisfiable one is the route that is charged.
 * Conditions are instanced vendor conditions, so the same object can be shared with a quest,
 * a dialogue or an interaction.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryUpgradeCostEntry
{
	GENERATED_BODY()

	/** Optional semantic route tag (Territory.Upgrade.Path.Currency / Resource / Quest / Free). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost",
		meta = (Categories = "Territory.Upgrade.Path"))
	FGameplayTag PathTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost")
	ETerritoryUpgradePayment Payment = ETerritoryUpgradePayment::Currency;

	/** Currency amount for a Currency line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost",
		meta = (EditCondition = "Payment == ETerritoryUpgradePayment::Currency", EditConditionHides,
			ClampMin = "0"))
	int32 CurrencyAmount = 500;

	/** Item consumed by an Item line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost",
		meta = (EditCondition = "Payment == ETerritoryUpgradePayment::Item", EditConditionHides))
	TSoftClassPtr<UNarrativeItem> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost",
		meta = (EditCondition = "Payment == ETerritoryUpgradePayment::Item", EditConditionHides,
			ClampMin = "1"))
	int32 ItemQuantity = 1;

	/** True = one satisfied optional route is enough; false = this line is mandatory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost")
	bool bOptional = false;

	/** Vendor conditions gating this specific route. Empty passes. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Cost")
	TArray<TObjectPtr<class UNarrativeCondition>> Conditions;
};

/** One purchasable Place level, authored on a UTerritoryUpgradeDefinition. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryUpgradeTier
{
	GENERATED_BODY()

	/** Target Place upgrade level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "1"))
	int32 Level = 1;

	/** Faction-scoped perk tier required before this Place level can be bought. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "0"))
	int32 RequiredFactionTier = 1;

	/** Hard requirements evaluated on the server before any cost is charged. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Tier")
	TArray<TObjectPtr<class UNarrativeCondition>> Requirements;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier")
	TArray<FTerritoryUpgradeCostEntry> Costs;

	/** Vendor events fired once after a verified upgrade commit. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Tier")
	TArray<TObjectPtr<class UNarrativeEvent>> OnUpgraded;
};

/**
 * Per-Place-tier upgrade recipe: perk gate, requirements, multi-purpose cost routes and events.
 * Assign it on a UTerritoryPlaceDefinition. With no definition the legacy
 * UpgradeCostPerLevel formula keeps working unchanged.
 */
UCLASS(BlueprintType, Const, meta = (DisplayName = "Territory Upgrade Definition"))
class TERRITORYFRAMEWORK_API UTerritoryUpgradeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrades")
	TArray<FTerritoryUpgradeTier> Tiers;

	/**
	 * Enforce the faction-scoped perk tier before a Place upgrade. Keep true for the designed
	 * progression; set false as a migration window while the skill-tree UI, skill points and quest
	 * grants are not authored yet. Costs, requirements and events still run when disabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrades")
	bool bRequireFactionPerkTier = true;

	/** Find the authored tier for a target Place level. */
	UFUNCTION(BlueprintPure, Category = "Territory|Upgrade")
	bool FindTier(int32 Level, FTerritoryUpgradeTier& OutTier) const;

	const FTerritoryUpgradeTier* FindTierConst(int32 Level) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

/**
 * Coarse, deterministic AI catch-up policy. Assigned on the level's Territory World State.
 * AI factions never roll: their tier and Place levels are a pure function of the player's tier,
 * so a save/reload reproduces the same campaign state.
 */
UCLASS(BlueprintType, Const, meta = (DisplayName = "Territory Upgrade Balance Profile"))
class TERRITORYFRAMEWORK_API UTerritoryUpgradeBalanceProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Keep AI-owned factions near the player's faction tier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balance")
	bool bAutoUpgradeAIFactions = true;

	/** Target tier = player tier + this offset, clamped to [0, MaxAutoTier]. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balance")
	int32 TierOffset = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0"))
	int32 MaxAutoTier = 3;

	/** Also buy the AI faction's owned Places up to that tier (free balance path). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balance")
	bool bUpgradeAIPlaces = true;

	/** Server evaluation cadence. Changes are idempotent, so a reload never re-rolls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "1.0"))
	float EvaluationInterval = 30.f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

namespace TerritoryUpgrade
{
	/** One validated payment route. Built before the level is staged, executed after it. */
	struct TERRITORYFRAMEWORK_API FTerritoryUpgradePaymentPlan
	{
		int32 CurrencyTotal = 0;
		TArray<TPair<TSubclassOf<UNarrativeItem>, int32>> Items;
		bool bFree = true;
	};

	/** Validate every required route and pick one satisfiable optional route. No side effects. */
	TERRITORYFRAMEWORK_API bool BuildPaymentPlan(const FTerritoryUpgradeTier& Tier,
		AActor* Requester, APawn* Target, APlayerController* Controller, UTalesComponent* Tales,
		FString& OutFailure, FTerritoryUpgradePaymentPlan& OutPlan);

	/** Consume items then debit currency. Refunds consumed items when the debit is rejected. */
	TERRITORYFRAMEWORK_API bool ExecutePaymentPlan(const FTerritoryUpgradePaymentPlan& Plan,
		AActor* Requester, const FGameplayTag& Faction, const FString& Reason,
		FString& OutFailure);

	/** Roll back a fully executed plan (stale transaction / destroyed actor path). */
	TERRITORYFRAMEWORK_API void RefundPaymentPlan(const FTerritoryUpgradePaymentPlan& Plan,
		AActor* Requester, const FGameplayTag& Faction, const FString& Reason);
}
