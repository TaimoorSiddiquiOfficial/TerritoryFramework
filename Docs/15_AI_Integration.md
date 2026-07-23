# Territory AI Integration — Complete Setup Guide

> **Version:** v0.2.1 (2026-07-23)
> **Depends on:** NarrativePro 2.3.3
> **All assets in `/Game/TerritoryFramework/`** — zero NarrativePro content modified

---

## Architecture Overview

```
NPCDefinition (NPC_TerritoryBandit)
  ├─ NPCClassPath: BP_TerritoryGuard (ATerritoryGuardCharacter)
  ├─ ActivityConfiguration: AC_TerritoryGuard (NPCActivityConfiguration)
  │    └─ DefaultActivities (12):
  │         ├── BPA_Attack_Melee          (NarrativePro)
  │         ├── BPA_Attack_Ranged_Strafe  (NarrativePro)
  │         ├── BPA_Attack_Investigate    (NarrativePro)
  │         ├── BPA_Attack_Grenade        (NarrativePro)
  │         ├── BPA_FollowCharacter       (NarrativePro)
  │         ├── BPA_Patrol                (NarrativePro — uses BT_Patrol)
  │         ├── BPA_Interact              (NarrativePro)
  │         ├── BPA_Idle                  (NarrativePro)
  │         ├── BPA_MoveToDestination     (NarrativePro)
  │         ├── BPA_ReturnToSpawn         (NarrativePro)
  │         ├── BPA_DriveToDestination    (NarrativePro)
  │         └── BPA_ReturnToTerritory     (PROJECT — territory-specific)
  └─ TriggerSets: Triggers_Bandit
       └─ BPT_TimeOfDayRange (trigger condition)
            └─ NarrativeEvent_AddGoalToNPC
                 └─ GoalToAdd: Goal_TerritoryPatrol_C (PROJECT)
                      ├─ score: 1.0
                      └─ owned_tags: Narrative.State.Movement.Walking
```

### Decision Flow at Runtime

```
1. Trigger fires → adds Goal_TerritoryPatrol to NPC
2. Activity system rescores every 0.5s (rescore_interval)
3. Each BPA scores against the goal:
   - Attack goals → BPA_Attack_* scores high (1.0+)
   - Patrol goals → BPA_Patrol or BPA_ReturnToTerritory scores low (0.01)
   - No goal → BPA_Idle scores lowest
4. Highest-scoring activity wins → its BT runs
5. For territory guards: BPA_ReturnToTerritory reads TerritoryHomeTransform
```

---

## Asset Reference

### NPC Definition

| Asset | Path | Type | Notes |
|---|---|---|---|
| **NPC_TerritoryBandit** | `/Game/TerritoryFramework/NPC_TerritoryBandit` | NPCDefinition | Bandit NPC. References AC_TerritoryGuard + Triggers_Bandit |

### Character Blueprints

| Asset | Path | Parent Class | Notes |
|---|---|---|---|
| **BP_TerritoryGuard** | `/Game/TerritoryFramework/BP_TerritoryGuard` | `ATerritoryGuardCharacter` (C++) | Guard character. Has `TerritoryHomeTransform`, `OwningTerritory` |
| **BP_TerritoryGuardSpawnPoint** | `/Game/TerritoryFramework/BP_TerritoryGuardSpawnPoint` | `ATerritoryGuardSpawnPoint` (C++) | Spawn point with patrol route data |

### Activity Configuration

| Asset | Path | Type | Notes |
|---|---|---|---|
| **AC_TerritoryGuard** | `/Game/TerritoryFramework/AC_TerritoryGuard` | NPCActivityConfiguration | Duplicated from AC_RunAndGun. 12 activities including BPA_ReturnToTerritory |

### Activities (BPA)

| Asset | Path | Source | BT Used | Notes |
|---|---|---|---|---|
| **BPA_ReturnToTerritory** | `/Game/TerritoryFramework/BPA_ReturnToTerritory` | Project | BT_ReturnToSpawn | Territory-specific. SetupBlackboard reads `TerritoryHomeTransform` |
| BPA_Patrol | NarrativePro | NarrativePro | BT_Patrol | Generic patrol. Uses `SpawnTransform` as home |
| BPA_ReturnToSpawn | NarrativePro | NarrativePro | BT_ReturnToSpawn | Generic return. Requires `OwningSpawn` (NarrativeSpawnComponent) |

