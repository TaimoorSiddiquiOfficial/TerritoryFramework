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
	InTerritory->OnTerritoryOwnershipChanged.AddDynamic(this, &UTerritoryMapMarker::OnTerritoryChanged);
	InTerritory->OnTerritoryStateChangedDelegate.AddDynamic(this, &UTerritoryMapMarker::OnTerritoryStateChanged);

	RefreshMarker();
}

void UTerritoryMapMarker::ClearTerritoryBinding()
{
	if (TerritoryVolume.IsValid())
	{
		TerritoryVolume->OnTerritoryOwnershipChanged.RemoveDynamic(this, &UTerritoryMapMarker::OnTerritoryChanged);
		TerritoryVolume->OnTerritoryStateChangedDelegate.RemoveDynamic(this, &UTerritoryMapMarker::OnTerritoryStateChanged);
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

	// Locked = invisible. No marker shown at all.
	if (State == ETerritoryState::Locked)
	{
		return FLinearColor(0.f, 0.f, 0.f, 0.f);
	}

	if (State == ETerritoryState::Contested)
	{
		return ContestedColor;
	}

	FGameplayTag Owner = TerritoryVolume->GetOwningFaction();
	if (Owner.IsValid())
	{
		// Check faction-specific color override (player faction = green via FactionColorMap)
		const FLinearColor* FactionColor = FactionColorMap.Find(Owner);
		if (FactionColor) return *FactionColor;

		// No faction-specific color → enemy owned → red
		return EnemyOwnedColor;
	}

	// No owner → unclaimed → red
	return UnclaimedColor;
}

FText UTerritoryMapMarker::GetMarkerDisplayText_Implementation(UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType, FText& OutSubtitleText) const
{
	if (!TerritoryVolume.IsValid())
	{
		return FText::GetEmpty();
	}

	// Locked = no text shown
	if (TerritoryVolume->GetTerritoryState() == ETerritoryState::Locked)
	{
		return FText::GetEmpty();
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
	Super::MarkerOnPaint_Implementation(Context, OnPaintData);

	if (!bDrawTerritoryOutline || !TerritoryVolume.IsValid()) return;

	// Check for invalid map data (not yet initialized)
	if (OnPaintData.MapOrigin.X == TNumericLimits<double>::Max()) return;

	// Get the territory's bounds shape
	UBoxComponent* Box = Cast<UBoxComponent>(TerritoryVolume->GetComponentByClass(UBoxComponent::StaticClass()));
	if (!Box) return;

	// Calculate the 4 corners of the box in world space using the component transform
	// (respects rotation, unlike the old axis-aligned approach)
	FVector BoxCenter = Box->GetComponentLocation();
	FVector BoxExtent = Box->GetScaledBoxExtent();
	FQuat BoxRotation = Box->GetComponentQuat();

	FVector WorldCorners[4];
	WorldCorners[0] = BoxCenter + BoxRotation.RotateVector(FVector(-BoxExtent.X, -BoxExtent.Y, 0));
	WorldCorners[1] = BoxCenter + BoxRotation.RotateVector(FVector( BoxExtent.X, -BoxExtent.Y, 0));
	WorldCorners[2] = BoxCenter + BoxRotation.RotateVector(FVector( BoxExtent.X,  BoxExtent.Y, 0));
	WorldCorners[3] = BoxCenter + BoxRotation.RotateVector(FVector(-BoxExtent.X,  BoxExtent.Y, 0));

	// Convert each world corner to paint space using the marker's own world position
	// as reference. We compute the offset from the territory volume's location to
	// each corner, then apply that offset in map-local space.
	FVector2D MarkerMapLocal = GetMarkerMapLocalPosition(OnPaintData.MapOrigin, OnPaintData.MapPan);
	FVector2D MarkerPaintLocal = GetMarkerTopLeftLocalPosition(OnPaintData);

	TArray<FVector2f> LinePoints;
	for (int32 i = 0; i < 4; ++i)
	{
		// Offset from the territory actor to this corner in world space (XY only)
		FVector2D WorldOffset = FVector2D(WorldCorners[i]) - FVector2D(TerritoryVolume->GetActorLocation());

		// Apply the same offset in map-local space (world units map 1:1 to map-local units)
		FVector2D CornerPaintLocal = MarkerPaintLocal + WorldOffset;

		LinePoints.Add(FVector2f(CornerPaintLocal));
	}
	LinePoints.Add(LinePoints[0]); // Close the loop

	// Draw the outline with current territory color
	FLinearColor DrawColor = GetMarkerColor(nullptr, FGameplayTag());
	DrawColor.A = 0.6f;

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
