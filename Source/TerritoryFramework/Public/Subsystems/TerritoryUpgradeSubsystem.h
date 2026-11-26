#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryUpgradeTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "TerritoryUpgradeSubsystem.generated.h"

class ATerritoryProperty;
class ATerritoryUpgradeBroker;
class APlayerController;
class UTerritoryUpgradeBalanceProfile;
class UTerritoryUpgradePerk;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFactionUpgradeTierChanged,
	FGameplayTag, Faction, int32, Tier);

/**
 * Faction-scoped upgrade authority.
 *
 * A perk unlocks a tier for the whole faction; the tier map is saved through TerritoryWorldState
 * and replicated to clients. Place upgrades still mutate only through ATerritoryProperty, so no
 * second ownership or upgrade authority is introduced.
 */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryUpgradeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Current faction-scoped tier, 0 when the faction has not unlocked any tier. */
	UFUNCTION(BlueprintPure, Category = "Territory|Upgrade")
	int32 GetFactionTier(FGameplayTag Faction) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Upgrade")
	bool HasFactionTier(FGameplayTag Faction, int32 MinimumTier) const;

	/** Server authority: write a faction tier (perk purchase, quest, AI balance). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Upgrade")
	bool SetFactionTier(FGameplayTag Faction, int32 Tier);

	/** Server authority: request the next Place upgrade through the existing atomic path. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Upgrade")
	bool TryUpgradePlace(ATerritoryProperty* Place, AActor* Requester);

	/** Server authority: scripted/quest upgrade, optionally for a Place the faction does not own. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Upgrade")
	bool ApplyScriptedUpgrade(ATerritoryProperty* Place, int32 TargetLevel, FGameplayTag RequestingFaction,
		bool bAllowForeignOwnership, bool bFree, AActor* Requester, FString& OutFailure);

	/** WorldState save/load and replication projection. */
	void ImportFactionTiers(const TArray<FReplicatedFactionUpgradeTier>& Tiers);
	TArray<FReplicatedFactionUpgradeTier> GetAllFactionTiers() const;

	/** Client entry point: a local player asks its upgrade broker to buy a perk. */
	UFUNCTION(BlueprintCallable, Category = "Territory|Upgrade")
	bool RequestBuyPerk(APlayerController* Controller, TSubclassOf<UTerritoryUpgradePerk> PerkClass);

	/** The upgrade broker owned by that controller, null until the broker exists. */
	UFUNCTION(BlueprintPure, Category = "Territory|Upgrade")
	ATerritoryUpgradeBroker* GetUpgradeBroker(APlayerController* Controller) const;

	/** Called after a verified ownership change so AI catch-up stays deterministic. */
	void NotifyPlaceCaptured(ATerritoryProperty* Place);

	/** Deterministic AI catch-up; safe to call repeatedly. */
	void EvaluateAIBalance();

	UPROPERTY(BlueprintAssignable, Category = "Territory|Upgrade")
	FOnFactionUpgradeTierChanged OnFactionTierChanged;

private:
	bool SetFactionTierInternal(FGameplayTag Faction, int32 Tier);
	void PublishFactionTierToWorldState(FGameplayTag Faction, int32 Tier);
	void EnsureBrokers();
	void TickMaintenance();
	UTerritoryUpgradeBalanceProfile* ResolveBalanceProfile() const;

	TMap<FGameplayTag, int32> FactionTiers;
	TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<ATerritoryUpgradeBroker>> Brokers;
	FTimerHandle MaintenanceTimer;
	float BalanceAccumulator = 0.f;
	bool bEvaluatingBalance = false;
};