### Behavior Trees

| Asset | Path | Source | Status |
|---|---|---|---|
| **BT_TerritoryPatrol** | `/Game/TerritoryFramework/BT_TerritoryPatrol` | Duplicated from NarrativePro | ✅ Fixed — uses `BTT_SetNextPatrolPoint_C` |
| BT_Patrol | NarrativePro | NarrativePro | ✅ Working (stock) |

### Goals

| Asset | Path | Source | Notes |
|---|---|---|---|
| **Goal_TerritoryPatrol** | `/Game/TerritoryFramework/Goal_TerritoryPatrol` | Duplicated from Goal_Patrol | Territory patrol goal. Used by Triggers_Bandit |

### Trigger Sets

| Asset | Path | Notes |
|---|---|---|
| **Triggers_Bandit** | `/Game/TerritoryFramework/Blueprints/Triggers_Bandit` | Time-of-day trigger → adds Goal_TerritoryPatrol |

### Hierarchy Blueprints

| Asset | Path | Parent Class |
|---|---|---|
| **BP_TerritoryVolume** | `/Game/TerritoryFramework/Core/BP_TerritoryVolume` | `ATerritoryVolume` |
| **BP_City_HavenReach** | `/Game/TerritoryFramework/BP_City_HavenReach` | `ATerritoryCity` |
| **BP_District_CastleHill** | `/Game/TerritoryFramework/BP_District_CastleHill` | `ATerritoryDistrict` |
| **BP_District_MarketSquare** | `/Game/TerritoryFramework/BP_District_MarketSquare` | `ATerritoryDistrict` |
| **BP_Property_Blacksmith** | `/Game/TerritoryFramework/BP_Property_Blacksmith` | `ATerritoryProperty` |

### Core / UI

| Asset | Path | Type |
|---|---|---|
| **BP_HopTerritoryGameMode** | `/Game/TerritoryFramework/Core/BP_HopTerritoryGameMode` | GameMode |
| **BP_HopTerritoryPlayerController** | `/Game/TerritoryFramework/Core/BP_HopTerritoryPlayerController` | PlayerController |
| **BP_TerritoryDebugWidget** | `/Game/TerritoryFramework/UI/BP_TerritoryDebugWidget` | UMG Widget |

---

## Setup Logic — How It All Connects

### 1. Guard Spawning (C++ → BP)

```
ATerritoryVolume::SpawnGuards()                          [C++]
  ├─ ResolveGuardDefinition(OwnerFaction)                → UNPCDefinition*
  │    └─ Checks FactionGuardDefinitions array first
  │    └─ Falls back to GuardNPCDefinition property
  ├─ GetGuardSpawnPoints()                               → sorted by Priority
  ├─ For each GuardSpawnCount:
  │    ├─ BeginDeferredActorSpawnFromClass(NPCClass)
  │    ├─ Guard->ConfigureTerritorySpawn(                [C++ — single entrypoint]
  │    │      Definition,                                → UNPCDefinition
  │    │      OwnerFaction,                              → exact faction (bOverride_DefaultFactions)
  │    │      TerritoryGUID,                             → territory identity
  │    │      GuardSaveGUID,                             → unique save GUID per guard
  │    │      SpawnTransform,                            → CRITICAL for BPA_ReturnToSpawn
  │    │      SpawnPointName)                            → optional spawn point name
  │    ├─ Guard->OwningTerritory = this                 → territory back-reference
  │    ├─ Guard->OwningTerritorySpawnPoint = SP          → spawn point back-reference
  │    ├─ FinishSpawningActor()                          → triggers BeginPlay
  │    ├─ RegisterDefender(Guard)                        → adds to defender list
  │    └─ SP->RegisterSpawnedGuard(Guard)                → spawn point tracking
  └─ Narrative handles all AI from here
```

**Key:** `ConfigureTerritorySpawn` sets `SpawnInfo.SpawnTransform` — this is what `BPA_ReturnToSpawn` and `BPA_ReturnToTerritory` read as the "home" position. Without this, guards don't know where to return.

### 2. Territory Home Transform (BP_TerritoryGuard)

