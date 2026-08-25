# TerritoryFramework stabilization re-audit — 2026-08-25

## Outcome

This pass re-audited the complete TerritoryFramework source inventory (120 runtime C++
headers/implementations including 5 runtime test files, plus 6 editor C++ files including
2 editor test files), the project-owned
integration Blueprints/data assets used by `HopDistrictTest`, and the relevant Narrative Pro
2.4.2 public source contracts. Narrative Pro source and assets were inspected only and were
not modified.

The capture transaction itself is not the reported failure. `UTerritoryControlSubsystem`
revalidates authority, diplomacy, the attacking faction, live participants, progress, and
zero defenders before calling the atomic mutation path. `ATerritoryVolume` remains the sole
owner/state/progress authority. The visible failures came from patrol configuration
precedence, spawn-point ordering, and an incomplete strategic-to-tactical goal bridge.

## Authority map confirmed by source

| State or behavior | Existing authority retained | Narrative Pro reuse |
|---|---|---|
| Territory owner/state/progress | `ATerritoryVolume` | Tales context/events around the committed transition |
| Capture admission/progress | `UTerritoryControlSubsystem` | Narrative faction identity and ASC death state |
| Hierarchy aggregation | City/District hierarchy reducer | No competing hierarchy |
| Guard creation/patrol/combat | Territory spawn/post adapters | `UNPCDefinition`, character subsystem, activity configuration, goals, TriggerSets, Narrative controller and GAS |
| Strategic assault lifecycle | `UTerritoryCounterAttackSubsystem` | Physical Narrative NPC force and Narrative campaign clock |
| Strategic attack slots | `UTerritoryCombatDirector` | Narrative attack-token system remains tactical authority |
| Diplomacy metadata | `UTerritoryDiplomacySubsystem` | `ANarrativeGameState` remains combat-attitude authority |
| Money and item quantities | Narrative inventories/accounts | Territory economy owns rates, ledgers, recipes, and settlement policy only |
| Persistence/late join | `ATerritoryWorldState` plus each savable Territory/post | Narrative save interfaces and stable editor-authored GUIDs |
| HUD/menu composition | Project widgets on Narrative CommonUI stacks | Narrative gameplay HUD, activatable layers, navigation widgets |

## Confirmed defects repaired in this pass

### 1. A newly recruited guard could stand still

The Blacksmith setup contains multiple equal-priority posts. One priority-100 post is
intentionally static and another priority-100 post has a three-node patrol. The previous
sort compared priority only, so equal entries had no stable or patrol-aware order. The
first recruited guard could occupy the static post.

The runtime now sorts by priority, then patrol capability, then stable actor path. This
preserves explicit higher priority while making a patrol-capable post win an equal-priority
tie. Public patrol getters now consume the effective route and loop policy from
`UTerritoryGuardPostDefinition` when an inline route is absent; previously the data asset
could be correctly assigned yet the patrol Blueprint received an empty array.

The editor validator now validates reusable guard-post assets directly. A one-node route
is rejected as misleading: author at least two nodes or leave it empty for an intentional
static sentry. Invalid transforms, waits, reserve bounds, and Narrative NPC definitions are
also reported.

### 2. Counterattack NPCs preferred the player over an assigned District guard

The first movement repair was incomplete. `UTerritoryAssaultParticipantComponent` did begin
moving toward the nearest registered defender, but it yielded to every different Narrative
goal without checking the goal target. Narrative's attack generator gives combat a higher
base score than Territory movement, and its EQS also scores distance, facing, attack tokens,
and character attack priority. Once the player was perceived, the player attack goal could
therefore interrupt movement and win selection even while an assigned guard remained alive.

The participant now keeps its low-priority Territory assault goal moving toward the nearest
live registered hostile defender, falling back to the territory center. It discovers the
configured Narrative attack-goal class through Narrative's public `GetGoalKey` contract and,
while a live registered hostile defender exists, temporarily suppresses only attack goals
whose target is not one of those defenders. A perceived defender retains its exact
Narrative-authored score. When the last defender dies, unregisters, or streams out, every
temporary score is restored and Narrative immediately reselects normally. Narrative still
owns perception, hostility, attack activities, behavior trees, GAS, attack tokens,
interruption, and combat. No custom AI controller, goal generator, or parallel combat tree
was introduced.

