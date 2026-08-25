#pragma once

#include "CoreMinimal.h"

class APlayerController;
class APawn;
class UNarrativeCondition;
class UNarrativeEvent;
class UTalesComponent;

/** Small adapter for Narrative condition semantics used outside a Narrative graph node. */
namespace TerritoryTales
{
	/** Evaluates CheckCondition and correctly applies Narrative's inherited Not option. */
	TERRITORYFRAMEWORK_API bool DoesConditionPass(UNarrativeCondition* Condition,
		APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent);

	/** All inherited event conditions must pass. Empty condition arrays pass. */
	TERRITORYFRAMEWORK_API bool DoEventConditionsPass(const UNarrativeEvent* Event,
		APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent,
		FString* OutFailedCondition = nullptr);
}
