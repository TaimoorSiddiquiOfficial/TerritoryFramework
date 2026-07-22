#include "Navigation/TerritoryMapMarker.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Navigation/NarrativeNavigationComponent.h"
#include "Components/BoxComponent.h"
#include "Rendering/DrawElements.h"
#include "Blueprint/UserWidget.h"

void UTerritoryMapMarker::SetTerritoryVolume(ATerritoryVolume* InTerritory)
{
	// Unbind from previous territory
	ClearTerritoryBinding();

	if (!IsValid(InTerritory)) return;

	TerritoryVolume = InTerritory;

	// Subscribe to ownership and state changes for auto-refresh
	InTerritory->OnTerritoryControlChanged.AddDynamic(this, &UTerritoryMapMarker::OnTerritoryChanged);
	InTerritory->OnTerritoryStateChanged.AddDynamic(this, &UTerritoryMapMarker::OnTerritoryStateChanged);

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

void UTerritoryMapMarker::SetFactionColor(FGameplayTag Faction, FLinearColor Color)
{
	FactionColorMap.Add(Faction, Color);
	RefreshMarker();
}

void UTerritoryMapMarker::ClearFactionColors()
{
	FactionColorMap.Empty();
	RefreshMarker();
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
	// Call base implementation first (draws breadcrumbs if enabled)
	Super::MarkerOnPaint_Implementation(Context, OnPaintData);

	if (!bDrawTerritoryOutline || !TerritoryVolume.IsValid()) return;

	// Get the territory's box bounds
	UBoxComponent* Box = Cast<UBoxComponent>(TerritoryVolume->GetComponentByClass(UBoxComponent::StaticClass()));
	if (!Box) return;

	// Get world-space bounds
	FVector BoxCenter = Box->GetComponentLocation();
	FVector BoxExtent = Box->GetScaledBoxExtent();

	// Calculate the 4 corners of the box in world space (XY plane, ignoring Z)
	FVector2D Corners[4];
	Corners[0] = FVector2D(BoxCenter.X - BoxExtent.X, BoxCenter.Y - BoxExtent.Y); // SW
	Corners[1] = FVector2D(BoxCenter.X + BoxExtent.X, BoxCenter.Y - BoxExtent.Y); // SE
	Corners[2] = FVector2D(BoxCenter.X + BoxExtent.X, BoxCenter.Y + BoxExtent.Y); // NE
	Corners[3] = FVector2D(BoxCenter.X - BoxExtent.X, BoxCenter.Y + BoxExtent.Y); // NW

	// Calculate scale using MapOrigin and MapPan from Narrative's FMarkerOnPaintData
	FVector2D MapSize = OnPaintData.MapGeometry.GetLocalSize();
	FVector2D Origin = OnPaintData.MapOrigin;
	FVector2D Pan = OnPaintData.MapPan;

	// MapScale: pixels per world unit
	// Narrative's Navigator uses MapSize to represent the world extent
	// The scale is derived from the ratio of map widget size to world coverage
	// If MapOrigin is valid (map is active), compute from available data
	float MapScale = 1.f;
	if (Origin.X != TNumericLimits<double>::Max())
	{
		// Default assumption: map widget covers 20000x20000 world units at 1:1 zoom
		// Adjust this based on actual Narrative Navigator zoom level in Blueprint
		MapScale = MapSize.X / 20000.f;
	}

	// Convert corners to local map space, accounting for MapPan offset
	TArray<FVector2f> LinePoints;
	for (int32 i = 0; i < 4; ++i)
	{
		FVector2D WorldCorner = Corners[i];
		FVector2D MapCorner;
		MapCorner.X = (WorldCorner.X - Origin.X) * MapScale + MapSize.X * 0.5f;
		MapCorner.Y = (WorldCorner.Y - Origin.Y) * MapScale + MapSize.Y * 0.5f;

		// Apply pan offset if valid
		if (Pan.X != TNumericLimits<double>::Max())
		{
			MapCorner += Pan;
		}

		LinePoints.Add(FVector2f(MapCorner.X, MapCorner.Y));
	}
	// Close the loop
	LinePoints.Add(LinePoints[0]);

	// Get the current color based on territory state
	FLinearColor DrawColor = GetMarkerColor(nullptr, FGameplayTag());
	DrawColor.A = 0.6f; // Semi-transparent outline

	// Draw the outline
	Context.MaxLayer++;
	FSlateDrawElement::MakeLines(
		Context.OutDrawElements,
		Context.MaxLayer,
		Context.AllottedGeometry.ToPaintGeometry(),
		LinePoints,
		ESlateDrawEffect::None,
		DrawColor,
		false,
		OutlineThickness);
}
