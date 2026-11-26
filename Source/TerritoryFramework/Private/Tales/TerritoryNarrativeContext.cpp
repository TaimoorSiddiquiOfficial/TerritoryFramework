#include "Tales/TerritoryNarrativeContext.h"

#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Tales/TalesComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"

namespace
{
	/**
	 * Recovers the Tales execution context from the character Narrative handed us.
	 * GetOwningController keeps working while a vehicle or possession swap owns the
	 * pawn, which is why Narrative exposes it separately from GetController().
	 */
	void ResolveNarrativeContext(ANarrativeCharacter* Character, APawn*& OutTarget,
		APlayerController*& OutController, UTalesComponent*& OutNarrativeComponent)
	{
		OutTarget = Character;
		AController* OwningController = IsValid(Character) ? Character->GetOwningController() : nullptr;
		if (!IsValid(OwningController) && IsValid(Character))
		{
			OwningController = Character->GetController();
		}
		OutController = Cast<APlayerController>(OwningController);
		OutNarrativeComponent = IsValid(OutController)
			? OutController->FindComponentByClass<UTalesComponent>() : nullptr;
	}
}

bool UTerritoryNarrativeCondition::CheckConditionInContext(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	return true;
}

bool UTerritoryNarrativeCondition::CheckCondition_Implementation(ANarrativeCharacter* Character)
{
	APawn* Target = nullptr;
	APlayerController* Controller = nullptr;
	UTalesComponent* NarrativeComponent = nullptr;
	ResolveNarrativeContext(Character, Target, Controller, NarrativeComponent);
	return CheckConditionInContext(Target, Controller, NarrativeComponent);
}

void UTerritoryNarrativeEvent::ExecuteEventInContext(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
}

void UTerritoryNarrativeEvent::ActivateInContext(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	// Narrative's OnActivate default is "activation means execution".
	ExecuteEventInContext(Target, Controller, NarrativeComponent);
}

void UTerritoryNarrativeEvent::ExecuteEvent_Implementation(ANarrativeCharacter* Character)
{
	APawn* Target = nullptr;
	APlayerController* Controller = nullptr;
	UTalesComponent* NarrativeComponent = nullptr;
	ResolveNarrativeContext(Character, Target, Controller, NarrativeComponent);
	ExecuteEventInContext(Target, Controller, NarrativeComponent);
}
