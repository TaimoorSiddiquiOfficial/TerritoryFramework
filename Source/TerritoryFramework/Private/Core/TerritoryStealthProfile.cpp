#include "Core/TerritoryStealthProfile.h"

#include "AI/TerritoryInvestigationActivity.h"
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
