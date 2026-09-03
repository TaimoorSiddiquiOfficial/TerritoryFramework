# Territory Framework — Remaining Work and Roadmap

> **Reviewed:** 2026-09-03
> **Purpose:** one current list of release gates, engineering debt, and possible future features.
> Historical audit reports are evidence, not the current task list.

## What is complete in this branch

- City, District, and Place authoring is Definition-first. A Place owns physical gameplay;
  Districts and Cities aggregate their children and provide higher-level policy.
- Territory capture, guards, patrols, diplomacy, counterattacks, production, UI, POIs,
  stealth, disguise, roads, music, save data, and Narrative Tales adapters use the existing
  Narrative Pro authorities instead of replacing them.
- Story Outcome Preview explains authored consequences without running conditions, rolling
  chance, or changing the asset.
- The Narrative Task library covers Territory state/capture, counterattacks, disguise, boss
  and chase outcomes, movement, GAS state, combat progress, and AI observation.
- Reusable Quest Cascade Recipe Data Assets generate safe, normal Narrative Quest graphs with
  ordered states, alternative branches, multiple AND tasks, functional state/route conditions,
  Quest Dialogue/tracking options, events, a live Mission Logic report, validation, automatic
  layout, compilation, and overwrite protection.
- Story Outcome Preview is registered once on the base Definition and inherited once by Place,
  District, and City; the previous duplicated panel has a regression test.
- Ownership, property upgrades, economy restore, and WorldState projections no longer expose
  low-level Blueprint writers that can bypass their authoritative subsystem.
- Story-owner automatic dialogue keeps the exact Narrative participant; environment/scripted
  kills never choose an arbitrary first player in multiplayer.
- The Blacksmith visible PIE fixture has completed defender death, owner spawn, dialogue,
  faction handover, and its Narrative capture task exactly once.
- Current-source verification on 2026-09-03 built the UE 5.7 `TDAEditor` Development target and
  passed all 197 `TerritoryFramework.*` tests. The automation log contains no failed
  test, Blueprint Runtime Error, Accessed None, assertion, fatal, or Territory error.
- All nested active-assault, diplomacy, strategic-directory, economy, and evaluation fields now
  participate in Unreal's `SaveGame` archive. A packaged game created an active assault, saved it,
  reloaded it once, exited, and a second independent packaged process restored the same live
  assault once from the same slot.
- A packaged dedicated-listener process admitted two independent clients while a real immediate
  Bandit-versus-Heroes assault was live. Both clients were welcomed and created their own server
  pawn. Three authored PlayerStarts now remove the previous second-client origin-spawn failure.
- A no-asset-registry-cache cook of `HopDistrictTest` and a fresh stage/package/archive completed
  with zero errors. The invalid Fire cue, missing mobile touch interface, missing player appearance
  materials, and project-owned Narrative demo loadout leaks were repaired.
- Scoped editor verification compiled all 69 Territory Blueprints and validated 101 assets under
  the project/plugin Territory paths plus `HopDistrictTest`, with zero errors or warnings.
- A current `/Game/HopDistrictTest` headless runtime smoke loaded the map and player/HUD, then
  exited normally with no Blueprint runtime, Accessed None, assertion, ensure, fatal, or Territory
  warning/error.
- The documentation learning path is uniquely numbered from 00 to 34. Reports and tutorials
  are separate appendices.

## Release verification status

These are verification jobs, not permission to invent a second gameplay authority.

1. **Dedicated topology — automated portion passed.** A packaged Game executable ran as a real
   dedicated listener with two separate client processes and a live immediate assault. Both joins
   and pawn spawns succeeded. Still manual: capture a Place, recruit one guard, patrol, resolve the
   assault, and observe exact-once capture/XP presentation on both rendered clients.
2. **Live save slot — passed.** A finite active assault survived save/reload once in-process and
   restored once again after a complete packaged-process restart. The 197-test suite also covers
   the nested archive contract.
3. **World Partition map — fixture still required.** Stable identity and stream-safe actor cleanup
   are automated, but `HopDistrictTest` is not World Partition-enabled. A physical stream-out and
   stream-in of a Place, its posts, and a live assault remains a map-level playtest.
4. **Packaged cook/game — passed for the installed engine.** A clean no-cache Windows cook plus
   stage, package, and archive completed with zero errors. Epic's installed UE 5.7 build cannot
   produce separate Client/Server target binaries; a source engine or installed Server support is
   required for that optional target split. The packaged Game binary successfully exercised the
   dedicated-server topology.
5. **Authored road playtest — partly passed.** The authored assault vehicle claimed its Narrative
   driver, followed ten ZoneGraph guide points, stopped, dismounted, and completed takeover in the
   existing runtime gate. Still manual: reverse boss pursuit, deliberate traffic blocking,
   damage-triggered abandonment, cleanup timing, player carjacking, and final-fight camera framing.
