#include "Tales/TerritoryTalesUtilities.h"

#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"

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
