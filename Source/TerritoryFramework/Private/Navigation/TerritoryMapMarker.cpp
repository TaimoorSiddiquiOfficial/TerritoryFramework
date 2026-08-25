#include "Navigation/TerritoryMapMarker.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "Navigation/NarrativeNavigationComponent.h"
#include "Navigation/NavigatorGameplayTags.h"
#include "Components/BoxComponent.h"
#include "Rendering/DrawElements.h"
#include "Blueprint/UserWidget.h"

UTerritoryMapMarker::UTerritoryMapMarker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Territory intel remains available on map surfaces. Compass and screen-space
	// are opt-in waypoint domains so overlapping Districts never flood the HUD.
	FGameplayTagContainer MapDomains;
	MapDomains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Minimap);
	MapDomains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Worldmap);
	SetDefaultDomains(MapDomains);
	DefaultMarkerActionText = NSLOCTEXT(
		"TerritoryMapMarker", "TrackTerritoryAction", "Set Territory Waypoint");
}

void UTerritoryMapMarker::SetTerritoryVolume(ATerritoryVolume* InTerritory)
{
	// Unbind from previous territory
	ClearTerritoryBinding();

	if (!IsValid(InTerritory)) return;

	TerritoryVolume = InTerritory;
	ActorOwner = InTerritory;

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
	ActorOwner = nullptr;
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

void UTerritoryMapMarker::SetTracked(bool bInTracked)
{
	if (bTracked == bInTracked) return;
	bTracked = bInTracked;
	FGameplayTagContainer Domains;
	Domains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Minimap);
	Domains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Worldmap);
	if (bTracked)
	{
		Domains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Compass);
		Domains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Screenspace);
	}
	SetDomains(Domains);
	SetDrawMarkerPathEnabled(bTracked);
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

	if (bTracked && State == ETerritoryState::Claimed && Selector)
	{
		const FGameplayTag ViewerFaction =
			UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Selector->GetOwner());
		if (ViewerFaction.IsValid()
			&& TerritoryVolume->GetOwningFaction() == ViewerFaction)
		{
			return TrackedCapturedColor;
		}
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

FText UTerritoryMapMarker::GetMarkerActionText_Implementation(
	UNarrativeNavigationComponent* Selector) const
{
	return bTracked
		? NSLOCTEXT("TerritoryMapMarker", "ClearWaypointAction", "Clear Territory Waypoint")
		: NSLOCTEXT("TerritoryMapMarker", "SetWaypointAction", "Set Territory Waypoint");
}

void UTerritoryMapMarker::OnSelect_Implementation(
	UNarrativeNavigationComponent* Selector)
{
	APlayerController* PlayerController = Selector
		? Cast<APlayerController>(Selector->GetOwner()) : nullptr;
	if (!PlayerController || !TerritoryVolume.IsValid()) return;
	if (bTracked)
	{
		UTerritoryUIBlueprintLibrary::ClearTerritoryWaypoint(PlayerController);
	}
	else
	{
		UTerritoryUIBlueprintLibrary::SetTerritoryWaypoint(
			PlayerController, TerritoryVolume.Get());
	}
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

	// P2-15: Convert each world corner to paint space using offset-from-center approach.
	// GetMarkerMapLocalPosition and GetMarkerTopLeftLocalPosition (inherited from base
	// NarrativeMapMarker) handle the full world-to-map transform including map scale,
	// zoom, pan, and tile offsets. The world-space offset from BoxCenter to each corner
	// is then applied in paint-local space, which is correct because the base class
	// positions the marker center correctly in the map's coordinate system.
	FVector2D MarkerPaintLocal = GetMarkerTopLeftLocalPosition(OnPaintData);

	TArray<FVector2f> LinePoints;
	for (int32 i = 0; i < 4; ++i)
	{
		// Offset from the box center to this corner in world space (XY only).
		// Uses BoxCenter (not actor location) so offset boxes render correctly.
		FVector2D WorldOffset = FVector2D(WorldCorners[i]) - FVector2D(BoxCenter);

		// Apply the same offset in map-local space (world units map 1:1 to map-local units)
		FVector2D CornerPaintLocal = MarkerPaintLocal + WorldOffset;

		LinePoints.Add(FVector2f(CornerPaintLocal));
	}
	// Copy before Add: passing LinePoints[0] by reference can be invalidated when
	// Add reallocates the same array (UE's debug allocator correctly asserts).
	const FVector2f FirstPoint = LinePoints[0];
	LinePoints.Add(FirstPoint); // Close the loop

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
