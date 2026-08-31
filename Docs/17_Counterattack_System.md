# Counterattack System

## Authority and lifecycle

`UTerritoryCounterAttackSubsystem` is the server-only strategic scheduler and physical
recapture coordinator. It cannot win from a probability roll: Narrative attackers must
enter the target and every registered defender must be defeated. It commits ownership only
for the explicit unattended/dead-player recapture outcomes described below; ordinary player
capture still belongs to `UTerritoryControlSubsystem`.

```text
Ownership changed / Place is Claimed
  -> Grace
  -> Evaluating
  -> ScheduledWarning
  -> WaitingForPlayerProximity (only when the profile requires it)
  -> Active physical assault
  -> optional RecaptureCountdown when defenders are gone and the player is absent
  -> Succeeded / Defeated / Cancelled
  -> optional recurring cooldown (strategic records only)
  -> saved and replicated by ATerritoryWorldState
```

A decision roll can schedule an assault; it cannot capture a territory. `LaunchProbability` is scheduling policy. `EstimatedSuccessProbability` is planning information derived from finite attacker power versus defence.

## The rules in easy English

- A strategic counterattack is a **reinforcement operation**. With **Require Reinforcement
  Capability** enabled, the faction must currently own a Territory whose active State Config
  grants `Territory.Capability.Reinforcements`. Losing that perk cancels an attack that has not
  physically deployed. An already deployed finite force is allowed to finish.
- The faction's **Signature Vehicle** is the recognizable car Blueprint used on every vehicle
  approach. Example: Bandits use a rusty pickup; the Regime uses a black sedan. An Approach car
  is only the fallback when the faction has no signature car.
- Narrative Pro difficulty chooses the total car budget when **Scale Car Count With Narrative
  Difficulty** is enabled. Defaults are Easy 1, Medium 1, Hard 2, Insane 3. The result is always
  clamped by the finite force and the authored road limits.
- Strategic attackers are **takeover-first**. They fight registered guards and defending players
  inside the local Territory battle area, then stand inside the Place and apply real capture
  pressure. A visible player far outside the battle area cannot lure the complete force away and
  freeze the takeover.
- A **Story Pursuit / Boss Chase** is different. It does not capture, does not require the
  reinforcement perk, and may pursue outside Territory bounds because the quest explicitly owns
  that story encounter.

Example: Heroes own Blacksmith. Bandits still own Radio District, whose Claimed State Config
grants Reinforcements. On Hard, two Bandit signature cars may arrive. If the player runs outside
Market Square, attackers continue taking Blacksmith. Use a separate Story Pursuit event when the
design calls for those enemies to abandon the Place and chase the player through the city.

## Four ways an enemy response can begin

All four paths enter the same authoritative assault subsystem. They do not create a second
combat, spawn, or capture implementation.

1. **Automatic ownership response:** after a Place changes owner, the framework asks the
   profile for the best eligible hostile faction. The former owner is only a tie-break; another
   stronger faction may attack when diplomacy and staging allow it.
2. **Scheduled follow-up:** after a strategic battle ends and its cooldown passes, Finite Series
   or Unlimited Schedule may create another separate finite battle.
3. **Narrative Wave event:** a quest/dialogue runs `Wave of Enemies` in Strategic mode. The
   event's inherited Narrative Conditions must pass, then the profile controls the battle and
   any allowed future schedule.
4. **Story Pursuit / Boss Chase:** a Narrative Event explicitly selects Story Pursuit. It may
   send hunters toward the player or reverse a vehicle route so the player chases an escaping
   capo. It may use the profile's authored staging exception, but it never repeats automatically.

Every normal strategic path still requires the attacker to hold a secure District when the
force uses the domination staging rule. A faction with no secure District cannot counter merely
because its tag appears in the profile.

Every arrow after the initial record creation emits one post-commit
`FTerritoryCounterAttackStateEvent`. Its embedded record is already in `NewState` and includes
the durable ID, target/factions, force counts, selected approaches, and resolution. It also
provides `PreviousState` and `EventGameTime`. Same-state casualty/data updates continue to use
`OnAssaultChanged` and do not duplicate `OnCounterHappened`; save/load hydration never replays
gameplay notifications.