### 3. Influence affected neither immediate pressure nor all reinforcement waves

Faction force definitions now expose normalized `TerritorialInfluence`. Influence increases
strategic launch probability and reduces post-capture grace, while remaining separate from
`EstimatedSuccessProbability`. It also accelerates each already-authored finite defender
reserve wave during an opposing live contest, including a pending timer restored from save.

This does not create force or change ownership. Player recruitment remains blocked while
contested. A reserve is consumed only after one collision-safe Narrative NPC is physically
spawned and registered. When the finite reserve is exhausted, no replacement is possible.

### 4. Diplomacy client projection could retain ghost or stale treaties

The diplomacy subsystem was not empty: it already owned alliance, trade, non-aggression,
war, ceasefire, expiry, reputation/history, Narrative attitude synchronization, and save
restore. The defect was its live `ATerritoryWorldState` projection. A transition to `None`
could leave a row with state `None`, rich timing metadata was not refreshed, and diplomacy
history did not update clients until the next save/export.

The live bridge now removes absent treaties, copies the authoritative rich record, preserves
metadata during a transient delegate-ordering gap, uses the canonical pair identity, and
replicates a bounded live event history.

### 5. Territory widgets were operational but poorly composed

The project-owned widgets were redesigned without replacing Narrative CommonUI:

- the modular capture HUD is now an on-screen top-left card instead of an off-screen lower slot;
- the journal uses a wide header, one KPI strip, and queue/directory/command columns;
- district management uses a wider scrollable operations panel and a bounded close action;
- economy uses separate summary and operations cards with practical resource/production viewports;
- surfaces, spacing, and contrast were normalized while preserving existing graph-bound widget names.

All five changed widget Blueprints compile with zero warnings/errors, and CommonUI audits
reported no structural issues. Only these packages were explicitly saved.

### 6. Guard and assault NPC definitions shared a persistent identity

MCP inspection found that `NPC_TerritoryBandit` and `NPC_TerritoryBanditAssault` had the
same `UNPCDefinition::UniqueNPCGUID`, inherited from asset duplication. Their `NPCID` and
`CharacterID` values were distinct, and Territory's multi-instance assault spawning already
uses explicit finite spawn GUIDs, so this was not the tactical targeting cause. It was still
an invalid persistent-identity contract and a future Narrative save/tether collision risk.
The assault definition now has a distinct editor-authored GUID; the guard definition and all
Narrative Pro assets were left unchanged.

### 7. A saved assault could rebind to the wrong Territory after streaming or tag reuse

An assault record already stores the target Territory GUID, but several lookup paths still
matched only the Territory tag, and restore fell back to the tag even when the saved GUID was
valid but its actor was unloaded. A replacement actor that reused the tag with a different
GUID could therefore receive, cancel, or resolve the old assault. A valid GUID record could
also be discarded when the tag had been renamed.

The target GUID is now authoritative whenever present. Tag matching is retained only as a
bounded migration for legacy records that have no GUID. Registration reconciles a legacy tag
to one stable GUID and updates a renamed tag only when the same GUID returns. Scheduling,
recurring lookup, ownership callbacks, restore, and streamed registration all use the same
identity rule. Regression coverage proves wrong-GUID/same-tag rejection, GUID-only renamed-tag
restore, legacy binding, and GUID-preserving rename migration.

### 8. Story-Locked Districts blocked every Bandit counter in `HopDistrictTest`

MCP inspection found both loaded Districts owned by `Narrative.Factions.Bandits`, but both use
the modular `Locked` state for story gating. Strategic admission counted only `Claimed`
Districts, so the Bandits appeared to own zero staging bases and no normal counterattack could
be scheduled. This was the concrete map-level reason the reported counter did not happen.

