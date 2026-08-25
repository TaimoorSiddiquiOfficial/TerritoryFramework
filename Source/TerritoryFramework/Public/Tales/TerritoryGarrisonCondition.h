#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeCondition.h"
#include "TerritoryGarrisonCondition.generated.h"

UENUM(BlueprintType)
enum class ETerritoryGarrisonMetric : uint8
{
	ActiveGuards UMETA(DisplayName="Active Guards", ToolTip="Physical living guards currently registered to the Territory."),
	LivingDefenders UMETA(DisplayName="Living Defenders", ToolTip="Every living actor registered as a defender, including project-assigned defenders that are not Territory guard pawns."),
	DesiredGuards UMETA(DisplayName="Desired Guards", ToolTip="Staffing target chosen for the current owner."),
	MaximumCapacity UMETA(DisplayName="Maximum Guard Capacity", ToolTip="Total physical guard slots available in the Territory."),
	RemainingReserve UMETA(DisplayName="Remaining Reserve", ToolTip="Finite replacement guards still stored at the Territory's guard posts."),
	PendingReserveDeployments UMETA(DisplayName="Pending Reserve Deployments", ToolTip="Finite replacements already committed but not physically spawned yet."),
	GuardShortfall UMETA(DisplayName="Guard Shortfall", ToolTip="Desired Guards minus Active Guards, never below zero.")
};

UENUM(BlueprintType)
enum class ETerritoryIntegerComparison : uint8
{
	Equal UMETA(DisplayName="Equal To"),
	NotEqual UMETA(DisplayName="Not Equal To"),
	AtLeast UMETA(DisplayName="At Least"),
	AtMost UMETA(DisplayName="At Most"),
	GreaterThan UMETA(DisplayName="Greater Than"),
	LessThan UMETA(DisplayName="Less Than")
};

/**
 * Checks one authoritative garrison value on a loaded Territory.
 *
 * Easy examples:
 * - Active Guards Equal To 0: all placed defenders have been defeated.
 * - Active Guards At Least 1: the player has assigned a living guard.
 * - Remaining Reserve At Most 2: the finite reserve is running low.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Garrison Condition"))
class TERRITORYFRAMEWORK_API UTerritoryGarrisonCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryGarrisonCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Territory",
			ToolTip="Territory to inspect. Example: Territory.HavenReach.MarketSquare.Blacksmith."))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Which garrison value should be compared."))
	ETerritoryGarrisonMetric Metric = ETerritoryGarrisonMetric::ActiveGuards;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="How the current value is compared with the number below."))
	ETerritoryIntegerComparison Comparison = ETerritoryIntegerComparison::AtLeast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ClampMin="0", ToolTip="Number used by the comparison. Example: 1 with Active Guards At Least means one living guard is required."))
	int32 Value = 1;

	/** Pure comparison shared by runtime evaluation and native behavioural tests. */
	static bool CompareValues(int32 ActualValue, ETerritoryIntegerComparison Operation,
		int32 RequiredValue);

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
