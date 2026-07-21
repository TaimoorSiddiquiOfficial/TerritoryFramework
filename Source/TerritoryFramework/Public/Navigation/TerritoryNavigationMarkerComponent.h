#pragma once

#include "CoreMinimal.h"
#include "Navigation/NavigationMarkerComponent.h"
#include "TerritoryNavigationMarkerComponent.generated.h"

UCLASS(BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class TERRITORYFRAMEWORK_API UTerritoryNavigationMarkerComponent : public UNavigationMarkerComponent
{
	GENERATED_BODY()

public:
	UTerritoryNavigationMarkerComponent(const FObjectInitializer& ObjectInitializer);
};