A valid owner now receives staging credit from a loaded District in either `Claimed` or
story-`Locked` state. `Contested`, `Unclaimed`, unloaded, or differently owned Districts still
fail closed. This keeps the state-config model modular: `Locked` controls story access without
erasing military ownership. Tooltips and community documentation include the simple example,
and the runtime regression covers Claimed, Locked, Contested, and streamed-out cases.

Live proof after the fix reported two secure Bandit Districts in both MCP PIE worlds. An atomic
Blacksmith capture for Heroes then created exactly one Bandit assault in `Grace`; its authored
influence-adjusted delay was 187.5 Narrative campaign-time units. Advancing Narrative time
moved the same finite record to `WaitingForPlayerProximity` with six pending attackers, zero
living attackers, zero capture pressure, a valid `Blacksmith_WestRoad` approach, and a stored
decision roll below its 0.477 launch probability.

### 9. Physical participants and read models still lost stable target identity

The durable assault record used a stable target GUID, but each live
`UTerritoryAssaultParticipantComponent`, its Narrative movement goal, the CombatDirector slot
check, recurring duplicate admission, command-center threat projection, gameplay debugger,
and loaded Tales queries still used only the readable Territory tag. A streamed replacement
with the same tag but a different GUID could therefore inherit physical pressure or display,
cancel, or block the old assault.

Every physical participant now carries the target GUID from the authoritative spawn context,
replicates it, passes it into the Narrative goal, and resolves/registers/releases pressure by
GUID. Recurring admission and strategic slots use the same exact identity. Loaded UI, debug,
and Tales consumers call `GetAssaultsForTerritoryActor`; tag-only lookup remains available only
when no actor is loaded or for bounded legacy migration. The stable rebind regression proves
wrong-GUID rejection at durable, physical, duplicate-scheduling, and read-model layers.

### 10. The public ownership node skipped the capture transaction callback

The Blueprint **Set Owning Faction** node committed the Territory actor directly. Its local
state and delegates changed, but the control subsystem's verified
`OnTerritoryControlChanged` transaction result did not fire, so strategic counter scheduling
and the WorldState live projection could be missed.

The compatibility node now builds one `FTerritoryMutationRequest` and routes through
`UTerritoryControlSubsystem::ApplyTerritoryMutation`. Lock, diplomacy, state-config
conditions, guards, capture cleanup, counters, save snapshots, and replication therefore see
one verified result. Detached native policy fixtures retain a no-world direct path only.
Blueprint metadata explains that **Apply Territory Mutation** is preferred when a developer
needs explicit instigator/Tales context and the structured success or failure response.

### 11. Active save restore could briefly replicate phantom living attackers

Restore correctly converts saved live assault NPCs into finite pending reserve because live
pointers are not campaign state. `ATerritoryWorldState` previously retained the serialized
pre-normalization array until a later assault update, so a late join could briefly read the old
`AliveForce` count. Authority now republishes the normalized scheduler state immediately and
forces one network update. The Narrative-compatible archive regression covers the exact
active-force case and verifies zero phantom living attackers plus the preserved finite reserve.

### 12. Return-to-Territory activity still dereferenced a dying controller at start

Project-owned `BPA_ReturnToTerritory` already guarded its cleanup and sequence paths, but
`K2_RunActivity` read `OwnerController` and sent the weapon-sheathe Gameplay Event without a
validity check. If Narrative destroyed the controller between scoring and execution, this
matched the reported pending-kill Blueprint error. The graph now validates the actual
`OwnerController` before `GetControlledNPC`/`SendGameplayEventToActor`, compiles cleanly, and
has an editor graph-contract regression. Narrative's vendor `BPA_Attack`, `BPA_FollowCharacter`,
and `NE_GiveXP` assets remain unmodified. Territory's configured Blacksmith XP event is already
gated by `UTerritoryEventContextCondition`, which requires a live player pawn and ASC.

## Counterattack lifecycle trace

