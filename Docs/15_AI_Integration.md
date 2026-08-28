# Territory AI Integration — Complete Setup Guide

> **Version:** v0.2.6 (re-audited 2026-08-24)
> **Depends on:** Narrative Pro 2.4.2
> **All assets in `/Game/TerritoryFramework/`** — zero NarrativePro content modified

> **Authority note:** Narrative `UNPCDefinition`, `FNPCSpawnInfo`, `UNPCActivityConfiguration`, `UNPCActivityComponent`, goals, TriggerSets, ASC/death delegates, and navigation remain the AI foundation. Territory patrol and counterattack intent are expressed as Narrative goals/activities. The Territory-named Behavior Tree assets documented below are existing compatibility/tactical content used by selected Narrative activities; they are not a second AI controller, scheduler, or capture system.

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

### Counterattack Activity Path

Counterattack NPCs use `ATerritoryAssaultCharacter`, which still derives through Narrative's character stack. `UTerritoryAssaultGoal` carries the durable assault/territory/faction intent, while `UTerritoryAssaultActivity` consumes that goal and routes the NPC toward the selected typed approach and target Territory. Physical creation goes through `UNarrativeCharacterSubsystem::SpawnNPC`, so Narrative's character/NPC maps and duplicate policy remain authoritative. The native pawn selects `ANarrativeNPCController` and auto-possession for placed or spawned actors. A profile NPC definition may resolve to a Blueprint subclass, but that subclass must preserve an `ANarrativeNPCController`-derived controller and spawned auto-possession; forces larger than one also require the definition to allow multiple instances. The participant waits for Narrative's asynchronous character load to finish, then adds the native assault activity when the assigned Narrative activity configuration does not already contain it. Invalid definition/controller/auto-possession contracts are rejected by planning preview, runtime scheduling/lifecycle checks, and TerritoryFramework data validation; no generic fallback pawn is spawned and no finite force is consumed.

Combat interrupts the assault through Narrative's highest-score selection. The Territory
movement goal scores `2`; Narrative Pro 2.4.2's attack generator scores `3` before optional
attack scoring. While a live registered hostile defender exists, the participant's target
policy temporarily suppresses attack goals aimed at non-defenders and leaves defender goals
at their exact Narrative-authored scores. This prevents a nearer player/EQS result from
skipping the assigned garrison. When the final defender is removed, the original goal scores
are restored and Narrative immediately reselects normally. The durable movement goal remains
until death, withdrawal, cancellation, or resolution. TerritoryFramework does not rely on
Narrative's currently unused `bIsInterruptable` flag.

### Decision Flow at Runtime

```
1. Trigger fires → adds Goal_TerritoryPatrol to NPC
2. Activity system rescores every 0.5s (rescore_interval)
3. Each BPA scores against the goal:
   - Attack goals → Narrative attack activities score from the generated attack goal (`3+`)
   - Territory assault movement → native durable goal scores `2`
   - Patrol/return goals → project-authored lower-priority movement scores
   - No goal → BPA_Idle scores lowest
4. During a physical assault, non-defender attack goals score `0` while a registered hostile
   defender remains; defender attack goals retain their Narrative scores
5. Highest-scoring activity wins → its BT runs
6. For territory guards: BPA_ReturnToTerritory reads TerritoryHomeTransform
```

---

## Asset Reference

### NPC Definition

| Asset | Path | Type | Notes |
|---|---|---|---|
| **NPC_TerritoryBandit** | `/Game/TerritoryFramework/AI/NPC_TerritoryBandit` | NPCDefinition | Territory guard definition; its class derives from `ATerritoryGuardCharacter` |
| **NPC_TerritoryBanditAssault** | `/Game/TerritoryFramework/AI/NPC_TerritoryBanditAssault` | NPCDefinition | Physical counterattack definition; class path is `ATerritoryAssaultCharacter` and shared Narrative combat/activity data may be reused |

Both Territory guard and assault native classes initialize their capsule and mesh from
valid `Pawn`/`CharacterMesh` profiles, then apply Narrative's weapon, projectile, cover,
traversal, climb, and interaction channel overrides after component registration. This
avoids treating Unreal's reserved `Custom` marker as a named collision profile.

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
| **Goal_TerritoryPatrol** | `/Game/TerritoryFramework/AI/Goal_TerritoryPatrol` | Duplicated from Goal_Patrol | Territory patrol goal. Used by Triggers_Bandit |

### Trigger Sets

