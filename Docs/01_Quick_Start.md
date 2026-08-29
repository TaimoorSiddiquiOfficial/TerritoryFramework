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

These are sample names, not framework requirements. Replace `HavenReach`,
`MarketSquare`, `Blacksmith`, and `Farm` with your own project hierarchy. Runtime
code never searches for these names.

## Step 3: Create and Place the Territory Hierarchy

1. Create one `Territory City Definition` Data Asset.
2. Create its `Territory District Definition` assets.
3. Create each capturable `Territory Place Definition` asset.
4. Put Places in their District `Places` arrays and Districts in the City `Districts` array.
5. Use `Refresh Hierarchy Links` on the City. Parent tags are derived automatically.
6. Assign project Blueprint classes and use `Synchronize Territory City In Current Level`.
7. Resize each placed actor's `BoundsShape` to cover its gameplay area.

See [Territory Definition Assets](21_Definition_Assets.md) for safe hierarchy duplication and
strict Data Asset authority.

Do not directly capture an aggregate District or City. Capturing all required Places reduces the District owner; capturing all required Districts reduces the City owner through the existing hierarchy authority.

## Step 4: Choose Story or Multiplayer Capture

For multiplayer/domination:

1. Enable `Capture Point` in the Place Definition.
2. Select the project Capture Point Blueprint and set its relative placement/radius in the asset.
3. Synchronize the City. The placed point receives its exact Place tag from the asset.

For story capture:

1. Enable `Story Capture From Bounds` on the Place Definition.
2. Resize its `Bounds Shape` to cover every playable floor of the location.
3. Enable/configure the Story Owner template and add an Owner Handover Event to the Definition's
   `All Defenders Defeated Events`. The owner's Narrative dialogue performs capture.

Story-bounds mode starts `Contested` anywhere inside the Place but never fills capture
progress. It automatically disables and hides any Capture Point targeting that Place, so
you do not need to delete a multiplayer point from a shared map.

On the server, both presentations resolve the player's exact current Narrative faction
and use `UTerritoryControlSubsystem`. Neither the point nor the full-volume adapter writes
ownership directly.

## Step 5: Place Guard Spawn Points

1. Add one Guard Post row per active guard slot to the Territory Definition.
2. Give every row a stable ID, project Blueprint class, relative placement, reserves, and patrol.
3. Synchronize the City. The placed post keeps only its Definition link and Guard Post row ID.

Each spawned Guard Post actor is one active guard slot. Add more rows for more simultaneous guards.

## Step 6: Place Persistence Actor

1. Drag `BP_TerritoryWorldState` into level. **Single-player projects: use only `BP_TerritoryWorldState`** — it handles both single-player and multiplayer.
2. One instance is enough — it persists economy, diplomacy, and capture state.

> **Note:** `BP_TerritorySavableData` is **deprecated**. Do not use it for new projects. Use `BP_TerritoryWorldState` instead. If both exist in the level, an editor validator will report an error.

## Step 7: Configure Guards (Optional)

On the Territory Definition asset:
1. Set **Default Guard Definition** to your Narrative NPC asset (or use **Faction Guard Definitions** for per-faction guards).
2. Set **Initial Guard Count** to 3 (the existing-owner and AI/script capture target).
3. Keep **Post Capture Garrison Policy** → Player Chooses when captures by a matching live Narrative player faction should begin unstaffed
4. Set **Guard Recruitment Cost** for the one-time price and **Guard Upkeep Per Cycle** for recurring upkeep.
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
| Territory Unlock Event changes nothing | Select the complete tag of the locked actor. A locked Place needs its Place tag, not its parent District tag. Check **Get Territory State** during PIE; **New Campaign Initial State** is an authoring seed and intentionally stays unchanged. |
| Player is treated as the wrong test faction | Assign the live faction through Narrative Pro. The optional Territory Framework player fallback is empty by default and never overrides Narrative. |
| Guards float on hit | Already fixed — BoundsShape has NoCollision |
| Map marker not showing | MapMarkerComponent is auto-created — check marker color settings |
