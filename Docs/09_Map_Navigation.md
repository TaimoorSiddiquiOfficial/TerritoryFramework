# Map & Navigation — Markers, Mini-Map, World Navigation

## Overview

TerritoryFramework integrates with Narrative Pro's map system to display territory borders, ownership colors, and navigation markers on the player's map.

```
ATerritoryVolume (territory actor)
└── UTerritoryNavigationMarkerComponent (attached to volume)
    └── UTerritoryMapMarker (created in BeginPlay)
        └── Draws colored polygon on Narrative's map canvas
        └── Auto-refreshes when ownership/state changes
```

## UTerritoryNavigationMarkerComponent

Attach this component to any `ATerritoryVolume` (or subclass) to automatically get a map marker.

### Properties

| Property | Type | Default | Notes |
|---|---|---|---|
| bAutoCreateMarker | bool | true | Auto-create marker in BeginPlay |
| MarkerDisplayText | FText | Territory display name | Override label on map |
| bShowOwnerColor | bool | true | Tint marker with owning faction color |
| bShowContestedFlash | bool | true | Flash when territory is contested |
| bShowOutline | bool | true | Draw border outline |

### Automatic Binding

On BeginPlay, the component:
1. Creates a `UTerritoryMapMarker` instance
2. Binds to the territory's `OnTerritoryControlChanged` delegate
3. Binds to the territory's `OnTerritoryStateChanged` delegate
4. Calls `RefreshMarker()` to set initial colors

On EndPlay (critical cleanup order):
1. `ClearTerritoryBinding()` — unbinds delegates
2. `RemoveMarker()` — removes marker from map
3. `Super::EndPlay()` — base cleanup
4. Null the marker pointer last

### Blueprint Access

```
Territory Volume → GetComponentByClass(TerritoryNavigationMarkerComponent)
  → SetMarkerDisplayText(NewText)
  → SetShowOwnerColor(true/false)
  → SetShowContestedFlash(true/false)
  → RefreshMarker()  — force redraw
```

---

## UTerritoryMapMarker

Extends `UMapMarker` (Narrative Pro base). Draws a colored region on the map representing the territory.

### Faction Color Mapping

| Territory State | Marker Color | Visual |
|---|---|---|
| Controlled (owned) | Faction color (from FactionColorMap) | Solid fill |
| Contested | Orange | Pulsing fill |
| Unclaimed | Gray | Hatched fill |
| Locked | Dark red | Crosshatched |

### FactionColorMap

```cpp
// Set in Blueprint or C++
TMap<FGameplayTag, FLinearColor> FactionColorMap = {
    {Narrative.Factions.Heroes,   FLinearColor(0.2f, 0.6f, 1.0f)},  // Blue
    {Narrative.Factions.Bandits,  FLinearColor(0.8f, 0.2f, 0.2f)},  // Red
    {Narrative.Factions.Merchants, FLinearColor(0.2f, 0.8f, 0.3f)}, // Green
};
```

Configure per-game in the TerritoryVolume Blueprint or via the navigation marker component.

### Drawing — MarkerOnPaint

`MarkerOnPaint` is called by Narrative's map canvas every frame the map is visible:

1. Call `Super::MarkerOnPaint()` — base Narrative marker drawing
2. Get territory bounds from `ATerritoryVolume::GetTerritoryBounds()`
3. Transform world bounds to map-space using `MapPan` offset
4. Fill the polygon with the current faction color
5. Draw outline if `bShowOutline`
6. Draw label text centered in bounds

### Auto-Refresh via Delegates

| Delegate | Marker Response |
|---|---|
| OnTerritoryControlChanged | Update faction color, broadcast refresh |
| OnTerritoryStateChanged | Update state-based visual (contested flash, etc.) |

The marker auto-refreshes — no manual polling needed.

### ClearTerritoryBinding

Unbinds all territory delegates. Must be called before the marker is destroyed to prevent use-after-free:

