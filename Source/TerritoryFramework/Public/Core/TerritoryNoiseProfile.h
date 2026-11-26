#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "TerritoryNoiseProfile.generated.h"

/**
 * One authored noise recipe shared by every system that makes a sound: distraction throws, weapon
 * fire, vehicles and scripted events. Narrative's hearing sense decides what a guard actually hears;
 * this asset only supplies the loudness, range and semantic tag so all sources stay consistent and a
 * designer can tune one number instead of three Blueprints.
 */
UCLASS(BlueprintType, Const, meta=(DisplayName="Territory Noise Profile"))
class TERRITORYFRAMEWORK_API UTerritoryNoiseProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UTerritoryNoiseProfile();

	/** Human-readable label for journals and the asset picker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="00 Noise")
	FText DisplayName;

	/** Stimulus loudness passed to UAISense_Hearing. 1 is a normal throw; 0 is silent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="00 Noise",
		meta=(ClampMin="0.0", ToolTip="Loudness handed to Narrative hearing. Crouched footstep foley scales its own value down already."))
	float Loudness = 1.f;

	/** Maximum hearing/report range in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="00 Noise",
		meta=(ClampMin="0.0", Units="cm"))
	float MaximumRange = 3000.f;

	/**
	 * Tag carried by the noise stimulus. The stealth observer matches it against its evidence routes
	 * (territory distraction, gunshot, footstep) and against profile Evidence Rules.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="00 Noise",
		meta=(Categories="GameplayEvent,Territory",
			ToolTip="Default is Territory.Distraction.Throwable so existing distraction evidence keeps working."))
	FGameplayTag NoiseTag;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
