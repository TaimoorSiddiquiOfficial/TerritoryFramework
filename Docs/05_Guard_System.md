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

## Guard Post Definitions (Data-Driven Configuration)

`UTerritoryGuardPostDefinition` is a `UPrimaryDataAsset` that packages a complete guard configuration for reuse across multiple spawn points. Instead of configuring NPC definitions, activities, patrol routes, and reserve parameters individually on each spawn point, create a single GuardPostDefinition asset and assign it to `ATerritoryGuardSpawnPoint::GuardPostDefinition`.

### When to Use

- Multiple spawn points sharing the same guard type and behavior
- Shared reserve deployment parameters (delay, radius, player distance)
- Consistent patrol route templates across a district or city

### Configuration Workflow

1. Create a `UTerritoryGuardPostDefinition` data asset in the Content Browser (primary asset type: `TerritoryGuardPost`)
2. Set `NPCDefinition`, `ActivityConfiguration`, and optional `TriggerSetOverrides`
3. Configure `PatrolRoute` waypoints and `bLoopPatrol`
4. Set capacity: `MaxGuards`, `ReserveSlots`
5. Tune reserve behavior: `ReserveSpawnDelay`, `ReserveSpawnRetryInterval`, `ReserveSpawnRadius`, `ReserveMinimumPlayerDistance`, `ReserveSpawnCandidateCount`
6. Assign the asset to one or more `ATerritoryGuardSpawnPoint::GuardPostDefinition` references

### Override Precedence

Per-spawn-point overrides take precedence over the data asset:

1. `SpawnPoint->NPCDefinitionOverride` (if set, overrides `GuardPostDefinition->NPCDefinition`)
2. `SpawnPoint->ActivityConfigurationOverride` (if set, overrides data asset)
3. `SpawnPoint->TriggerSetOverrides` (if non-empty, overrides data asset)
4. `SpawnPoint->FactionOverride` (if valid, overrides `GuardPostDefinition->FactionOverride`)

If no `GuardPostDefinition` is assigned, the territory's `GuardNPCDefinition` and `FactionGuardDefinitions` are used as fallback.

## Capture Flow

1. Kill all defenders → `OnAllGuardsDefeated` → defender count becomes zero; the incumbent owner remains authoritative but the territory is vulnerable
	- **Note:** checks ALL `RegisteredDefenders` (guards + any non-guard defenders registered via `RegisterDefender`), not just `SpawnedGuards`
	- A BP override may call Super for the native zero-defence update; it must never clear ownership as a casualty side effect
2. Designer triggers capture via:
   - `RegisterAttacker(Territory, Actor, Faction)` — progressive capture (identity-based, TSet per faction)
   - `ForceCapture(Territory, Faction)` → bool — authority-only instant capture; validates inputs, bypasses gameplay capture rules, sets progress to 1.0 and state to Claimed. Returns true if territory actually changed.
   - `TerritoryCaptureEvent` — from quest/dialogue (server-authoritative, skips on client)
3. On capture → `ApplyTerritoryMutation` atomically commits Claimed ownership → guards respawn for the new owner

On save, each guard post records reserves, pending deployments, and its finite active count. Load recreates only the saved survivors (or uses the bounded legacy aggregate defender count for older saves); pawn health/activity and live pointers are intentionally not persisted.

## Debug

Enable in Project Settings → Territory Framework:
- `bDrawTerritoryBounds` — territory bounds
- `bDrawOwnershipOverlay` — green overlay for owned territories
- `bDrawCaptureProgress` — capture progress bar
- `bDrawGuardSpawnPoints` — spawn points and patrol routes