## Player presence and recapture

`Require Player Proximity For Activation` defaults off. This lets a faction attack an
unlocked Place and its registered guards without waiting for the player to arrive.

After the complete defence front has no living registered guard and an attacker is
physically inside the target:

- A living defending-faction player in the Place or parent District keeps the assault
  Active. The attackers must defeat that player.
- With `Allow Unattended Recapture Countdown`, an absent player starts the saved
  `Unattended Recapture Time`. Returning alive cancels the countdown.
- With `Concede When Defending Player Dies`, a defending player death inside that battle
  area immediately hands the Place to the attacker before respawn.
- An expired unattended countdown hands the Place to the attacker and publishes the normal
  ownership/UI intelligence events.

Example: Bandits reach a Heroes-owned Blacksmith while the player is elsewhere. They defeat
its final guard, and a 30-second report appears. If the player returns, combat is required.
If the player stays away—or dies defending it—the Place is recaptured by Bandits.

## Required setup

1. Create a `UTerritoryCounterAttackProfile` data asset.
2. Add one `FTerritoryFactionAssaultConfig` for each possible attacking Narrative faction. Give
   each faction a Signature Vehicle, or run `Territory.Editor.MigrateFactionSignatureVehicles`
   once to copy the first authored vehicle-approach fallback into blank faction entries. Review
   the result and Save All.
3. Assign a `UNPCDefinition`. Its `NPCClassPath` must derive from
   `ATerritoryAssaultCharacter`, use an `ANarrativeNPCController`-derived controller,
   and set Auto Possess AI to include dynamically spawned actors. When `PlannedForce`
   is greater than one, enable the definition's **Allow Multiple Instances** option;
   Narrative otherwise rejects every pawn after the first.
4. Set finite `PlannedForce` and `WaveSize` values.
5. Choose the force's staging rule. `OwnsSecureDistrict` is the domination default: a
   faction with no loaded, unlocked District securely held in `Claimed` cannot launch a
   normal counterattack. Locked, `Contested`, and `Unclaimed` Districts never qualify.
6. Choose **One Assault**, **Finite Series**, or **Unlimited Schedule**. For a repeating
   schedule, set a non-zero recurring cooldown. For a finite series, also set the maximum
   number of separate battles.
7. Optionally choose a Narrative time window. `1800 -> 0500` means evening through early
   morning and correctly wraps across midnight.
8. Enable `bAllowStoryPursuitWithoutStagingDistrict` only for a force that an explicit Tales
   story-pursuit event may launch after the faction loses its final District.
9. Assign the profile to the capturable Place Definition. City and District Definitions cannot
   own physical assault policy.
10. Add one or more enabled `CounterAttackApproaches` with unique `ApproachID` values and transforms relative to the territory.
11. Choose **On Foot** or **Narrative Vehicle** on each approach. A vehicle approach needs an
    existing Narrative vehicle class and a roadside drop-off relative to the Place.
12. Build a ZoneGraph **Road** from each vehicle spawn to its drop-off. For a simple straight,
    unobstructed street, run **Ensure Straight Vehicle Approach Road** from the Territory editor
    library. Use normal ZoneShape editing for curves, intersections, bridges, or obstacles.
13. Build navigation from every approach to at least one shared defence objective: a live
    defence-front guard, overlapping patrol/post point, or the target center fallback.
14. Place exactly one `ATerritoryWorldState` for persistence and late-join replication.

The editor validator now uses the exact runtime route contract: the approach must project to
navigable ground within 500 cm, the Territory target within 1,000 cm, and the resulting path
must be complete rather than partial. If a built navmesh is unavailable, validation reports a
warning instead of pretending the route passed. If navigation exists and the route is invalid,
the Territory is invalid before play.

The demo map currently provides two validated physical approaches:

