#pragma once

#include "CoreMinimal.h"
#include "SkillTrees/TreeSkill.h"
#include "Skills/TerritoryUpgradePerk.h"

#include "TerritoryUpgradeSkill.generated.h"

/**
 * The Territory skill set: Upgrade I (unlock) -> II -> III.
 *
 * The vendor USkillTreeComponent stores its skills as instanced UObjects and does not replicate
 * them, so the broker injects one instance of this class into the server component before the
 * first purchase. Designs that author the skill inside a PlayerState Blueprint still get the same
 * perk classes and chain.
 */
UCLASS(BlueprintType, EditInlineNew, meta = (DisplayName = "Territory Upgrades"))
class TERRITORYFRAMEWORK_API UTerritoryUpgradeSkill : public UTreeSkill
{
	GENERATED_BODY()

public:
	UTerritoryUpgradeSkill(const FObjectInitializer& ObjectInitializer);
};