```
ATerritoryGuardCharacter (C++ base class)
  ├─ TerritoryHomeTransform: FTransform                  → set by ConfigureTerritorySpawn
  ├─ OwningTerritory: TWeakObjectPtr<ATerritoryVolume>  → territory back-reference
  └─ OwningTerritorySpawnPoint: TWeakObjectPtr          → spawn point back-reference

BP_TerritoryGuard (Blueprint)
  ├─ Inherits TerritoryHomeTransform from C++ parent
  ├─ GetActorGUID override → returns SpawnAssignedSaveGUID (prevents crash)
  └─ ShouldRespawn → returns false (prevents stale guard restoration on load)
```

### 3. Activity Scoring & Selection

```
Every 0.5s (rescore_interval in AC_TerritoryGuard):

1. GoalContainer holds active goals (added by triggers)
2. Each BPA in DefaultActivities calls ScoreActivity(GoalContainer):
   - BPA_Attack_* → scores high for attack goals (1.0+)
   - BPA_Patrol → scores 0.01 (low fallback) if not in dialogue
   - BPA_ReturnToTerritory → scores 0.01 (low fallback) if:
     * NOT tagged Narrative.State.DontReturnToSpawn
     * NOT tagged Narrative.State.DialogueControlled
     * NPC is a BP_TerritoryGuard with valid OwningTerritory [PENDING FIX]
   - BPA_Idle → scores lowest (0.001)

3. Highest score wins → that BPA's BT runs
```

### 4. BPA_ReturnToTerritory — SetupBlackboard Flow

```
SetupBlackboard(BB: BlackboardComponent) → bool:
  1. Get OwnerController → GetControlledNPC → NarrativeNPCCharacter
  2. Cast To BP_TerritoryGuard                    ← territory-specific gate
  3. Get TerritoryHomeTransform                   ← from C++ parent
  4. Break Transform → Location + Rotation
  5. BB->SetValueAsVector("TargetLocation", Location)
  6. BB->SetValueAsRotator("TargetRotation", Rotation)
  7. Return true

If Cast fails (not a territory guard):
  → Execution stops at CastFailed pin [PENDING FIX — pin unconnected]
  → Function doesn't reach Return Node
  → Activity system treats as setup failure
```

### 5. BT Execution (Patrol)

```
BT_TerritoryPatrol / BT_Patrol (identical structure):
  ROOT
  └─ Sequence (with ClearAIFocus service)
      └─ Sequence (with Blackboard decorator: TargetLocation exists)
          ├─ MoveTo (TargetLocation from blackboard)
          ├─ Sequence (with Blackboard decorator: TargetRotation exists)
          │   └─ RotateToGoal (face the target direction)
          ├─ WaitBlackboardTime (pause at waypoint)
          └─ SetNextPatrolPoint                    ← advances to next patrol node
```

### 6. Patrol Route Data Flow

```
ATerritoryGuardSpawnPoint (placed in level)
  ├─ PatrolRoute: TArray<FTerritoryPatrolNode>     → waypoint data
  │    └─ Each node: Location, Rotation, WaitTime, ActivityTag
  ├─ bLoopPatrol: bool                              → loop back to start
  └─ GetPatrolRouteAsTransforms() → TArray<FTransform>
  └─ GetPatrolWaitTimes() → TArray<float>

Goal_TerritoryPatrol (Goal item)
  └─ PatrolPoints: TArray<S_PatrolPoint>            → populated from spawn point data

BPA_Patrol / BPA_ReturnToTerritory
  └─ Reads patrol points from goal
  └─ Sets blackboard keys for each waypoint
  └─ BT executes MoveTo → RotateToGoal → Wait → SetNextPatrolPoint cycle
```

### 7. Territory Capture → Guard Respawn

```
Territory captured (SetOwningFaction called):
  1. DespawnGuards()                                → destroys old faction guards
  2. OwnershipData.OwningFaction = NewOwner
  3. OwnershipData.State = Claimed
  4. SpawnGuards()                                  → spawns new faction guards
     └─ ResolveGuardDefinition(NewOwner)            → may use different NPC definition
     └─ ConfigureTerritorySpawn with new faction    → new TerritoryHomeTransform
     └─ Narrative AI takes over with new faction context
```

---

## Combat Director — Strategic Assault Budget

The `UTerritoryCombatDirector` is a `UWorldSubsystem` that limits how many AI can simultaneously attack within a single territory. This is **separate** from NarrativePro's per-target attack tokens (`UNarrativeAbilitySystemComponent::TryClaimToken`).

