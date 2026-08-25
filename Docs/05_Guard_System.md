# Guard System

## Overview

Territory guards are Narrative Pro NPCs spawned by TerritoryVolume. All AI behavior (combat, patrol, idle) is driven by Narrative's `UNPCDefinition` system — no custom Territory BT or AI controller.

## How Guards Work

```
ATerritoryVolume::SpawnGuards()
  ├─ Resolve NPC class from GuardNPCDefinition->NPCClassPath (LoadSynchronous)
  ├─ For each unique authored spawn point (one active slot each):
  │   ├─ preserve the marker's exact X/Y and facing
  │   ├─ align only Z to local navigation ground + capsule half-height
  │   ├─ reject the slot when the resolved capsule is blocked
  │   ├─ Guard->SpawnThroughNarrative(Definition, Faction, GUIDs)
  │   │    ├─ calls UNarrativeCharacterSubsystem::SpawnNPC
  │   │    └─ applies the scoped Territory context BEFORE SetNPCDefinition:
  │   │       - bOverride_DefaultFactions = true (exact territory owner)
  │   │       - Optional ActivityConfiguration override
  │   │       - Optional TriggerSet overrides
  │   ├─ verify Narrative controller/activity and exact authored transform
  │   └─ RegisterDefender + RegisterSpawnedGuard
  └─ Narrative handles all AI from here
```

## Faction Assignment

Guard faction is determined by precedence:

1. `SpawnPoint->FactionOverride` (if valid)
2. `TerritoryVolume->OwnershipData.OwningFaction`

Faction is set via `FNPCSpawnParams.bOverride_DefaultFactions` before `SetNPCDefinition`, so Narrative's initialization reads the correct faction. The NPCDefinition's default factions are overridden — the guard belongs ONLY to the territory owner.

Narrative's character subsystem owns physical creation, duplicate admission,
`CharacterMap`, `NPCMap`, controller creation, and teardown. A multi-slot garrison must use
a definition with **Allow Multiple Instances** enabled. Runtime and editor validation reject
an incompatible class, controller, auto-possession policy, or instance policy instead of
silently falling back to a different pawn class.

Definition appearance and equipment can finish asynchronously. Automatic main-hand wielding
waits for Narrative's visual/loadout initialization callback and the requested equipment slot's
weapon visual; unrelated cosmetic mesh/groom loads do not block it. This prevents an early wield
state from losing its animation overlay without treating slow cosmetic streaming as loadout failure.
An unarmed NPC definition is valid: after a bounded inventory admission window the guard stops
polling and continues its Territory activity without a weapon or a misleading warning.
Capsule and mesh initialization starts from valid named Unreal profiles before the exact
Narrative trace-channel overrides are applied at runtime.

## Patrol System

Territory spawn points can define patrol routes. These are consumed by Narrative's existing patrol activity:

1. Configure `PatrolRoute` waypoints on `ATerritoryGuardSpawnPoint`
2. In BP (BPA_Patrol or custom activity), call:
   - `GetPatrolRouteAsTransforms()` → populates `Goal_Patrol.PatrolPoints`
   - `GetPatrolWaitTimes()` → populates `S_PatrolPoint.WaitDuration`
3. Narrative's `BT_Patrol` + `BPA_Patrol` handles all movement

No custom Territory BT needed — thin adapter into Narrative's infrastructure.

The public patrol getters use the inline spawn-point route when it is non-empty, otherwise
they use the assigned `UTerritoryGuardPostDefinition` route and loop policy. Spawn-point
selection is deterministic: higher priority fills first, a patrol-capable post wins an
equal-priority tie over an intentional static post, and actor path is the final stable tie-break.
The editor validator rejects a misleading one-node data-asset route; use at least two nodes
or leave the route empty for an intentional static sentry.

## Reserve System

- Spawn-point ownership resolves by precedence: the territory's authored `GuardSpawnPoints` reference, `OwnerTerritoryTag`, then proximity
- Tag/proximity-resolved posts register into the same `ATerritoryVolume::GetGuardSpawnPoints()` union and therefore contribute capacity, reserves, save reconciliation, and counterattack defence; they are not a second authority
- Every placed post requires an editor-baked `SpawnPointGUID`. Runtime play logs an error and skips that post's Narrative save load when the GUID is invalid; it never invents a campaign identity at runtime
- Authored references support spawn points intentionally placed outside territory bounds
- each unique spawn point is exactly one active combat slot; the Territory's loaded
  capacity is `GetGuardSpawnPoints().Num()`
- legacy `MaxGuards`, Territory `MaxGuardCount`, and `GuardSpawnRadius` values are
  ignored. Add or remove spawn-point actors to change capacity
- normal recruitment never uses a random fallback or collision-driven relocation. A
  blocked authored point fails that slot and reports the transaction failure
- `ReserveSlots` = replacement entitlements for when active guards die
- Strategic counterattack defence counts no more reserve entitlements than the
  Territory's current `DesiredGuardCount`; target zero therefore has zero reserve
  defence even if posts retain saved replacement stock
- Initial population uses `HasAvailableSlot()` only — reserves not consumed
- When a guard dies: `UnregisterGuard()` queues one finite replacement only when active guards are below `DesiredGuardCount`
- Defender death delegates bind to Narrative's ASC on the server. If asynchronous NPC initialization has not exposed the ASC yet, the Territory retries every 0.25 seconds for up to 10 seconds; unregister/end-play cancels the retry and removes the exact delegate binding.
- Lowering the desired target cancels pending reserve deployments; a delayed reserve can never raise the live garrison above the new target
- `FTerritoryGarrisonSnapshot` replicates exact active, reserve, and pending-deployment counts without replicating live pawn pointers
- During an opposing `Contested` state, the incumbent may deploy only its already-authored
  finite pending reserves. `TerritorialInfluence` can reduce each deployment delay, including
  a timer restored from save, but never creates a reserve, permits player recruitment during
  contest, changes ownership, or bypasses physical Narrative NPC spawning.

