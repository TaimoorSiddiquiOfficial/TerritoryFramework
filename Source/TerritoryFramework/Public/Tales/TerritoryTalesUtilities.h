#pragma once

#include "CoreMinimal.h"

class APlayerController;
class APawn;
class UNarrativeCondition;
class UNarrativeEvent;
class UTalesComponent;
class UWorld;

/** Small adapter for Narrative condition semantics used outside a Narrative graph node. */
namespace TerritoryTales
{
	/**
	 * Marks one event as already condition-checked for the current synchronous call.
	 * Territory state dispatch uses this so generic Narrative events are checked once,
	 * while Territory events that defensively check themselves do not evaluate a
	 * Blueprint condition twice.
	 */
	class TERRITORYFRAMEWORK_API FScopedPrevalidatedEvent
	{
	public:
		explicit FScopedPrevalidatedEvent(const UNarrativeEvent* Event);
		~FScopedPrevalidatedEvent();

		FScopedPrevalidatedEvent(const FScopedPrevalidatedEvent&) = delete;
		FScopedPrevalidatedEvent& operator=(const FScopedPrevalidatedEvent&) = delete;

	private:
		const UNarrativeEvent* Event = nullptr;
	};

	/**
	 * Resolves the gameplay world from the live Narrative execution context before
	 * falling back to the event/condition outer. This keeps reusable quest/dialogue
	 * assets working even when their UObject outer has no world.
	 */
	TERRITORYFRAMEWORK_API UWorld* ResolveWorld(const UObject* ContextObject,
		APawn* Target, APlayerController* Controller,
		const UTalesComponent* NarrativeComponent);

	/** Evaluates CheckCondition and correctly applies Narrative's inherited Not option. */
	TERRITORYFRAMEWORK_API bool DoesConditionPass(UNarrativeCondition* Condition,
		APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent);

	/** All inherited event conditions must pass. Empty condition arrays pass. */
	TERRITORYFRAMEWORK_API bool DoEventConditionsPass(const UNarrativeEvent* Event,
		APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent,
		FString* OutFailedCondition = nullptr);
}