```cpp
void UTerritoryMapMarker::ClearTerritoryBinding()
{
    if (TerritoryVolume)
    {
        TerritoryVolume->OnTerritoryControlChanged.Remove(RefreshHandle);
        TerritoryVolume->OnTerritoryStateChanged.Remove(StateRefreshHandle);
        TerritoryVolume = nullptr;
    }
}
```

---

## Setup Guide

### Step 1: Add Component to Territory Blueprint

Open your `BP_TerritoryCity`, `BP_TerritoryDistrict`, or `BP_TerritoryProperty` Blueprint:
1. Add Component → `TerritoryNavigationMarkerComponent`
2. Set `bAutoCreateMarker = true`
3. Configure `FactionColorMap` in the Construction Script or defaults

### Step 2: Configure Faction Colors

In the TerritoryVolume Blueprint defaults:
```cpp
// In Blueprint Construction Script or C++
FactionColorMap.Add(FGameplayTag::RequestGameplayTag("Narrative.Factions.Heroes"), FLinearColor::Blue);
FactionColorMap.Add(FGameplayTag::RequestGameplayTag("Narrative.Factions.Bandits"), FLinearColor::Red);
```

### Step 3: Set Display Text

The marker label defaults to the territory's `TerritoryDisplayName`. Override via:
```
NavigationMarkerComponent → SetMarkerDisplayText("Custom Label")
```

### Step 4: Test in PIE

1. Open the map (press M or whatever key opens Narrative's map)
2. Territory should appear as a colored region
3. Capture the territory → marker color should update instantly

---

## Navigation Integration

### Quest Navigation

Narrative Pro's quest system can direct players to territories:

```
Quest Objective: "Capture the Bandit Camp"
└── Navigation Target: Territory.Territory.Tags.BanditCamp
```

The `UTerritoryNavigationMarkerComponent` implements the interface Narrative Pro uses to resolve navigation targets, so quest markers will route players to the territory volume's location.

### Custom Navigation Points

You can add sub-markers within a territory (e.g., capture point, NPC location):

```cpp
// In ATerritoryVolume subclass
UPROPERTY(EditAnywhere, Category = "Navigation")
TArray<FVector> CustomNavigationPoints;
```

These can be exposed to Narrative's quest system as navigation targets by implementing the appropriate Narrative interface.

---

## Mini-Map Integration

TerritoryFramework markers work on both the full map and mini-map (if Narrative Pro supports mini-map). The `MarkerOnPaint` function checks the canvas type and adjusts detail level:

- **Full map**: Full polygon + label + outline
- **Mini-map**: Simplified dot or small rectangle (performance)

---

## Performance Considerations

1. **Marker count** — one marker per territory. With 50+ territories, markers use spatial culling (Narrative handles this).
2. **Refresh frequency** — markers only redraw when the map is visible. No tick cost when map is closed.
3. **Color lookup** — `FactionColorMap` is a TMap, O(1) lookup per paint.
4. **Bounds calculation** — `GetTerritoryBounds()` returns cached bounds from the volume's collision component, not a fresh scan.

---

## Troubleshooting

### Marker Not Appearing

1. Verify `UTerritoryNavigationMarkerComponent` is attached to the territory actor
2. Check `bAutoCreateMarker = true`
3. Verify the territory is registered (`GetTerritoryRegistry → GetTerritoryByTag` returns valid)
4. Check Narrative's map is configured to show custom markers

### Marker Not Updating on Capture

1. Verify delegates are bound (check `UTerritoryMapMarker::TerritoryVolume` is not null)
2. Ensure `ClearTerritoryBinding` was NOT called prematurely
3. Check that `OnTerritoryControlChanged` is broadcasting (add a `Print String` in the delegate)

### Wrong Colors

1. Verify `FactionColorMap` has an entry for the owning faction tag
2. Check the faction tag matches exactly (case-sensitive)
3. If using Blueprint Construction Script, ensure colors are set before `BeginPlay`

### Marker Leaking After Destroy

If markers persist after territory destruction:
1. Check EndPlay cleanup order — `ClearTerritoryBinding` must run before `RemoveMarker`
2. Verify `Super::EndPlay()` is called after cleanup
3. Ensure marker pointer is nulled last, not first