| System | Scope | What It Limits |
|---|---|---|
| **Narrative Tokens** | Tactical (per-target) | How many AI gang up on ONE defender |
| **Assault Slots** | Strategic (per-territory) | How many AI participate in a territory assault |

AI should use **both**: `RequestAssaultSlot` (strategic gate) → `RequestAttackToken` (tactical).

### Slot Budget

Each `ATerritoryVolume` has `MaxConcurrentAttackers` (default 3). The CombatDirector enforces:

```
If GrantedSlots(Territory) >= MaxConcurrentAttackers → deny new slot
```

### Internal Data Structure

```
UTerritoryCombatDirector (UWorldSubsystem)
  ├─ SlotMap: TMap<TWeakObjectPtr<ATerritoryVolume>, FPerTerritorySlots>
  │    └─ FPerTerritorySlots
  │         └─ GrantedControllers: TArray<TWeakObjectPtr<ANarrativeNPCController>>
  └─ BoundControllers: TSet<TWeakObjectPtr<ANarrativeNPCController>>
       └─ Tracks which controllers have ASC OnDied bound
```

### API Reference

#### Actions (BlueprintAuthorityOnly)

| Function | Parameters | Returns | Behavior |
|---|---|---|---|
| `RequestAssaultSlot` | `Territory, NPCController` | `bool` | Grants slot if budget available and territory not Locked. Binds ASC OnDied for auto-release. Runs stale cleanup. |
| `ReleaseAssaultSlot` | `Territory, NPCController` | `void` | Releases one slot in a specific territory. |
| `ReleaseAllSlots` | `NPCController` | `void` | Releases all slots across ALL territories for this controller. |

#### Queries (BlueprintPure)

| Function | Returns | Notes |
|---|---|---|
| `HasAssaultSlot(Territory, Controller)` | `bool` | Does this controller hold a slot in this territory? |
| `GetGrantedSlots(Territory)` | `int32` | Active slots — filters dead controllers (weak ptr check) |
| `GetAvailableSlots(Territory)` | `int32` | `MaxConcurrentAttackers - GetGrantedSlots` |

### RequestAssaultSlot — Full Flow

```
RequestAssaultSlot(Territory, Controller):
  1. Null checks (Territory, Controller) → false if null
  2. Lock check → false if TerritoryState == Locked
  3. CleanupStaleTerritoryKeys() → remove destroyed territory entries from SlotMap
  4. FindOrAdd territory in SlotMap
  5. CleanupInvalidControllers() → remove dead controller weak pointers
  6. Budget check → false if GrantedControllers.Num() >= MaxConcurrentAttackers
  7. Duplicate check → true if controller already has a slot (idempotent)
  8. Grant slot → add controller to GrantedControllers
  9. BindControllerDeath(Controller) → bind ASC OnDied for auto-release
  10. Return true
```

### Automatic Slot Release on NPC Death

When a slot is granted, the CombatDirector binds to the controller's `UNarrativeAbilitySystemComponent::OnDied` delegate:

```
BindControllerDeath(Controller):
  1. Check if already bound (BoundControllers set) → skip if duplicate
  2. Get ASC via IAbilitySystemInterface
  3. ASC->OnDied.AddUniqueDynamic(this, OnAssaultControllerDied)
  4. Add to BoundControllers set

OnAssaultControllerDied(KilledActor, KilledASC):
  1. Resolve controller from killed pawn (or direct cast)
  2. ReleaseAllSlots(DeadController) → frees all territory slots
  3. Remove from BoundControllers set
```

This ensures slots are freed even if the NPC dies mid-assault and the BT never reaches `BTTask_ReleaseTerritoryPermission`.

### Stale Entry Cleanup

Two cleanup mechanisms prevent memory/budget leaks:

| Cleanup | When | What It Removes |
|---|---|---|
| `CleanupInvalidControllers` | Every `RequestAssaultSlot` call | Dead controller weak pointers (NPC killed, despawned) |
| `CleanupStaleTerritoryKeys` | Every `RequestAssaultSlot` call | Destroyed territory entries from SlotMap (level streaming, destruction) |

### BT Task Integration

Two BT tasks gate attack behavior through the CombatDirector:

#### BTTask_RequestTerritoryPermission

Requests an assault slot before allowing attack actions. Place this **before** attack subtrees in guard/enemy BTs.

