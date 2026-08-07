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
3. Assign a `UNPCDefinition`. Its `NPCClassPath` must derive from
   `ATerritoryAssaultCharacter`, use an `ANarrativeNPCController`-derived controller,
   and set Auto Possess AI to include dynamically spawned actors. When `PlannedForce`
   is greater than one, enable the definition's **Allow Multiple Instances** option;
   Narrative otherwise rejects every pawn after the first.
4. Set finite `PlannedForce` and `WaveSize` values.
5. Assign the profile to the capturable `ATerritoryVolume`.
6. Add one or more enabled `CounterAttackApproaches` with unique `ApproachID` values and transforms relative to the territory.
7. Build navigation from every approach to the territory center.
8. Place exactly one `ATerritoryWorldState` for persistence and late-join replication.

On ownership change, automatic scheduling calls
`ScheduleBestCounterAttack(Territory, OldOwner)`. It evaluates every valid force in the
assigned profile. Diplomacy/capture policy is a hard gate before power scoring; among
the remaining candidates the highest attack priority wins, followed by estimated
success, military power, the former owner as a tie-break only, and stable faction-tag
order. Add every faction that may retaliate to `FactionForces`; the scheduler never
invents a faction or NPC definition outside that data asset.

The native `ATerritoryAssaultCharacter` is spawn-ready by default: it selects
`ANarrativeNPCController` and `PlacedInWorldOrSpawned`. A project Blueprint subclass may
select Narrative Pro's Blueprint controller, but it must preserve those two contracts.
Runtime admission, read-only UI preview, every non-terminal lifecycle phase, and editor
data validation share the same definition/class check; an invalid pawn or incompatible
single-instance definition cannot reserve or consume force.

Do not point the force at a normal guard definition whose class derives only from
`ATerritoryGuardCharacter`; runtime evaluation cancels that record as
`ConfigurationInvalid`. Use a separate assault NPC definition even when it reuses the
same Narrative character definition, activity configuration, TriggerSets, equipment,
and visual data. The reference project uses
`/Game/TerritoryFramework/AI/NPC_TerritoryBanditAssault`, whose `NPCClassPath` is
`/Script/TerritoryFramework.TerritoryAssaultCharacter`.

### Setting Approach ID

`ApproachID` is not stored in the counterattack profile. It belongs to each placed
Territory actor:

1. Select the capturable Property/District actor in the level.
2. Open **Territory > Counter Attack > Counter Attack Approaches**.
3. Add an element and set **Approach ID** to a stable, unique FName such as
   `Blacksmith_WestRoad` (no display text or localization).
4. Set its type, relative spawn transform, per-wave limit, and Enabled flag.
5. Build navigation and verify both the approach position and Territory center project
   to navigation with a full, non-partial path between them.

New blank entries are editor-filled with a type/index ID such as `Road_01`. Rename that
ID to a meaningful stable value before shipping; never rename it after saves can contain
scheduled assault records. The transform is relative to the Territory actor, so actor
scale and rotation affect the final world position. A valid ID with an invalid route is
still rejected as `InvalidApproachOrRoute`.

The editor validator rejects invalid stable GUIDs, duplicate faction definitions,
incompatible Narrative NPC classes, non-positive military power, invalid force/wave
counts, duplicate approach IDs, and territories with no enabled approach. Runtime
scheduling revalidates authority, world, state, faction, profile, NPC class, diplomacy,
route, and global budgets.

## Narrative Pro reuse

Physical attackers are `ANarrativeNPCCharacter` instances spawned and registered through
`UNarrativeCharacterSubsystem::SpawnNPC`, then configured through the public Narrative APIs:

- `UNPCDefinition` and `FNPCSpawnInfo`
- optional `UNPCActivityConfiguration` (combat/patrol configuration may be reused)
- optional `UTriggerSet` overrides
- `UNPCActivityComponent`
- `UTerritoryAssaultGoal`, derived from `UNPCGoalItem`
- `UTerritoryAssaultActivity`, derived from `UNPCActivity`
- Narrative factions, ASC death delegate, navigation, and save clock