| Asset | Path | Notes |
|---|---|---|
| **Triggers_Bandit** | `/Game/TerritoryFramework/AI/Triggers_Bandit` | Time-of-day trigger -> adds Goal_TerritoryPatrol. The project package redirect preserves the legacy `/Blueprints/` reference. |

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
  ├─ For each free unique spawn point, up to DesiredGuardCount:
  │    ├─ validate exact slot collision and Narrative definition contract
  │    ├─ Guard->SpawnThroughNarrative(                  [C++ — single authority adapter]
  │    │      Definition,                                → UNPCDefinition
  │    │      OwnerFaction,                              → exact faction (bOverride_DefaultFactions)
  │    │      TerritoryGUID,                             → territory identity
  │    │      GuardSaveGUID,                             → unique save GUID per guard
  │    │      SpawnTransform,                            → CRITICAL for BPA_ReturnToSpawn
  │    │      SpawnPointName)                            → optional spawn point name
  │    ├─ UNarrativeCharacterSubsystem::SpawnNPC()       → Narrative registry/controller owner
  │    ├─ scoped SetNPCDefinition override applies home/territory/spawn point context
  │    ├─ verify exact transform + Narrative controller/activity
  │    ├─ RegisterDefender(Guard)                        → adds to defender list
  │    └─ SP->RegisterSpawnedGuard(Guard)                → spawn point tracking
  └─ Narrative handles all AI from here
```

**Key:** each unique spawn-point actor is exactly one active combat slot. Its authored X/Y and facing are preserved; only Z is aligned to navigation ground and raised by the NPC capsule half-height. The resolved transform is collision-validated before Narrative spawning, stored in `SpawnInfo.SpawnTransform` and replicated `TerritoryHomeTransform`. Narrative's public spawn function may adjust a colliding actor, so TerritoryFramework verifies the finished transform and destroys any unexpectedly relocated guard. A blocked slot therefore fails instead of moving the guard away from the staged marker.

### 2. Territory Home Transform (BP_TerritoryGuard)

```
ATerritoryGuardCharacter (C++ base class)
  ├─ TerritoryHomeTransform: FTransform                 → replicated, set before Narrative definition assignment
  ├─ OwningTerritory: TObjectPtr<ATerritoryVolume>      → replicated territory back-reference
  └─ OwningTerritorySpawnPoint: TObjectPtr              → replicated spawn point back-reference

BP_TerritoryGuard (Blueprint)
  ├─ Inherits TerritoryHomeTransform from C++ parent
  ├─ Inherits stable GetActorGUID behavior from C++ parent
  └─ Inherits ShouldRespawn=false from C++ parent (prevents stale guard restoration on load)
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

### 7. Territory Capture → Player-Managed Garrison

```
Territory captured (SetOwningFaction called):
  1. DespawnGuards()                                → destroys old faction guards
  2. OwnershipData.OwningFaction = NewOwner
  3. OwnershipData.State = Claimed
  4. Resolve post-capture target from explicit transition context
     ├─ physical player + PlayerChooses             → target 0
     └─ AI/script or ConfiguredForEveryOwner        → authored GuardSpawnCount
  5. Reconcile exactly to the resolved target
     └─ if target > 0, Narrative NPC definition/activity/TriggerSets are applied
  6. Player may set District/Property target through the server-authoritative
     management component; an increase is debited and deployed atomically
```

City/District/Property cascades preserve the same explicit transition context. A player District capture therefore cannot cause a child Property to silently fall back to the authored three-guard target.

---

## Territory Guard Combat Gate

Narrative perception may see a player without authorizing combat. The final attitude is
resolved by the Territory character, not by a duplicated AI Controller.

| Character | Becomes Hostile only when |
|---|---|
| `ATerritoryGuardCharacter` | Its exact owning Territory is `Contested` and one guard/target faction pair has an explicit `War` treaty. |
| `ATerritoryAssaultCharacter` | Its configured assault is physically `Active` and one attacker/target faction pair has an explicit `War` treaty. |

Friendly Narrative results remain Friendly. Neutral results, stale Hostile results, and a
character's old `Hostiles` entry are downgraded to Neutral when the contextual gate is closed.
This means a Bandit guard does not chase a neutral Hero merely walking through a Claimed
Blacksmith. Use `Can Engage Territory Target` or `Can Engage Assault Target` while debugging.

Do not solve Territory diplomacy by duplicating Narrative's controller. Controller/activity
Blueprints still own movement and combat selection, but the character and the rich Territory
diplomacy subsystem own this authorization.

---

## Combat Director — Strategic Assault Budget

The `UTerritoryCombatDirector` is a `UWorldSubsystem` that limits how many AI can simultaneously attack within a single territory. This is **separate** from NarrativePro's per-target attack tokens (`UNarrativeAbilitySystemComponent::TryClaimToken`).

