# Quick Start — 5-Minute Setup

## Step 1: Enable Plugin

Ensure `TerritoryFramework` is enabled in `.uproject`:
```json
{
  "Name": "TerritoryFramework",
  "Enabled": true
}
```

## Step 2: Configure Tags

Add territory tags to `Config/DefaultGameplayTags.ini`:
```ini
+GameplayTagList=(Tag="Territory.HavenReach")
+GameplayTagList=(Tag="Territory.HavenReach.MarketSquare")
+GameplayTagList=(Tag="Territory.HavenReach.CastleHill")
+GameplayTagList=(Tag="Territory.HavenReach.MarketSquare.Blacksmith")
+GameplayTagList=(Tag="Territory.HavenReach.CastleHill.Farm")
+GameplayTagList=(Tag="Guard.Activity.Patrol")
+GameplayTagList=(Tag="Guard.Activity.Rest")
+GameplayTagList=(Tag="Guard.Activity.Inspect")
```

## Step 3: Place the Territory Hierarchy

1. Place one `ATerritoryCity` with tag `Territory.HavenReach` and `AggregateOnly` control.
2. Place child `ATerritoryDistrict` actors, set `ParentTerritoryTag` to the City tag, and use `AggregateOnly` control.
3. Place capturable child `ATerritoryProperty` actors, set each `ParentTerritoryTag` to its District, and use `Independent` control.
4. Give every placed Territory a unique gameplay tag and save the level so its editor-authored stable GUID is baked.
5. Resize each `BoundsShape` to cover its gameplay area.

Do not directly capture an aggregate District or City. Capturing all required Places reduces the District owner; capturing all required Districts reduces the City owner through the existing hierarchy authority.

## Step 4: Choose Story or Multiplayer Capture

For multiplayer/domination:

1. Place `BP_TerritoryCapturePoint` at each capturable Place.
2. Set `TargetTerritoryTag` to the exact independent Property tag.
3. Set `CaptureRadius` to the intended physical zone.

For story capture:

1. Enable `Story Capture From Territory Bounds` on the Place.
2. Resize its `Bounds Shape` to cover every playable floor of the location.
3. Configure `On All Defenders Defeated Events` to activate a Blueprint child of
   `ATerritoryStoryOwnerSpawner`, then let the owner's Narrative dialogue perform capture.

Story-bounds mode starts `Contested` anywhere inside the Place but never fills capture
progress. It automatically disables and hides any Capture Point targeting that Place, so
you do not need to delete a multiplayer point from a shared map.

On the server, both presentations resolve the player's exact current Narrative faction
and use `UTerritoryControlSubsystem`. Neither the point nor the full-volume adapter writes
ownership directly.

## Step 5: Place Guard Spawn Points

1. Drag `BP_GuardSpawnPoint` into level inside the territory bounds
2. In Details panel:
   - **Owner Territory Tag**: `Territory.HavenReach.MarketSquare`
   - **Reserve Slots**: 1
   - **Patrol Route**: Add 3-4 waypoints around the district

Each spawn-point actor is one active guard slot. The legacy `MaxGuards` property is ignored; place multiple authored posts for multiple active guards.

## Step 6: Place Persistence Actor

1. Drag `BP_TerritoryWorldState` into level. **Single-player projects: use only `BP_TerritoryWorldState`** — it handles both single-player and multiplayer.
2. One instance is enough — it persists economy, diplomacy, and capture state.

> **Note:** `BP_TerritorySavableData` is **deprecated**. Do not use it for new projects. Use `BP_TerritoryWorldState` instead. If both exist in the level, an editor validator will report an error.

## Step 7: Configure Guards (Optional)

On the territory volume:
1. Set **Guard NPC Definition** → your NPC data asset (or use **Faction Guard Definitions** for per-faction guards)
2. Set **Guard Spawn Count** → 3 (the existing-owner and AI/script capture target)
3. Keep **Post Capture Garrison Policy** → Player Chooses when captures by a matching live Narrative player faction should begin unstaffed
4. Set **Initial Guard Recruitment Cost** for the one-time price and **Initial Guard Cost** for recurring upkeep
5. Guards inherit combat AI from the NPCDefinition asset (configured in NarrativePro)

## Step 8: Test in PIE

1. Press **PIE**
2. Check Output Log for:
   ```
   LogTerritory: Registered territory: ... (tag: Territory.HavenReach.MarketSquare)
   LogTerritory: Registered territory: ...
   LogTerritory: ... ownership committed ...
   ```
3. Multiplayer: remain in a configured Capture Point until capture completes. Story:
   enter anywhere inside the Place bounds, defeat its defenders, and accept the owner
   handover dialogue.
4. Verify the Place becomes claimed, then verify its aggregate District and City reduce from their children.

With `PlayerChooses`, a capture whose context belongs to the new owner starts at target zero. `ForceCaptureWithContext` is for explicit scripted transitions and tests, not physical gameplay. Select the District or one of its Properties in the District Command Center, preview recruitment/upkeep/net yield, and submit an exact staffing target.

## Step 9: Enable Debug (Optional)

1. Edit → Project Settings → Territory Framework
2. Enable **"Enable All Debug Output"**
3. Enable individual categories
4. Check Output Log for detailed territory events

## Troubleshooting

| Problem | Solution |
|---|---|
| Guards don't spawn | Assign GuardNPCDefinition on the territory volume |
| Guards don't fight | Configure ActivityConfiguration + TriggerSets on the NPCDefinition asset |
| Territory doesn't save | Place TerritoryWorldState actor in level |
| Capture doesn't work | Confirm the point targets an `Independent` Property, the pawn is player-controlled, its Narrative faction is valid, and diplomacy allows capture |
| Guards float on hit | Already fixed — BoundsShape has NoCollision |
| Map marker not showing | MapMarkerComponent is auto-created — check marker color settings |