Narrative remains authoritative for `CharacterMap`, `NPCMap`, duplicate policy, controller
creation, and definition loading. A scoped Territory spawn context supplies stable spawn
identity, exact faction, activity configuration, TriggerSets, assault ID, and target before
`SetNPCDefinition`; no live pawn pointer is persisted. Goal/activity startup waits until
Narrative reports the character definition, appearance, equipment, and visual load complete.
The assault activity is interruptible, so Narrative combat can take priority and the movement
goal can resume afterward. TerritoryFramework does not add a competing spawn registry, AI
controller, or custom assault Behavior Tree.

`UTerritoryAssaultParticipantComponent` adds the native `UTerritoryAssaultActivity` to
the spawned Narrative activity component when it is missing, then adds one finite
`UTerritoryAssaultGoal`. The assigned activity configuration therefore does not need a
separate hardcoded assault entry, but it must remain compatible with Narrative's normal
combat interruption/rescoring.

## Evaluation inputs

The deterministic calculator consumes:

- active, desired, maximum, and reserve guards;
- guard quality, fortification, and nearby allied support;
- attacker military power, economy readiness, and supply readiness;
- district strategic value and recent faction momentum.

Defence cascades without changing capture authority. For a Property target, evaluation
includes the target, its owning District, and loaded same-owner sibling Properties. For
a District target, it includes the District and loaded same-owner child Properties.
Counts, weighted guard quality, fortification, allied support, and strategic value are
accumulated once. The physical force still attacks one `ATerritoryVolume` and must
complete that volume's existing capture flow.

Spawn-point reserves are replacement entitlements, not hidden active defenders. For
each Territory in the cascade, strategic evaluation counts at most
`min(raw post reserves, DesiredGuardCount)`. Therefore a player-owned target of zero
contributes zero reserve defence even when its posts retain finite replacement stock;
raising the target to one can expose at most one of those reserves to defence planning.

After diplomacy and admission gates, `UnguardedLaunchProbability` owns the launch
policy when the complete local cascade has zero active guards. Its default is `1.0`, so
an empty front certainly launches after grace. This is not an ownership roll:
approach/navigation, player proximity, finite physical NPCs, combat casualties,
defender validation, and normal capture completion remain mandatory.

For the same campaign seed, stable territory GUID, attacking faction, and evaluation cycle, the decision seed is stable. The evaluated result and decision roll are stored in `FTerritoryAssaultRecord`, so save/load does not reroll it. `ATerritoryWorldState` also saves a server-only `FTerritoryAssaultCycleRecord` high-water ledger. Bounded terminal-history trimming therefore cannot reuse an old evaluation cycle or decision seed.

Defence invariants are enforced by tests:

- increasing valid defence never increases priority or launch probability;
- increasing attacker power never reduces launch probability or estimated success;
- strong factions can still schedule an attack against a fully guarded strategic district unless a hard rule blocks it.

`GracePeriodGameTime` uses Narrative Pro accumulated time-of-day units, which are saved by Narrative GameState. Radii and navigation positions use Unreal units.

## Warning and proximity activation

`ScheduledWarning` and `WaitingForPlayerProximity` contain zero live attackers and produce zero capture pressure. Notifications are sent only to relevant controllers inside `NotificationRadius`; `bNotifyDefendingFactionOnly` restricts them to the defending Narrative faction.

The first relevant player entering `ActivationRadius` commits the record to `Active` before spawning. That state transition prevents two nearby players from duplicating the assault. Additional players do not create another force or wave.

Physical registration legitimately transitions the target from `Claimed` to
`Contested`. The lifecycle accepts that state only while the assault is `Active` and the
Territory's contesting faction exactly matches the assault attacker. Warning/grace
records, locked/unclaimed targets, and third-faction contests remain invalid and cancel.

## Finite force and casualties

Every physical attacker belongs to one durable `AssaultID`. The invariant is:

```text
PlannedForce = AliveForce + PendingReserveForce + KilledForce + WithdrawnForce
```

