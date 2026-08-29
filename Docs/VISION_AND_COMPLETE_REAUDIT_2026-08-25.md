# TerritoryFramework vision and source/editor re-audit — 2026-08-25

## 1. The game and plugin vision

TerritoryFramework is a community extension for Narrative Pro. It is not a second game
framework. Narrative Pro remains responsible for factions, AI attitudes, NPC definitions,
activities, combat, inventory, Tales, saving, navigation, maps, and the gameplay HUD.
TerritoryFramework adds the domination layer around those public systems.

The intended player loop is:

1. Learn why a place matters through a Narrative quest or dialogue.
2. Reach the physical place and defeat its real defenders.
3. Capture it through the existing Territory capture subsystem.
4. Decide how much profit to keep and how many guards to fund.
5. See the political and military consequences.
6. Defend it when a finite physical counterattack arrives.
7. Let later story decisions change which faction the player serves and who is hostile.

The framework must support Far Cry-style domination without hard-coding one story. A
developer supplies faction tags, quests, dialogue, NPC definitions, attack approaches,
conditions, events, rewards, and widgets. The plugin supplies consistent rules.

## 2. Easiest betrayal example

The player begins in `Narrative.Factions.Heroes` and works for
`Narrative.Factions.Regime` against `Narrative.Factions.Bandits`.

- Early game: a Tales quest asks the player to investigate Bandits. Captures may award
  Districts to the Regime.
- Middle game: dialogue reveals that the Regime caused the crisis.
- Betrayal: a Tales event changes the Regime/Heroes relationship to War and changes the
  player's Narrative faction membership if the story requires it.
- Immediate result: Narrative AI attitude becomes Hostile; Territory capture validates
  the new exact faction; peace/alliance rules no longer admit the old relationship; and
  waiting counterattacks are revalidated.
- Later choice: the player may join Rebels, stay independent, or rebuild trust. Those
  choices should be authored as Tales conditions/events and rich Territory treaty data,
  not hard-coded in a capture widget.

Current support is enough for a base betrayal: Narrative faction identity and attitude
can change at runtime, capture-point registration reconciles a changed faction, and the
capture/counterattack systems re-check diplomacy. A later feature batch is still needed
for pair-specific trust/fear/influence, treaty proposals, costs, cooldowns, and story UI.
The existing single `Faction -> Reputation` integer is not enough for a nuanced betrayal.

## 3. One authority for each responsibility

| Responsibility | Existing authority | Important rule |
|---|---|---|
| Owner, state, progress, local configuration | `ATerritoryVolume` | One atomic ownership record |
| Capture admission and active progress | `UTerritoryControlSubsystem` | Server only; physical participants |
| City/District derived state | Hierarchy reducer | Parent state comes from children |
| Registration and loaded spatial lookup | `UTerritoryRegistrySubsystem` | Never invent a second registry |
| Currency | Narrative inventory/account | Territory only calculates rates/policy |
| Treaty metadata and reputation | `UTerritoryDiplomacySubsystem` | Narrative GameState remains AI-attitude authority |
| Save and late-join read models | `ATerritoryWorldState` | Stable values, never live pawn pointers |
| Assault decision/lifecycle | `UTerritoryCounterAttackSubsystem` | Deterministic and finite |
| Strategic attacker slots | `UTerritoryCombatDirector` | Separate from Narrative attack tokens |
| Tactical combat | Narrative activities, GAS, tokens | No custom parallel combat AI |
| HUD/menu styling | Narrative CommonUI + project widgets | C++ exposes state and hooks |

Narrative Pro source was inspected as the authority but was not modified.

## 4. Full counterattack lifecycle traced

```text
Captured
-> grace saved in the assault record
-> deterministic evaluation on the server
-> optional warning (zero attackers and zero capture pressure)
-> waiting for a relevant player to enter activation radius
-> one physical activation for one durable AssaultID
-> Narrative NPC spawn from UNPCDefinition/FNPCSpawnInfo
-> Narrative activity configuration and TriggerSets
-> approach/navigation verification
-> attacker registration with UTerritoryControlSubsystem
-> Narrative combat and casualties
-> capture succeeds through the normal capture flow
   OR alive + pending reserve reaches zero and assault is defeated
-> capture pressure decays/resets through the capture subsystem
-> state, casualties, decision cycle, and read model save/replicate
```

