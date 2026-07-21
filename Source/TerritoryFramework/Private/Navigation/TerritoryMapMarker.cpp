#include "Navigation/TerritoryMapMarker.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Navigation/NarrativeNavigationComponent.h"

void UTerritoryMapMarker::SetTerritoryVolume(ATerritoryVolume* InTerritory)
{
	// Unbind from previous territory
	ClearTerritoryBinding();

	TerritoryVolume = InTerritory;

	// Subscribe to ownership and state changes for auto-refresh
	if (InTerritory)
	{
		InTerritory->OnTerritoryControlChanged.AddDynamic(this, &UTerritoryMapMarker::OnTerritoryChanged);
		InTerritory->OnTerritoryStateChanged.AddDynamic(this, &UTerritoryMapMarker::OnTerritoryStateChanged);
	}

	RefreshMarker();
}

void UTerritoryMapMarker::ClearTerritoryBinding()
{
	if (TerritoryVolume.IsValid())
	{
		TerritoryVolume->OnTerritoryControlChanged.RemoveDynamic(this, &UTerritoryMapMarker::OnTerritoryChanged);
		TerritoryVolume->OnTerritoryStateChanged.RemoveDynamic(this, &UTerritoryMapMarker::OnTerritoryStateChanged);
	}
	TerritoryVolume = nullptr;
}

void UTerritoryMapMarker::OnTerritoryChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && Settings->ShouldDebugMarkers())
	{
		UE_LOG(LogTerritory, Log, TEXT("[Marker] Refresh: %s owner changed %s → %s"),
			*Territory->GetTerritoryTag().ToString(), *OldOwner.ToString(), *NewOwner.ToString());
	}
	RefreshMarker();
}

void UTerritoryMapMarker::OnTerritoryStateChanged(ATerritoryVolume* Territory, ETerritoryState NewState)
{
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && Settings->ShouldDebugMarkers())
	{
		UE_LOG(LogTerritory, Log, TEXT("[Marker] Refresh: %s state → %d"),
			*Territory->GetTerritoryTag().ToString(), static_cast<int32>(NewState));
	}
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
