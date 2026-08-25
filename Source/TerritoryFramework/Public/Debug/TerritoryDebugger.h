#pragma once

#include "CoreMinimal.h"
#include "TerritoryDebugger.generated.h"

class ATerritoryVolume;

/**
 * Shared Territory debug-summary API used by Blueprint tools and the Gameplay
 * Debugger category registered by the runtime module.
 */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryDebugger : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Territory|Debug",
		meta=(WorldContext="WorldContextObject"))
	static FText BuildTerritoryDebugSummary(const UObject* WorldContextObject,
		const AActor* DebugActor);
};