| Territory | Approach | World spawn | Result |
|---|---|---:|---|
| Blacksmith | `Blacksmith_WestRoad` | `(-3952, 795.4146, 3.0717)` | Complete ZoneGraph drive plus NavMesh entry |
| Farm | `Farm_WestField` | `(2200, 0, 3.0717)` | Projects within 500 cm; complete path to the projected Farm target |

On ownership change, automatic scheduling calls
`ScheduleBestCounterAttack(Territory, OldOwner)`. It evaluates every valid force in the
assigned profile. Diplomacy/capture policy is a hard gate before power scoring; among
the remaining candidates the highest attack priority wins, followed by estimated
success, military power, the former owner as a tie-break only, and stable faction-tag
order. Add every faction that may retaliate to `FactionForces`; the scheduler never
invents a faction or NPC definition outside that data asset.

Normal scheduling and every undeployed lifecycle phase recheck the configured staging rule.
With the domination default, losing the final secure District cancels a grace/warning/waiting
record as `StagingDistrictUnavailable`. A physically Active finite force is already deployed
and may finish; losing a District does not erase living NPCs. After a terminal strategic
record, `RecurringCounterCooldownGameTime` must elapse before the same still-hostile front is
eligible again. The profile then applies its schedule mode:

| Schedule mode | Meaning | Easy example |
|---|---|---|
| One Assault | Never creates a later automatic battle | A one-time tutorial raid |
| Finite Series | Stops when the authored total battle count is reached | Maximum 3 = the first attack plus at most two later counterattacks |
| Unlimited Schedule | May keep creating later finite battles while every rule continues to pass | A regime periodically tries to reclaim its frontier for the rest of the campaign |

“Unlimited” never means infinite spawning inside one fight. Every battle still has its own
finite `PlannedForce`, finite waves, casualty accounting, and terminal result. Story-pursuit
records never repeat automatically.

### Narrative Pro and Ultra Dynamic Sky time

`Any Narrative Time` admits the battle after grace/cooldown. `Narrative / Ultra Dynamic Sky
Time Window` keeps a strategic record in Grace until `ANarrativeGameState::GetTimeOfDay()` is
inside the configured range. The Narrative Pro Ultra Dynamic Sky add-on describes its sky as
synchronized to Narrative Pro time, so Territory deliberately reads the same Narrative clock
instead of depending directly on the commercial sky asset.

Examples:

- `0600 -> 1000`: dawn and morning attacks.
- `1800 -> 0500`: night attacks; the wrapped range includes 23:00 and 03:00.
- `0000 -> 0000`: all day.

The schedule still rechecks ownership, War diplomacy, secure-District staging, quest rules,
route, and budgets. A night window changes *when* a battle may evaluate; it does not force an
otherwise invalid attack to happen.

## Narrative quest rules

`Quest Rules` on the counterattack profile gate automatic strategic counters without copying
quest state into Territory. Every configured rule must pass, and each rule reads the selected
Narrative Quest from the scoped online players' real `UTalesComponent`.

| Setting | Meaning | Easy example |
|---|---|---|
| Block Counter When Matched | No strategic counter while any scoped player matches the selected quest state | Block while the defending player's Stealth Investigation quest is In Progress |
| Require Match Before Counter | No strategic counter until at least one scoped player matches | Require Betrayal Revealed to be Succeeded before the Regime retaliates |
| Any Online Player | Any connected Narrative player may satisfy or block the rule | Pause a shared-world event while anyone is in the tutorial |
| Defending Faction Players | Only players with the exact current defender faction tag count | Protect Heroes players during their story mission |
| Attacking Faction Players | Only players with the exact selected attacker faction tag count | Allow a faction-led assault only after that faction's quest succeeds |

Available quest states are Not Started, In Progress, Succeeded, Failed, Finished, and Started
or Finished. A missing Quest asset fails closed. Pending Grace, Warning, or Waiting records
recheck the rules and cancel as `QuestRuleBlocked` when the story state changes. Already Active
physical attackers remain in the world and may finish the fight.

