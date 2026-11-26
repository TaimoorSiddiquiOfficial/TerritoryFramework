#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "TerritoryUpgradeTypes.generated.h"

/** Replicated, saved faction-scoped upgrade tier (one member's perk unlocks it for the team). */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FReplicatedFactionUpgradeTier
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Upgrades")
	FGameplayTag Faction;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Territory|Upgrades")
	int32 Tier = 0;
};
