#include "Core/TerritoryStealthProfile.h"

#include "AI/TerritoryInvestigationActivity.h"
#include "Core/TerritoryDialogueTags.h"
#include "Core/TerritoryStealthTags.h"
#include "NarrativeGameplayTags.h"

UTerritoryStealthProfile::UTerritoryStealthProfile()
	: InvestigationActivityClass(UTerritoryInvestigationActivity::StaticClass())
{
	BreakStealthGameplayEventTag = TerritoryStealthTags::Exposed;
	StealthAbilityTagsToCancel.AddTag(TerritoryStealthTags::StealthAbility);
	StealthAbilityTagsToCancel.AddTag(FNarrativeGameplayTags::Get().Ability_Crouch);
}

FPrimaryAssetId UTerritoryStealthProfile::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("TerritoryStealthProfile"), GetFName());
}

FGameplayTag UTerritoryStealthProfile::GetEffectiveBreakStealthGameplayEventTag() const
{
	return BreakStealthGameplayEventTag.IsValid()
		? BreakStealthGameplayEventTag
		: TerritoryStealthTags::Exposed.GetTag();
}

FGameplayTag UTerritoryStealthProfile::GetEffectiveHolsteredWeaponWarningTag() const
{
	return HolsteredWeaponWarningTag.IsValid()
		? HolsteredWeaponWarningTag
		: TerritoryDialogueTags::WarnHolsteredWeapon.GetTag();
}

float UTerritoryStealthProfile::GetEffectiveHolsteredWeaponWarningCooldown() const
{
	return FMath::Max(0.f, HolsteredWeaponWarningCooldown);
}

bool UTerritoryStealthProfile::FindEvidenceRule(const ETerritoryStealthEvidence Evidence,
	FTerritoryEvidenceRule& OutRule) const
{
	if (Evidence == ETerritoryStealthEvidence::None)
	{
		return false;
	}
	for (const FTerritoryEvidenceRule& Rule : EvidenceRules)
	{
		if (Rule.Evidence == Evidence)
		{
			OutRule = Rule;
			return true;
		}
	}
	return false;
}

bool UTerritoryStealthProfile::FindEvidenceRuleForTag(const FGameplayTag Tag,
	FTerritoryEvidenceRule& OutRule) const
{
	if (!Tag.IsValid())
	{
		return false;
	}
	for (const FTerritoryEvidenceRule& Rule : EvidenceRules)
	{
		if (Rule.Tag.IsValid() && Rule.Tag == Tag)
		{
			OutRule = Rule;
			return true;
		}
	}
	return false;
}

float UTerritoryStealthProfile::GetEvidenceSuspicion(const ETerritoryStealthEvidence Evidence,
	const float Fallback) const
{
	FTerritoryEvidenceRule Rule;
	return FindEvidenceRule(Evidence, Rule)
		? FMath::Clamp(Rule.Suspicion, 0.f, 1.f)
		: Fallback;
}

bool UTerritoryStealthProfile::IsEvidenceForcedExposure(
	const ETerritoryStealthEvidence Evidence) const
{
	FTerritoryEvidenceRule Rule;
	return FindEvidenceRule(Evidence, Rule) && Rule.bForcesExposure;
}