| Property | Type | Purpose |
|---|---|---|
| `TerritoryKey` | BlackboardKeySelector (Object) | Target territory. Falls back to spatial lookup at NPC location if not set. |
| `bPermissionGrantedKey` | BlackboardKeySelector (Bool) | Output: true if granted, false if denied. **Required** — task fails if not configured. |

**ExecuteTask flow:**
```
1. Get AIController → cast to NarrativeNPCController
2. Get CombatDirector subsystem
3. Validate bPermissionGrantedKey is configured (fails if NAME_None)
4. Get territory from TerritoryKey BB key, or:
   └─ Fallback: Registry->GetTerritoryAtLocation(Pawn location)
   └─ If no territory found → wilderness (no restriction) → Succeeded + Granted=true
5. Director->RequestAssaultSlot(Territory, NPCController)
6. Write result to bPermissionGrantedKey
7. Return Succeeded (granted) or Failed (denied)
```

**Note:** Does NOT auto-release on task end. NPC may continue attacking across multiple BT ticks. Release is handled by `BTTask_ReleaseTerritoryPermission`.

#### BTTask_ReleaseTerritoryPermission

Releases assault slot(s) for an NPC. Place this **after** attack subtrees or in cleanup paths.

| Property | Type | Purpose |
|---|---|---|
| `TerritoryKey` | BlackboardKeySelector (Object) | Target territory for targeted release. If not configured, releases ALL slots. |

**ExecuteTask flow:**
```
1. Get AIController → cast to NarrativeNPCController
2. Get CombatDirector subsystem
3. If TerritoryKey is configured and resolves:
   └─ Director->ReleaseAssaultSlot(Territory, NPCController) → targeted release
4. Else (no key configured):
   └─ Director->ReleaseAllSlots(NPCController) → releases across all territories
5. Always returns Succeeded
```

#### Recommended BT Placement

```
Guard/Enemy Behavior Tree:
  ROOT
  └─ Sequence
      ├─ BTTask_RequestTerritoryPermission    ← GATE: must pass before attacking
      │    └─ TerritoryKey: CurrentTerritory (from perception/BB)
      │    └─ bPermissionGrantedKey: HasPermission
      ├─ [Attack Subtree]                      ← only runs if permission granted
      │    └─ BT_Attack_Melee / BT_Attack_Ranged / etc.
      └─ BTTask_ReleaseTerritoryPermission    ← CLEANUP: release after attack
           └─ TerritoryKey: CurrentTerritory
```

### Integration with ControlSubsystem

The CombatDirector is a **standalone strategic gate**. The capture system (`UTerritoryControlSubsystem`) uses its own identity-based attacker tracking. They are independent:

| System | What It Tracks | When Used |
|---|---|---|
| CombatDirector | Assault slots per territory per controller | AI deciding whether to attack |
| ControlSubsystem | Attackers per territory per faction (TSet) | Capture progress calculation |

An NPC needs a CombatDirector slot to **initiate** an attack. Once attacking inside the territory, the ControlSubsystem's `RegisterAttacker` tracks them for capture progress.

---

## Pending Fixes (Editor Required)

### BPA_ReturnToTerritory ScoreActivity — Add Territory Check

**Current:** Returns 0.01 for all NPCs after tag checks, even non-territory NPCs.

**Fix needed (in UE editor):**

After the two tag checks, add these nodes:

```
GetControlledNPC → Cast To ATerritoryGuardCharacter
  → Get OwningTerritory → Is Valid?
    → TRUE: continue to Return Node (score = 0.01)
    → FALSE: Return Node (score = -999.0)
```

Also connect the `CastFailed` pin in SetupBlackboard to a Return Node (ReturnValue = false).

### BTTask_RequestTerritoryPermission — Not in Any BT

The C++ BT tasks `BTTask_RequestTerritoryPermission` and `BTTask_ReleaseTerritoryPermission` exist but are not used in any behavior tree. These should be added to guard BTs to enforce the CombatDirector assault slot budget before attack actions.

---

## Creating a New Territory NPC

Step-by-step for adding a new NPC type (e.g., `NPC_TerritoryMerchant`):

1. **Duplicate NPC_TerritoryBandit** → `NPC_TerritoryMerchant`
2. **Set NPCClassPath** → `BP_TerritoryGuard` (or create a new guard BP)
3. **Set ActivityConfiguration** → `AC_TerritoryGuard` (or duplicate for custom activities)
4. **Set TriggerSets** → create or reference a TriggerSet with goals
5. **Set DefaultFactions** → e.g., `Narrative.Factions.Merchants`
6. **Place ATerritoryVolume** in level → assign `FactionGuardDefinitions`:
   - Faction: `Narrative.Factions.Merchants`
   - NPCDefinition: `NPC_TerritoryMerchant`

