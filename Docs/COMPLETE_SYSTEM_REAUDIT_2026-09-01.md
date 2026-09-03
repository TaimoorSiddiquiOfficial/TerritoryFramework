# Territory Framework — Complete System Re-audit

> **Date:** 2026-09-03 (updated)
> **Scope:** Territory runtime C++, editor/Definition integration, Narrative Pro boundaries,
> Blueprint API safety, save/replication read models, tests, documentation, and story readiness.

> **2026-09-03 follow-up:** Narrative damage ownership, Game/Editor target boundaries, and editor
> mutation safety were re-audited and remediated in
> [Narrative damage and dual-target re-audit](NARRATIVE_DAMAGE_REAUDIT_2026-09-03.md).

## Decision

The framework has a coherent architecture for starting game-story design. It is not yet a final
shipping build: the real two-client server, live save-slot assault, World Partition, packaged
server/cook, and authored road tests remain release gates.

The important result is that game story can now be built on one set of authorities. Story quests
do not need special duplicate ownership, diplomacy, wallet, dialogue, AI, or save systems.

## Authority map

| Question | One authority | Easy example |
|---|---|---|
| Who owns a Place? | `ATerritoryVolume` ownership data, mutated by Control subsystem | Heroes claim Blacksmith through one mutation request |
| Who owns a District or City? | Hierarchy reducer derives it from children | Heroes claim all unlocked Places, so the District becomes Claimed |
| Is content available? | Definition-backed availability and unlock cascade | Locked Farm is absent from gameplay until a quest unlocks it |
| Are factions hostile? | Territory Diplomacy writes Narrative faction attitudes | Guards attack on sight only when Narrative attitude resolves Hostile |
| Who owns money/items? | Narrative inventory and ability system | Blacksmith production gives Grain to a registered Narrative inventory |
| What does UI/save/late join read? | `ATerritoryWorldState` replicated projection | An unloaded Place still appears in the Command Center directory |
| Who schedules an assault? | Counterattack subsystem and Combat Director | Bandits schedule a finite road reinforcement against Blacksmith |
| Who owns quests/dialogue? | Narrative Tales | Owner dialogue requests capture; a Narrative Task observes completion |

## Defects fixed in this re-audit

### 1. Wrong-player story handover on multiplayer

**Defect:** an environment or scripted defender kill could fall back to the first player
controller. On a dedicated server, Player Two could receive Player One's dialogue.

**Fix:** owner dialogue now uses only the exact pawn, controller, or Tales component supplied by
the event. If no participant exists, the owner appears and remains manually interactable.

### 2. Invalid owner setup reported false success

**Defect:** an enabled Story Owner template without a Narrative NPC Definition accepted the
handover, then retried for three seconds before failing.

**Fix:** activation rejects this setup immediately with one clear warning. The valid asynchronous
spawn retry remains for correctly configured Narrative spawners.

### 3. Replicated read model could be edited as gameplay authority

**Defect:** WorldState exposed direct Blueprint writers for treasury, production, transactions,
treaties, reputation, and capture summaries. A graph could make UI/save data disagree with the
real subsystem or Territory actor.

**Fix:** the complete WorldState write surface, including save/import and directory publication,
is native-only. Blueprint changes the owning subsystem and reads WorldState queries. Treaty,
reputation, and transaction projection updates now also request an immediate network update.

### 4. Raw ownership setter was a Blueprint gameplay node

**Defect:** `Set Owning Faction` returned no structured reason and carried no exact instigator
context, making it easy to build an incomplete multiplayer transition.

**Fix:** the actor method is native compatibility only. Blueprint uses **Apply Territory
Mutation**, supplies `FTerritoryTransitionContext`, and receives a structured result such as
Success, Locked, Defenders Remain, or Diplomacy Blocked.

### 5. Free property-upgrade bypass and two update paths

