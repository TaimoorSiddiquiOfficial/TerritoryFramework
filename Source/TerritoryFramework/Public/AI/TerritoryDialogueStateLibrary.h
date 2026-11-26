#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "TerritoryDialogueStateLibrary.generated.h"

class AActor;

/**
 * Blueprint helpers for the NPC attitude state machine (`SM_NPC_Attitude`).
 *
 * Every function only *reads* an authority that already exists:
 *  - `UTerritoryDiplomacyDialogueComponent` resolves the relationship (multi-faction, disguise-aware),
 *  - `UTalesComponent` owns the running dialogue,
 *  - `ANarrativeCharacter` owns health.
 *
 * Nothing here writes state, so a Logic Driver graph can build a condition out of two or three nodes
 * instead of a private relationship resolver. That keeps one authority per piece of state.
 */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryDialogueStateLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** True when the two actors are within Radius (uu). A null actor is never in range. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue",
		meta=(DisplayName="Territory Actor Within Range"))
	static bool IsActorWithinRange(const AActor* Actor, const AActor* OtherActor, float Radius);

	/**
	 * True when the relationship between the two actors equals Expected.
	 * Prefers the NPC's existing `UTerritoryDiplomacyDialogueComponent` resolver; without one it
	 * falls back to the primary factions and the existing diplomacy subsystem.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue",
		meta=(DisplayName="Territory Actor Relationship Is"))
	static bool IsActorRelationship(const UObject* WorldContextObject, AActor* Actor,
		AActor* OtherActor, EDiplomacyState Expected);

	/** True when the actor has a Tales component that is inside a running dialogue. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue",
		meta=(DisplayName="Territory Actor Is In Dialogue"))
	static bool IsActorInDialogue(AActor* Actor);

	/** True when the actor's health is at or below Percent (0..1) of its max health. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue",
		meta=(DisplayName="Territory Actor Health Below"))
	static bool IsActorHealthBelow(const AActor* Actor, float Percent);
};
