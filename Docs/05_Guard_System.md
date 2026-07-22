# Guard System — Spawning, Patrols, Reserves, BT Integration

## Overview

The guard system uses `ATerritoryGuardSpawnPoint` actors placed in the level to define:
- Where guards spawn (exact location, not random)
- How many active guards (slots)
- How many reserve guards (replacements)
- Patrol routes (ordered waypoints)
- Guard activity at each waypoint

## Guard Spawn Flow

```
TerritoryVolume.BeginPlay()
  └── Sync Initial* → OwnershipData.*
      └── If Claimed + GuardNPCDefinition assigned:
          └── SpawnGuards()
              ├── For each guard (up to GuardSpawnCount):
              │   ├── Find available GuardSpawnPoint (priority order)
              │   │   └── Check: HasAvailableSlot() || HasReserveAvailable()
              │   ├── BeginDeferredActorSpawnFromClass(TerritoryGuardCharacter)
              │   │   ├── SetTerritorySaveGUID(NewGUID)
              │   │   ├── SetOwningTerritoryGUID(TerritoryGUID)
              │   │   └── SetNPCDefinition(GuardNPCDefinition)
              │   ├── FinishSpawningActor
              │   ├── AddFaction(TerritoryOwner) ← GUARD FACTION FROM TERRITORY
              │   ├── RunBehaviorTree(GuardBehaviorTree)
              │   │   └── Set Blackboard: PatrolSpawnPoint, PatrolNodeIndex=0
              │   ├── RegisterSpawnedGuard(SpawnPoint)
              │   └── RegisterDefender(Guard)
              └── If no SpawnPoints: random position within BoundsShape
```

## Faction Assignment

**Guards take their faction from the TERRITORY OWNER, not from the NPC Definition.**

This means:
- A district owned by Bandits → guards get `Narrative.Factions.Bandits`
- Same district captured by Heroes → old guards despawn, new guards get `Narrative.Factions.Heroes`
- NPC Definition provides only the character template (mesh, abilities, stats)

## Reserve System

When a guard dies:
1. `OnDefenderDied` fires
2. Spawn point checks `bWasTracked` (was this guard from THIS spawn point?)
3. If yes and `CurrentReserveCount > 0`:
   - Decrement reserve count
   - Call `Territory->SpawnGuards()` to spawn replacement
4. If no reserves: territory is undefended at this point

## Patrol AI

### BT Structure (BT_GuardPatrol)

```
ROOT
└── Sequence
    ├── [Service] BTS_ClearAIFocus (NarrativePro — clears stale focus)
    ├── [Service] BTService_UpdatePatrolRoute (0.5s tick — advances node index)
    ├── BTTask_MoveToPatrolNode (reads PatrolRoute[index], writes TargetLocation/Rotation/Delay to BB)
    ├── BTTask_RotateToGoal_C (NarrativePro — rotates to TargetRotation from BB)
    └── BTTask_WaitBlackboardTime (engine — waits Delay seconds from BB)
```

### Blackboard Keys (BB_GuardPatrol)

| Key | Type | Writer | Reader |
|---|---|---|---|
| SelfActor | Object | AI Controller | — |
| PatrolSpawnPoint | Object | TerritoryVolume (on spawn) | MoveToPatrolNode |
| PatrolNodeIndex | Int | UpdatePatrolRoute | MoveToPatrolNode |
| TargetLocation | Vector | MoveToPatrolNode | AIController::MoveTo |
| TargetRotation | Rotator | MoveToPatrolNode | RotateToGoal_C |
| Delay | Float | MoveToPatrolNode | WaitBlackboardTime |

### Assigning a Combat BT

For guards that should fight enemies:
1. Assign `BT_Attack_Generic` (from NarrativePro) as the **GuardBehaviorTree**
2. Assign `BB_Attack` (from NarrativePro) as the **GuardBlackboardAsset**
3. The guard will switch to combat when it detects enemies via perception

### Custom BT

You can create a custom BT that combines patrol + combat:
```
ROOT
├── [Decorator: HasEnemyTarget] → BT_Attack_Generic
└── [Default] → BT_GuardPatrol
```

## Guard Death

When a guard dies:
1. `UNarrativeAbilitySystemComponent::OnDied` fires (from Narrative GAS)
2. `ATerritoryVolume::OnDefenderDied` handles it:
   - Unregisters defender from territory
   - Notifies all assigned GuardSpawnPoints via `UnregisterGuard`
   - Removes from `SpawnedGuards` array
   - Broadcasts `OnGuardDied` delegate
   - Logs `[GuardDeath]` if debug enabled

## Setup Checklist

For guards to work, ALL of these must be set:

- [ ] `GuardNPCDefinition` assigned on territory volume
- [ ] `GuardSpawnCount` > 0
- [ ] Territory is `Claimed` with a valid `OwningFaction`
- [ ] `GuardBehaviorTree` assigned (e.g., `BT_Attack_Generic`)
- [ ] `GuardBlackboardAsset` assigned (e.g., `BB_Attack`)
- [ ] GuardSpawnPoint actors placed inside territory bounds (optional but recommended)
- [ ] Navigation mesh covers the territory area

## Common Issues

| Problem | Cause | Fix |
|---|---|---|
| Guards don't spawn | No GuardNPCDefinition | Assign NPC definition asset |
| Guards spawn but stand still | No GuardBehaviorTree | Assign BT on territory volume |
| Guards don't fight | Wrong BT assigned | Use `BT_Attack_Generic` for combat |
| Guards float on hit | (Fixed) | BoundShape has NoCollision |
| Guards have wrong faction | NPC Definition has DefaultFactions | Faction comes from territory owner, not definition |
| Guards don't patrol | No patrol route on spawn point | Add waypoints to PatrolRoute |
| Guards clip through walls | Pathfinding issue | Ensure NavMesh covers patrol area |
