#pragma once

#include "CoreMinimal.h"
#include "TerritoryDebugger.generated.h"

/**
 * Gameplay Debugger category for Territory Framework.
 * Displays territory ownership, state, capture progress, economy, and guard info
 * in the in-game debugger overlay.
 *
 * Only active when GameplayDebugger module is available.
 * Registration is a no-op otherwise.
 */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryDebugger : public UObject
{
	GENERATED_BODY()

public:
	/** Register the Territory category with the Gameplay Debugger (call from module StartupModule) */
	UFUNCTION(BlueprintCallable, Category = "Territory|Debug")
	static void RegisterCategory();

	/** Unregister the Territory category (call from module ShutdownModule) */
	UFUNCTION(BlueprintCallable, Category = "Territory|Debug")
	static void UnregisterCategory();
};
