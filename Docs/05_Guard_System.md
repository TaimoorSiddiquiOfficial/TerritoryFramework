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

- Spawn-point ownership resolves by precedence: the territory's authored `GuardSpawnPoints` reference, `OwnerTerritoryTag`, then proximity
- Authored references support spawn points intentionally placed outside territory bounds
- `MaxGuards` per spawn point = active slots
- `ReserveSlots` = replacement entitlements for when active guards die
- Initial population uses `HasAvailableSlot()` only — reserves not consumed
- When a guard dies: `UnregisterGuard()` → if reserves available → `SpawnSingleGuard()` (one replacement, not full batch)

## Capture Flow

1. Kill all defenders → `OnAllGuardsDefeated` → territory goes Unclaimed
   - **Note:** checks ALL `RegisteredDefenders` (guards + any non-guard defenders registered via `RegisterDefender`), not just `SpawnedGuards`
   - Super call in BP override is **CRITICAL** — clears owner, resets progress, sets Unclaimed
2. Designer triggers capture via:
   - `RegisterAttacker(Territory, Actor, Faction)` — progressive capture (identity-based, TSet per faction)
   - `ForceCapture(Territory, Faction)` — authority-only instant capture; validates inputs, bypasses gameplay capture rules, sets progress to 1.0 and state to Claimed
   - `TerritoryCaptureEvent` — from quest/dialogue (server-authoritative, skips on client)
3. On capture → `SetOwningFaction` → guards respawn for new owner

## Debug

Enable in Project Settings → Territory Framework:
- `bDrawTerritoryBounds` — territory bounds
- `bDrawOwnershipOverlay` — green overlay for owned territories
- `bDrawCaptureProgress` — capture progress bar
- `bDrawGuardSpawnPoints` — spawn points and patrol routes
