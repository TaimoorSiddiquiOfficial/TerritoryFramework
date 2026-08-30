#include "Tales/TerritoryTalesUtilities.h"

#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"
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
