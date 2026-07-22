# Territory Framework — Blueprint Setup Tutorial

> Complete step-by-step guide for setting up a functional territory capture system in your level.

---

## 1. Prerequisites

- TerritoryFramework plugin enabled in `.uproject`
- Narrative Pro plugin enabled (for NPC, faction, quest systems)
- NavMesh generated in your level

---

## 2. Creating Territory Actors

### 2.1 City Volume

1. Drag `BP_TerritoryCity` (or create a BP child of `ATerritoryCity`) into the level
2. Set in Details panel:
   - **Territory Tag** = `Territory.YourCityName` (e.g., `Territory.HavenReach`)
   - **Territory Display Name** = `Haven Reach`
   - **Bounds Shape** = resize the BoxComponent to cover the city area
3. Resize: select the BoundsShape component, edit Box Extent in details

### 2.2 District Volume

1. Drag `BP_TerritoryDistrict` into the level inside the city bounds
2. Set:
   - **Territory Tag** = `Territory.HavenReach.MarketSquare`
   - **Parent Territory Tag** = `Territory.HavenReach`
   - **Bounds Shape** = resize to cover the district

### 2.3 Property Volume

1. Drag `BP_TerritoryProperty` inside the district bounds
2. Set:
   - **Territory Tag** = `Territory.HavenReach.MarketSquare.Blacksmith`
   - **Parent Territory Tag** = `Territory.HavenReach.MarketSquare`
   - **Bounds Shape** = resize to cover the building

---

## 3. Guard Setup

### 3.1 NPC Definition

Create an `NPCDefinition` data asset for your territory guards:
- **NPC Class Path** = your guard character BP
- **Default Factions** = `Narrative.Factions.Bandits` (or whatever enemy faction)
- Configure ActivityConfiguration, ActivitySchedules, TriggerSets as needed

### 3.2 Guard Spawn Points

1. Place `BP_TerritoryGuardSpawnPoint` actors inside the Property/District volume
2. Set on each:
   - **Max Guards** = 2
   - **Reserve Slots** = 1
   - **Priority** = 50 (higher = fills first)
   - **Patrol Route** = add waypoints (optional — Narrative handles patrol via NPCDefinition)
3. Reference these in the Territory Volume's **Guard Spawn Points** array

### 3.3 Territory Guard Configuration

On the Property/District volume:
- **Guard NPC Definition** = your NPCDefinition asset
- **Guard Spawn Count** = 3
- **Guard Spawn Radius** = 500 (fallback if no spawn points)

---

## 4. Faction Setup

### 4.1 Initial Owner

On each territory volume:
- **Initial Owning Faction** = `Narrative.Factions.Bandits`
- This sets the territory to **Claimed** (red marker) on BeginPlay

### 4.2 Player Faction

For the map marker to show green for the player:
1. Open the `BP_TerritoryMapMarker` class defaults (or create a child BP)
2. In **Faction Color Map**, add entry:
   - Key: `Narrative.Factions.Heroes` (your player faction)
   - Value: `(R=0, G=1, B=0, A=1)` (green)
3. All other factions without an entry default to red

---

## 5. Capture Setup

Choose ONE capture method per territory:

### Method A: Quest/Dialogue Capture (Recommended)

1. Create a quest with a state that fires on guard death
2. Add **TerritoryCaptureEvent** to the quest node:
   - **Target Territory Tag** = `Territory.HavenReach.MarketSquare.Blacksmith`
   - **Capturing Faction** = `Narrative.Factions.Heroes`
   - **Force Capture** = false (respects rules) or true (bypasses all checks)

**Flow:** Kill all guards → territory goes red → complete quest objective → capture fires → territory turns green → guards respawn for player faction.

### Method B: Overlap Trigger Capture

1. Add a `BoxTrigger` actor inside the territory volume
2. In the BoxTrigger's BP (or level BP):
   ```
   On Actor Begin Overlap:
     Get Territory Control (World Context)
     → RegisterAttacker(Territory, Other Actor, Faction)
   
   On Actor End Overlap:
     Get Territory Control (World Context)
     → UnregisterAttacker(Territory, Other Actor, Faction)
   ```
3. Capture progress fills while player is inside (rate: `CaptureProgressPerSecond` in Project Settings)
4. Takes ~10 seconds at default rate (0.1/s)