These profile rules deliberately apply only to normal strategic and recurring counters. An
explicit `Wave of Enemies` event in Story Pursuit / Boss Chase mode uses the Narrative Event's
inherited `Conditions` array instead. Add `Narrative Quest State Condition` there when a pursuit
must happen only during one quest; use its inherited **Not** option when the pursuit must not
happen during that quest.

In Strategic Counterattack mode, `Wave of Enemies` schedules the first finite battle through
the same profile. Its inherited event Conditions gate that activation. Later automatic battles
are controlled by the force's schedule mode and recheck the profile Quest Rules; event
Conditions are not silently replayed without a Narrative event context.

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
`/Game/TerritoryFramework/AI/BP_TerritoryAssualtGuard1`. That Blueprint derives from
`ATerritoryAssaultCharacter` and selects Narrative's NPC controller.

### Setting Approach ID

`ApproachID` is not stored in the counterattack profile. It belongs to each placed
Territory actor:

1. Select the capturable Place (`ATerritoryProperty`) actor in the level. City and District are aggregate authorities and never own physical approaches.
2. Open **Territory > Counter Attack > Counter Attack Approaches**.
3. Add an element and set **Approach ID** to a stable, unique FName such as
   `Blacksmith_WestRoad` (no display text or localization).
4. Set its type, relative spawn transform, per-wave limit, and Enabled flag.
5. Build navigation and verify both the approach position and Place objective project
   to navigation with a full, non-partial path between them.

New blank entries are editor-filled with a type/index ID such as `Road_01`. Rename that
ID to a meaningful stable value before shipping; never rename it after saves can contain
scheduled assault records. The transform is relative to the Territory actor, so actor
scale and rotation affect the final world position. A valid ID with an invalid route is
still rejected as `InvalidApproachOrRoute`.

### Vehicle and on-foot entries

Vehicle entry composes Narrative Pro with Territory's ZoneGraph road driver:

```text
Territory vehicle approach
  -> spawn the configured ANarrativeVehicleBase (server only, runtime-only)
  -> spawn one finite ATerritoryAssaultCharacter beside it
  -> wait until Narrative finishes the NPC Definition and visual spawn
  -> Narrative claims the seat, runs its mount ability, animation and controller possession
  -> Territory follows sampled ZoneGraph road points through the vehicle's Chaos movement
  -> at the drop-off, Territory calls Narrative's public StopInteractBehavior API
     so the authored mount ability performs its normal dismount and cleanup
  -> only now allow Territory hostility, reserve waves and capture pressure
  -> Territory assault movement/combat continues from the roadside drop-off
```

Easy Blacksmith example:

- `Blacksmith_WestRoad`: Narrative Vehicle; Sedan at the west road; drop-off beside
  the market entrance; Maximum Vehicle Deployments = 1. The first tracked attacker is the
  driver; later members of the same finite force enter on foot from that drop-off.
- Optionally add `Blacksmith_Alley_Foot`: On Foot; guards run through the alley.
- Set Maximum Approaches to at least 2 when a strong assault should select one valid route of
  each entry type. The selector preserves vehicle/on-foot variety when both are available.

The vehicle route must have a complete ZoneGraph path from car spawn to drop-off. The drop-off
must then have a complete NavMesh path into the Place. The same tests run in editor validation
and at runtime. After the bounded number of cars has deployed, remaining finite attackers may
enter on foot from the drop-off, but they wait until the current vehicle finishes ingress. The
driver is neutral to combat and cannot contest the Place while mount/drive/park/dismount is active.
A failed car, invalid route, driver spawn, or bounded ingress timeout withdraws cleanly.
Terminal cleanup stops all vehicle inputs before giving the transient car a short retirement delay.
This avoids Narrative's stock `BTS_ClaimInteractableSlot` null-target error without modifying
Narrative Pro content. Assault
cars and actor pointers are never saved; active survivors are reconstructed from the saved route
and remaining force after load.

