# Changelog

## v0.2.0 — 2026-07-23

Comprehensive audit-driven stabilization release. 32+ fixes across capture, guards, hierarchy, economy, diplomacy, replication, and Narrative Pro integration.

### Breaking Changes
- `OnGuardDied` delegate renamed to `OnGuardKilled` with new signature `(Territory, Guard, Killer, RemainingDefenders)`
- `OnTerritoryControlChanged` delegate renamed to `OnTerritoryOwnershipChanged`
- `OnTerritoryStateChanged` delegate renamed to `OnTerritoryStateChangedDelegate`
- `RequestAttackPermission` → `RequestAssaultSlot` on TerritoryCombatDirector
- `ReleaseAttackPermission` → `ReleaseAssaultSlot`
- `HasAttackPermission` → `HasAssaultSlot`
- `GetGrantedPermissions` → `GetGrantedSlots`
- Removed `GuardBehaviorTree` and `GuardBlackboardAsset` properties from TerritoryVolume
- Removed custom BT tasks: `BTTask_MoveToPatrolNode`, `BTService_UpdatePatrolRoute`, `BTTask_SetPatrolPoint`
- `SetTerritorySaveGUID` and `SetOwningTerritoryGUID` replaced by `ConfigureTerritorySpawn()`
- District capture policy changed from mixed majority+unanimity to **unanimity only**
- `AutoMapMarkerComponent` replaced by proper `MapMarkerComponent` default subobject

### Capture System
- **Capture flow fixed**: `RegisterAttacker` → progress accumulates → `CompleteCapture` → ownership transfer
- **Deferred command list**: evaluate-then-apply pattern prevents TMap mutation during iteration
- **Identity-based attackers**: `TSet<TWeakObjectPtr<AActor>>` per faction — no count inflation
- **Deterministic tie-breaking**: progress → attacker count → lexicographic tag
- **ContestingFaction maintained**: set on register, cleared on reset/complete
- **AddCaptureProgress validates** via `AttemptCapture` before adding progress
- **Capture decay restores state**: decayed territories return to Claimed/Unclaimed, not stuck Contested
- **No auto-capture**: removed `PollPlayerPresence` — capture is designer-driven only
- **Late binding**: `TerritoryCaptureTask` subscribes to `OnTerritoryRegistered` for streaming recovery

### Guard Spawn Contract
- **`ConfigureTerritorySpawn()`**: single entrypoint sets `FNPCSpawnParams` before `SetNPCDefinition`
  - `bOverride_DefaultFactions = true` → exact territory owner faction only
  - Optional activity configuration override
  - Optional trigger set overrides
- **Faction set before `FinishSpawningActor`** — Narrative's BeginPlay reads correct faction
- **`FactionOverride`** on spawn points now applied (precedence: SpawnPoint > Territory Owner)
- **`HasAvailableSlot()` only** for initial population — reserves not consumed as active capacity
- **`SpawnSingleGuard()`** — one-for-one reserve replacement, not full batch
- **`LoadSynchronous()`** for NPC class — no silent fallback to base C++ class
- **Late binding**: spawn points subscribe to `OnTerritoryRegistered` when initial resolution fails

### Hierarchy
- **Unanimity policy**: district captures ONLY when ALL properties owned by one faction
- **District goes Contested** when owner loses any property
- **City goes Contested** when not all districts owned by one faction
- **`SetOwningFaction` called** on district majority capture (was missing)
- **Property side effects** run on every ownership path via `OnOwnershipChanged_Implementation` override
- **Authority guards** on all hierarchy handlers — no client-side mutations

### Lock System
- **`LockConditions`** array (`UNarrativeCondition` instanced) for quest-gated unlocks
- **`LockTerritory`/`TryUnlock`/`CanUnlock`/`IsLocked`** Blueprint API
- **`LockReason`** in replicated `FTerritoryOwnershipData` — visible on clients and in saves
- **`bStartsLocked`** overrides initial owner — locked territories stay locked regardless
- **`TerritoryLockEvent`/`TerritoryUnlockEvent`** — drop into quest/dialogue nodes

### Replication & Save
- **`OnRep_OwnershipData`** diffs `PreviousOwningFaction` and `PreviousState` — no false events
- **Client BlueprintNativeEvent parity** — `OnOwnershipChanged`/`OnStateChanged` fire on clients
- **`GetDefenderCount`** returns replicated `OwnershipData.DefenderCount` — correct on clients
- **Property `BeginPlay`** preserves saved ownership — only syncs to district on first init
- **`NavigationMarkerComponent`** is proper `CreateDefaultSubobject` — visible in BP Components panel

### Economy
- **Leaf-only income**: only `ATerritoryProperty` contributes — no hierarchy double-counting
- **Sequential ledger**: income applied first, then upkeep — each with own running balance
- **Insufficient gold clamps** upkeep to available — logs "partial — insufficient gold"

### Authority Enforcement
- All diplomacy mutation APIs check `GetAuthGameMode()`: `DeclareWar`, `DeclarePeace`, `FormAlliance`, `BreakAlliance`, `SignTradeAgreement`, `AddReputation`, `SetReputation`
- All territory mutations check `HasAuthority()`: `SetOwningFaction`, `SetTerritoryState`, `LockTerritory`, `TryUnlock`

### Map Markers
- **Red/Yellow/Green** defaults: unclaimed=red, enemy=red, player=green (via FactionColorMap), contested=yellow
- **Locked = invisible** (zero alpha)
- **Map outline** uses Narrative's coordinate system — respects rotation, works at any zoom
- All colors `BlueprintReadWrite` — fully customizable

### Spatial Index
- **`GetTerritoryAtLocation`** returns smallest-volume territory (most specific)
- **Auto-update on move/resize**: 2s poll compares cached bounds, re-indexes on change
- **`UpdateTerritoryBounds()`** BlueprintCallable

### Patrol Data
- **`GetPatrolRouteAsTransforms()`** and **`GetPatrolWaitTimes()`** — bridge territory patrol data into Narrative's `Goal_Patrol.PatrolPoints` format
- `FactionOverride` actively applied during spawn
- Debug visualization unchanged

### Combat Director
- Renamed to clarify **strategic assault budget** vs Narrative's per-target attack tokens
- `RequestAssaultSlot`/`ReleaseAssaultSlot`/`HasAssaultSlot`/`GetGrantedSlots`
- Header documents the two-system approach

### Tales Integration
- **`TerritoryCaptureEvent`**: completes capture via `AddCaptureProgress(1.0)` when non-forced
- **`TerritoryLockEvent`/`TerritoryUnlockEvent`**: new event subclasses
- **`TerritoryCaptureTask`**: late binding via `OnTerritoryRegistered`
- **Validator warnings emitted**: `[WARNING]` prefix in Data Validation output

### NavMesh
- Guard spawn positions projected to NavMesh via `ProjectPointToNavigation`
- `GetRandomSpawnPoint` respects territory actor rotation

### Cleanup
- Removed `OnGuardTakeAnyDamage` — was breaking knockback, ragdoll, hit reactions
- Removed `PlayerFactionFallback` — auto-capture removed
- Removed `ATerritorySavableData` deprecation metadata (UE requires ADEPRECATED_ prefix)
- `GetActorLabel()` → `GetName()` in runtime logs
- Debug draw bounds respect rotation
- Plugin version: `0.2.0`

### Documentation
- New: `Docs/Blueprint_Setup_Tutorial.md` — complete step-by-step setup guide
