#include "Tales/TerritoryNarrativeConditionTask.h"

#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeNodeBase.h"
#include "Tales/TalesComponent.h"

#define LOCTEXT_NAMESPACE "TerritoryNarrativeConditionTask"

UTerritoryNarrativeConditionTask::UTerritoryNarrativeConditionTask()
{
	RequiredQuantity = 1;
	bOptional = false;
	bHidden = true;
	TickInterval = EvaluationInterval;
	MarkerSettings.bAddNavigationMarker = false;
}

void UTerritoryNarrativeConditionTask::BeginTask()
{
	bHasLatched = false;
	TickInterval = FMath::Max(0.05f, EvaluationInterval);
	ConditionProbe = NewObject<UNarrativeNodeBase>(this, NAME_None, RF_Transient);
	if (ConditionProbe)
	{
		ConditionProbe->Conditions.Reset();
	}
	Super::BeginTask();
}

void UTerritoryNarrativeConditionTask::TickTask_Implementation()
{
	const bool bPassed = bHasLatched || AreGateConditionsMet();
	if (bPassed && bLatchOnceSatisfied)
	{
		bHasLatched = true;
	}
	SetProgress(bPassed ? RequiredQuantity : 0);
}

void UTerritoryNarrativeConditionTask::EndTask()
{
	ConditionProbe = nullptr;
	Super::EndTask();
}

bool UTerritoryNarrativeConditionTask::AreGateConditionsMet() const
{
	if (Conditions.IsEmpty() || !IsValid(ConditionProbe)
		|| !IsValid(OwningComp) || bEvaluatingConditions)
	{
		return false;
	}
	TGuardValue<bool> EvaluationGuard(bEvaluatingConditions, true);
	UNarrativeNodeBase* Probe = ConditionProbe;
	UTalesComponent* Component = OwningComp;
	const TArray<TObjectPtr<UNarrativeCondition>> Requirements = Conditions;
	for (UNarrativeCondition* Condition : Requirements)
	{
		if (!IsValid(Condition)) return false;
		// Narrative's party-policy evaluator returns after one condition. Run each
		// row separately so every authored State + Branch requirement still uses
		// Narrative semantics while retaining the recipe's documented AND logic.
		Probe->Conditions.Reset();
		Probe->Conditions.Add(Condition);
		if (!Probe->AreConditionsMet(
			OwningPawn, OwningController, OwningComp))
		{
			return false;
		}
		// A condition can transition the quest and end this task synchronously.
		if (ConditionProbe != Probe || OwningComp != Component || !IsValid(Component)) return false;
	}
	return true;
}

FText UTerritoryNarrativeConditionTask::GetTaskDescription_Implementation() const
{
	return RequirementDescription.IsEmpty()
		? LOCTEXT("DefaultRequirement", "Meet the mission requirements")
		: RequirementDescription;
}

FText UTerritoryNarrativeConditionTask::GetTaskNodeDescription_Implementation() const
{
	return FText::Format(LOCTEXT("NodeDescription", "Wait for {0} condition(s)"),
		FText::AsNumber(Conditions.Num()));
}

#undef LOCTEXT_NAMESPACE
