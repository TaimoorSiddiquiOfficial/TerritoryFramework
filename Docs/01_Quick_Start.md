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
+GameplayTagList=(Tag="Guard.Activity.Patrol")
+GameplayTagList=(Tag="Guard.Activity.Rest")
+GameplayTagList=(Tag="Guard.Activity.Inspect")
```

## Step 3: Place Territory Actors

1. Drag `BP_TerritoryDistrict` from Content Browser into level
2. In Details panel, set:
   - **Territory Tag**: `Territory.HavenReach.MarketSquare`
   - **Territory Display Name**: `Market Square`
   - **Initial Owning Faction**: `Narrative.Factions.Bandits`
   - **Initial Periodic Income**: 200
   - **Initial Guard Cost**: 75

3. Resize the **BoundsShape** box to cover your district area

## Step 4: Place Guard Spawn Points

1. Drag `BP_GuardSpawnPoint` into level inside the territory bounds
2. In Details panel:
   - **Owner Territory Tag**: `Territory.HavenReach.MarketSquare`
   - **Max Guards**: 3
   - **Reserve Slots**: 1
   - **Patrol Route**: Add 3-4 waypoints around the district

## Step 5: Place Persistence Actor

1. Drag `BP_TerritorySavableData` (or `BP_TerritoryWorldState` for multiplayer) into level
2. One instance is enough — it persists economy and diplomacy state

## Step 6: Configure Guards (Optional)

On the territory volume:
1. Set **Guard NPC Definition** → your NPC data asset
2. Set **Guard Behavior Tree** → `BT_Attack_Generic` from NarrativePro
3. Set **Guard Blackboard Asset** → `BB_Attack` from NarrativePro
4. Set **Guard Spawn Count** → 3

## Step 7: Test in PIE

1. Press **PIE**
2. Check Output Log for:
   ```
   LogTerritory: Registered territory: ... (tag: Territory.HavenReach.MarketSquare)
   LogTerritory: SpawnGuards: ... spawning 3 guards, faction=...
   LogTerritory: Spawned guard 1/3 for ...
   ```
3. Use the **Territory Control** subsystem to attempt capture:
   - Blueprint: `GetTerritoryControl → AttemptCapture(Territory, Heroes)`
4. Check the territory changes ownership

## Step 8: Enable Debug (Optional)

1. Edit → Project Settings → Territory Framework
2. Enable **"Enable All Debug Output"**
3. Enable individual categories
4. Check Output Log for detailed territory events

## Troubleshooting

| Problem | Solution |
|---|---|
| Guards don't spawn | Assign GuardNPCDefinition on the territory volume |
| Guards don't fight | Assign GuardBehaviorTree (e.g., BT_Attack_Generic) |
| Territory doesn't save | Place TerritorySavableData actor in level |
| Capture doesn't work | Check faction attitudes (Friendly factions can't capture each other) |
| Guards float on hit | Already fixed — BoundShape has NoCollision |
| Map marker not showing | Add TerritoryNavigationMarkerComponent to the territory volume |