Confirmed safeguards in current source:

- Probability never calls `SetOwningFaction`.
- Warning/waiting states do not spawn or generate progress.
- Activation is server-owned and is not duplicated by a second nearby player.
- Assault force is finite and casualty counts are durable.
- Dead/withdrawn attackers are removed from capture participation.
- Diplomacy can block or cancel an assault.
- Launch probability and estimated success probability remain separate.
- The registered-guard target preference uses Narrative's public goal API; no custom AI
  controller, Behaviour Tree, Blackboard, or combat authority was introduced.

## 5. Confirmed findings and resolution status

### A. Counterattackers ignored the assigned player guard

Root cause: Narrative could have simultaneous attack goals for the player and the
registered guard. The player goal could have a higher score, so the assault NPC chased the
player as soon as the player entered the activation area.

Current source resolution: `TerritoryAssaultTargetPolicy` temporarily suppresses only
non-defender Narrative attack-goal scores while at least one live registered hostile
defender exists. The registered guard keeps Narrative's authored combat score. Original
scores are restored exactly when the final defender disappears. Combat, GAS, perception,
and tactical attack tokens remain Narrative-owned.

Regression:
`TerritoryFramework.CounterAttack.Regression.RegisteredGuardOutranksPlayerGoal`.

### B. Three competing lock systems

Confirmed old paths:

- `Starts Locked` changed BeginPlay state directly.
- `Lock Conditions` controlled `TryUnlock`.
- `State Configs[Locked].EntryConditions` controlled entry into Locked.
- Generic state/owner setters could use a different validation path.

Resolution:

- New visible `Initial State`: Automatic, Unclaimed, Claimed, or Locked.
- `State Configs` now owns both Entry Conditions and Exit Conditions.
- Locked Exit Conditions are the normal unlock rules.
- All state changes pass through `CommitOwnershipData` and validate old-state exit plus
  new-state entry rules before one atomic commit.
- The old serialized lock fields and migration functions were removed after their values were
  reconciled into the Definition assets.
- HavenReach, MarketSquare, and CastleHill receive their initial state from their assigned
  City/District Definitions.
- Placed actors cannot override the Definition's state rules.

Easy example: put “Quest OpenCityGate is complete” in
`State Configs -> Locked -> Exit Conditions`. Put “Play gate opening sequence” in
`Locked -> Exit Events`.

### C. Capture HUD was too large and too dark

Confirmed editor values were 440 x 246 with an almost opaque black `#010202F5` surface.
It was attached to the existing Narrative gameplay HUD, which is the correct stack.

Resolution:

- Capture widget reduced again to 328 x 108.
- Surface changed to a lighter translucent teal-gray; the long description moved to the menu.
- Padding and vertical spacing reduced.
- The HUD keeps its compact top-left Narrative gameplay-layer placement.
- Both widgets compile, save, and pass CommonUI/focus audits.

The combat card shows place, owner, state, pressure, and progress. Garrison, finance,
production, threat, and diplomacy detail live in the Territory menu instead of covering play.

### D. Command center looked limited

Current C++ and widgets provide search, operational filters, available/unlocked, owned/managed,
garrison, economy, production, threats, and diplomacy. The journal follows Narrative's
active/completed quest-list pattern and groups visible Districts by City. Locked Districts and
Places do not appear in the player list; a locked or unverifiable unloaded parent hides its
descendants. Developer/debug read models can still request the complete registered projection.

Real remaining limit: the directory is built from loaded registered District actors.
World Partition can therefore hide an unloaded District from strategic UI. The correct
future fix is a stable `ATerritoryWorldState` projection keyed by Territory GUID/tag, not a
client query of the server registry and not a second Territory database.

### E. Diplomacy live-replication test

