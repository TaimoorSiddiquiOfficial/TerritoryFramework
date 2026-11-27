#include "Tales/TerritoryNarrativeContext.h"

#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Tales/NarrativeFunctionLibrary.h"
#include "Tales/NarrativePartyComponent.h"
#include "Tales/TalesComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"

namespace
{
	/**
	 * Picks the Tales component that actually owns the dialogue Narrative is dispatching for.
	 * A controller can host more than one Tales component (Pro's personal one plus a party
	 * member slot), so "the first component found" is not enough: the one with an open dialogue
	 * wins, and failing that the party owning the shared dialogue does. Candidates are ordered
	 * character-first so a character that hosts its own Tales component keeps precedence.
	 */
	UTalesComponent* PickActiveTales(const TArray<UTalesComponent*>& Candidates)
	{
		// A party member mirrors the shared dialogue, so the member's own slot and the party
		// component can both report one. The party is the component that owns a shared dialogue
		// (OwningComp), and that is the context Narrative 2.4.2 passed explicitly, so it wins
		// whenever the two agree. A member holding a dialogue of its own keeps it.
		UTalesComponent* FirstWithDialogue = nullptr;
		for (UTalesComponent* Candidate : Candidates)
		{
			if (!IsValid(Candidate) || !Candidate->CurrentDialogue)
			{
				continue;
			}
			if (UNarrativePartyComponent* Party = Candidate->GetParty())
			{
				if (Party->CurrentDialogue == Candidate->CurrentDialogue)
				{
					return Party;
				}
			}
			if (!FirstWithDialogue)
			{
				FirstWithDialogue = Candidate;
			}
		}
		if (FirstWithDialogue)
		{
			return FirstWithDialogue;
		}
		for (UTalesComponent* Candidate : Candidates)
		{
			if (!IsValid(Candidate))
			{
				continue;
			}
			if (UNarrativePartyComponent* Party = Candidate->GetParty())
			{
				if (Party->CurrentDialogue)
				{
					return Party;
				}
			}
		}
		for (UTalesComponent* Candidate : Candidates)
		{
			if (IsValid(Candidate))
			{
				return Candidate;
			}
		}
		return nullptr;
	}

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

		TArray<UTalesComponent*> Candidates;
		if (IsValid(Character))
		{
			Character->GetComponents<UTalesComponent>(Candidates);
		}
		if (IsValid(OutController))
		{
			TArray<UTalesComponent*> ControllerCandidates;
			OutController->GetComponents<UTalesComponent>(ControllerCandidates);
			Candidates.Append(ControllerCandidates);
		}

		OutNarrativeComponent = PickActiveTales(Candidates);
		if (!IsValid(OutNarrativeComponent) && IsValid(Character))
		{
			// Canonical Narrative resolution, kept as the last resort so the adapter still works
			// when the Tales component lives somewhere the two actors above do not expose.
			OutNarrativeComponent = UNarrativeFunctionLibrary::GetTalesComponentFromTarget(Character);
		}
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
