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

### Functions

| Function | Returns | Purpose |
|---|---|---|
| GetTerritoryMapMarker() | UTerritoryMapMarker* | Get the map marker instance |
| GetOwningTerritory() | ATerritoryVolume* | Get the territory this belongs to |
| RefreshTerritoryMarker() | void | Force refresh the marker display |

### Automatic Binding

On BeginPlay, the component:
1. Creates a `UTerritoryMapMarker` instance
2. Binds to the territory's ownership and state change handlers
3. Automatically refreshes on ownership/state changes

On EndPlay, the component unbinds all delegates.

---

## UTerritoryMapMarker

Extends `UMapMarker` (Narrative Pro base). Draws a colored region on the map representing the territory.

### Properties

| Property | Type | Default | Notes |
|---|---|---|---|
| FactionColorMap | TMap<FGameplayTag, FLinearColor> | — | Per-faction colors |
| DefaultColor | FLinearColor | Gray (0.5,0.5,0.5) | Fallback when no faction color |
| UnclaimedColor | FLinearColor | Red | Territory with no owner |
| ContestedColor | FLinearColor | Yellow | Territory being contested |
| EnemyOwnedColor | FLinearColor | Red | Enemy faction territory |
| LockedColor | FLinearColor | Purple | Locked territory |
| bDrawTerritoryOutline | bool | true | Draw border outline |
| OutlineThickness | float | 2.0 | Outline width in pixels |

### Functions

| Function | Purpose |
|---|---|
| SetTerritoryVolume(Volume) | Link this marker to a territory volume |
| ClearTerritoryBinding() | Unbind all delegates (call before destroy) |
| GetTerritoryVolume() | Get the linked territory |
| SetFactionColor(Faction, Color) | Set color for a specific faction |
| ClearFactionColors() | Reset all faction colors |

### Faction Color Mapping

```cpp
// Set in Blueprint or C++
TMap<FGameplayTag, FLinearColor> FactionColorMap = {
    {Narrative.Factions.Heroes,   FLinearColor(0.2f, 0.6f, 1.0f)},  // Blue
    {Narrative.Factions.Bandits,  FLinearColor(0.8f, 0.2f, 0.2f)},  // Red
    {Narrative.Factions.Merchants, FLinearColor(0.2f, 0.8f, 0.3f)}, // Green
};
```

Configure per-game in the TerritoryVolume Blueprint or via the map marker component.

### Drawing — MarkerOnPaint

`MarkerOnPaint_Implementation` is called by Narrative's map canvas every frame the map is visible:

1. Get territory bounds from `ATerritoryVolume::GetTerritoryBounds()`
2. Transform world bounds to map-space using `MapPan` offset
3. Fill the polygon with the current state color (faction/contested/unclaimed/locked)
4. Draw outline if `bDrawTerritoryOutline`
5. Draw label text centered in bounds

### Auto-Refresh

The marker auto-refreshes on ownership change and state change — no manual polling needed.

### ClearTerritoryBinding

Unbinds all territory delegates. Must be called before the marker is destroyed to prevent use-after-free.

---

## Setup Guide

### Step 1: Add Component to Territory Blueprint

Open your `BP_TerritoryCity`, `BP_TerritoryDistrict`, or `BP_TerritoryProperty` Blueprint:
1. Add Component → `TerritoryNavigationMarkerComponent`
2. Configure `FactionColorMap` in defaults

### Step 2: Configure Faction Colors

In the territory Blueprint defaults or C++:
```cpp
FactionColorMap.Add(FGameplayTag::RequestGameplayTag("Narrative.Factions.Heroes"), FLinearColor::Blue);
FactionColorMap.Add(FGameplayTag::RequestGameplayTag("Narrative.Factions.Bandits"), FLinearColor::Red);
```

### Step 3: Test in PIE

1. Open the map (press M or whatever key opens Narrative's map)
2. Territory should appear as a colored region
3. Capture the territory → marker color should update instantly

---

## Navigation Integration

### Quest Navigation

Narrative Pro's quest system can direct players to territories via the navigation marker component.

### Custom Navigation Points

You can add sub-markers within a territory (e.g., capture point, NPC location) by implementing the appropriate Narrative navigation interfaces.

---

## Performance Considerations

1. **Marker count** — one marker per territory. With 50+ territories, markers use spatial culling (Narrative handles this).
2. **Refresh frequency** — markers only redraw when the map is visible. No tick cost when map is closed.
3. **Color lookup** — `FactionColorMap` is a TMap, O(1) lookup per paint.
4. **Bounds calculation** — `GetTerritoryBounds()` returns cached bounds from the volume's collision component.

---

## Troubleshooting

### Marker Not Appearing

1. Verify `UTerritoryNavigationMarkerComponent` is attached to the territory actor
2. Verify the territory is registered (`GetTerritoryRegistry → GetTerritoryByTag` returns valid)
3. Check Narrative's map is configured to show custom markers

### Marker Not Updating on Capture

1. Verify the marker is linked (`GetTerritoryVolume()` returns valid)
2. Ensure `ClearTerritoryBinding` was NOT called prematurely

### Wrong Colors

1. Verify `FactionColorMap` has an entry for the owning faction tag
2. Check the faction tag matches exactly (case-sensitive)
