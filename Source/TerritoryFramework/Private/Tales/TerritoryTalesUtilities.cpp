#include "Tales/TerritoryTalesUtilities.h"

#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"
#include "Tales/NarrativeNodeBase.h"
#include "Tales/NarrativePartyComponent.h"
#include "Tales/TalesComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	thread_local TArray<const UNarrativeEvent*> GPrevalidatedTerritoryEvents;
}

TerritoryTales::FScopedPrevalidatedEvent::FScopedPrevalidatedEvent(
	const UNarrativeEvent* InEvent) : Event(InEvent)
{
	if (Event) GPrevalidatedTerritoryEvents.Add(Event);
}

TerritoryTales::FScopedPrevalidatedEvent::~FScopedPrevalidatedEvent()
{
	if (Event) GPrevalidatedTerritoryEvents.RemoveSingleSwap(Event, EAllowShrinking::No);
}

UWorld* TerritoryTales::ResolveWorld(const UObject* ContextObject, APawn* Target,
	APlayerController* Controller, const UTalesComponent* NarrativeComponent)
{
	if (IsValid(Target) && !Target->IsActorBeingDestroyed() && Target->GetWorld())
	{
		return Target->GetWorld();
	}
	if (IsValid(Controller) && !Controller->IsActorBeingDestroyed()
		&& Controller->GetWorld())
	{
		return Controller->GetWorld();
	}
	if (IsValid(NarrativeComponent) && NarrativeComponent->GetWorld())
	{
		return NarrativeComponent->GetWorld();
	}
	return ContextObject ? ContextObject->GetWorld() : nullptr;
}

bool TerritoryTales::DoesConditionPass(UNarrativeCondition* Condition, APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	return Condition && Condition->CheckCondition(Target, Controller, NarrativeComponent) != Condition->bNot;
}

bool TerritoryTales::EvaluateConditionWithNarrative(UNarrativeNodeBase* Probe,
	UNarrativeCondition* Condition, APawn* Target, APlayerController* Controller,
	UTalesComponent* NarrativeComponent)
{
	if (!IsValid(Probe) || !IsValid(Condition) || !IsValid(NarrativeComponent)) return false;
	UNarrativePartyComponent* Party = Cast<UNarrativePartyComponent>(NarrativeComponent);
	if (!Party) Party = NarrativeComponent->GetParty();
	if (IsValid(Party) && NarrativeComponent->GetNetMode() != NM_Standalone)
	{
		// Narrative owns membership and condition execution. Its node evaluator in
		// the supported version accepts an Any policy with zero passing members,
		// and dereferences an absent leader. Correct only these two boundaries here.
		if (Condition->PartyConditionPolicy == EPartyConditionPolicy::AnyPlayerPasses)
		{
			const TArray<UTalesComponent*> Members = Party->GetPartyMembers();
			for (UTalesComponent* Member : Members)
			{
				if (IsValid(Member) && DoesConditionPass(Condition,
					Member->GetOwningPawn(), Member->GetOwningController(), Member)) return true;
			}
			return false;
		}
		if (Condition->PartyConditionPolicy == EPartyConditionPolicy::PartyLeaderPasses)
		{
			UTalesComponent* Leader = Party->GetPartyLeader();
			return IsValid(Leader) && DoesConditionPass(Condition,
				Leader->GetOwningPawn(), Leader->GetOwningController(), Leader);
		}
	}
	// One row per probe also avoids Narrative's early return after a party row
	// skipping the later AND requirements of the Territory event or task.
	Probe->Conditions.Reset();
	Probe->Conditions.Add(Condition);
	return Probe->AreConditionsMet(Target, Controller, NarrativeComponent);
}

bool TerritoryTales::DoEventConditionsPass(const UNarrativeEvent* Event, APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent,
	FString* OutFailedCondition)
{
	if (OutFailedCondition) OutFailedCondition->Reset();
	if (!Event) return false;
	if (GPrevalidatedTerritoryEvents.Contains(Event)) return true;
	// Conditions are Blueprint callbacks and may edit the owning event's array.
	// One evaluation uses the requirements that existed when it began.
	const TArray<UNarrativeCondition*> Conditions = Event->Conditions;
	if (IsValid(NarrativeComponent))
	{
		// UNarrativeEvent owns Conditions but Narrative's Quest event dispatcher does
		// not evaluate them. Reuse the real node evaluator so Not, character targets,
		// and ordinary party policies keep Narrative's semantics.
		UNarrativeNodeBase* Probe = NewObject<UNarrativeNodeBase>(
			GetTransientPackage(), NAME_None, RF_Transient);
		if (Probe)
		{
			for (UNarrativeCondition* Condition : Conditions)
			{
				if (!IsValid(Condition)) continue;
				if (!EvaluateConditionWithNarrative(Probe, Condition, Target, Controller, NarrativeComponent))
				{
					if (OutFailedCondition)
					{
						*OutFailedCondition = Condition->GetGraphDisplayText();
					}
					return false;
				}
			}
			return true;
		}
	}
	// Detached definition/state tests may intentionally have no Tales component.
	// Preserve the direct single-target fallback while still honoring Narrative Not.
	for (UNarrativeCondition* Condition : Conditions)
	{
		if (Condition && !DoesConditionPass(Condition, Target, Controller, NarrativeComponent))
		{
			if (OutFailedCondition) *OutFailedCondition = Condition->GetGraphDisplayText();
			return false;
		}
	}
	return true;
}