The territory will spawn merchant guards when owned by the Merchants faction.

---

## Key C++ Entry Points

### Territory / Guard Spawning

| Function | Class | Purpose |
|---|---|---|
| `ConfigureTerritorySpawn()` | `ATerritoryGuardCharacter` | Single entrypoint for guard config. Sets SpawnInfo, faction, transforms |
| `SpawnGuards()` | `ATerritoryVolume` | Spawns all guards for current owner |
| `SpawnSingleGuard()` | `ATerritoryVolume` | Reserve replacement (one-for-one) |
| `ResolveGuardDefinition()` | `ATerritoryVolume` | Picks NPCDefinition per faction (FactionGuardDefinitions first, then default) |
| `RegisterDefender()` | `ATerritoryVolume` | Adds actor to defender list + binds ASC OnDied |
| `UnregisterDefender()` | `ATerritoryVolume` | Removes from defender list + unbinds death delegate |
| `GetPatrolRouteAsTransforms()` | `ATerritoryGuardSpawnPoint` | Bridge patrol data to Narrative goals |
| `GetPatrolWaitTimes()` | `ATerritoryGuardSpawnPoint` | Parallel array of wait durations |

### Combat Director

| Function | Class | Purpose |
|---|---|---|
| `RequestAssaultSlot()` | `UTerritoryCombatDirector` | Strategic attack budget gate. Cleans stale entries, binds death hook |
| `ReleaseAssaultSlot()` | `UTerritoryCombatDirector` | Release one slot in a specific territory |
| `ReleaseAllSlots()` | `UTerritoryCombatDirector` | Release all slots across all territories for a controller |
| `HasAssaultSlot()` | `UTerritoryCombatDirector` | Query: does controller hold a slot? |
| `GetGrantedSlots()` | `UTerritoryCombatDirector` | Active slots (filters dead controllers) |
| `GetAvailableSlots()` | `UTerritoryCombatDirector` | MaxSlots - GrantedSlots |
| `BindControllerDeath()` | `UTerritoryCombatDirector` | Bind ASC OnDied for auto-release on NPC death |
| `OnAssaultControllerDied()` | `UTerritoryCombatDirector` | Death handler: releases all slots, cleans binding |
| `CleanupInvalidControllers()` | `UTerritoryCombatDirector` | Remove dead weak pointers per territory |
| `CleanupStaleTerritoryKeys()` | `UTerritoryCombatDirector` | Remove destroyed territory entries from SlotMap |

### BT Tasks

| Class | Purpose | BB Keys | Returns |
|---|---|---|---|
| `BTTask_RequestTerritoryPermission` | Gate before attack subtrees | TerritoryKey (Object, optional), bPermissionGrantedKey (Bool, required) | Succeeded (granted) / Failed (denied) |
| `BTTask_ReleaseTerritoryPermission` | Cleanup after attack subtrees | TerritoryKey (Object, optional — targeted or all) | Always Succeeded |

---

## NarrativePro Integration Points

| TerritoryFramework | NarrativePro | Connection |
|---|---|---|
| `ATerritoryGuardCharacter` | `ANarrativeNPCCharacter` | Inherits. `GetActorGUID` override prevents crash |
| `ConfigureTerritorySpawn` | `FNPCSpawnParams` | Sets `bOverride_DefaultFactions = true` |
| `TerritoryHomeTransform` | `SpawnInfo.SpawnTransform` | Same transform — used by BPA_ReturnToSpawn |
| `BPA_ReturnToTerritory` | `NPCActivity` | Scores goals, sets up blackboard |
| `Goal_TerritoryPatrol` | `NPCGoalItem` | Patrol goal with score and tags |
| `AC_TerritoryGuard` | `NPCActivityConfiguration` | Lists available activities |
| `Triggers_Bandit` | `TriggerSet` | Time-based goal injection |
| `BT_TerritoryPatrol` | `BehaviorTree` | Uses NarrativePro BT tasks |
| Guard spawn points | `UNPCSpawnComponent` | Alternative path — territory uses its own spawn system |
