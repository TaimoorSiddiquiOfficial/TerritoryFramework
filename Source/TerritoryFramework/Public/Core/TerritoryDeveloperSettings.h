#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "TerritoryDeveloperSettings.generated.h"

UCLASS(BlueprintType, config = Engine, defaultconfig, meta = (DisplayName = "Territory Framework"))
class TERRITORYFRAMEWORK_API UTerritoryDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTerritoryDeveloperSettings();

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Economy")
	float EconomyTickIntervalSeconds = 300.f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Economy")
	int32 DefaultTerritoryIncome = 100;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Economy")
	int32 DefaultGuardCost = 50;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Capture")
	float CaptureProgressPerSecond = 0.1f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Capture")
	float CaptureProgressDecayPerSecond = 0.05f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Capture")
	int32 DefaultMaxConcurrentAttackers = 3;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Territory|Tags",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag DefaultPlayerFaction;
};
