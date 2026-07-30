# Counterattack System

## Authority and lifecycle

`UTerritoryCounterAttackSubsystem` is the server-only strategic scheduler. It never writes territory ownership. Ownership changes only after physical Narrative NPCs enter the target, register with `UTerritoryControlSubsystem`, survive defender validation, and complete the normal capture flow.

```text
Captured
  -> Grace
  -> Evaluating
  -> ScheduledWarning
  -> WaitingForPlayerProximity
  -> Active physical assault
  -> Succeeded / Defeated / Cancelled
  -> saved and replicated by ATerritoryWorldState
```

A decision roll can schedule an assault; it cannot capture a territory. `LaunchProbability` is scheduling policy. `EstimatedSuccessProbability` is planning information derived from finite attacker power versus defence.

## Required setup

1. Create a `UTerritoryCounterAttackProfile` data asset.
2. Add one `FTerritoryFactionAssaultConfig` for each possible attacking Narrative faction.
3. Assign a `UNPCDefinition`. Its `NPCClassPath` must derive from `ATerritoryAssaultCharacter`.
4. Set finite `PlannedForce` and `WaveSize` values.
5. Assign the profile to the capturable `ATerritoryVolume`.
6. Add one or more enabled `CounterAttackApproaches` with unique `ApproachID` values and transforms relative to the territory.
7. Build navigation from every approach to the territory center.
8. Place exactly one `ATerritoryWorldState` for persistence and late-join replication.

The editor validator rejects invalid stable GUIDs, duplicate faction definitions, incompatible Narrative NPC classes, invalid force/wave counts, duplicate approach IDs, and territories with no enabled approach. Runtime scheduling revalidates authority, world, state, faction, profile, NPC class, diplomacy, route, and global budgets.

## Narrative Pro reuse

Physical attackers are `ANarrativeNPCCharacter` instances configured through the public Narrative APIs:

- `UNPCDefinition` and `FNPCSpawnInfo`
- optional `UNPCActivityConfiguration`
- optional `UTriggerSet` overrides
- `UNPCActivityComponent`
- `UTerritoryAssaultGoal`, derived from `UNPCGoalItem`
- `UTerritoryAssaultActivity`, derived from `UNPCActivity`
- Narrative factions, ASC death delegate, navigation, and save clock

Spawn information, exact faction, activity configuration, and TriggerSets are filled before `SetNPCDefinition`. The assault activity is interruptible, so Narrative combat can take priority and the movement goal can resume afterward. TerritoryFramework does not add a competing AI controller or custom assault Behavior Tree.

## Evaluation inputs

The deterministic calculator consumes:

- active, desired, maximum, and reserve guards;
- guard quality, fortification, and nearby allied support;
- attacker military power, economy readiness, and supply readiness;
- district strategic value and recent faction momentum.

For the same campaign seed, stable territory GUID, attacking faction, and evaluation cycle, the decision seed is stable. The evaluated result and decision roll are stored in `FTerritoryAssaultRecord`, so save/load does not reroll it.

Defence invariants are enforced by tests:

- increasing valid defence never increases priority or launch probability;
- increasing attacker power never reduces launch probability or estimated success;
- strong factions can still schedule an attack against a fully guarded strategic district unless a hard rule blocks it.

`GracePeriodGameTime` uses Narrative Pro accumulated time-of-day units, which are saved by Narrative GameState. Radii and navigation positions use Unreal units.

## Warning and proximity activation

`ScheduledWarning` and `WaitingForPlayerProximity` contain zero live attackers and produce zero capture pressure. Notifications are sent only to relevant controllers inside `NotificationRadius`; `bNotifyDefendingFactionOnly` restricts them to the defending Narrative faction.

The first relevant player entering `ActivationRadius` commits the record to `Active` before spawning. That state transition prevents two nearby players from duplicating the assault. Additional players do not create another force or wave.

## Finite force and casualties

Every physical attacker belongs to one durable `AssaultID`. The invariant is:

```text
PlannedForce = AliveForce + PendingReserveForce + KilledForce + WithdrawnForce
```

Waves consume `PendingReserveForce`; no death can replenish it. Each participant reports death or withdrawal exactly once. Death immediately unregisters capture pressure and releases the CombatDirector slot. When alive plus pending force reaches zero, the assault resolves `Defeated` and the existing capture subsystem performs its normal progress decay/reset.

Global limits are configured in Project Settings:

| Setting | Default |
|---|---:|
| `CounterAttackCampaignSeed` | 1337 |
| `CounterAttackUpdateInterval` | 2 seconds |
| `MaxConcurrentScheduledAssaults` | 8 |
| `MaxConcurrentAssaultsPerFaction` | 2 |
| `MaxLiveCounterAttackNPCs` | 24 |
| `MaxRetainedAssaultRecords` | 100 |

`FTerritoryAssaultApproach::MaxWaveSize` also limits how many attackers one approach may contribute to a wave.

## Diplomacy and resolution

Alliance, trade agreement, non-aggression, ceasefire, and Narrative Friendly policy block or cancel an assault. War and supported neutral/hostile policy allow it. Ownership change to the attacking faction resolves success; ownership change to a third faction cancels it. Locking the active target cancels it.

No counterattack code calls `SetOwningFaction` or produces offscreen capture progress.

## Save, World Partition, and late join

`ATerritoryWorldState` saves and replicates `FTerritoryAssaultRecord` values only: IDs, tags, counts, timing, probabilities, approaches, state, and resolution. Live pawn/controller/UObject pointers are never campaign state.

On authoritative load, saved live survivors become pending finite force; killed and withdrawn counts remain consumed. An unloaded World Partition territory is resolved first by stable GUID, then by tag, and the record waits for registry registration. Client RepNotify hydrates the local counterattack read model for UI and late join.

Offscreen simulation is intentionally disabled. A future implementation must use multiple saved attrition rounds, not a hidden ownership roll.

## Blueprint and debug API

Server mutations:

- `ScheduleCounterAttack(Territory, AttackingFaction)`
- `CancelAssault(AssaultID, Reason)`

Queries:

- `GetAssault`
- `GetAllAssaults`
- `GetAssaultsForTerritory`
- `IsAssaultActive`
- `GetAssaultDebugString`

Bind `UTerritoryPlayerManagementComponent::OnAssaultNotification` for targeted UI warnings. `GetAssaultDebugString` reports target/attacking/defending factions, guard counts, reserve, attacker and defence power, ratio, strategic value, priority, launch/success probability, finite force counts, approaches, notification state, grace time, roll, state, and reason.