6. **Project cleanup — project-owned defects fixed; vendor/headless warnings remain.** Fire cue and
   touch-interface configuration are valid, missing player appearance overrides were removed,
   all three Territory NPC definitions now grant only the Territory weapon, three PlayerStarts are
   present, and the map owns a RecastNavMesh. Remaining cook warnings are inside Narrative Pro's
   Character Creator/configuration, while `-nullrhi` clients report Narrative UI widgets that do
   not exist in a headless viewport. Do not patch vendor assets from Territory Framework.

## Engineering improvements after the gates

| Priority | Improvement | Why it matters |
|---|---|---|
| High | Add a one-click Definition setup audit and hierarchy repair report | Makes community onboarding safer and faster |
| High | Add patrol, guard-post, capture-bound, and assault-route visualizers | Shows physical mistakes before PIE |
| Medium | Publish an explicit streamed global capture-progress read model | Lets a world map show unloaded Places without treating UI data as ownership |
| Medium | Move large replicated histories to `FFastArraySerializer` | Reduces bandwidth in long campaigns |
| Medium | Add automated accessibility and gamepad navigation checks for Command Center | Protects the compact UI as more controls are added |
| Later | Persist optional live-NPC detail such as health/activity | Current finite-count reconstruction is safer; add detail only when truly needed |

## Best next gameplay ideas

### 1. Faction Doctrine Profile

Give each faction a recognizable strategy as well as a signature vehicle. A doctrine can select
reinforcement size, preferred approach, aggression range, disguise checks, dialogue tone,
music theme, and post-capture policy.

**Example:** Bandits arrive quickly in light cars and flank. The Regime arrives slowly in armoured
vehicles, establishes a roadblock, and sends an officer who can order a withdrawal.

### 2. District Heat and Manhunt

Heat is temporary pressure, not ownership or diplomacy. Witnesses, alarms, guard deaths, and
failed disguises raise heat; hiding, bribery, changing clothes, or a Narrative quest lowers it.

**Example:** Heroes are neutral with Bandits, but stealing a uniform raises local heat. Guards
investigate the player without declaring a permanent faction war.

### 3. Supply Lines

Use controlled adjacent Places and road guides to derive whether production and reinforcements
are supplied. Never transfer ownership offscreen; lack of supply changes capability only.

**Example:** Capturing the Fuel Depot disables the Regime's vehicle wave at Blacksmith. Destroying
one convoy delays the next wave; it does not magically capture either Place.

### 4. Officer and Underboss Network

Named officers provide a local perk, doctrine modifier, intel secret, or reinforcement route.
They can be killed, captured, persuaded, or allowed to escape into a later chase.

**Example:** Capturing the radio officer reveals two hidden Places. Letting the underboss escape
makes the next District counterattack stronger but creates a tracked boss-chase quest.

### 5. Post-capture Stabilization Choice

After a Place is Claimed, let the player choose one short policy: guards, relief, propaganda,
extraction, or local autonomy. Apply consequences through existing economy, diplomacy,
production, perks, dialogue, and Narrative Events.

**Example:** Relief reduces immediate funds but improves civilian reputation and production.
Extraction gives funds now but increases unrest and the next counterattack chance.

### 6. Intelligence Quality and False Reports

Espionage should return a confidence level. Better scouts, nearby owned Places, captured officers,
and faction reputation improve accuracy. Poor intelligence may show a range, never secretly alter
the actual guard count.

**Example:** “Two to five defenders; vehicle support possible” becomes “Three defenders, one
reserve sedan, west-road approach” after the radio tower is controlled.

### 7. Cooperative Territory Tasks

Add observer-only Narrative Tasks for **all required players present**, **revive ally inside a
Place**, **two objectives completed together**, and **escort player reaches extraction**.

**Example:** one player disables the alarm on floor one while another rescues the owner on floor
two. The Place becomes capturable only after both Narrative objectives complete.

## Design rules for every future feature

1. Narrative Pro stays authoritative for factions, quests, dialogue, inventory, GAS, AI,
   vehicles, music, POIs, save orchestration, and difficulty.
2. Territory Framework stays authoritative for availability, ownership, capture pressure,
   garrisons, production, strategic assaults, and Territory read models.
3. A Place owns physical actors and combat. A District owns Place policy and aggregated control.
   A City owns District policy and aggregated control.
4. Locked means unavailable. Claimed, Unclaimed, and Contested describe control only.
5. Conditions observe, Events request one action, and Tasks observe progress. None should tick a
   second version of the same system.
6. AI ownership changes require physical participants. Strategic simulation may schedule and
   inform, but it must not silently capture a Place.
7. New community options need easy metadata, one small example, validation, automation coverage,
   multiplayer authority notes, and a migration path.
