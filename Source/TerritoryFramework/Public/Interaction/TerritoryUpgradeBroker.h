#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TerritoryUpgradeBroker.generated.h"

class UTerritoryUpgradePerk;

/**
 * Owner-routed server entry point for Territory perk purchases.
 *
 * The vendor USkillTreeComponent::BuyPerk is BlueprintCallable without an authority check and the
 * skill tree is not replicated. This replicated, controller-owned broker gives clients one validated
 * RPC that performs the purchase on the server only. It is spawned by UTerritoryUpgradeSubsystem;
 * designers do not place it by hand.
 */
UCLASS(NotPlaceable)
class TERRITORYFRAMEWORK_API ATerritoryUpgradeBroker : public AActor
{
	GENERATED_BODY()

public:
	ATerritoryUpgradeBroker();

	/** UI entry: buy Upgrade I/II/III by tier number. Routes to the server RPC on clients. */
	UFUNCTION(BlueprintCallable, Category = "Territory|Upgrade")
	bool RequestUpgradeTier(int32 Tier);

	/** Faction resolved from the owning controller's pawn. */
	UFUNCTION(BlueprintPure, Category = "Territory|Upgrade")
	FGameplayTag GetOwnerFaction() const;

	/** Faction-scoped tier currently unlocked for that faction (read model). */
	UFUNCTION(BlueprintPure, Category = "Territory|Upgrade")
	int32 GetOwnerFactionTier() const;

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerBuyPerk(TSubclassOf<UTerritoryUpgradePerk> PerkClass);
};
