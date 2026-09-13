#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

enum class ETerritoryCaptureFactionSource : uint8;

class APlayerController;
class APawn;
class UNarrativeCondition;
class UNarrativeEvent;
class UNarrativeNodeBase;
class UTalesComponent;
class UWorld;

/** Small adapter for Narrative condition semantics used outside a Narrative graph node. */
namespace TerritoryTales
{
	/** Live quest controller, with Native's initialized task context as a fallback. */
	TERRITORYFRAMEWORK_API APlayerController* ResolveTaskController(
		const UTalesComponent* Tales, APlayerController* CachedController);
	/** Live quest character. Native retains this character while its controller drives a vehicle. */
	TERRITORYFRAMEWORK_API APawn* ResolveTaskPawn(const UTalesComponent* Tales,
		APawn* CachedPawn, APlayerController* CachedController);

	/** Reuses Narrative's live primary faction. Never substitutes another player. */
	TERRITORYFRAMEWORK_API FGameplayTag ResolveFaction(const UObject* Context,
		ETerritoryCaptureFactionSource Source, FGameplayTag ExplicitFaction,
		APawn* Target, APlayerController* Controller, const UTalesComponent* Tales);
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

	/** Evaluates one node condition, including fail-closed Any/Leader party admission. */
	TERRITORYFRAMEWORK_API bool EvaluateConditionWithNarrative(UNarrativeNodeBase* Probe,
		UNarrativeCondition* Condition, APawn* Target, APlayerController* Controller,
		UTalesComponent* NarrativeComponent);

	/** All inherited event conditions must pass. Empty condition arrays pass. */
	TERRITORYFRAMEWORK_API bool DoEventConditionsPass(const UNarrativeEvent* Event,
		APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent,
		FString* OutFailedCondition = nullptr);
}
