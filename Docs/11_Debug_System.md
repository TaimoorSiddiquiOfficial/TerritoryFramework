# Debug System

## Overview

TerritoryFramework has 18 individual debug toggles organized into categories, plus 5 visual debug toggles. All are in **Project Settings → Territory Framework → Debug**.

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
| **Debug BT** | `[PatrolAI]` | **New** — BT move tasks, route advancement |
| **Debug Combat** | `[Combat]` | **New** — Attack permissions, budget |

## Visual Debug Toggles

| Toggle | Effect |
|---|---|
| Draw Territory Bounds | (Not implemented yet — placeholder) |
| Draw Ownership Overlay | (Not implemented yet — placeholder) |
| Draw Capture Progress | (Not implemented yet — placeholder) |
| Draw Guard Spawn Points | (Not implemented yet — placeholder) |
| Draw Spatial Grid | (Not implemented yet — placeholder) |

## Developer Settings — Guard / Patrol

| Setting | Default | Range | Description |
|---|---|---|---|
| DefaultPatrolArrivalThreshold | 100.0 | 50–500 | Distance to consider "arrived" at patrol node |
| DefaultPatrolAcceptanceRadius | 50.0 | 10–200 | Move completion distance for BT task |
| DefaultPatrolWaitTime | 2.0 | 0–30 | Wait time at patrol nodes |
| MaxPatrolRouteNodes | 32 | 0–100 | Sanity cap for patrol route length |
| EconomyStartingGold | 0 | 0+ | Starting gold for new factions |
| MaxCaptureHistory | 50 | 10–500 | Max capture history per territory |

## UTerritoryDebugWidget

A tick-based live overlay widget showing all territory state.

### Usage

1. Create a Blueprint child of `UTerritoryDebugWidget`
2. Override `OnUpdateDebugText(FText)` to update a TextBlock
3. Add widget to viewport
4. Call `SetDebugEnabled(true)` to start polling

### Sections Displayed

- **Territories**: Name, tag, owner, state, guard count, income
- **Economy**: Faction gold, income, costs, territory count
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