A newly loaded test exposed a false fixture: an actor spawned in an EditorPreview world
has `ROLE_None`, so every correct server-authority handler returned early. The fixture was
changed to an authoritative transient WorldState. Production code already updates rich
treaty rows after the subsystem mutation, removes `None` ghost rows, caps history at 500,
and forces a network update.

This still needs a real dedicated-server/two-client functional gate before release.

### F. Counterattack record existed but no counter arrived

Confirmed runtime root cause: Blacksmith's relative approach was transformed by the scaled
Territory actor to `(-5022.61, 795.41, 0)`. That point was about 1,070.6 cm from the nearest
navigable ground, but physical assault admission deliberately allows only 500 cm. Evaluation
therefore cancelled with `InvalidApproachOrRoute` before the warning, proximity activation,
force assignment, or spawn.

Resolution:

- `Blacksmith_WestRoad` now resolves to the verified navigable world point
  `(-3952, 795.4146, 3.0717)` and has a complete path to Blacksmith.
- Farm now has `DA_CounterAttack` and the verified `Farm_WestField` route at
  `(2200, 0, 3.0717)`.
- Runtime and editor validation share `ValidateNavigationRoute`; the validator can no longer
  pass a built-map approach that runtime will reject after grace.
- Cancelled records and the gameplay debugger show `State=Cancelled` and
  `Reason=InvalidApproachOrRoute` instead of numeric enum values.
- Runtime logs the approach ID, calculated world spawn, and exact projection/path failure.

Exact PIE lifecycle after the repair:

```text
Heroes capture Blacksmith
-> State Config changes Heroes/Bandits Alliance to War
-> one Grace record
-> Narrative time advances past grace
-> ScheduledWarning / proximity gate
-> one Active assault, roll 0.2369 <= LaunchP 1.0
-> 3 physical Bandit Narrative NPCs + 3 finite pending reserve
-> attackers register with the existing capture subsystem
-> Blacksmith becomes Contested and progress changes physically
-> all six finite participants are withdrawn exactly once
-> Defeated / AllAttackersRemoved
-> Heroes remain owner; no probability changed ownership
```

### G. Reusable State Config story adapters

Added easy-English, `EditInlineNew` Narrative classes:

- Diplomacy Condition: exact treaty state between two Narrative faction tags.
- Garrison Condition: active, desired, maximum, or finite reserve with a clear number comparison.
- Set Diplomacy Event: changes the rich treaty and synchronizes Narrative AI attitude.
- Modify Reputation Event: adds to or sets the existing saved faction reputation integer.

Demo examples are data-authored. Blacksmith Claimed entry sets Heroes/Bandits to War. Farm
Locked exit requires both “Blacksmith owned by Heroes” and Heroes/Bandits War. Core C++ does
not contain a Bandit, Heroes, Regime, betrayal, or reward rule.

### H. Defender-death hooks and expanded Tales authoring

Added seven reusable conditions and five mutation events, for a total of twelve new
community-facing Tales nodes:

- State, control-progress, reputation, assault, character-presence, production-status,
  and Narrative-resource conditions.
- Schedule finite enemy wave, cancel enemy waves, set desired garrison, upgrade one
  property level, and execute one resource recipe events.

Every Territory event can use Narrative Pro's inherited `Conditions` array. The adapter also
honours the inherited `Not` option. Mutation events do not refire while a quest is restored,
so loading cannot purchase, reward, or schedule a second time.

`On Defender Died Events` runs for each registered defender death. `On All Defenders
Defeated Events` runs only after the registered defender set and pending reserve deployments
are both empty. Unregistered and duplicate death callbacks are ignored, so mutation events do
not refire for the same casualty. A death is a momentary trigger, while diplomacy, owner, guards, assault state,
and resources are conditions checked at that moment.

The placed Blacksmith example now reads:

```text
All defenders defeated
AND Heroes/Bandits diplomacy is War
-> Schedule one finite Bandit wave against Blacksmith
```

This schedules through `UTerritoryCounterAttackSubsystem`; it does not directly spawn an
unlimited army or change ownership. The durable assault record remains saved and replicated,
while the defender-death callback itself is intentionally not replayed during load.

