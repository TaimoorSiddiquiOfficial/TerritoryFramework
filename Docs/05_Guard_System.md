# Guard System

## Overview

Territory guards are Narrative Pro NPCs spawned by TerritoryVolume. All AI behavior (combat, patrol, idle) is driven by Narrative's `UNPCDefinition` system — no custom Territory BT or AI controller.

## How Guards Work

```
ATerritoryVolume::SpawnGuards()
  ├─ Resolve NPC class from GuardNPCDefinition->NPCClassPath (LoadSynchronous)
  ├─ For each spawn slot:
  │   ├─ Get spawn transform from ATerritoryGuardSpawnPoint (NavMesh-projected)
  │   ├─ BeginDeferredActorSpawnFromClass
  │   ├─ Guard->ConfigureTerritorySpawn(Definition, Faction, GUIDs)
  │   │    └─ Sets FNPCSpawnParams BEFORE SetNPCDefinition:
  │   │       - bOverride_DefaultFactions = true (exact territory owner)
  │   │       - Optional ActivityConfiguration override
  │   │       - Optional TriggerSet overrides
  │   ├─ FinishSpawningActor
  │   └─ RegisterDefender + RegisterSpawnedGuard
  └─ Narrative handles all AI from here
```

## Faction Assignment

Guard faction is determined by precedence:

1. `SpawnPoint->FactionOverride` (if valid)
2. `TerritoryVolume->OwnershipData.OwningFaction`

Faction is set via `FNPCSpawnParams.bOverride_DefaultFactions` before `SetNPCDefinition`, so Narrative's initialization reads the correct faction. The NPCDefinition's default factions are overridden — the guard belongs ONLY to the territory owner.

## Patrol System

Territory spawn points can define patrol routes. These are consumed by Narrative's existing patrol activity:

1. Configure `PatrolRoute` waypoints on `ATerritoryGuardSpawnPoint`
2. In BP (BPA_Patrol or custom activity), call:
   - `GetPatrolRouteAsTransforms()` → populates `Goal_Patrol.PatrolPoints`
   - `GetPatrolWaitTimes()` → populates `S_PatrolPoint.WaitDuration`
3. Narrative's `BT_Patrol` + `BPA_Patrol` handles all movement

No custom Territory BT needed — thin adapter into Narrative's infrastructure.

## Reserve System

- `MaxGuards` per spawn point = active slots
- `ReserveSlots` = replacement entitlements for when active guards die
- Initial population uses `HasAvailableSlot()` only — reserves not consumed
- When a guard dies: `UnregisterGuard()` → if reserves available → `SpawnSingleGuard()` (one replacement, not full batch)

## Capture Flow

1. Kill all guards → `OnAllGuardsDefeated` → territory goes Unclaimed
2. Designer triggers capture via:
   - `RegisterAttacker(Territory, Actor, Faction)` — progressive capture
   - `ForceCapture(Territory, Faction)` — instant capture
   - `TerritoryCaptureEvent` — from quest/dialogue
3. On capture → `SetOwningFaction` → guards respawn for new owner

## Debug

Enable in Project Settings → Territory Framework:
- `bDrawTerritoryBounds` — territory bounds
- `bDrawOwnershipOverlay` — green overlay for owned territories
- `bDrawCaptureProgress` — capture progress bar
- `bDrawGuardSpawnPoints` — spawn points and patrol routes