| System | Scope | What It Limits |
|---|---|---|
| **Narrative Tokens** | Tactical (per-target) | How many AI gang up on ONE defender |
| **Assault Slots** | Strategic (per-territory) | How many configured physical counterattack NPCs participate |

Physical assault NPCs use both systems. Defending guards and unrelated NPCs use only
Narrative tactical tokens and are excluded from strategic assault slots.

### Slot Budget and Narrative Difficulty

Each `ATerritoryVolume` has `MaxConcurrentAttackers` (default 3). With
`bCapConcurrentAttackersToNarrativeDifficulty` enabled on the counterattack profile, the
CombatDirector enforces:

```
Effective limit = min(Territory MaxConcurrentAttackers,
                      Narrative attack tokens for current difficulty)

If GrantedSlots(Territory) >= Effective limit → deny new slot
```

With Narrative's default Combat Settings, Easy/Medium/Hard/Insane allow 1/2/4/6 tactical
attack tokens. Example: a Place limit of 3 becomes 1 on Easy, 2 on Medium, and 3 on Hard or
Insane. Narrative still grants the real per-defender token at attack time; Territory does not
claim or duplicate that token. Disable the profile option only when a game deliberately wants
its strategic wave size to ignore Narrative difficulty.

### Internal Data Structure

```
UTerritoryCombatDirector (UWorldSubsystem)
  ├─ SlotMap: TMap<TWeakObjectPtr<ATerritoryVolume>, FPerTerritorySlots>
  │    └─ FPerTerritorySlots
  │         └─ GrantedControllers: TArray<TWeakObjectPtr<ANarrativeNPCController>>
  └─ BoundControllers: TSet<TWeakObjectPtr<ANarrativeNPCController>>
       └─ Tracks which controllers have ASC OnDeathStateChanged bound
```

### API Reference

#### Actions (BlueprintAuthorityOnly)

| Function | Parameters | Returns | Behavior |
|---|---|---|---|
| `RequestAssaultSlot` | `Territory, NPCController` | `bool` | Grants only to a configured participant targeting this Territory when budget is available and the Territory is not Locked. |
| `ReleaseAssaultSlot` | `Territory, NPCController` | `void` | Releases one slot in a specific territory. |
| `ReleaseAllSlots` | `NPCController` | `void` | Releases all slots across ALL territories for this controller. |

#### Queries (BlueprintPure)

| Function | Returns | Notes |
|---|---|---|
| `HasAssaultSlot(Territory, Controller)` | `bool` | Does this controller hold a slot in this territory? |
| `GetGrantedSlots(Territory)` | `int32` | Active slots — filters dead controllers (weak ptr check) |
| `GetAvailableSlots(Territory)` | `int32` | Effective limit minus granted slots. |
| `GetEffectiveMaxConcurrentAttackers(Territory)` | `int32` | Final strategic limit after the optional Narrative difficulty cap. |

### RequestAssaultSlot — Full Flow

```
RequestAssaultSlot(Territory, Controller):
  1. Null checks (Territory, Controller) → false if null
  2. Validate configured participant, assault ID, faction, and matching target tag
  3. Lock check → false if TerritoryState == Locked
  4. CleanupStaleTerritoryKeys() → remove destroyed keys and orphaned death bindings
  5. FindOrAdd territory in SlotMap
  6. CleanupInvalidControllers() → remove dead controller weak pointers
  7. Resolve the effective Territory/Narrative-difficulty limit
  8. Budget check → false if GrantedControllers.Num() >= effective limit
  9. Duplicate check → true if controller already has a slot (idempotent)
  10. Grant slot and bind ASC death for automatic release
  11. Return true
```

### Multi-floor Place staging

Counterattack movement is navigation-aware when `bUseNavigationAwareObjectives` is enabled.
It considers live defenders, overlapping defence posts, patrol points, and the Territory
center, then accepts complete NavMesh paths. It never treats two actors on different floors as
near merely because their X/Y positions overlap.

`bDistributeParticipantsAcrossObjectives` uses each participant's stable save GUID to spread a
wave across reachable objectives. This makes a multi-floor fight look staged instead of making
every attacker crowd one guard.

Level-authoring requirements:

1. Connect floors with NavMesh-covered stairs or explicit Nav Links.
2. Put patrol/post points on every floor where defenders should stage.
3. Keep those points overlapping the intended Place; an overlapping patrol point is both a
   spawn/staging source and a counterattack objective.
