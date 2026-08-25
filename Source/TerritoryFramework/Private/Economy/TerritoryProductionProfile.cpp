#include "Economy/TerritoryProductionProfile.h"

#include "Items/NarrativeItem.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "TerritoryProductionProfile"

bool UTerritoryProductionProfile::CalculateScaledQuantity(
	const FTerritoryResourceRate& Rate, int32 UpgradeLevel, int32 CycleCount,
	int32& OutQuantity)
{
	OutQuantity = 0;
	if (!Rate.ItemClass || Rate.QuantityPerCycle < 0 || Rate.QuantityPerUpgradeLevel < 0
		|| UpgradeLevel < 0 || CycleCount <= 0)
	{
		return false;
	}

	const int64 PerCycle = static_cast<int64>(Rate.QuantityPerCycle)
		+ static_cast<int64>(Rate.QuantityPerUpgradeLevel) * UpgradeLevel;
	const int64 Total = PerCycle * CycleCount;
	if (PerCycle < 0 || Total <= 0 || Total > MAX_int32)
	{
		return false;
	}

	OutQuantity = static_cast<int32>(Total);
	return true;
}

bool UTerritoryProductionProfile::IsRuleConfigurationValid(
	const FTerritoryProductionRule& Rule, FText& OutFailureReason)
{
	if (!Rule.RuleTag.IsValid())
	{
		OutFailureReason = LOCTEXT("MissingRuleTag", "Production rule requires a stable GameplayTag.");
		return false;
	}
	if (Rule.MinimumUpgradeLevel < 0)
	{
		OutFailureReason = LOCTEXT("InvalidMinimumUpgrade", "Minimum upgrade level cannot be negative.");
		return false;
	}
	if (Rule.Outputs.IsEmpty())
	{
		OutFailureReason = LOCTEXT("MissingOutputs", "Production rule requires at least one output.");
		return false;
	}

	auto ValidateRates = [&OutFailureReason](const TArray<FTerritoryResourceRate>& Rates,
		TSet<UClass*>& OutClasses) -> bool
	{
		for (const FTerritoryResourceRate& Rate : Rates)
		{
			if (!Rate.ItemClass)
			{
				OutFailureReason = LOCTEXT("MissingItemClass", "Every resource rate requires a Narrative item class.");
				return false;
			}
			if (Rate.QuantityPerCycle < 0 || Rate.QuantityPerUpgradeLevel < 0
				|| (Rate.QuantityPerCycle == 0 && Rate.QuantityPerUpgradeLevel == 0))
			{
				OutFailureReason = LOCTEXT("InvalidQuantity", "Resource quantities must be non-negative and produce a positive rate.");
				return false;
			}
			if (OutClasses.Contains(Rate.ItemClass.Get()))
			{
				OutFailureReason = LOCTEXT("DuplicateItemRate", "Each Narrative item class may appear only once in an input or output list.");
				return false;
			}
			OutClasses.Add(Rate.ItemClass.Get());
		}
		return true;
	};

	TSet<UClass*> InputClasses;
	TSet<UClass*> OutputClasses;
	if (!ValidateRates(Rule.Inputs, InputClasses)
		|| !ValidateRates(Rule.Outputs, OutputClasses))
	{
		return false;
	}
	for (UClass* InputClass : InputClasses)
	{
		if (OutputClasses.Contains(InputClass))
		{
			OutFailureReason = LOCTEXT("InputOutputOverlap", "The same Narrative item class cannot be both an input and output of one atomic rule.");
			return false;
		}
	}
	return true;
}

bool UTerritoryProductionProfile::CanRuleRunForState(
	const FTerritoryProductionRule& Rule, ETerritoryState State,
	int32 UpgradeLevel, FText& OutFailureReason)
{
	if (!Rule.bEnabled)
	{
		OutFailureReason = LOCTEXT("RuleDisabled", "Production rule is disabled.");
		return false;
	}
	if (UpgradeLevel < Rule.MinimumUpgradeLevel)
	{
		OutFailureReason = LOCTEXT("UpgradeRequired", "Production site requires another upgrade.");
		return false;
	}
	if (Rule.bPauseWhileContested && State == ETerritoryState::Contested)
	{
		OutFailureReason = LOCTEXT("Contested", "Production is paused while the Territory is contested.");
		return false;
	}
	if (Rule.bRequiresClaimedState && State != ETerritoryState::Claimed)
	{
		OutFailureReason = LOCTEXT("NotClaimed", "Production requires claimed Territory control.");
		return false;
	}
	return true;
}

int32 UTerritoryProductionProfile::CalculatePendingCycleCount(
	int64 LastProcessedCycle, int64 CurrentCycle, int32 MaximumCatchupCycles)
{
	if (LastProcessedCycle == INDEX_NONE || CurrentCycle <= LastProcessedCycle
		|| MaximumCatchupCycles <= 0)
	{
		return 0;
	}
	return static_cast<int32>(FMath::Min<int64>(
		CurrentCycle - LastProcessedCycle, MaximumCatchupCycles));
}

bool UTerritoryProductionProfile::ValidateProfile(FText& OutFailureReason) const
{
	if (Rules.IsEmpty())
	{
		OutFailureReason = LOCTEXT("MissingRules", "Production profile requires at least one rule.");
		return false;
	}

	TSet<FGameplayTag> SeenRuleTags;
	for (const FTerritoryProductionRule& Rule : Rules)
	{
		if (!IsRuleConfigurationValid(Rule, OutFailureReason))
		{
			return false;
		}
		if (SeenRuleTags.Contains(Rule.RuleTag))
		{
			OutFailureReason = FText::Format(
				LOCTEXT("DuplicateRuleTag", "Duplicate production rule tag: {0}"),
				FText::FromName(Rule.RuleTag.GetTagName()));
			return false;
		}
		SeenRuleTags.Add(Rule.RuleTag);
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UTerritoryProductionProfile::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	FText FailureReason;
	if (!ValidateProfile(FailureReason))
	{
		Context.AddError(FailureReason);
		return EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::Invalid
		? Result : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
