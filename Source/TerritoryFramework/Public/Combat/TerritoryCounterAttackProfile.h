#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "TerritoryCounterAttackProfile.generated.h"

/** Reusable balance and Narrative NPC configuration for counterattacks. */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryCounterAttackProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Grace duration in Narrative Pro accumulated time-of-day units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling", meta=(ClampMin="0.0"))
	float GracePeriodGameTime = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling", meta=(ClampMin="100.0"))
	float ActivationRadius = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling", meta=(ClampMin="100.0"))
	float NotificationRadius = 12000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling")
	bool bNotifyDefendingFactionOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling", meta=(ClampMin="1", ClampMax="8"))
	int32 MaximumApproaches = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0", ClampMax="1.0"))
	float BaseLaunchProbability = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinimumLaunchProbability = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MaximumLaunchProbability = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0"))
	float DefenceDeterrenceWeight = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0"))
	float AttackerPowerWeight = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0"))
	float StrategicValueWeight = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0"))
	float ReadinessWeight = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force")
	TArray<FTerritoryFactionAssaultConfig> FactionForces;

	const FTerritoryFactionAssaultConfig* FindFactionForce(const FGameplayTag& Faction) const;
};