| Transition | Authored by | Persisted/replicated by | Player presentation / proof |
|---|---|---|---|
| capture committed | Control subsystem -> atomic Territory commit | Territory `OwnershipData` SaveGame + RepNotify | capture HUD and control-change delegate |
| former owner candidate | counterattack control-change handler | durable assault ID and evaluation-cycle ledger | operations queue |
| influence-adjusted grace | counterattack scheduler | grace end time in assault record | queue/watch state |
| deterministic evaluation | profile + stored seed/cycle/input | result and decision roll in record | debug/operations read model |
| scheduled warning | scheduler state transition | WorldState assault snapshot | relevant-controller warning only |
| wait for player proximity | scheduler | state/notification bookkeeping | zero NPCs and zero capture pressure |
| one-time activation | atomic proximity state commit | activated time and finite counts | first relevant player only; later players do not duplicate |
| physical deployment | Narrative character subsystem | IDs/counts only; never live pointers | typed approach + complete navigation route required |
| movement/combat | Territory target policy + Narrative goal selection | transient only; never saved or replicated | non-defender attack goals are suppressed until registered defenders are gone, then exact Narrative scores are restored |
| capture participation | control subsystem identity sets | Territory replicated contest state | only live registered attackers add pressure |
| casualty/withdrawal | participant exact-once retirement | killed/withdrawn/alive/pending counts | death immediately removes capture pressure |
| success | existing capture flow only | Territory ownership + terminal assault | no probability ownership roll |
| exhaustion/recovery | counterattack resolve + control decay/reset | terminal record | zero alive + pending resolves defeated |
| load/World Partition | WorldState restore + registry callback | stable GUID/tag records and cycle high-water | waits for streamed Territory; decision is not rerolled |

## False positives ruled out

- Counterattack probability does not call `SetOwningFaction` and cannot capture offscreen.
- Scheduled warnings and proximity waiting do not spawn NPCs or register pressure.
- Capture completion already requires zero live registered defenders and a valid active
  attacking faction.
- Guard and assault NPCs use Narrative spawning, faction identity, activity/goals,
  TriggerSets, controller, GAS death state, and navigation rather than a duplicate NPC stack.
- Diplomacy is a real metadata subsystem, not an empty stub; its missing product layer is
  proposal policy, player UX, economy/Tales consequences, and richer outcomes.
- Real currency is not duplicated in WorldState; Narrative inventory remains authoritative.

## Remaining stabilization blockers

The clean-restart code/content gate is now complete. UnrealHeaderTool and the full
`TDAEditor Win64 Development` runtime/editor link succeeded, all 119 loaded
`TerritoryFramework.*` tests passed, 63 Territory/map assets passed validation with zero
errors and zero warnings, and the repaired map passed a 15-second PIE smoke with no Blueprint
runtime or `Accessed None` errors. The Blacksmith XP event now requires an explicit player pawn
with a valid Ability System, and Territory NPC removal stops Narrative activity scoring before
the cached controller can become pending-kill.

The follow-up release run completed the native save/load and load-order gates. Clean live MCP
runs produced zero Blueprint runtime, `Accessed None`, pending-kill controller, XP Ability
System, assert, or capture-invariant errors. A later verification found that the MCP smoke
runner's apparent server/client worlds were not a valid network baseline: the playable pawn
remained `ROLE_Authority`, the server world had no connected player controller, and no
`Join succeeded` line existed. Those runs are therefore recorded only as multi-world editor
smokes, not as dedicated-server/two-client proof. The save regression covers Grace, Scheduled
Warning, Waiting For Player Proximity, Active, and partially depleted finite forces. A second regression
serializes `ATerritoryWorldState` through the same `ArIsSaveGame` archive contract used by
Narrative Save, restores the deterministic cycle ledger, and proves casualty/reserve counts do
not reroll. A streamed-target regression unregisters the live Territory, keeps the same durable
assault, rejects tag reuse by a different GUID, and rebinds a replacement actor by the same
editor-authored GUID without duplication.

The remaining blockers are narrower release-environment gates: run the complete physical
capture/recruit/patrol/assault/death sequence with both clients, perform a real World Partition
cell unload/reload (the native registry replacement path is covered), and package with an
engine/content set that can pass the external cook and Server-target blockers documented in
`REMEDIATION_2026-08-24.md`.