**Defect:** Blueprint could call `SetUpgradeLevel` without paying Narrative currency. Validated
upgrades also changed the field directly instead of using the reset/restore update path.

**Fix:** Blueprint keeps only `TryUpgrade(Requester)`. All level changes now use one internal
writer that refreshes production, recalculates income, notifies Blueprint, replicates, and forces
the network update.

### 6. Dead direct economy setter

**Defect:** the Economy subsystem still exposed an unused direct treasury assignment node even
though save/load already uses the native bulk snapshot bridge.

**Fix:** the dead function was removed. Gameplay credits/debits Narrative inventory; save/load
restores only Territory income/cost/count parameters through the bulk native bridge.

### 7. Guards removed outside the normal death callback stayed registered

**Defect:** streaming, story cleanup, or external destruction could leave a dying guard counted
by its post and Territory until a later reconciliation.

**Fix:** authoritative `EndPlay` now releases both registrations without spending reserve lives.
All active-count queries also ignore actors already being destroyed.

### 8. Blacksmith Wave configured itself out of existence

**Defect:** the Claimed row scheduled a Bandit Wave and then changed the same faction pair to
peace. The scheduler correctly requires War, so delayed deployment was cancelled.

**Fix:** the row now establishes the live owner-versus-opponent War before the Wave and leaves
peace to an explicit quest or assault-resolution event. Fresh-campaign initialization does not
run this transition-only War event.

### 9. Espionage could reroll after save/load

**Defect:** reconnaissance used process-global random state, so retrying the same save could
silently produce a different result.

**Fix:** each authoritative attempt advances a saved sequence and uses the campaign seed plus
District GUID to produce a deterministic roll.

## Blueprint migration

| Old graph action | Supported action now |
|---|---|
| Set Owning Faction | Control subsystem → Apply Territory Mutation |
| Set Upgrade Level | Property → Try Upgrade, with the exact requester |
| Set Faction Treasury | Credit/Debit Currency for gameplay; native snapshot restore for save |
| Set WorldState treaty/reputation/capture row | Mutate Diplomacy/Control/Economy; read WorldState only |

The TDA project asset scan found no map or Blueprint references to the removed nodes. Community
projects upgrading from an older plugin version should compile Blueprints once and replace any
reported node using this table.

## Re-audit findings that are already correct

- **Locked is availability, not political control.** A Locked Place may remember its owner, but
  capture, production, commands, counters, and POIs remain unavailable.
- **State rows are transition rules, not Tick logic.** Entry/exit conditions and events run when
  the authoritative state transition is evaluated. They do not continuously award the same
  quest result every frame.
- **City and District are aggregate control.** Places own physical guards, owner, capture bounds,
  spawn posts, and production. Districts and Cities derive control from children.
- **Counterattacks do not capture offscreen.** The strategic layer schedules and reports. A Place
  changes owner only through physical participants and the normal capture/handover mutation.
- **No campaign identity is hard-coded in runtime decisions.** Haven Reach, Blacksmith, Heroes,
  and Bandits occur in tests, examples, and metadata, not as forced game-story ownership.
- **No first-player shortcut remains in runtime Territory code.** Participant-sensitive state,
  dialogue, conditions, rewards, and capture use explicit context.
- **No TODO/FIXME/stub marker remains in runtime source.** Empty Blueprint extension events are
  intentional hooks; they are not unfinished gameplay implementations.

## Verification status

