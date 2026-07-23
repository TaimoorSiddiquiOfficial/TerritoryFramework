#pragma once

#include "CoreMinimal.h"
#include "TerritoryDebugger.generated.h"

/**
 * Gameplay Debugger integration for Territory Framework.
 *
 * Currently not implemented — registration functions are empty.
 * To add: implement IGameplayDebuggerCategoryExtender for UE 5.7
 * and register via IGameplayDebuggerModule.
 */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryDebugger : public UObject
{
	GENERATED_BODY()
};