Additional debt found but deliberately not folded into this focused repair:

1. `ATerritoryVolume` still exposes low-level Blueprint setters for state and progress.
   Ownership now uses the validated control transaction, but state/progress nodes remain easy
   to misuse as a chained transition. Deprecate them only with a Blueprint migration pass
   toward structured control-subsystem requests.
2. `ATerritoryWorldState` uses whole-array replication. Move high-volume histories to
   `FFastArraySerializer` after behavior is stable.
3. The capture summary is a client read model, not ownership persistence, and is refreshed
   mainly at ownership/export boundaries. UI for loaded Territories should continue reading
   the replicated Territory actor; a future streamed global map read model needs an explicit
   progress publication contract.
4. The editor module provides strong validation and migration tests but no custom authoring
   toolkit. A patrol/approach visualizer, GUID repair command, and one-click setup audit would
   materially improve level-design safety.

## Release-environment gates after the clean restart

Completed:

1. Built `TDAEditor Win64 Development`; UHT, runtime/editor DLLs, and vendor links succeeded.
2. Ran all `TerritoryFramework.*` automation tests, including
   `CounterAttack.Regression.RegisteredGuardOutranksPlayerGoal`,
   `Editor.Identity.ProjectNPCDefinitionsUseDistinctGuids`, and the new influence, patrol,
   contested reserve, diplomacy projection, save/restore, death, reward-context, and editor
   validator cases: 119/119 passed.
3. Validated `HopDistrictTest`, guard-post definitions, the counterattack profile, assault NPC
   definitions, and UI Blueprints: 63 valid, zero warnings.
4. Ran clean MCP multi-world smokes with zero targeted runtime errors, but rejected them as
   network proof after role/controller/log inspection showed no connected client. A direct
   headless `UnrealEditor` server attempt is also blocked before world startup by Narrative
   Pro's editor-only save-menu module constructing Slate without a Slate application. No
   Narrative Pro source was changed to hide this external failure.
5. Passed all four `CounterAttack.SaveLoad.*` tests, including the Narrative-compatible
   WorldState archive round-trip and stable streamed-target rebind.
6. Built the non-editor `TDA Win64 Development` game target successfully.
7. Ran a scoped Windows cook. It processed all 6,960 discovered packages and found no
   TerritoryFramework warning/error, but the commandlet failed with 271 project/vendor content
   errors and 236 warnings. The failures are missing/NeverCook Narrative demo dependencies and
   Narrative vehicle Vulkan material shader errors, not this Territory batch.
8. Rebuilt both `TDAEditor Win64 Development` and `TDA Win64 Development` after the final
   stable-participant/read-model changes. Six focused regressions and the full 119-test
   `TerritoryFramework.*` suite passed. In live MCP PIE, invoking the real Blueprint
   `Set Owning Faction` node on Blacksmith changed Bandits to Heroes and immediately created
   exactly one Bandit `Grace` record with Blacksmith's same stable GUID. The editor then
   reported zero dirty packages, errored Blueprints, `Accessed None`, Blueprint runtime,
   pending-kill, or `NE_GiveXP` log matches.
9. Re-audited the final pre-push tree for global player-controller lookup, runtime `/Game`
   dependencies, random-on-Tick assault decisions, saved live object pointers, direct ownership
   bypasses, and missing server-only Blueprint contracts. No new runtime violation was found.
   Two contained editor/API defects were fixed: optional TDA integration fixtures now skip only
   when wholly absent from a community project, while partial fixtures still fail; and guard-post
   registration/removal nodes are now marked `BlueprintAuthorityOnly`. UHT, `TDAEditor`, and
   non-editor `TDA` builds passed, followed by 119/119 automation tests with revision control
   deliberately disabled via `-SCCProvider=None` so Git asset discovery could not block the run.

Still required for release:

1. Run a genuine dedicated server with two connected clients, then repeat the complete physical
   scenario: capture Blacksmith, recruit
   one guard, verify continuous patrol, activate one assault, verify attackers engage registered
   guards before players, verify exact-once deaths, and confirm XP is awarded only through the
   real player/Ability System context. Each native policy regression passes, but the MCP smoke
   runner did not connect its client and the headless editor server is blocked by the external
   Narrative editor-module Slate assertion.
2. Perform one real disk-slot save/load while a physical assault has living Narrative NPCs.
   The complete durable-state and Narrative-compatible archive behavior is covered natively;
   this final run checks project save-slot configuration and live pawn reconstruction together.
3. Stream the target and its posts out/in in a real World Partition cell; verify no duplicate assault,
   defender, post, or ownership record. `HopDistrictTest` is not a World Partition map: it has
   no actor descriptors or `__ExternalActors__/HopDistrictTest` content, so it cannot satisfy
   this release gate.
4. Cook/package Win64 and run a packaged dedicated-server smoke test after the documented
   project/vendor cook defects and installed-engine Server-target limitation are cleared. The
   installed binary engine reports that Server targets are unsupported, so this needs a source
   engine or a distribution with server support.

## Product roadmap after the release gate

### Phase 1 — player-facing diplomacy

Keep Narrative faction tags and `UTerritoryDiplomacySubsystem` as the only authorities.
Add a structured server mutation request/result, proposal/accept/reject lifecycle,
reputation thresholds, trust/fear/influence, treaty costs and cooldowns, war exhaustion,
trade income/supply effects, replicated proposal read models, CommonUI diplomacy screen,
and Tales conditions/events/tasks. Peace, alliance, non-aggression, trade, and ceasefire
must continue to block/cancel assaults consistently.

### Phase 2 — domination cascade

Build on the finite contested reserves added here: linked-territory support routes, alarm
state, radio/convoy warning, commander survival bonuses, sabotage of reinforcement and
supply infrastructure, patrol heat, and temporary fast-travel lockdown. Every reinforcement
remains a finite physical Narrative NPC. Offscreen ownership remains disabled.

### Phase 3 — Far Cry-like outpost loop

Use Narrative POIs/map/minimap/compass for intel, alarm towers, reinforcement routes,
commanders, prisoner/rescue opportunities, loadout/vendor access, and post-capture services.
Represent strategic changes with Territory profiles, tags, activities, TriggerSets, and
Tales content—not another faction, quest, navigation, inventory, or AI framework.

### Phase 4 — Cargo shop/interior integration

Treat each useful interior as a Territory Property or service POI with an editor-authored
GUID, Narrative vendor inventory, Territory production profile, management point, map marker,
guard posts/patrol routes, and explicit World Partition/navmesh validation. Start with one
vertical slice (for example Blacksmith or market shop) before bulk-converting the asset pack.

### Phase 5 — story, quests, dialogue, and cutscenes

Author the campaign in Narrative Tales: quests, dialogue, conditions, tasks, events, and
Narrative level-sequence/cutscene APIs. Territory ownership, alarms, diplomacy, production,
and assault outcomes become conditions and consequences. A practical first arc is:

1. learn the outpost loop and capture a vulnerable Property;
2. choose allies/suppliers and expose the defeated faction's reinforcement network;
3. assault a strategic District while previous territory/diplomacy decisions change the
   physical force, approach routes, support, and ending.

Do not begin bulk story or asset integration until the clean-restart release gate passes.

## Change impact

- **Authority:** unchanged; all new mutations remain server-only.
- **Replication:** richer live diplomacy snapshots/history plus the physical participant's
  transient target GUID; no live pawn pointers added. Normalized active-assault counts publish
  immediately after authoritative restore.
- **Save/load:** new influence fields default to neutral `0.5`; existing assault records keep
  stored inputs/rolls and do not reroll. Pending finite reserve timers resume through the
  same influence-aware delay policy.
- **Blueprint migration:** no node removal in this batch. `Set Owning Faction` now uses the
  validated control transaction; `Get Assaults for Territory Actor` is the preferred exact
  loaded-actor query. New profile fields are optional.
- **Narrative Pro:** public APIs reused; zero vendor files modified.