| Check | Status at this report |
|---|---|
| Static source/authority scan | Passed |
| Project Blueprint/map reference scan for removed nodes | Passed; zero references |
| Unreal Header Tool | Passed for Editor and Game targets |
| UE 5.7 `TDAEditor` Development build | Passed |
| Full `TerritoryFramework.*` automation | Passed: 200/200; 194 clean and 6 successful warning-producing fixtures |
| UE 5.7 `TDA` Game Development build | Passed after explicit runtime include-boundary fixes |
| Automation runtime-error scan | Passed: no failed tests, Blueprint Runtime Error, Accessed None, assertion, fatal, or Territory error |
| UE 5.7 `TDAEditor` Development build | Passed after the final source changes |
| Scoped Territory Blueprint compile | Passed: 69/69, zero compiler errors or warnings |
| Scoped Territory asset validation | Passed: 101/101 project/plugin Territory assets plus the test map, zero errors or warnings |
| `/Game/HopDistrictTest` headless runtime smoke | Passed: map loaded, player/HUD initialized, benchmark exited with status 0, and no Blueprint runtime, Accessed None, assertion, ensure, fatal, or Territory warning/error was logged |
| Visible capture-quest PIE fixture | Passed before this hardening batch |

UE 5.7's Data Validation commandlet does not implement an `AssetPath` parameter; using that label
therefore scans the complete 30K+ project library. This audit instead used the commandlet's real
class filter for all five Definitions, then an editor-side Asset Registry scope for the 101 assets
under the two Territory paths plus `HopDistrictTest`. Both targeted passes completed cleanly.

The smoke log still reports project/vendor-content warnings: two missing City Sample Crowd
materials, two missing Narrative demo weapons, an invalid `GameplayCue.TakeDamage.Fire` tag, and
no RecastNavMesh during world teardown. They are not Territory state failures, but they remain
release cleanup because they can hide equipment, damage-cue, or navigation behavior.

## Remaining release gates before shipping

1. Real dedicated server with two connected clients.
2. Save and reload a live finite assault through the project's actual Narrative save slot.
3. Stream an active Place/assault out and back in on a World Partition map.
4. Repair project/vendor cook blockers, then test packaged client/server.
5. Playtest the authored road network with vehicle arrival, chase, abandonment, cleanup, and
   final on-foot combat.
6. Clean stale Narrative demo references, the invalid Fire gameplay cue tag, and gameplay-map
   navigation warnings so they cannot hide Territory defects.

## Plan for the game story

### Phase 1 — Freeze the reusable rules

- Choose final faction tags and create one Faction Doctrine per major faction.
- Choose the campaign City → District → Place Definition hierarchy.
- Decide which Places are open world, story locked, stealth-capable, or multiplayer capture.
- Assign Story Owner, guard posts, production, POI, music, counterattack, and approach data in
  each Place Definition.

### Phase 2 — Build the main betrayal arc

1. **Service:** the player captures Places for the Regime while investigating Bandits.
2. **Doubt:** evidence, civilian dialogue, disguises, and optional rescues reveal false orders.
3. **Betrayal:** a Narrative quest changes the player's represented faction/reputation; the
   Regime blames the player and changes diplomacy.
4. **Choice:** the player may support Rebels, protect civilians, remain loyal, or operate alone.
5. **Domination:** the same Places can be reclaimed for the player's current represented faction;
   no Heroes/Bandits tag is hard-coded into the capture or diplomacy rules.

### Phase 3 — Author reusable mission recipes

- Scout → disable alarm → rescue owner → handover.
- Stealth infiltration → disguise check → expose evidence → escape without permanent war.
- Defeat captain → capture Place → survive finite vehicle reinforcement.
- Chase underboss on road → force abandonment → final fight → reveal hidden Place.
- Defend production Place → keep guards alive → earn resources → unlock District perk.

Narrative Tasks observe each objective. Territory Events request one state change. Definitions
hold reusable configuration. This keeps a mission understandable and prevents graph-side systems
from fighting each other.

### Phase 4 — Story vertical slice

Build one complete District first:

- one neutral-owner handover Place;
- one multi-floor stealth Place;
- one production Place;
- one vehicle counterattack route;
- one underboss chase;
- one betrayal quest that changes faction relations;
- one post-capture choice with visible economy, dialogue, music, and Command Center consequences.

When that District survives the multiplayer/save/streaming gates, duplicate the proven pattern
for the rest of the City instead of designing every Place independently.
