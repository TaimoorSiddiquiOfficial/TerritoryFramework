#include "Navigation/TerritoryMapMarker.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Navigation/NarrativeNavigationComponent.h"

void UTerritoryMapMarker::SetTerritoryVolume(ATerritoryVolume* InTerritory)
{
	TerritoryVolume = InTerritory;
	RefreshMarker();
}

ATerritoryVolume* UTerritoryMapMarker::GetTerritoryVolume() const
{
	return TerritoryVolume.IsValid() ? TerritoryVolume.Get() : nullptr;
}

FLinearColor UTerritoryMapMarker::GetMarkerColor_Implementation(UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType) const
{
	if (!TerritoryVolume.IsValid())
	{
		return DefaultColor;
	}

	ETerritoryState State = TerritoryVolume->GetTerritoryState();

	if (State == ETerritoryState::Locked)
	{
		return LockedColor;
	}

	if (State == ETerritoryState::Contested)
	{
		return ContestedColor;
	}

	FGameplayTag Owner = TerritoryVolume->GetOwningFaction();
	if (Owner.IsValid())
	{
		const FLinearColor* FactionColor = FactionColorMap.Find(Owner);
		return FactionColor ? *FactionColor : DefaultColor;
	}

	return DefaultColor;
}

FText UTerritoryMapMarker::GetMarkerDisplayText_Implementation(UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType, FText& OutSubtitleText) const
{
	if (!TerritoryVolume.IsValid())
	{
		return FText::FromString(TEXT("Unknown Territory"));
	}

	FText Name = TerritoryVolume->GetTerritoryDisplayName();
	if (Name.IsEmpty())
	{
		Name = FText::FromString(TerritoryVolume->GetTerritoryTag().ToString());
	}

	ETerritoryState State = TerritoryVolume->GetTerritoryState();
	if (State == ETerritoryState::Contested)
	{
		OutSubtitleText = FText::FromString(TEXT("Contested"));
		return Name;
	}

	FGameplayTag Owner = TerritoryVolume->GetOwningFaction();
	if (Owner.IsValid())
	{
		OutSubtitleText = FText::FromString(Owner.ToString());
	}

	return Name;
}

void UTerritoryMapMarker::MarkerOnPaint_Implementation(FPaintContext& Context, FMarkerOnPaintData& OnPaintData) const
{
	// TODO: Implement territory outline drawing using BoundsShape extents
	// projected onto the map canvas via FPaintContext.
	// Requires coordinate conversion using Navigator's MapOrigin and MapWidth.
}
