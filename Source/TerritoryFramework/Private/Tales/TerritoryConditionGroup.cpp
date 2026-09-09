#include "Tales/TerritoryConditionGroup.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "Tales/NarrativeNodeBase.h"
#include "Tales/TalesComponent.h"
#include "Misc/DataValidation.h"

UTerritoryConditionGroup::UTerritoryConditionGroup()
{
	ConditionFilter = EConditionFilter::CF_DontTarget;
}

bool UTerritoryConditionGroup::HasValidStructure(
	TSet<const UTerritoryConditionGroup*>& Path, int32 Depth) const
{
	if (Conditions.IsEmpty() || Depth >= 32 || Path.Contains(this)
		|| (Match != ETerritoryConditionGroupMatch::All && Match != ETerritoryConditionGroupMatch::Any)) return false;
	Path.Add(this);
	for (const UNarrativeCondition* Condition : Conditions)
	{
		if (!IsValid(Condition)) return false;
		const UTerritoryConditionGroup* Child = Cast<UTerritoryConditionGroup>(Condition);
		if (Child && !Child->HasValidStructure(Path, Depth + 1)) return false;
	}
	Path.Remove(this);
	return true;
}

bool UTerritoryConditionGroup::CheckCondition_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	TSet<const UTerritoryConditionGroup*> Path;
	if (bEvaluating || !HasValidStructure(Path, 0)) return false;
	TGuardValue<bool> Guard(bEvaluating, true);
	const TArray<TObjectPtr<UNarrativeCondition>> Rows = Conditions;
	const ETerritoryConditionGroupMatch RequiredMatch = Match;
	UNarrativeNodeBase* Probe = IsValid(NarrativeComponent)
		? NewObject<UNarrativeNodeBase>(GetTransientPackage()) : nullptr;
	for (UNarrativeCondition* Row : Rows)
	{
		if (!IsValid(Row)) return false;
		const bool bPass = Probe
			? TerritoryTales::EvaluateConditionWithNarrative(Probe, Row, Target, Controller, NarrativeComponent)
			: TerritoryTales::DoesConditionPass(Row, Target, Controller, NarrativeComponent);
		if (RequiredMatch == ETerritoryConditionGroupMatch::All && !bPass) return false;
		if (RequiredMatch == ETerritoryConditionGroupMatch::Any && bPass) return true;
	}
	return RequiredMatch == ETerritoryConditionGroupMatch::All;
}

FString UTerritoryConditionGroup::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("%s of %d Narrative conditions must pass"),
		Match == ETerritoryConditionGroupMatch::All ? TEXT("All") : TEXT("Any"), Conditions.Num());
}

#if WITH_EDITOR
EDataValidationResult UTerritoryConditionGroup::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	TSet<const UTerritoryConditionGroup*> Path;
	if (!HasValidStructure(Path, 0)) Context.AddError(NSLOCTEXT("TerritoryConditions", "InvalidGroup",
		"Add at least one condition to each group. Remove empty rows and circular groups. Nest fewer than 32 groups."));
	return Context.GetNumErrors() ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
