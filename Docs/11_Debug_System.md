# Debug System

## Overview

TerritoryFramework has 16 individual debug toggles organized into categories, plus 5 visual debug toggles. All are in **Project Settings → Territory Framework → Debug**.

## Master Toggle

**Enable All Debug Output** — When false, no debug logs fire regardless of individual toggles.

## Log Categories

| Toggle | Log Prefix | What It Shows |
|---|---|---|
| Debug Registry | `[Registry]` | Territory registration/unregistration |
| Debug Capture | `[CaptureTick]` | Progress per contested territory every tick |
| Debug Capture Attempts | `[CaptureAttempt]` | Who's attacking what, current state |
| Debug Ownership | `[Ownership]` | Faction changes with before/after |
| Debug State Transitions | `[StateChange]` | Unclaimed→Claimed→Contested etc. |
| Debug Economy Ticks | `[EconomyTick]` | Gold, income, costs per faction per tick |
| Debug Transactions | `[Transaction]` | Every CREDIT/DEBIT with reason |
| Debug Guard Spawning | (in SpawnGuards) | Guard spawn events with faction/GUID |
| Debug Guard Deaths | `[GuardDeath]` | Name + remaining defenders |
| Debug Diplomacy | `[Diplomacy]` | Treaty state changes |
| Debug Faction Attitudes | `[Attitude]` | Attitude query results |
| Debug SaveLoad | `[SaveLoad]` | Territory load state |
| Debug Spatial | `[Spatial]` | QueryPoint results |
| Debug Map Markers | `[Marker]` | Refresh events |
| Debug Tales | `[TalesCaptureTask/Event]` | Task/event integration |
| Debug Combat | `[Combat]` | Assault slot grants/denials, slot cleanup |

## Visual Debug Toggles

| Toggle | Effect |
|---|---|
| Draw Territory Bounds | ✅ Draws box bounds in PIE (respects rotation) |
| Draw Ownership Overlay | ✅ Green overlay for owned territories |
| Draw Capture Progress | ✅ Progress bar above contested territories |
| Draw Guard Spawn Points | ✅ Yellow spheres at spawn points, cyan for patrol nodes |
| Draw Spatial Grid | ✅ Spatial index cell visualization |

## Developer Settings — Economy / Capture

| Setting | Default | Range | Description |
|---|---|---|---|
| EconomyTickIntervalSeconds | 300 | 10–3600 | Seconds between economy ticks |
| CaptureTickInterval | 0.1 | 0.01–1.0 | Seconds between capture ticks |
| CaptureProgressPerSecond | 0.1 | 0.01–1.0 | Progress rate per second per attacker |
| CaptureProgressDecayPerSecond | 0.05 | 0.01–0.5 | Decay rate when no attackers present |
| TreatyExpirationCheckInterval | 10 | 1–60 | Seconds between treaty expiration checks |
| SpatialCellSize | 2000 | 500–10000 | Spatial index cell size in UU |
| DefaultPatrolArrivalThreshold | 50 | 5–200 | Distance to consider patrol node reached |
| DefaultPatrolAcceptanceRadius | 30 | 5–100 | Movement acceptance radius for patrol |
| DefaultPatrolWaitTime | 3.0 | 0.5–30 | Default wait time at patrol nodes |
| MaxPatrolRouteNodes | 32 | 2–128 | Maximum patrol route length |
| MaxConcurrentAttackers | 3 | 1–10 | Default NPC attack slot limit per territory |

## UTerritoryDebugWidget

A tick-based live overlay widget showing all territory state.

### Usage

1. Create a Blueprint child of `UTerritoryDebugWidget`
2. Override `OnUpdateDebugText(FText)` to update a TextBlock
3. Add widget to viewport
4. Call `SetDebugEnabled(true)` to start polling

**Performance:** Widget throttles rebuilds to every **0.5 seconds** (not every frame). Subsystem pointers are cached on first access.

### Sections Displayed

- **Territories**: Name, tag, owner, state, guard count, income
- **Economy**: Faction wealth (aggregate from player inventories), income, costs, territory count
- **Diplomacy**: Active treaties with state type
- **Capture**: Contested territories with progress %

## Console Commands

```bash
# Enable territory logging
log LogTerritory All

# Set specific verbosity
log LogTerritory Verbose
```

## Reading Debug Output

All territory debug output goes to the **LogTerritory** category. Filter the Output Log by `LogTerritory` to see only territory events.
