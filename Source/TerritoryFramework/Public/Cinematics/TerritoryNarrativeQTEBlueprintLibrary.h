#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "TerritoryNarrativeQTEBlueprintLibrary.generated.h"

class ULevelSequenceDirector;

/** Result of resolving a failed Narrative sequencer QTE. */
UENUM(BlueprintType)
enum class ETerritoryNarrativeQTEFailureResult : uint8
{
	Applied,
	InvalidDirector,
	MissingSequenceActor,
	NotAuthority,
	InvalidOwnerContext,
	MissingAbilitySystem
};

/**
 * Focused adapters for Narrative Pro QTE outcome events.
 *
 * Narrative's sequence actor remains the authority for the explicit audience and for cinematic
 * gameplay tags. This library never discovers a player by index: it resolves the one controller
 * authored into ANarrativeLevelSequenceActor::OwnerControllers by the sequence trigger.
 */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryNarrativeQTEBlueprintLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Apply the failed-QTE instakill to the sequence's single explicit owning controller.
	 *
	 * Narrative cinematics normally grant Narrative.State.Invulnerable while bound. The function
	 * temporarily removes exactly one loose count of that sequence-authored tag, dispatches
	 * UNarrativeAbilitySystemComponent::Instakill(), then restores the count so Narrative's normal
	 * OnStop cleanup remains balanced. Only the authoritative sequence instance may mutate GAS.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
		Category = "Territory|Cinematics|QTE",
		meta = (DisplayName = "Apply Narrative QTE Failure Instakill", DefaultToSelf = "SequenceDirector"))
	static ETerritoryNarrativeQTEFailureResult ApplyNarrativeQTEFailureInstakill(
		ULevelSequenceDirector* SequenceDirector);
};