### Method C: Blueprint Event on Guard Death

1. In your game mode or level BP, bind to the territory's `OnAllGuardsDefeatedDelegate`:
   ```
   Bind Event to Territory → OnAllGuardsDefeatedDelegate
   On Triggered:
     Get Territory Control (World Context)
     → ForceCapture(Territory, PlayerFaction)
   ```

---

## 6. Lock System (Quest-Gated Territories)

### 6.1 Start Locked

On the territory volume:
- **Starts Locked** = true
- Territory starts invisible on map, no guards, can't be captured

### 6.2 Unlock via Quest

Add **TerritoryUnlockEvent** to a quest/dialogue node:
- **Target Territory Tag** = the territory to unlock
- **Force Unlock** = true

### 6.3 Lock Conditions (Optional)

On the territory volume, add conditions to the **Lock Conditions** array:
- These are `UNarrativeCondition` instances (e.g., quest completed, level reached)
- `TryUnlock()` checks all conditions — only unlocks if ALL pass

---

## 7. Map Markers

The `TerritoryNavigationMarkerComponent` is automatically created on each territory volume. To customize:

| State | Color | Property |
|---|---|---|
| Unclaimed | Red | `UnclaimedColor` |
| Enemy owned | Red | `EnemyOwnedColor` |
| Player owned | Green | Via `FactionColorMap` |
| Contested | Yellow | `ContestedColor` |
| Locked | Invisible | (zero alpha) |

All colors are editable in the marker component's defaults or at runtime via `SetFactionColor`.

---

## 8. Hierarchy Capture Rules

The system enforces bottom-up capture:

```
City (captures when ALL districts owned by one faction)
  └── District (captures when ALL properties owned by one faction)
       └── Property (captures individually — kill guards → capture trigger)
```

**Example flow:**
1. Player captures Blacksmith property → property turns green
2. Player captures all properties in MarketSquare district → district turns green
3. Player captures all districts in HavenReach city → city turns green + capital bonus gold

If any property remains enemy-owned, the district stays contested (yellow).

---

## 9. Economy

Territory income flows automatically:

- Each territory has **Periodic Income** (default: 100 gold)
- Economy subsystem ticks every 300s (configurable in Project Settings)
- Income goes to the owning faction's treasury
- Capital districts give 2x income multiplier
- Capital city capture gives bonus gold

Check treasury via Blueprint:
```
Get Territory Economy → GetTreasury(FactionTag)
```

---

## 10. Tales Integration (Quests & Dialogue)

### Conditions (gate dialogue/quest nodes)

| Condition | Checks |
|---|---|
| **TerritoryOwnershipCondition** | Who owns a territory |

### Events (fire from quest/dialogue nodes)

| Event | Effect |
|---|---|
| **TerritoryCaptureEvent** | Capture territory by tag |
| **TerritoryLockEvent** | Lock territory |
| **TerritoryUnlockEvent** | Unlock territory |

### Tasks (quest objectives)

| Task | Completes when |
|---|---|
| **TerritoryCaptureTask** | Territory changes owner |

---

## 11. Debug

Enable in **Project Settings → Territory Framework**:
- `bDebugEnabled` = true — enables OnScreen messages
- `bDebugOwnership` = true — logs ownership changes
- `bDebugCapture` = true — logs capture progress
- `bDebugGuardDeaths` = true — logs guard deaths
- `bDebugStateTransitions` = true — logs state changes

Use Blueprint debug helpers:
```
PrintTerritoryDebug(WorldContext, Territory, Duration)
PrintAllTerritoryDebug(WorldContext, Duration)
```

---

## 12. Project Settings

**Project Settings → Territory Framework:**

| Setting | Default | Description |
|---|---|---|
| `CaptureProgressPerSecond` | 0.1 | Progress per second with attacker present |
| `CaptureProgressDecayPerSecond` | 0.05 | Progress decay when no attacker |
| `CaptureTickInterval` | 0.1s | Evaluation frequency |
| `EconomyTickIntervalSeconds` | 300 | Income tick frequency |
| `DefaultMaxConcurrentAttackers` | 3 | Max attackers per territory |
| `SpatialCellSize` | 2000 | Spatial index grid size |