Waves consume `PendingReserveForce`; no death can replenish it. Each participant reports death or withdrawal exactly once. Territory binds casualty accounting only after Narrative has completed its definition/controller/appearance death contract, so Narrative's replicated `HandleDeath` ragdoll presentation runs before the durable assault record can resolve. The participant callback also idempotently requests Narrative ragdoll as an ordering safeguard. Death then immediately unregisters capture pressure and releases the CombatDirector slot. When alive plus pending force reaches zero, the assault resolves `Defeated` and the existing capture subsystem performs its normal progress decay/reset. `HasValidDeathRagdollSetup` is the live Blueprint/MCP diagnostic for the spawned base skeletal mesh and physics asset.

If an activated wave has a valid budget but spawns zero physical NPCs (for example,
every approach is collision-blocked), the durable `ConsecutiveSpawnFailures` count
increments. Any successful spawn resets it. At the profile's bounded
`MaxConsecutiveSpawnFailures` threshold (default 5), the record cancels with
`SpawnFailed` instead of retrying forever. A temporarily exhausted global NPC budget
does not count as a deployment failure.

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

`IsAssaultActive` means the physical `Active` state only.
`IsAssaultPendingOrActive` includes grace, evaluation, warning, proximity waiting, and
physical activation. Use the latter for strategic queue UI.

## Diplomacy and resolution

Alliance, trade agreement, non-aggression, ceasefire, and Narrative Friendly policy block scheduling or cancel an existing assault. War and supported neutral/hostile policy allow it. Ownership change to the attacking faction resolves success; ownership change to a third faction cancels it. A locked/unclaimed target or a removed/incompatible profile/force cancels every non-terminal phase instead of leaving a warning or active record stranded.

The diplomacy gate runs before strongest-faction scoring and is rechecked in every
non-terminal phase. Military power cannot override a treaty or Friendly Narrative
attitude.

No counterattack code calls `SetOwningFaction` or produces offscreen capture progress.

## Save, World Partition, and late join

`ATerritoryWorldState` saves and replicates `FTerritoryAssaultRecord` values: IDs, tags, counts, timing, probabilities, approaches, state, and resolution. It additionally saves—but does not replicate—the scheduler's `FTerritoryAssaultCycleRecord` high-water ledger. Live pawn/controller/UObject pointers are never campaign state.

On authoritative load, saved live survivors become pending finite force; killed and withdrawn counts remain consumed. Evaluation-cycle high-water marks are restored independently of the bounded assault history; a pre-ledger save reconstructs the best available marks from its retained records. An unloaded World Partition territory is resolved first by stable GUID, then by tag, and the record waits for registry registration. Client RepNotify hydrates the local counterattack read model for UI and late join.

Offscreen simulation is intentionally disabled. A future implementation must use multiple saved attrition rounds, not a hidden ownership roll.

## Blueprint and debug API

Server mutations:

- `ScheduleCounterAttack(Territory, AttackingFaction)`
- `ScheduleBestCounterAttack(Territory, PreferredFaction)`
- `CancelAssault(AssaultID, Reason)`

Queries:

- `GetAssault`
- `GetAllAssaults`
- `GetAssaultsForTerritory`
- `IsAssaultActive`
- `GetAssaultDebugString`
- `GetBestEligibleAttackerPreview` (planning only; reserves no cycle and makes no roll)
- `ATerritoryAssaultCharacter::IsNarrativeSpawnReady` (live Blueprint/MCP diagnostic;
  checks definition, appearance, controller, and activity readiness without treating an
  optional weapon visual as a movement prerequisite)

Bind `UTerritoryPlayerManagementComponent::OnAssaultNotification` for targeted UI warnings.
The supplied Territory HUD renders this as a timed inline alert and exposes
`OnCounterAttackAlert` for non-modal Blueprint animation. It deliberately does not invoke the
project's Narrative notification widgets because both pause this world's active assault.
`GetAssaultDebugString`
reports target/attacking/defending factions, guard counts, reserve, attacker and defence power,
ratio, strategic value, priority, launch/success probability, finite force counts, approaches,
notification state, grace time, roll, state, and reason.
