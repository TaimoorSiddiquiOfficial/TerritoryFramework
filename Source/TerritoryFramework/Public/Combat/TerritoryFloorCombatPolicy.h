#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TerritoryFloorCombatPolicy.generated.h"

class UTerritoryPlaceDefinition;

/**
 * How one floor's defender relates to the floor a target is standing on.
 *
 * This is the only thing floors ever restrict. It is a proactive-engagement rule: real damage is
 * answered on any floor regardless of the policy, so a guard shot from above still defends itself.
 *
 * AnyFloor is the zero value on purpose. An unset, missing or zero-initialised policy reads as
 * "floors separate nothing", which is exactly how a Place behaved before floors were enforced, so
 * the feature cannot change a Place that never asked for separation.
 */
UENUM(BlueprintType)
enum class ETerritoryFloorEngagementPolicy : uint8
{
	AnyFloor UMETA(DisplayName="Any Floor (No Separation)",
		ToolTip="Floors do not restrict anything. This is the default, and it is what a Place with no policy keeps doing."),
	SameOrAdjacent UMETA(DisplayName="Same Floor Or Adjacent",
		ToolTip="A defender answers a target on its own floor or on the floor directly above or below it. Easy example: a stair landing where the fight should not pull the rest of the building in."),
	SameFloorOnly UMETA(DisplayName="Same Floor Only",
		ToolTip="A defender refuses every proactive engagement across floors. A guard that is actually damaged still answers on any floor, so it can never be shot at from another floor without responding.")
};

/**
 * Reusable floor-separation policy for one Place, optionally overridden by a single floor row.
 *
 * Floors are only enforced where a Place has declared them as geometry. A FTerritoryFloorTemplate
 * row is a grouping key with no bounds, so ATerritoryFloorVolume is what makes "which floor is this
 * actor on" answerable at all. In a Place with rows but no authored region every location resolves
 * to "no floor", which is read as "no separation is declared here" and never as a refusal. That is
 * what keeps an unauthored or partially authored map inert: HopDistrictTest declares floors on the
 * Blacksmith and no regions at all, so nothing about its guards changes until someone authors them.
 *
 * Selected per Place (UTerritoryPlaceDefinition::DefaultFloorCombatPolicy) and overridable per
 * floor row (FTerritoryFloorTemplate::CombatPolicy). The policy of the floor the DEFENDER is
 * standing on decides, not the target's, so one guard always answers to one policy.
 */
UCLASS(BlueprintType, Const, meta=(DisplayName="Territory Floor Combat Policy"))
class TERRITORYFRAMEWORK_API UTerritoryFloorCombatPolicy : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Readable name shown to designers and in data validation output. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="00 Display")
	FText DisplayName;

	/** Designer-facing explanation of what this policy is for. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="00 Display",
		meta=(MultiLine="true"))
	FText Description;

	/**
	 * Authored separation rule. Applied only where a floor region exists for both the defender and
	 * the target; anywhere else the decision is unchanged.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Separation",
		meta=(ToolTip="Any Floor keeps today's behaviour, since floors without a region already mean 'no separation declared'. The stricter settings only matter once ATerritoryFloorVolume regions exist for the floors being separated."))
	ETerritoryFloorEngagementPolicy EngagementPolicy =
		ETerritoryFloorEngagementPolicy::AnyFloor;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