4. Test paths in PIE with the player absent. Counterattackers must advance on the Territory and
   its guards; player presence is not an activation requirement for an active assault.

If no complete path is available, selection falls back to true 3D distance and the normal
bounded stalled-movement withdrawal policy prevents an assault from remaining active forever.

### Automatic Slot Release on NPC Death

When a slot is granted, the CombatDirector binds to the controller's
`UNarrativeAbilitySystemComponent::OnDeathStateChanged` delegate:

```
BindControllerDeath(Controller):
  1. Check if already bound (BoundControllers set) → skip if duplicate
  2. Get ASC via IAbilitySystemInterface
  3. ASC->OnDeathStateChanged.AddUniqueDynamic(this, OnAssaultControllerDied)
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

The shipped Narrative attack trees use `BTService_TerritoryAssaultPermission`. It holds a
slot for the active attack branch when the pawn is a physical assault participant. Ordinary
guards receive branch permission without allocating a strategic slot. The older request and
release tasks remain available for custom trees.

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

## Resolved Fixes (Verified 2026-08-09)

### BPA_ReturnToTerritory ScoreActivity — Territory Check ✅

**Status:** Resolved. Binary asset inspection confirms:
- `-999` return value present in ScoreActivity (non-territory NPCs now score low instead of 0.01)
- `CastFailed` pin wired to a `FunctionResult` (Return node) in SetupBlackboard
- `IsValid` check present (used via the `TerritoryGuardCharacter` cast as the gate — non-territory NPCs fail the cast and receive `-999`)
- Implementation uses the cast itself as the territory check rather than a separate `GetOwningTerritory` call, achieving the same result

### Pending-kill Narrative controller activity cleanup ✅

Territory guards and finite attackers now deactivate their Narrative activity component, remove
transient goals, and stop movement before death cleanup or direct removal. This closes the window
where target-death delegates could ask `BPA_Attack`, `BPA_FollowCharacter`, or
`BPA_ReturnToTerritory` to score/end after Narrative's cached controller became pending-kill.
The fix is in the Territory-owned C++ adapter; no Narrative Pro Blueprint or source is modified.

### Territory permission placement

Live Editor inspection confirms `BTService_TerritoryAssaultPermission` is placed in the
melee, ranged, and grenade attack trees. Do not add the legacy request/release tasks to
defender trees; defenders must never consume the physical counterattack budget.

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
| `SpawnThroughNarrative()` | `ATerritoryGuardCharacter` | Native Territory adapter into Narrative's registered NPC spawn path |
| `ConfigureTerritorySpawnWithContext()` | `ATerritoryGuardCharacter` | Authority-only Blueprint adapter that validates typed Territory/spawn-point context before applying the Narrative definition |
| `ConfigureTerritorySpawn()` | `ATerritoryGuardCharacter` | Deprecated migration node; resolves complete typed context or fails closed |
| `SpawnGuards()` | `ATerritoryVolume` | Spawns all guards for current owner |
| `SpawnSingleGuard()` | `ATerritoryVolume` | Reserve replacement (one-for-one) |
| `ResolveGuardDefinition()` | `ATerritoryVolume` | Picks NPCDefinition per faction (FactionGuardDefinitions first, then default) |
| `RegisterDefender()` | `ATerritoryVolume` | Adds actor to defender list + binds ASC OnDeathStateChanged with bounded readiness retry |
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
| `IsEligibleAssaultController()` | `UTerritoryCombatDirector` | Requires a configured physical assault participant targeting the exact Territory |
| `BindControllerDeath()` | `UTerritoryCombatDirector` | Bind ASC OnDeathStateChanged for auto-release on NPC death |
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
| `ConfigureTerritorySpawnWithContext` | `FNPCSpawnParams` | Sets the complete spawn context and `bOverride_DefaultFactions = true` before definition assignment |
| `TerritoryHomeTransform` | `SpawnInfo.SpawnTransform` | Same transform — used by BPA_ReturnToSpawn |
| `BPA_ReturnToTerritory` | `NPCActivity` | Scores goals, sets up blackboard |
| `Goal_TerritoryPatrol` | `NPCGoalItem` | Patrol goal with score and tags |
| `AC_TerritoryGuard` | `NPCActivityConfiguration` | Lists available activities |
| `Triggers_Bandit` | `TriggerSet` | Time-based goal injection |
| `BT_TerritoryPatrol` | `BehaviorTree` | Uses NarrativePro BT tasks |
| Guard spawn points | `UNarrativeCharacterSubsystem::SpawnNPC` | Territory owns staged slot selection; Narrative owns NPC creation and registries |
