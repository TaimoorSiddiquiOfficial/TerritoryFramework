# Debug System

Use this system to answer four questions:

1. Which exact Territory did gameplay or UI select?
2. What does the Definition say for a new campaign?
3. What does the runtime save/server say now?
4. Which rule allowed or blocked the action?

## Enable it

Open `Edit -> Project Settings -> Territory Framework -> Debug`.

1. Enable **Enable Debug System (Master Gate)**.
2. Enable one or more categories.
3. Choose a verbosity level.
4. Filter Output Log with `LogTerritory`.

The master setting is a gate. It does not automatically turn on every category.

## Log categories

| Category | Prefix or subject | Use it for |
|---|---|---|
| Availability + Hierarchy | `[Availability]`, `[Lock]` | Initial versus runtime lock, parent path, unlock failures |
| Registry | `[Registry]` | Registration, duplicate identity, unload/reload |
| Capture Progression | `[CaptureTick]` | Progress and decay |
| Capture Attempts | `[CaptureAttempt]` | Attacker, owner, and eligibility |
| Ownership | `[CommitOwnership]` | Atomic owner changes |
| State Transitions | `[StateChange]` | Unclaimed, Contested, and Claimed transitions |
| Economy | `[EconomyTick]` | Income, upkeep, and faction settlement |
| Transactions | `[Transaction]` | Individual credit/debit records |
| Production | `[Production]` | Rule, cycle, status, inputs, outputs, failure reason |
| Guard Spawning | guard spawn records | Spawn definition, posts, and counts |
| Guard Deaths | `[GuardDeath]` | Defender and reserve lifecycle |
| Diplomacy | `[Diplomacy]` | War, peace, and treaty changes |
| Faction Attitudes | `[Attitude]` | Narrative attitude decisions |
| Counterattacks | `[CounterAttack]` | Schedule and every assault state transition |
| Stealth | `[Stealth]` | Evidence, suspicion, exposure, and observers |
| Save/Load | `[SaveLoad]` | Saved runtime source and restore |
| WorldState | `[WorldState]` | Replicated directory changes and hydration |
| Spatial | `[Spatial]` | Overlap query and most-specific Territory selection |
| Markers | `[Marker]` | POI and waypoint refresh |
| UI | `[UI]` | Exact bound Territory, status, and overlapping candidates |
| Interaction | `[Interaction]` | Capture point and story/management interaction decisions |
| Narrative/Tales | state events and task/event logs | Conditions and explicit event context |
| Behavior Trees | `[BT]` | Territory AI task flow |
| Combat | combat director records | Attack permission and concurrent attack budget |

## Verbosity

| Value | Meaning |
|---:|---|
| 0 | No optional debug logs |
| 1 | Fatal threshold |
| 2 | Error threshold |
| 3 | Warning threshold |
| 4 | Display threshold |
| 5 | Normal category logs |
| 6 | Detailed/verbose category logs |

Normal debug categories require level 5. Very detailed call sites may also require level 6.
Unreal's Output Log category can still be changed with `log LogTerritory Verbose` when verbose
engine log lines are needed.

## Exact Territory report

Use the Blueprint-pure function:

```text
Build Territory Debug Summary By Tag
```

This is safer than a location query when City, District, and Place bounds overlap.

Example tag:

```text
Territory.HavenReach.CastleHill.Farm
```

The report contains:

- exact actor path and Definition path;
- runtime source: Definition seed, campaign save, or server replication;
- runtime Availability and Political State;
- UI Status after availability-first formatting;
- new-campaign Availability and State;
- local and effective hierarchy availability;
- complete loaded parent path;
- owner, contesting faction, progress, guard counts, and assault records;
- an explanation when runtime differs from the Definition seed.

The UI category logs both the most-specific spatial result and the visible Territory finally
shown to the player. Easy example: the spatial result may be the locked Farm, while the HUD safely
falls back to the unlocked Castle Hill District without revealing the Farm name.

## Farm example

If the Definition says `Initial Availability = Locked`, but the report says:

```text
Runtime source = Campaign save
Runtime Availability = Unlocked
Political State = Contested
```

then the save is intentionally overriding the new-campaign seed. Use a new campaign to test the
initial setup, or run an explicit Lock Event to change the current campaign.

If runtime Availability is Locked and Political State is Contested, the report will show
`UI Status = Locked`. That is the correct presentation.

## Whole-system report

`Build Territory System Debug Report` includes the active Debug Settings summary and one sorted
line for every loaded Territory. This is useful after World Partition streaming, save load, or a
hierarchy cascade.

`Build Debug Settings Summary` lists the master gate, verbosity, and every active category. It
also lists active visual overlays and explains everything that intentionally stays outside the
optional debug gate.

## Gameplay Debugger

Developer builds register a `Territory` Gameplay Debugger category. When a Territory actor is
selected, it shows the same detailed report. When another actor is selected, the framework uses
the actor location and reports the most-specific overlapping Place, then District, then City.

## Debug widget

Create a Blueprint child of `UTerritoryDebugWidget`, implement `OnUpdateDebugText`, and call
`SetDebugEnabled(true)`. It presents a throttled overview of Territories, economy, diplomacy,
capture, and counterattacks.

## Visual toggles

| Toggle | Result |
|---|---|
| Draw Territory Bounds | Draws volume bounds in PIE |
| Draw Ownership Overlay | Shows ownership color |
| Draw Capture Progress | Shows contested progress |
| Draw Guard Spawn Points | Shows posts and patrol route data |
| Draw Spatial Grid | Shows spatial index cells |

## Diagnostics outside the optional gate

These Error/Warning diagnostics are always visible by design:

- invalid or missing Definition;
- duplicate tag or GUID;
- invalid/cyclic hierarchy;
- authority or security rejection;
- save corruption or missing persistent identity;
- invalid guard or assault class;
- an asset configuration that would make gameplay silently fail.

They are product safety and content validation, not optional debug noise.

The following tools also work outside the master gate because a developer must request them
directly:

- `Print Territory Debug` and `Print All Territory Debug`;
- `Build Territory Debug Summary`, exact-tag summary, and whole-system report;
- the Territory Gameplay Debugger category;
- a `UTerritoryDebugWidget` after `Set Debug Enabled(true)` is called;
- editor-only notices that a stable GUID or counterattack Approach ID was generated while an
  author saves an asset.

These tools cannot create background log spam by themselves. Calling or opening one is the opt-in.

## Useful Output Log commands

```text
log LogTerritory Log
log LogTerritory Verbose
log LogTerritory Warning
```

## Recommended problem presets

| Problem | Categories |
|---|---|
| Locked or unlock | Availability + Hierarchy, Save/Load, WorldState, Narrative/Tales, UI |
| Capture | Capture Attempts, Capture Progression, Ownership, State Transitions, Diplomacy, Interaction |
| Guard or AI | Guard Spawning, Guard Deaths, Diplomacy, Attitudes, Behavior Trees, Combat |
| Counterattack | Counterattacks, Diplomacy, WorldState, Guards, Combat |
| Production | Production, Economy, Transactions, Save/Load, WorldState |
| Stealth | Stealth, Diplomacy, Capture Attempts, Narrative/Tales |
