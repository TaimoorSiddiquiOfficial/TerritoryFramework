#include "Core/TerritoryNoiseProfile.h"

#include "Core/TerritoryStealthTags.h"

UTerritoryNoiseProfile::UTerritoryNoiseProfile()
{
	// Existing distraction evidence keeps working without an author touching the tag.
	NoiseTag = TerritoryStealthTags::DistractionThrowable.GetTag();
}

FPrimaryAssetId UTerritoryNoiseProfile::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("TerritoryNoiseProfile"), GetFName());
}