## 6. Save, replication, authority, World Partition, and migration

- Initial State affects only a fresh campaign. Saved ownership/state remains authoritative.
- Legacy Starts Locked and Lock Conditions are not saved campaign state; they are editor
  configuration migration inputs.
- `OwnershipData` remains the replicated/save record. No second lock field is replicated.
- Transition conditions are evaluated only by the authoritative server.
- Forced quest/save/admin paths remain explicit and can bypass conditions deliberately.
- No live pawn/controller/subsystem pointer was added to saves.
- The lock refactor does not change stable Territory GUIDs or tags.
- World Partition load order still requires the registry and WorldState projection rules;
  the UI-wide unloaded-District projection remains open work.

## 7. Community-facing metadata standard

Important author options now use plain English plus an example:

- Territory state and new Initial State.
- Control mode.
- Post-capture garrison policy.
- Income payout policy.
- State entry/exit conditions and events.
- Diplomacy states and treaty timing.
- Counterattack approach types and finite force fields.
- Command-center filters and threat levels.

Metadata must explain what owns the behavior and what it does not do. Example:
`Military Power` changes strategic planning; it does not directly capture a District.

## 8. Verification performed in this batch

- UnrealHeaderTool succeeded.
- Runtime and Editor modules compiled and linked.
- Both edited widgets compiled and were saved.
- Capture HUD CommonUI and focus audits: zero issues.
- New initial-state migration test: passed.
- New atomic entry/exit condition test: passed.
- Exact registered-guard-over-player goal test: passed after fixing its Narrative component
  fixture.
- Added route-diagnostic, State Config Narrative-extension, and placed-map configuration
  regressions.
- Added an exact defender-death event-condition regression and extended the placed-map
  regression to verify the diplomacy-gated finite Bandit wave.
- Full loaded TerritoryFramework automation run after the final rebuild: **117/117 passed**.
- `/Game/TerritoryFramework` plus `/Game/HopDistrictTest` Unreal data validation: **63 valid,
  zero errors, zero warnings** after naming the standard guard-post definition.
- Loaded Blueprint error scan: zero errored Blueprints.
- Capture HUD and modular gameplay HUD CommonUI audits: zero issues.
- Replication audit: zero `ReplicatedUsing` properties with a missing OnRep function.
- Exact repaired counterattack PIE lifecycle: one durable record, physical Narrative spawn,
  capture registration/progress, finite-force exhaustion, and `AllAttackersRemoved` defeat.
- Fifteen-second PIE smoke on `/Game/HopDistrictTest`: passed with no Blueprint runtime
  error, Accessed None, ensure, assertion, or chooser error during active play.
- Exact reward-context and Narrative activity-removal regressions passed. Blacksmith `NE_GiveXP`
  now requires a player-controlled Target with a valid ASC, and dead/removed Territory NPCs can
  no longer rescore activities through a pending-kill Narrative controller.

Monolith's heuristic module-dependency audit reported false positives for Narrative Pro
types because it identified the plugin folder name as the module. The actual declaring
runtime module is `NarrativeArsenal`, it is declared in `TerritoryFramework.Build.cs`,
and the clean Runtime/Editor link proves that dependency is present.

## 9. Honest remaining release work

Do not call the complete framework release-ready until these gates pass:

1. Run a dedicated-server smoke test with at least two clients.
2. The physical assault and finite-force path now passes in PIE, and the exact automated
   registered-guard priority regression passes. A final human/packaged playtest should still
   run the combined presentation scenario: one Heroes guard, player enters, guard is visibly
   preferred, guard dies once, and combat presentation remains clear.
3. Save/load during warning, waiting, active assault, and after casualties.
4. Verify a late join sees the same treaty, assault, garrison, and Territory snapshots.
5. Run cook/package smoke.
6. Build the WorldState-backed unloaded-District operations directory.
7. Design pair-specific political reputation and Tales adapters for the later betrayal
   story batch; do not overload the current global faction reputation integer.