## Player-managed staffing

`GuardSpawnCount` is authored initial staffing, not an unavoidable player expense. `PostCaptureGarrisonPolicy` controls a new owner's target:

| Policy | Result |
|---|---|
| `PlayerChooses` (default) | A capture owned by a live Narrative player faction starts at target 0; AI/global script capture uses `GuardSpawnCount` |
| `ConfiguredForEveryOwner` | Every new owner uses `GuardSpawnCount` (legacy behavior) |
| `AlwaysUnstaffed` | Every new owner starts at target 0 |

The explicit transition context is propagated through District → Property and District → City hierarchy cascades, so a player capture cannot be reclassified as a script capture by a child ownership callback.

The legacy `ForceCapture` Blueprint node now resolves a matching live player controller
through Narrative faction membership without calling `GetFirstPlayerController`. For a
quest or custom script that already has the exact instigator, prefer
`ForceCaptureWithContext`. Use `ConfiguredForEveryOwner` when an authored player faction
should deliberately receive the automatic target.

Use `TrySetDesiredGuardCount(Requester, NewTarget)` for an absolute target. Increasing it debits the requester's Narrative inventory using `GuardRecruitmentCost`; decreasing it reduces future upkeep and works even when assigned guards are dead. Multi-guard placement is all-or-nothing: incomplete placement removes the guards created by that request and refunds the debit.

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
4. Set `ReserveSlots`; place one spawn-point actor for every desired active guard slot
5. Tune reserve behavior: `ReserveSpawnDelay`, `ReserveSpawnRetryInterval`, `ReserveSpawnRadius`, `ReserveMinimumPlayerDistance`, `ReserveSpawnCandidateCount`
6. Assign the asset to one or more `ATerritoryGuardSpawnPoint::GuardPostDefinition` references

### Override Precedence

Per-spawn-point overrides take precedence over the data asset:

1. `SpawnPoint->NPCDefinitionOverride` (if set, overrides `GuardPostDefinition->NPCDefinition`)
2. `SpawnPoint->ActivityConfigurationOverride` (if set, overrides data asset)
3. `SpawnPoint->TriggerSetOverrides` (if non-empty, overrides data asset)
4. `SpawnPoint->FactionOverride` (if valid, overrides `GuardPostDefinition->FactionOverride`)

If no `GuardPostDefinition` is assigned, the territory's `GuardNPCDefinition` and `FactionGuardDefinitions` are used as fallback.

### Placement and patrol overlap ownership

Guard-post ownership resolves in this order: a Territory's typed `GuardSpawnPoints` reference,
the post's explicit `OwnerTerritoryTag`, then spatial placement/patrol overlap. For an untagged
post, both the actor origin and every world-space patrol node are queried. The most specific hit
wins (`Property > District > City`, then smaller bounds and stable tag order). This means a post
placed just outside a District can still become one of its physical guard slots when its patrol
route enters that District. The editor validator uses the same overlap rule and no longer reports
such a post as orphaned.

### Staged combat placement

The spawn-point marker represents the guard's foot position and facing. Normal initial,
player-purchased, restored, and manually deployed guards preserve the authored X/Y exactly.
Only vertical grounding is automatic. Place markers on navigation ground with enough capsule
clearance; a blocked marker does not silently move the NPC elsewhere. Automatic reserve
deployment with camera avoidance is the sole path allowed to select a nearby concealed point.

## Capture Flow

1. Kill all defenders → `OnAllGuardsDefeated` → defender count becomes zero; the incumbent owner remains authoritative but the territory is vulnerable
	- **Note:** checks ALL `RegisteredDefenders` (guards + any non-guard defenders registered via `RegisterDefender`), not just `SpawnedGuards`
	- A BP override may call Super for the native zero-defence update; it must never clear ownership as a casualty side effect
	- `Claimed -> Contested` preserves every surviving incumbent defender. Capture pressure
	  never despawns the garrison, and `Contested -> Claimed` never creates free replacement
	  guards. Only a verified Narrative death consumes an active guard and may queue a finite
	  reserve deployment.
2. Designer triggers capture via:
   - `RegisterAttacker(Territory, Actor, Faction)` — progressive capture (identity-based, TSet per faction)
   - `ForceCapture(Territory, Faction)` → bool — authority-only instant capture; validates inputs, bypasses gameplay capture rules, sets progress to 1.0 and state to Claimed. Returns true if territory actually changed.
   - `TerritoryCaptureEvent` — from quest/dialogue (server-authoritative, skips on client)
3. On capture → `ApplyTerritoryMutation` atomically commits Claimed ownership and resolves the post-capture target from the explicit context. With the default policy, player captures spawn zero friendlies; the player staffs District/Property garrisons from the command UI.

On save, each guard post records reserves, pending deployments, and its finite active count. Load recreates only the saved survivors (or uses the bounded legacy aggregate defender count for older saves); pawn health/activity and live pointers are intentionally not persisted.

## Debug

Enable in Project Settings → Territory Framework:
- `bDrawTerritoryBounds` — territory bounds
- `bDrawOwnershipOverlay` — green overlay for owned territories
- `bDrawCaptureProgress` — capture progress bar
- `bDrawGuardSpawnPoints` — spawn points and patrol routes
