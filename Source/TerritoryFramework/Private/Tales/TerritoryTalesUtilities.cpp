#include "Tales/TerritoryTalesUtilities.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "Tales/TerritoryCaptureEvent.h"
#include "Tales/TerritoryNarrativeContext.h"

#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"
#include "Tales/NarrativeNodeBase.h"
#include "Tales/NarrativePartyComponent.h"
#include "Tales/TalesComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	thread_local TArray<const UNarrativeEvent*> GPrevalidatedTerritoryEvents;
}

FGameplayTag TerritoryTales::ResolveFaction(const UObject* Context,
	ETerritoryCaptureFactionSource Source, FGameplayTag ExplicitFaction,
	APawn* Target, APlayerController* Controller, const UTalesComponent* Tales)
{
	if (!IsValid(Target) && IsValid(Tales)) Target = Tales->GetOwningPawn();
	if (!IsValid(Controller) && IsValid(Tales)) Controller = Tales->GetOwningController();
	switch (Source)
	{
	case ETerritoryCaptureFactionSource::ExplicitFaction: return ExplicitFaction;
	case ETerritoryCaptureFactionSource::NarrativeTargetFaction:
		return UTerritoryBlueprintLibrary::GetActorPrimaryFaction(Context, Target);
	case ETerritoryCaptureFactionSource::ControllerPawnFaction:
		return UTerritoryBlueprintLibrary::GetActorPrimaryFaction(Context,
			IsValid(Controller) ? Controller->GetPawn().Get() : nullptr);
	default: return FGameplayTag();
	}
}

APlayerController* TerritoryTales::ResolveTaskController(
	const UTalesComponent* Tales, APlayerController* CachedController)
{
	if (IsValid(Tales))
	{
		if (APlayerController* Controller = Tales->GetOwningController()) return Controller;
	}
	return IsValid(CachedController) ? CachedController : nullptr;
}

APawn* TerritoryTales::ResolveTaskPawn(const UTalesComponent* Tales,
	APawn* CachedPawn, APlayerController* CachedController)
{
	if (APlayerController* Controller = ResolveTaskController(Tales, CachedController))
	{
		// Do not fall back to the cached pawn after a player loses possession or
		// changes character. Native's retained character also covers vehicle trips.
		return FTerritoryNarrativeProAdapter::ResolvePlayerCharacter(Controller);
	}
	if (IsValid(Tales))
	{
		if (APawn* Pawn = Tales->GetOwningPawn()) return Pawn;
	}
	return IsValid(CachedPawn) ? CachedPawn : nullptr;
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
	if (!IsValid(Condition)) return false;
	if (UTerritoryNarrativeCondition* TerritoryCondition = Cast<UTerritoryNarrativeCondition>(Condition))
	{
		return TerritoryCondition->CheckConditionInContext(Target, Controller, NarrativeComponent) != Condition->bNot;
	}
	// Narrative 2.5 keeps CheckCondition protected and character-typed; its public
	// entry point already applies the inherited Not option and character targets.
	return Condition->ConditionPasses(Cast<ANarrativeCharacter>(Target));
}

void TerritoryTales::ExecuteEvent(UNarrativeEvent* Event, APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!IsValid(Event)) return;
	if (UTerritoryNarrativeEvent* TerritoryEvent = Cast<UTerritoryNarrativeEvent>(Event))
	{
		TerritoryEvent->ExecuteEventInContext(Target, Controller, NarrativeComponent);
		return;
	}
	// ExecuteEvent is protected in Narrative 2.5, so a plain Narrative event can only
	// be reached through its public dispatch, which re-applies its own conditions.
	ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(Target);
	if (!Character && !IsValid(Target) && IsValid(Controller))
	{
		Character = Cast<ANarrativeCharacter>(Controller->GetPawn());
	}
	if (IsValid(Character)) Event->DispatchExecute(Character);
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
		if (Condition->PartyConditionPolicy == EPartyConditionPolicy::AllPlayersPass)
		{
			// Narrative 2.5 commented its node-level All Players branch out (the whole party
			// policy block in NarrativeNodeBase.cpp), so nothing upstream rejects a later
			// failing member any more. Territory owns this policy for its own conditions:
			// every member must pass, and an empty party has no member that can satisfy it.
			const TArray<UTalesComponent*> Members = Party->GetPartyMembers();
			if (Members.Num() == 0) return false;
			for (UTalesComponent* Member : Members)
			{
				if (!IsValid(Member)) continue;
				if (!DoesConditionPass(Condition, Member->GetOwningPawn(),
					Member->GetOwningController(), Member)) return false;
			}
			return true;
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
	// Narrative 2.5's node evaluator narrows the pawn to a Narrative character, so the
	// explicit Tales context is evaluated directly to keep plain pawns supported.
	return DoesConditionPass(Condition, Target, Controller, NarrativeComponent);
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
