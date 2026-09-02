#include "Tales/TerritoryTalesUtilities.h"

#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"
#include "Tales/NarrativeNodeBase.h"
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

bool TerritoryTales::DoEventConditionsPass(const UNarrativeEvent* Event, APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent,
	FString* OutFailedCondition)
{
	if (OutFailedCondition) OutFailedCondition->Reset();
	if (!Event) return false;
	if (GPrevalidatedTerritoryEvents.Contains(Event)) return true;
	if (IsValid(NarrativeComponent))
	{
		// UNarrativeEvent owns Conditions but Narrative's Quest event dispatcher does
		// not evaluate them. Reuse the real node evaluator so Not, character targets,
		// and multiplayer party policies have exactly Narrative's normal semantics.
		UNarrativeNodeBase* Probe = NewObject<UNarrativeNodeBase>(
			GetTransientPackage(), NAME_None, RF_Transient);
		if (Probe)
		{
			for (UNarrativeCondition* Condition : Event->Conditions)
			{
				if (!IsValid(Condition)) continue;
				// Evaluate one row at a time. Narrative's party-policy path returns
				// after one condition, so a multi-row probe would skip later AND rows.
				Probe->Conditions.Reset();
				Probe->Conditions.Add(Condition);
				if (!Probe->AreConditionsMet(Target, Controller, NarrativeComponent))
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
	for (UNarrativeCondition* Condition : Event->Conditions)
	{
		if (Condition && !DoesConditionPass(Condition, Target, Controller, NarrativeComponent))
		{
			if (OutFailedCondition) *OutFailedCondition = Condition->GetGraphDisplayText();
			return false;
		}
	}
	return true;
}
