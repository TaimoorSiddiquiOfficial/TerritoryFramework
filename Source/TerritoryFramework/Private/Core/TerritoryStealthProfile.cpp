#include "Core/TerritoryStealthProfile.h"

#include "AI/TerritoryInvestigationActivity.h"
#include "Core/TerritoryStealthTags.h"

UTerritoryStealthProfile::UTerritoryStealthProfile()
	: InvestigationActivityClass(UTerritoryInvestigationActivity::StaticClass())
{
	BreakStealthGameplayEventTag = TerritoryStealthTags::Exposed;
}

FPrimaryAssetId UTerritoryStealthProfile::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("TerritoryStealthProfile"), GetFName());
}
