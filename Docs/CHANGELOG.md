# Changelog

## v0.2.1 — 2026-07-23 (Session 2)

Additional 22 fixes from deep re-audit session 2. All P1 and P2 findings resolved.

### Diplomacy (P0)
- **Inbound bridge fixed**: `OnFactionAttitudeChanged` no longer collapses TradeAgreement/NonAggression/Ceasefire to Alliance when external Friendly attitude arrives. Rich treaties are preserved. Hostile always overrides.
- **Reentrancy guard**: `bSuppressSync` RAII guard prevents recursive mutation during diplomacy broadcasts
- **const_cast removed**: Uses non-const `FindTreaty` overload instead of `const_cast`

### Save/Load (P0/P1)
- **SavableData gold fixed**: `LoadFromSelf` uses `SetFactionTreasury` (exact restore) instead of `AddToTreasury` (additive, caused double gold with WorldState)
- **PIE duplicate guard**: `PostDuplicate` only regenerates GUID for editor duplication, not PIE world creation

### Economy (P0/P1/P2)
- **Null-guard GetWorld**: EconomySubsystem `Initialize`/`Deinitialize` check `GetWorld()` before dereferencing
- **Deferred RecalculateIncome**: Ownership changes mark factions dirty, actual recalculation runs once per economy tick (was O(3N) per capture cascade)
- **Delegate cleanup**: EconomySubsystem unbinds per-territory `OnTerritoryOwnershipChanged` delegates on `Deinitialize`
- **Ledger trim moved**: `TransactionLedger` trimming runs once after all factions processed, not per-faction

### Capture (P1)
- **ForceCapture state**: Explicitly sets state to Claimed after `SetOwningFaction` (was stuck Contested)
- **ContestingFaction tracks leader**: Updated by `EvaluateCaptureState` based on highest progress, not last-registered attacker
- **EvaluateCaptureState safety**: Re-fetches State pointer after cleanup to handle potential reentrancy

### Guards (P1)
- **RegisteredDefenders check**: `OnAllGuardsDefeated` checks ALL `RegisteredDefenders` (includes non-guard defenders), not just `SpawnedGuards`
- **CombatDirector death hook**: Binds ASC `OnDied` when granting assault slots, auto-releases on NPC death
- **Stale slot counts**: `GetGrantedSlots` filters dead weak pointers
- **Stale map keys**: `CleanupStaleTerritoryKeys` removes dead territory entries from SlotMap

### Hierarchy (P1/P2)
- **Empty district protected**: `AllPropertiesOwnedBy` returns false for empty property list (was trivially true)
- **Contested clears owner**: `SetTerritoryState(Contested)` clears `OwningFaction` so `IsOwnedByFaction` and `GetOwningFaction` agree
- **Cascade single event**: `CascadeCaptureToProperties` no longer double-fires `OnPropertyCaptured`
- **Property upgrade reset**: Uses `SetUpgradeLevel(0)` instead of direct assignment (triggers income recalc)

### Combat (P1/P2)
- **BTTask_Release targeted**: Reads `TerritoryKey` from blackboard for per-territory slot release, fallback to `ReleaseAllSlots`
- **BTTask_Request validates**: Fails on unconfigured `bPermissionGrantedKey` instead of silent success

### Tales (P1)
- **Server authority**: `TerritoryCaptureEvent`, `TerritoryLockEvent`, `TerritoryUnlockEvent` skip on `NM_Client`
- **Capture fallback**: Logs warning when ControlSubsystem is unavailable instead of silent direct `SetOwningFaction`

### Registry (P1)
- **Client timer removed**: `PollBoundsChanges` timer only starts on server (`NM_Client` guard added)

### UI (P2)
- **DebugWidget throttled**: Rebuilds every 0.5s instead of every frame; caches subsystem pointers
- **Switch default**: `ETerritoryState` switch adds `default: "Unknown"` case

### Navigation (P2)
- **MapMarker outline**: Uses `BoxCenter` as reference (handles offset BoxComponents correctly)
- **Double bind removed**: `TerritoryNavigationMarkerComponent` no longer binds delegates that the marker already binds

### Validator (P2)
- **WorldState/SavableData**: Individual asset validation now checks GUID validity

### Build (P2)
- **Private dependencies**: `NarrativeArsenal`, `NarrativeSaveSystem`, `DeveloperSettings` moved from Public to Private

### Documentation
- **Blueprint Extension Guide**: Complete rewrite with Super-call quick reference table, all delegates with fire context, interface reference, state model diagram, and common patterns

---

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
