#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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

	/** Exact-tag lookup avoids ambiguity when City, District, and Place bounds overlap. */
	UFUNCTION(BlueprintPure, Category="Territory|Debug",
		meta=(WorldContext="WorldContextObject"))
	static FText BuildTerritoryDebugSummaryByTag(
		const UObject* WorldContextObject, FGameplayTag TerritoryTag);

	/** One sorted report for every loaded Territory plus the active debug settings. */
	UFUNCTION(BlueprintPure, Category="Territory|Debug",
		meta=(WorldContext="WorldContextObject"))
	static FText BuildTerritorySystemDebugReport(const UObject* WorldContextObject);

	/** Explains the master gate, category switches, verbosity, and always-on diagnostics. */
	UFUNCTION(BlueprintPure, Category="Territory|Debug")
	static FText BuildDebugSettingsSummary();
};