The straight-road editor helper is idempotent: it finds its ZoneShape by a stable Territory
GUID + Approach ID tag, updates its two points and Road lane profile, rebuilds ZoneGraph, and
marks the map dirty. It never edits Narrative Pro. Re-run it after moving either transform.
For a road that is not truly straight and clear, adjust the generated ZoneShape in the normal
ZoneGraph editor before validation.

Use the Tales event **Start Territory Boss Chase** for a one-shot pursuit. Select the claimed
target Place, hostile Narrative faction, and one reusable **Pursuit Options** structure:

- **Enemy Chases Player:** the route runs spawn -> drop-off; Narrative dismounts the hunter and
  normal combat begins.
- **Player Chases Enemy:** only Narrative Vehicle approaches are eligible; the route runs
  drop-off -> spawn. Killing the finite target resolves `AllAttackersRemoved`; reaching the exit
  resolves `TargetEscaped`.
- **Allows Territory Capture:** off by default. Leave it off for an optional capo/underboss
  objective that must not silently reclaim the Place. Turn it on only for a real story invasion.
- **Attacker Definition Override:** optional Narrative NPC Definition for the capo or hunter.
- **Planned Force / Wave Override:** defaults to one target in the Boss Chase event; increase it
  for a finite escort or hunter squad.
- **Use Strategic Decision Roll:** off by default, so a quest event that passes its Conditions is
  deterministic. Enable it only when the story intentionally wants the profile probability.
- **Scenario ID:** optional stable name such as `CastleHill_UnderbossEscape` for save/log/outcome
  handling.

The event still requires War, valid directional routes, budgets and the force's explicit
story-pursuit staging exception. Its inherited Narrative Conditions are the correct place for
“Betrayal quest is In Progress”, “District was Claimed”, or “Underboss has not already died”.

The editor validator rejects invalid stable GUIDs, duplicate faction definitions,
incompatible Narrative NPC classes, non-positive military power, invalid force/wave
counts, duplicate approach IDs, and territories with no enabled approach. Runtime
scheduling revalidates authority, world, state, faction, profile, NPC class, diplomacy,
route, and global budgets.

## Narrative Pro reuse

### Damage authority

Territory does not calculate a second combat-damage result. A scaled assault NPC may receive
an optional self Gameplay Effect that adds to Narrative's `AttackDamage` attribute. Territory
always supplies that modifier through Narrative Pro's existing `SetByCaller.AttackDamage` tag;
there is no editable Territory damage tag anymore. The actual hit still runs Narrative's damage
effect/execution, including Attack Damage, Attack Rating, target Armor, physical-material or
head multipliers, faction attitude and friendly-fire policy.

Easy example: level 6 and `1.5 per level` supplies `7.5` through
`SetByCaller.AttackDamage` to the additive Attack Damage effect. It does not mean “deal 7.5 raw
damage now.” The sword/gun ability later makes the real Narrative damage spec.

### Player gunfire while seated

Narrative's architecture can keep player abilities alive in a vehicle: its player controller
caches the Narrative character, leaves the player ASC avatar on that character, and binds ability
input on the controller so possession of a car does not remove the inputs. Weapon targeting also
resolves the cached Narrative character and controller view.

The stock `GA_Mount_Vehicle`, however, grants `Narrative.State.Weapon.ForceHolster`. Therefore
the supplied Sedan intentionally does **not** support player gunfire from its stock seats. Do not
remove that tag from Narrative Pro content. For a project drive-by seat, create project-owned
children of the mount ability and seat behavior, omit Force Holster only on the intended passenger
seat, add seated upper-body/aim animations and limits, and make weapon traces/projectiles ignore
the occupied vehicle. Narrative's current `GetIgnoreCharacterParams()` ignores the character and
its children/attached actors, but a parent vehicle is not automatically ignored. Verify muzzle
clearance, camera, server ability activation and remote-client montage/aim before enabling it.

Physical attackers are `ANarrativeNPCCharacter` instances spawned and registered through
`UNarrativeCharacterSubsystem::SpawnNPC`, then configured through the public Narrative APIs:

- `UNPCDefinition` and `FNPCSpawnInfo`
- optional `UNPCActivityConfiguration`
- optional `UTriggerSet` overrides
- `UNPCActivityComponent`
- `UTerritoryAssaultGoal`, derived from `UNPCGoalItem`
- `UTerritoryAssaultActivity`, derived from `UNPCActivity`
- Narrative factions, ASC death delegate, navigation, and save clock

Narrative remains authoritative for `CharacterMap`, `NPCMap`, duplicate policy, controller
creation, and definition loading. A scoped Territory spawn context supplies stable spawn
identity, exact faction, activity configuration, TriggerSets, assault ID, and target before
`SetNPCDefinition`; no live pawn pointer is persisted. Goal/activity startup waits until
Narrative reports the character definition, controller/activity, appearance object, and
visual load complete. Optional weapon visuals are not a movement prerequisite.
The durable movement goal scores `2`, below Narrative Pro 2.4.2's attack goal score of `3`.
Registered-defender combat takes priority and movement resumes afterward. Strategy and physical
AI consume the same Place defence-front adapter. The target Place may receive help from loaded,
unlocked, same-owner sibling Places; its parent District grants strategic staging authority but
is never a guard container or physical objective. While at least one
live registered hostile defender exists anywhere on that front, the Territory participant temporarily suppresses
only Narrative attack goals whose `GetGoalKey()` target is not a registered defender. A
defender goal retains its exact Narrative-authored score. Original non-defender scores are
restored on final-defender removal, retirement, EndPlay, or Territory streaming, followed by
normal Narrative reselection. The framework does not rely on Narrative's currently unused
`bIsInterruptable` flag and does not add a competing spawn registry, goal generator, AI
controller, or custom assault Behavior Tree.

`UTerritoryAssaultParticipantComponent` adds the native `UTerritoryAssaultActivity` to
the spawned Narrative activity component when it is missing, then adds one finite
`UTerritoryAssaultGoal`. The assigned activity configuration therefore does not need a
separate hardcoded assault entry, but it must remain compatible with Narrative's normal
combat interruption/rescoring.

The reference project assigns `/Game/TerritoryFramework/AI/AC_TerritoryAssault`. It keeps
Narrative's attack and interaction goal generators and the Territory attack activity, but
does not include guard-only patrol or Return-to-Territory activities. The ordinary guard
definition continues to use `AC_TerritoryGuard`; assault and garrison activity policy are
therefore independently reusable without creating a second AI controller or Behavior Tree.

## Evaluation inputs

The deterministic calculator consumes:

- active, desired, maximum, and reserve guards;
- guard quality, fortification, and nearby allied support;
- attacker military power, economy readiness, and supply readiness;
- district strategic value, territorial influence, and recent faction momentum.

`TerritorialInfluence` is authored per faction force. Higher influence cannot reduce launch
probability and shortens the post-capture grace toward `MinimumInfluenceTimingScale`. It does
not change `EstimatedSuccessProbability`, which remains a finite power-versus-defence estimate.
The same timing scale accelerates only already-authored incumbent reserve deployments during
a live contest; it never creates force or rolls ownership.

Defence support never changes capture authority. For a Place target, evaluation includes the
target and loaded, unlocked, same-owner sibling Places once. City and District actors are not
physical targets and contribute no guards, posts, capture progress, or spawn routes. The force
must enter and complete the target Place's existing capture flow.

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
- strong factions can still schedule an attack against a fully guarded strategic Place unless a hard rule blocks it.

`GracePeriodGameTime` uses Narrative Pro accumulated time-of-day units, which are saved by Narrative GameState. Radii and navigation positions use Unreal units.

## Warning and proximity activation

`ScheduledWarning` and `WaitingForPlayerProximity` contain zero live attackers and produce zero capture pressure. Notifications are sent only to relevant controllers inside `NotificationRadius`; `bNotifyDefendingFactionOnly` restricts them to the defending Narrative faction.

The first relevant player entering `ActivationRadius` commits the record to `Active` before spawning. That state transition prevents two nearby players from duplicating the assault. Additional players do not create another force or wave. With the recommended `bContinueFiniteWavesAfterActivation`, the already-launched assault deploys its remaining finite reserve waves even after the player leaves. Disabling that policy pauses only later reserve deployment; it never removes the initial proximity gate.

Each wave uses deterministic three-column formation slots around every selected approach,
facing the target. `ParticipantSpacing` controls slot separation and
`SpawnPlacementAttemptsPerParticipant` bounds route/occupancy candidates. A candidate must
retain a complete navigation route and minimum separation from existing live assault pawns;
the finite reserve count is decremented only after a verified physical spawn. Narrative's
spawn API remains the creation authority.

Physical registration legitimately transitions the target from `Claimed` to
`Contested`. The lifecycle accepts that state only while the assault is `Active` and the
Territory's contesting faction exactly matches the assault attacker. Warning/grace
records, locked/unclaimed targets, and third-faction contests remain invalid and cancel.
The incumbent garrison remains registered when this transition occurs. Attackers must defeat
those actors through Narrative combat before `UTerritoryControlSubsystem` can complete capture.

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

`UTerritoryAssaultParticipantComponent` maintains its low-priority movement goal both outside
and inside the target. It selects a reachable live registered defender on the complete defence
front that the active-assault/War gate marks Hostile, then falls back to an overlapping
patrol/post objective inside the exact target and finally its center. Complete NavMesh paths
are preferred, and stable participant GUIDs distribute a wave across reachable objectives.
The fallback uses true 3D distance, so vertically stacked floors are never treated as the same
location. If the defender is not perceived yet,
non-defender attack goals are suppressed so movement continues closing the distance. Once a
defender attack goal exists, Narrative combat interrupts movement normally. After all
registered defenders are gone, exact non-defender scores are restored so the player or other
hostile targets become eligible again. `StalledMovementRetryInterval` throttles recovery and
`MaxStalledMovementRetries` converts a permanently invalid mover into one finite withdrawal,
preventing an active assault from retaining a motionless participant forever.

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

Alliance, trade agreement, non-aggression, ceasefire, Narrative Friendly, and Narrative
Neutral policy block physical scheduling or cancel an existing assault. Only an explicit
Territory `War` treaty admits combat NPCs. A stale Narrative Hostile value is repaired or
ignored; it cannot bypass the rich diplomacy state. Ownership change to the
attacking faction resolves success; ownership change to a third faction cancels it. A
locked/unclaimed target or removed/incompatible profile cancels every non-terminal phase.

The diplomacy gate runs before strongest-faction scoring and is rechecked in every
non-terminal phase. Military power cannot override a treaty or Friendly Narrative
attitude.

Ordinary defenders use an even stricter local gate: their exact Territory must be
`Contested` and the faction pair must be at `War`. A Claimed Place therefore stays peaceful on
sight even if an old NPC `Hostiles` entry exists. Physical assault characters instead require
an `Active` configured assault plus `War`; arrival is allowed to create the Contested state.

Concurrent physical attackers use the smaller of the Territory limit and Narrative's current
difficulty attack-token count when `bCapConcurrentAttackersToNarrativeDifficulty` is enabled.
This is only the strategic participation cap. Narrative remains responsible for granting its
real tactical per-defender attack token during combat.

No counterattack code calls `SetOwningFaction` or produces offscreen capture progress.

## Save, World Partition, and late join

`ATerritoryWorldState` saves and replicates `FTerritoryAssaultRecord` values: IDs, tags, counts, timing, launch mode, terminal time, probabilities, approaches, state, and resolution. It additionally saves—but does not replicate—the scheduler's `FTerritoryAssaultCycleRecord` high-water ledger. Live pawn/controller/UObject pointers are never campaign state.

District staging is deliberately fail-closed under World Partition. Only a registered,
authoritative, unlocked District derived as `Claimed` grants staging power. Locked,
`Contested`, `Unclaimed`, and unloaded Districts cannot create a phantom army. Registration and
the recurring scheduler re-evaluate when authoritative actors become available.

On authoritative load, saved live survivors become pending finite force; killed and withdrawn counts remain consumed. The normalized record is immediately republished, so late join never sees saved `AliveForce` as phantom live NPCs. Evaluation-cycle high-water marks are restored independently of the bounded assault history; a pre-ledger save reconstructs the best available marks from its retained records. A valid saved Territory GUID is authoritative and never falls back to a same-tag actor with a different GUID. Tag lookup is used only to migrate a legacy record that has no GUID; after one successful bind, the stable GUID is stored. Every physical participant and its Narrative movement goal carry the same replicated target GUID. Participants release strategic slots and controller death bindings while the target is unloaded, then register against the same-GUID replacement actor after streaming. Loaded-actor UI, debug, and Tales reads also use exact GUID matching. Client RepNotify hydrates the local counterattack read model for UI and late join.

Offscreen simulation is intentionally disabled. A future implementation must use multiple saved attrition rounds, not a hidden ownership roll.

## Blueprint and debug API

Vehicle counterattacks and boss pursuits can use a placed spline Road Guide, two-way lanes,
Narrative Mass traffic controls, chase-distance failure, a disabled-vehicle final-fight handoff,
and delayed player-safe vehicle retirement. See [23_Road_Missions.md](23_Road_Missions.md) for
the complete level-design workflow and easy examples.

Server mutations:

- `ScheduleCounterAttack(Territory, AttackingFaction)`
- `ScheduleBestCounterAttack(Territory, PreferredFaction)`
- `ScheduleStoryPursuit(Territory, AttackingFaction)`
- `GetSecureDistrictCountForFaction(Faction)`
- `CanFactionStageStrategicCounterAttack(Faction)`
- `CancelAssault(AssaultID, Reason)`

Queries:

- `GetAssault`
- `GetAllAssaults`
- `GetAssaultsForTerritory` (tag fallback, including streamed-out records)
- `GetAssaultsForTerritoryActor` (preferred exact GUID query when the actor is loaded)
- `IsAssaultActive`
- `GetAssaultDebugString`
- `GetBestEligibleAttackerPreview` (planning only; reserves no cycle and makes no roll)
- `ATerritoryAssaultCharacter::IsNarrativeSpawnReady` (live Blueprint/MCP diagnostic;
  checks definition, appearance, controller, and activity readiness without treating an
  optional weapon visual as a movement prerequisite)

Bind `UTerritoryPlayerManagementComponent::OnAssaultNotification` for the one-time strategic
warning. Bind its `OnCounterHappened` delegate for state-wise owning-client delivery, or
implement the identically named Blueprint event on `UTerritoryHUDWidget`. Delivery follows the
same relevant-faction and `NotificationRadius` policy as warnings and uses a reliable client RPC.

Blueprint notification flow:

```text
Event OnCounterHappened
  -> Switch on NewState
     ScheduledWarning -> Show Narrative HUD Notification("Counterattack incoming")
     Active           -> Show Narrative HUD Notification("The attack has begun")
     Succeeded        -> Show Narrative HUD Notification("District lost")
     Defeated         -> Show Narrative HUD Notification("Counterattack defeated")
     Cancelled        -> optional reason-specific notification
```

Use Narrative Pro's **Show Narrative HUD Notification** (`UArsenalStatics::PushHUDNotification`)
or your project presentation. TerritoryFramework does not automatically open a major/modal
notification, so combat is never paused by the framework event. The supplied HUD still renders
the older warning as a timed inline alert and exposes `OnCounterAttackAlert` for animation.
`GetAssaultDebugString`
reports target/attacking/defending factions, guard counts, reserve, attacker and defence power,
ratio, strategic value, priority, launch/success probability, finite force counts in
planned/alive/reserve/killed/withdrawn order, approaches,
notification state, grace time, roll, and human-readable state/reason enum names. When every
authored approach fails, runtime logs each approach ID, calculated world spawn, and exact nav
failure before resolving `InvalidApproachOrRoute`.
