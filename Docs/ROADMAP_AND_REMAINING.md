# Territory Framework — Remaining Work and Roadmap

> **Reviewed:** 2026-09-14 (Narrative pattern and lifecycle audit)
> **Purpose:** one current list of release gates, engineering debt, and possible future features.
> Historical audit reports are evidence, not the current task list.

## Current remaining work

The latest framework batch fixes distraction cancellation, stale/foreign item
sources after inventory restore, and repeated impact callbacks. All six builds
and 322 tests per engine pass. The shipped rock's last-unit behavior and a listen
server with two clients pass. Six distraction Blueprints compile cleanly and
eight focused assets validate without warnings. The earlier task/presentation
fixes remain verified; six Farm/owner authoring-warning assets remain open. See the
[Narrative pattern audit and usage guide](NARRATIVE_PATTERN_AUDIT_2026-09-14.md)
for source references, tooling exclusions and the limits of this evidence.
The broader audit and release gates remain open.

| Area | Still required |
|---|---|
| Narrative pattern audit | Next: audio-enabled music loading/override ownership, remote dialogue replacement, shared split-screen presentation, full cinematic playback and complete asset dependency/unused-system coverage. Distraction item/cancellation regressions pass; remote input, broader authored combat/montage interruption and full campaign recovery still need gameplay acceptance. The targeted 30-asset check distinguishes unused story options from authoring-library assets. Resolve Farm camera and owner appearance warnings intentionally. |
| Hashir's Farm trip — route and boarding fixed, story acceptance open | Standalone and remote travel/exit pass; simulated seated capsules no longer push the client car. Author durable arrival/continuation and save recovery, then verify the full Blacksmith-to-Farm flow, compiled dedicated server and AlMalik streaming. [Evidence and remaining checks](HASHIR_CASTLE_FARM_DRIVE_TODO.md). |
| AlMalik release blockers | Fix cold appearance loads that stay pending, isolate the crash after a restored assault wave dies, then finish returning-client streaming verification. |
| Finite reserves and Narrative tasks | Add reserve events through existing finite post commands; prove capture/defeat behavior with pending reserves and missing posts; migrate applicable authored Native record tasks. |
| Guard conversations | Bind speakers, select story context, handle multiplayer, let hidden players overhear, interrupt for combat and resume the real patrol activity. |
| Full gameplay verification | Finish the multiplayer capture/recruitment/defeat/reward playthrough and authored road obstruction, abandonment and carjacking checks. Resolve the tracked AI/weapon/cue warnings and optional intro-cutscene error through adapters. |
| Story-map presentation | Finish UDS day/night, interior, fog, shadow, HDR-display and frame-rate checks in AlMalik; retain the tracked appearance/Chaos/content warning review. |
| Story authoring and release | Finish Act 1 after the behavior above is verified; decide later retake/peaceful-handover rules and Farm's reward. Refresh 5.7/5.8 release artifacts and documentation after the remaining release gates pass. |

Next audit work: **audio-enabled overlapping music requests**, followed by
multiplayer dialogue/cinematic cleanup and broader authored combat interruption
acceptance. Next story work remains **Hashir's durable arrival/quest continuation
and save recovery**. Finite reserve events and capture/defeat checks remain in
the framework backlog.
The dated checkpoints below preserve earlier evidence; their older test counts
and then-current task lists do not supersede this list.

## Current checkpoint

### Distraction ability and Native inventory lifecycle — 2026-09-14

Throw activation now checks exact Native inventory membership, equipment and
the current avatar before spawning/payment. Commit and spawn callbacks cannot
continue a cancelled invocation or complete a newer invocation. Impact reporting
commits its existing one-shot flag before publishing. Native owns ability grants,
item consumption, save/replication and the successful projectile's lifetime.
All six builds, 322 tests per engine, the shipped Blueprint last-unit regression,
a listen-server/two-client check, and cook/package startup smoke pass. See the
[audit and its limits](NARRATIVE_PATTERN_AUDIT_2026-09-14.md). No Narrative source
or project story asset was changed.

### Narrative pattern and lifecycle audit — 2026-09-14

Native Tales retains quest progress and persistence; transient Territory listeners
now follow the current Native player identity across character changes and driving.
Exact tag objectives read explicit ownership and recover engine-skipped removal
callbacks without patching GAS. Local music and dialogue presentation use Native
playback and dialogue ownership. Optional objectives now earn progress, and
presence/disguise/AI observations reset when their actors change. Five new
regressions pass on UE 5.7 and 5.8; the suite totals 320 tests per engine.
Cooking, staging and a packaged game server-mode smoke pass. This batch does not
close full campaign save/load, dedicated-server or multiplayer story acceptance.
See the [audit](NARRATIVE_PATTERN_AUDIT_2026-09-14.md). Narrative Pro is unchanged.

### Remote mounted character collision — 2026-09-12

Native mount attachment now drives a local capsule-collision adapter on simulated
characters. Territory defenders and assault guards include it; TDA's player and
Hashir opt in through their Blueprints. Native keeps seating, occupancy, driving,
owning-client abilities and save authority. All six builds and 315 tests per
engine pass. A listen host, two clients and a late third client pass 16 boarding,
travel, arrival, exit and collision checks. See [setup](MOUNT_CLIENT_COLLISION.md)
and [verification](MOUNT_CLIENT_COLLISION_VERIFICATION_2026-09-12.md).

The fixture starts the real drive dialogue node and stages the passenger. The
complete Blacksmith quest, durable arrival, mid-trip recovery and city streaming
are still open. No Narrative Pro source was changed.

### Production stock limits and per-rule messages — 2026-09-12

Production Profile rules now expose inventory comparisons, strict output stock
caps and individual notification switches/text. Free refills add only missing
stock; paid recipes wait for their whole batch to fit. Reaching a cap consumes
no inputs and expires the stopped cycle. Native inventory, account routing and
the existing save/checkpoint authority remain responsible for stock and progress.

The TDA Blacksmith ammo rule is configured for a 1,000-round target with messages
off. Both engines pass 314 tests, all six builds pass, and the live host/two-client
fixture passes 19 stock, message, authority and faction-change checks. See the
[setup guide](PRODUCTION_LIMITS_AND_NOTIFICATIONS.md) and
[verification report](PRODUCTION_LIMITS_VERIFICATION_2026-09-12.md) for scope and
remaining validation limits.

Next framework work remains finite reserve Narrative event adapters and the
pending-reserve capture/missing-post defeat checks, followed by authored task
migration and coordinated guard conversations. The release blockers below remain.

### Defender response and exposure ownership — 2026-09-12

Defenders can retaliate through Native personal hostility while a quest keeps
capture and State Events paused. An explicit **Pause Defender Combat** rule now
controls combat separately. Protective treaties, optional exact faction filters,
per-player exposure and Local Alarm apply through the same guard policy. Existing
Territory combat activities consume that policy and stop/resume through Native
activity selection. Actual ASC damage also closes the early observer-binding gap.

Outside threat evidence is configurable. Seeing an owned projectile does not
expose its hidden owner. Clearing exposure removes only exposure/bounds contest
participation, preserving explicit story and multiplayer flag capture reasons.
Empty zero-pressure contests recover even while quest capture remains paused.
See [implementation and verification](DEFENDER_RESPONSE_AND_EXPOSURE_2026-09-12.md).

Next: reserve Narrative events through finite post commands, the pending-reserve
capture/missing-post defeat checks, and migration of applicable authored Native
record tasks. Then implement coordinated guard conversations, speaker binding,
multiplayer presentation and the patrol activity hook. The AlMalik cold-appearance,
restored-wave crash and returning-client release blockers below remain open.

### Reserve perception, evidence identity and Native record task — 2026-09-11

Implemented the stored-perception adapter and corrected anonymous clue exposure,
sight timing and immediate Local Alarm investigation. A server/two-client test
confirms all three finite replacement guards receive Native attack goals without
manual refresh; both clients match the server's garrison snapshot and cannot
submit authoritative awareness changes. Added Wait For Narrative Data Task with
per-listener cleanup, full quantity handling and a saved starting-count cursor.
See [changes, migration and verification](GUARD_RESPONSE_FIXES_2026-09-11.md).

Defender retaliation and contest registration sources were completed in the newer
checkpoint above. Reserve Narrative events and coordinated guard conversations
remain ahead.

### Tagged guards and overheard story conversations — 2026-09-11

Added the requested defender/assault story participation to the remaining plan:
role GameplayTags, optional faction filters, current quest/district context,
proximity while the player stays hidden, two guards talking at a patrol stop,
combat interruption and return to their own duties. Implementation is pending.
See [the source audit and acceptance checklist](GUARD_STORY_CONVERSATIONS.md).

Native tagged dialogue is already assigned to both TDA guard definitions. The
coordinated scene still needs exact multi-speaker binding, preserved client
bindings and one server-owned story result. The supplied patrol Activity Tag is
copied but not consumed by either patrol Blueprint. Add its real activity hook
with this work. Repair the AI/stealth findings below before wiring the example;
do not mask idle combat or make an overheard conversation reveal the player.

### State, stealth and finite reserves — 2026-09-11

Fixed the initial All Defenders Defeated task check accepting zero living guards
while replacements are queued. It now uses the existing garrison snapshot.
The regression covers Native deaths, pending-post save/load, a streamed replacement
owner, client read models and failure paths. Verification results are recorded in
[the current audit](STATE_STEALTH_RESERVE_AUDIT_2026-09-11.md).

Open findings in this newly requested scope:

- Replacement-guard perception and anonymous clue exposure are fixed in the newer
  checkpoint above; the original audit remains evidence of the reproduced faults.
- The newer September 12 checkpoint fixes quest-blocked retaliation, outside
  threat evidence, per-player combat gating and exposure registration cleanup.
- Capture-versus-pending-reserve and missing-post defeat checks need further proof.
- Add the requested reserve Narrative event through existing finite post commands.
- Connect hand-authored quest conditions through the existing condition task, and
  use the new Native record-task adapter instead of expecting a new Data Asset to
  observe gameplay automatically.
- Migrate applicable authored record tasks to the new adapter. Vendor task assets
  remain unchanged; their old cleanup and per-notification counting remain unsafe.

Next coherent batch: add finite reserve Narrative events and verify the remaining
capture/defeat checks around delayed or unloaded posts. Preserve per-player stealth,
Native factions, explicit story waves and autonomous physical assaults. The
September 10 appearance, crash and returning-client blockers below remain open.
Release artifacts and Act 1 work remain gated.

### Native appearance readiness — 2026-09-10

Fixed a separate premature-ready defect: an assault visual could exist before
Native finished its base appearance. The existing Territory readiness query now
uses Native's complete pending-load check. Failure logs expose the missing
initialization stage. No Narrative source, save schema, replicated property,
Blueprint signature or initialization timeout changed.

Both engine versions pass 307 automation tests and all six Editor, Development
and Shipping builds. All 741 Native source files match the installed vendor.
Validation checks 244 assets and compiles 147 Blueprints with zero errors and
eight existing warnings.
The UE 5.8 package and 60-second Development Game server-mode startup pass with
exit zero; the optional missing cutscene-player warning remains.
The capture-enabled AlMalik streaming/save fixture passes on a server and two
clients when its appearance assets are already loaded. The cold failure still
reproduces: appearance loads remain pending and all four attackers withdraw.
The corrected post-load defeat harness also crashed in behavior-tree execution
after the first restored wave died. These are unresolved release blockers.
The returning-client test also remains open after a temporary memory streaming
package disconnected one original client. See
[evidence and reproduction](ASSAULT_APPEARANCE_READINESS_2026-09-10.md).

### Independent guard-post cells — 2026-09-10

Fixed four physical AlMalik findings: startup staffing becoming zero before posts
load, tag-bound dead guards retaining their slots, free replacements on post
reload, and lost garrison records/bindings when only the Place cell unloads.
Posts retain their finite requests while waiting for their registered owner.
No Native source, save schema, replicated field or Blueprint signature changed.

Both engines pass all 306 tests and all six Editor/Development/Shipping builds.
The server/two-client fixture verifies delayed posts, exact reserve spending,
and a Native save while the Place is absent followed by two restored guards and
six reserves. Validation checks 244 assets and 147 Blueprints with zero errors;
eight existing warnings remain. The UE 5.8 package and 60-second Development Game
server-mode startup pass with exit zero. Original map hashes are unchanged, and
temporary assets and saves were removed. See [evidence](ALMALIK_POST_CELLS_2026-09-10.md).

At this checkpoint, the next blocker was the project assault NPC's missing Narrative character visual.
Fresh capture-enabled attempts withdraw after the existing 20-second initialization
limit, before streaming begins. Its definition, controller and activity component
exist. Trace the Native definition/appearance callback and the project Blueprint;
the cause is not yet established. Do not treat force counts alone as a passed
physical assault test. City-wide navigation and a returning client remain open.

### AlMalik actor persistence — 2026-09-10

Fixed a confirmed physical World Partition regression: a changed Place returned
with its old owner and unlocked after its cell unloaded. TerritoryWorldState now
writes departing Territory actors and guard posts through Narrative's existing
actor records before cleanup. It also asks Native to initialize its save system
if the project reaches WorldState BeginPlay without a save object. No vendor
source, saved schema, replicated fields or Blueprint mutation API changed.

The real AlMalik fixture passes on a server and two clients: changed owner and
story lock survive, old guards disappear, exactly two guards return, and all
three garrison read models retain six reserves and zero pending replacements.
Native tests also cover exhausted reserves and both actor restore orders.
Editor, Development and Shipping builds pass on UE 5.7 and 5.8; both suites pass
305 tests. Asset validation checks 244 assets and compiles 147 Blueprints with
zero errors and eight existing warnings. All 741 Native source files still
match the vendor. The refreshed UE 5.8 package and 60-second Game server-mode
startup pass; the existing missing cutscene-player warning remains. See
[evidence and limits](ALMALIK_STREAMING_2026-09-10.md).

At this earlier checkpoint, the fixture had no navigation and the route gate
correctly rejected the assault. Temporary bounds and a complete entry-to-Place
path are now built for verification; the next gate is described above.
AlMalik also reports existing power-line, catenary
Arrow and cinematic-overlay Blueprint errors during multiplayer startup.

The fixture was temporary. Original AlMalik and HopDistrictTest map hashes are
unchanged. Story placement still needs a persistent TerritoryWorldState outside
unloadable Data Layers, plus its actual City/District/Place definitions.

### Dead guard collision audit — 2026-09-10

The reported defenders and assault NPCs were already using Native ragdoll and
pelvis physics, but their upright capsules and meshes still blocked the Camera
channel. Territory now ignores Pawn and Camera collision on corpses and disables
stepping onto them. Ground collision, Native physics and loot remain enabled.
Existing death, ragdoll, BeginPlay and visual-ready hooks apply this local policy;
revival restores the original runtime settings. No save fields or replicated
death authority were added. See [dead NPC collision](36_Dead_NPC_Collision.md).

TDA's replacement `BP_TerritoryAssualtGuard` also had a disconnected Is Dead input
on its parent death call. That wire is repaired. A fresh-client test found that
late visual initialization could reset corpse loot collision. The existing Native
client-death presentation adapter now runs after those setup hooks as well.

The rebuilt UE 5.8 live check passed on a server, two connected clients and a
fresh late join: both corpse types simulate their pelvis bodies, settle onto the
floor, ignore player/camera collision and remain lootable. All six initial and
eight late-join corpse observations passed. Walking across a fallen defender
recorded 15 movement samples with zero player or camera height change. The killed
attacker contributed no capture participation; Native corpse records were removed,
and the temporary verification save was deleted.

Both UE 5.8 TDA and the isolated UE 5.7 host pass all 304 automation tests, with
zero failures or skipped tests. The new corpse regression has zero warnings.
Editor, Development and Shipping builds pass on both engines. The suites include
Native save/load, authority, casualty, streaming-order and Blueprint contracts;
the live checks above cover the actual replicated project NPCs.

TDA validation checked 244 assets and compiled 147 Blueprints with zero errors.
Eight existing appearance and dialogue-camera warnings remain. The refreshed
UE 5.8 cook, stage and package passed, followed by a 60-second Development Game
server-mode startup with exit zero. This is not a separate compiled TDAServer
binary or an AlMalik streaming certification. All 741 Narrative Pro source files
still match the installed vendor copy.

The next remaining work, in order:

1. Resolve cold appearance loads remaining pending and isolate the behavior-tree
   crash after a Native load followed by assault deaths. Repeat finite defeat,
   then test a returning client using a persisted isolated World Partition map.
   Capture-enabled streaming/save passes with appearance assets already loaded;
   this does not certify cold spawning or post-load defeat.
2. Finish the full multiplayer capture, guard recruitment, assault defeat and
   exact-once reward playthrough. Separate Server binaries need an engine with
   Server target support; the packaged Game listener is already a tested topology.
3. Complete authored-road obstruction, damage abandonment and carjacking checks.
   Resolve the tracked Native attack-query, weapon, decal and activity warnings
   through project or plugin adapters. Guard the optional demo intro cutscene.
4. Finish UDS visual and frame-rate checks in the actual story map, including
   daytime, night, interiors, fog and shadows.
5. Author Hashir's Act 1 quests and dialogue. Decide later Blacksmith retake
   encounters, peaceful handover and Farm's actual story reward as needed.

The older published preview remains unchanged. These later corrections belong
in the next verified release. Optional engineering ideas below are not claims
that a required gameplay path is missing.

### Narrative condition and event audit — 2026-09-09

Rechecked all 23 current conditions and 23 events. Fixed the legacy Locked state
condition, client reserve reads, reserve-count overflow, and misleading context
checkbox and assault-query fields. Added local lock/known/loaded queries, optional
campaign-directory state reads, All/Any groups, current-faction reputation sources,
and exact story encounter filtering for cancellation. Existing defaults remain
compatible. See [conditions and events](35_Territory_Conditions_and_Events.md).

Both engines pass 303 automation tests. Editor, Development and Shipping builds
pass for UE 5.8 TDA and the isolated UE 5.7 plugin host. TDA validation checked
244 assets and compiled 147 Blueprints with zero errors and eight existing
appearance/camera warnings. A live server, two clients and a new late join agree
on lock conditions and seven reserve guards; client lock mutations fail, and
Native save/load restores the same results. Both cooks pass; the UE 5.8 package
completes a 60-second Game server-mode startup with exit zero.

The packaged demo controller still logs an optional missing CutscenePlayerActor
when its loading-screen event starts a cutscene. Track this project/controller
setup warning separately; it was not changed in the condition audit. A compiled
TDAServer target and actual AlMalik World Partition playthrough remain separate
release gates. All 741 Narrative Pro source files still match the vendor copy.

### Faction integration and restore correction — 2026-09-08

The current implementation ledger is [Finding Resolution Plan](Finding_Resolution_Plan_2026-09-08.md).
Live faction changes now update resource routing and open economy screens. Tied
account priorities create a visible conflict; Native inventory remains the balance
authority. Both example controllers follow their owner's political faction.
Both engines now pass 299 automation tests, and a server/two-client fixture plus a
fresh late join passes. Two consecutive Native world/player restores preserve
garrisons, reserves and faction/account state without the reproduced record-reader
crash or stale guard attack-goal errors. Saved vehicle ledgers now reject inflated
budgets, duplicate approach rows and inconsistent spent totals; native save tests
and live server/two-client cancellation checks pass. Production stops and
compensates in the original inventory when a callback changes its faction or
selected depot. Currency receipts now preserve restored purchases and stop old
payments after a Native load; live wallet/history replication passes on two clients.
See the implementation ledger for the latest build/test gates.
Territory's Native controller now rebuilds saved generator snapshots and supports
explicitly retired classes. Hashir now selects that adapter through a project
controller, while the shared story NPC fixes client death and late-join presentation.
The standard attack goals are not saved, and two live restores selected no friendly
targets. Territory combat activities now respect boarding, travel and current
engagement rules. See [combat eligibility](Combat_Activity_Eligibility.md).
Attack-query, weapon and activity warnings, plus actual AlMalik streaming audits,
remain. See also [story NPC setup](Story_NPC_Integration.md).
These changes are not certification of the older published preview.

TDA's starter sword now uses its existing Territory combo ability. The previous
demo ability dealt enough damage with the starter clothes to execute a healthy
100-health guard on the first hit. Normal attacks and lethal finishers remain
owned by Narrative. Existing saves retain their saved weapons. Hashir's tagged
greeting now references the generated dialogue class; an editor validator rejects
the incorrect Blueprint asset reference. See [melee and dialogue guidance](Melee_And_Tagged_Dialogue_Checks.md)
and the current verification entry in the implementation ledger.

### Active story preparation — 2026-09-08

User decision: Blacksmith reinforcements arrive **before handover**. The owner
must wait until that finite force is defeated. Do not change ownership to make
the existing post-capture counterattack accept the request.

- [x] Add an optional exact Narrative faction tag to situation conditions. Show
  the selected faction source in graph text; document player, owner and contesting
  faction differences. Support Place, District and City holdings.
- [x] Add a deliberate pre-handover reinforcement mode to the existing assault
  scheduler. Keep diplomacy, state policy, routes, finite waves, casualties,
  server authority, persistence and streaming checks. A failed or cancelled
  deployment must not count as defeated.
- [x] Repair the project Blacksmith handover's disconnected "Not Now" wave node
  and replace its defence-power test with the intended faction City-holdings
  condition. Gate capture on the matching story encounter's verified defeat.
- [x] Add optional faction/scenario filters to existing assault conditions and
  use existing capture-progress/eligibility conditions for pressure and handover.
- [x] Arrange project and plugin dialogue graphs from top to bottom and left to
  right while preserving Narrative's position-based reply priority. Compile and
  compare branches after saving; keep shared plugin content UE 5.7 compatible.
- [x] Inventory and validate all authored Territory data assets and Blueprints, including the
  user's new quest/task edits. Record confirmed defects, validation results and
  remaining behavioral checks; do not rebuild assets merely for appearance.
- [x] Reject oversized saved assault vehicle budgets and duplicate/inconsistent
  approach ledgers before reconstruction. Preserve casualties and decision data;
  publish the cancelled record to server clients, including with an unloaded target.
- [x] Stop production when its faction account changes during Native item callbacks;
  compensate in the original inventory and preserve the consumed cycle through load.
- [x] Verify currency settlement/refund callbacks, partial upkeep and loaded purchases.
  Both engine suites and a real Native world/player load with two clients pass.
- [x] Verify physical AlMalik Place/guard cell streaming with two clients and
  fix Native actor record writes before cleanup.
- [ ] Build navigation for the AlMalik fixture and finish active finite-assault
  streaming, separate post cells and returning-client coverage.
- [x] Protect Hashir's pacifist perception callback after controller destruction.
  His project activity config preserves all eight Native activities and uses a
  minimal child Blueprint with the existing Territory safety function.
- [x] Reproduce and correct the Native record-reader crash: retiring Blacksmith
  guards wrote EndPlay save records while Native still held the territory record.
  Existing deferred removal now keeps those writes outside the reader; Native
  target goals are detached before retired guards disappear. The exact regression
  and two real world/player restores pass. Native source is unchanged.
- [x] Adapt Native `BP_NarrativeNPC` client death through a shared child Blueprint.
  Keep the Native parent on authority; remote presentation reads the Native ASC
  and waits for Native ragdoll replication. BeginPlay and visual-ready refreshes
  cover late join. Actual server/two-client death and a fresh late join pass.
- [x] Integrate Hashir with the existing controller save adapter. His project
  controller retires only the old exact Native attack-generator class. Preserve
  the safe replacement, Native Blueprint inheritance and stable definition IDs.
  Native record regressions and a real Hashir actor/controller restore pass;
  actual AlMalik streaming remains a separate gate.
- [x] Fix repeated generator snapshots on Territory controllers. Keep the latest
  old record, restore existing generator objects without duplicate bindings, and
  support an explicit list of retired saved classes. Both engine suites and the
  Native actor-record recreation regression pass. The thin controller preserves
  Native Blueprint inheritance; portable and TDA guards/assaults now select it.
  Actual server/two-client snapshot checks and both cooks pass. Hashir's later
  project integration is described in the story NPC guide above.
- [x] Stop melee, ranged and grenade activities from interrupting vehicle ingress.
  Existing participant/guard rules gate the actual shared and project scorers.
  Both engines pass 299 tests; the final server/two-client run records zero combat
  selections during ingress, resumes on-foot combat after two Native restores,
  and stops combat on peace. Both cooks and the packaged assault smoke pass.
- [x] Classify the observed same-team damage cancellations. Native's damage
  protection also covers melee bystanders. The before/after observations selected
  no friendly targets and saved no default attack goals. Deliberately saved custom
  goal subclasses still need their own restore tests; do not disable friendly fire
  protection to suppress this log.
- [ ] Resolve Native attack-target EQS/Blackboard and weapon-decal warnings from
  the latest packaged assault smoke through compatible plugin/project adapters.
  Include the Native activity restart warning after Hashir's death, repeated
  `GA_Weapon_Wield` execution messages and the weapon notify-state warning.
- [x] Inspect Hashir for Act 1: check both NPC_Hashir and NPC_Hahsir references,
  his greeting/dialogue assets and quest-giver setup. Hashir is the player's
  friend and works for the system. Author story content after framework checks.
- [x] Repair Hashir's tagged greeting class reference and validate configured
  tagged dialogue rows. Preserve the existing text, reply, IDs and cooldown.
- [x] Stop `NQ_CaptureBlacksmith` starting automatically in HopDistrictTest.
  The separate `QuestStarter_CaptureBlacksmith` level actor kept starting it after
  the controller's quest setting was cleared. Removed that actor only. Fresh
  server/two-client PIE has no quests; Hashir's explicit event and Native save
  restoration still work. Current quest/dialogue Blueprints validate without errors.
- [x] Fix the Territory State Condition's legacy Locked query. Current authored
  Blacksmith starts Locked; the condition now agrees on server, clients and after
  Native restore. Hashir's current offer uses Not Started and its start line unlocks
  Blacksmith. The author's dialogue rules and map were not rewritten in this audit.
- [ ] Guard the demo controller's optional intro cutscene when no cutscene player
  exists; the packaged startup currently logs Accessed None in Narrative's cutscene path.
- [x] Correct TDA's starting sword balance so ordinary melee can play before a
  lethal execution. Keep Narrative's combo, backstab and finishing-blow rules.
- [ ] Finish Hashir's Act 1 quest and main dialogue after story requirements are
  agreed. The current main dialogue now contains prototype quest-giving, follow
  and reputation branches. Those authored branches are not a finished Act 1.
- [ ] Investigate Hashir's drive to Castle Hill Farm after successful Blacksmith
  capture. Reported: invalid ZoneGraph start/end lane, identical logged segment
  endpoints and Return To Spawn SetupBlackboard failure. Root cause is unverified;
  see the [logs, investigation and acceptance TODO](HASHIR_CASTLE_FARM_DRIVE_TODO.md).
- [ ] Decide whether later Blacksmith losses start a new named reinforcement
  encounter, and whether diplomacy should offer a separate peaceful handover.
  The current example is one finite named battle and requires its actual defeat.
- [ ] Choose Farm's actual weapon reward when the story needs one. Its existing
  WeaponUpgrades benefit tag and upgrade level remain; no weapon is invented.

The latest HopDistrictTest server/two-client run proves the pre-handover battle,
eight Native vehicle arrivals and dismounts, one-survivor capture blocking, normal
Tales handover, outcome/ownership replication and repeated Native save/load.
The separate quest's duplicate immediate wave is now gated by the same completed
encounter. All 288 automation tests pass on both UE 5.8 and UE 5.7. See
[story situation verification](STORY_SITUATIONS_2026-09-08.md) for the exact scope.

All 10 in-scope dialogues (130 nodes) now have distinct graph positions, with
reply order, links and text preserved by the layout operation. The two edited
shared quest/task assets were restored through UE 5.7 editor APIs; all authored
settings match their UE 5.8 snapshots. UE 5.7 validates 118 plugin assets and
compiles 75 Blueprints with zero errors and four example warnings. TDA's latest
UE 5.8 audit validates 237 assets and compiles 143 Blueprints with zero errors
and eight presentation warnings. Narrative Pro's 741 source files match the
installed Marketplace package exactly.

The current TDA cook, stage and package pass with zero errors and 30 existing
content/tooling warnings. The matching packaged Game passes a 90-second
server-mode assault smoke with exit zero and no runtime errors, including the
previous invalid-controller warning. The new Native restore crash and client death findings
above remain release blockers even when the packaged startup/combat smoke passes.

The published `0.3.0-preview.1` release remains immutable. These changes belong
to the next verified batch; the release checks below do not certify new edits.

Batch 47 adds community field help and imports 118 example asset packages into
the plugin. The 108 original project assets retain their pre-import hashes.
Clean UE 5.7.4 and 5.8.2 compilation passes for Editor, Game Development, and
Game Shipping, with 287 automation tests passing on each engine. Both versions
compile 75 included Blueprints and validate 118 assets with zero errors and four
example warnings. The shared content is saved in 5.7 format; current definitions
and both retake conversations were restored through editor APIs and compared.
The installed-package consuming Game builds, cooks, stages, and packages pass.
Both packaged Entry-map startup checks exit zero without runtime errors. The
cooks retain 48 dependency warnings each. UDS missing-dependency checks pass in
the clean hosts; the separate functional UDS test passes in TDA. Visual quality,
performance, and the broader gameplay gates below remain open.
The community release is `0.3.0-preview.1`. See [release checks](RELEASE_VERIFICATION.md)
and [included content](INCLUDED_CONTENT.md) for the exact scope and migration notes.

TDA is on UE 5.8.2 with the local, unmodified Narrative Pro 2.4.2 package.
[Batch 46](DATA_ASSET_AUTHORING_AUDIT_2026-09-08.md) audits all 12 concrete
Territory data asset types and their authoring implementations. It fixes derived
Definition categories preceding `01`, adds the two missing Story & Quests creation
entries, rejects incompatible factory requests, and adds numeric and Native
dialogue-template validation. Dialogue recipe coordinates are bounded before
editor graph creation. The registry inventory finds 15 authored data assets;
Disguise has no project instance. **286 tests pass**, Editor/runtime/UHT and Game
builds pass, and 77 Blueprints/128 assets validate with zero errors and four
existing warnings. Farm's user-authored automatic capture configuration is
preserved. No gameplay authority, save schema, replication, or vendor source
changed. The broader implementation audit remains open; next inspect oversized
saved assault budgets and duplicate per-approach ledgers, then production/refund
settlement and the isolated AlMalik streaming fixture.
Its cook/stage passes with zero errors and 30 existing warnings. The matching
packaged Game passes the 90-second server-mode assault smoke with exit zero and
no runtime errors; the compiled TDAServer gate remains open.

[Batch 45](ASSAULT_SAVE_VEHICLE_BUDGET_2026-09-08.md) rejects negative saved
vehicle budgets and usage before they can grant fresh deployment credit. Invalid
nonterminal assaults cancel once, preserving deaths and withdrawing the remaining
finite force. A real Narrative save during NPC construction also verifies the
existing admission boundary; that suspected duplication was a false positive.
Editor/runtime/UHT and Game builds pass, with **282 passing tests**, 77 Blueprint
compilations and 128-asset validation (zero errors, four existing warnings).
The server/two-client load test passes client rejection, invalid-record
cancellation, physical NPC cleanup, unchanged decisions and repeat-load
idempotence. Oversized positive budgets and duplicate per-approach ledgers remain
the next malformed-record cases to inspect. A distinct cook/stage passes with
zero errors and 30 existing warnings. Its matching packaged Game completes the
90-second server-mode assault smoke with exit zero and no runtime errors; the
compiled TDAServer gate remains open.

[Batch 44](ASSAULT_TARGET_STREAMING_2026-09-08.md) fixes living attackers being
withdrawn after exhausting AI initialization retries against an unloaded target.
The existing registry wait now precedes goal initialization and preserves the
finite survivor and retry budget. Editor/runtime/UHT and Game builds pass, with
**281 passing tests**, 77 Blueprint compilations and 128-asset validation (zero
errors, four existing warnings). A listen-server/two-client probe preserves all
eight attackers during a 30-second target-registry absence, then recreates the
four tested Native goals for the same target. The native SaveGame regression
preserves the survivor GUID and decision. This is registry-boundary verification;
physical AlMalik cell streaming remains open. Its 19,726-descriptor inventory
contains no Territory actors, so that gate first needs an isolated authored fixture.
A distinct cook/stage succeeds with zero cook errors and 30 existing warnings;
the matching packaged Game completes a 90-second server-mode assault smoke with
exit zero and no fatal, assertion, ensure or Blueprint runtime error.

[Batch 43](ASSAULT_RETRY_BUDGET_2026-09-07.md) fixes saved spawn-failure counts
wrapping negative and bypassing finite cancellation. New-wave, physical survivor
and legacy failure paths share a saturating counter. Editor/runtime/UHT and Game
builds pass, with **280 passing tests**, 77 Blueprint compilations and 128-asset
validation (zero errors, four existing warnings). A 150-second server/two-client
probe agrees on eight killed, zero living/reserve/withdrawn and finite defeat,
with immediate capture removal for every injected death. It does not establish
the cause of the older combat behavior-tree crash. The batch report distinguishes
the successful gameplay probe from later Python reload and editor shutdown faults.
A distinct cook/stage/package passes with zero cook errors and 30 existing
content/tooling warnings; the staged executable matches the verified Game build
and completes a 90-second Game server-mode smoke with exit zero. This does not
replace the compiled TDAServer gate.

[Batch 42](CONDITIONAL_RETAKE_DIALOGUE_2026-09-07.md) adds modular Native planning
and handover examples, saved former ownership, and live diplomacy/power/Place
majority conditions. Multiplayer keeps flag-based automatic capture; the new
handover example requires explicit story capture and otherwise only reacts after
recapture. Editor/runtime/UHT and Game builds pass, with **279 passing tests**,
77 Blueprint compilations and 128-asset validation (zero errors, four existing
warnings). Seven live dialogue scenarios matched between a listen server and the
requesting client, including cached-offer ceasefire rejection and verified retake;
both clients received ownership/history and the unrelated client remained outside
the conversation. New examples are ready to assign; the custom existing Blacksmith
dialogue and Act 1 content are preserved. A fresh cook/stage included all five
new assets, and the matching packaged Game completed a 90-second server-mode
assault smoke with exit zero. Full release gates remain below.

[Batch 41](CASUALTY_DRIVER_LOSS_2026-09-07.md) remains the evidence for finite defeat after
a mounted driver casualty and seven later actual ASC deaths. Real save/reload
after driver loss also preserves the finite force without duplicate assaults.
The earlier weapon-melee, vehicle-departure and UDS fixes are in
[batch 40](MELEE_VEHICLE_LIGHTING_2026-09-07.md).

The immediate remaining work, in order:

1. Reproduce and symbolize the earlier behavior-tree decorator-search crash after
   clustered casualties. Passenger recovery is a separately confirmed fix; it
   does not establish that crash's cause. Batch 43's dismounted casualty probe
   passes but does not reproduce the combat/decorator stack. Matching AIModule
   symbols are still absent. A separate UnrealEd/Slate shutdown crash also needs
   attribution; a later clean editor shutdown does not establish its cause.
2. Complete remaining assault callback and malformed-record arithmetic review,
   next inspecting oversized positive vehicle budgets and duplicate per-approach
   ledgers. Batch 45 rejects negative saved car counts and verifies the actual
   Native save-only spawn callback. Batch 42 closed recurrence-counter overflow
   and stale ownership-directory reload; batch 43 closed spawn-failure retry
   overflow; batch 44 closed target streaming being charged as AI initialization
   failure. Prepare an isolated
   AlMalik assault fixture (the city inventory currently has no Territory actors),
   then exercise physical stream-out/in with posts, routes and returning clients.
3. Finish production save-only callback and refunded item-instance metadata
   handling, and the broader payout/currency settlement review.
4. Resolve the intermittent Manny bone-visibility and city empty-vehicle Chaos
   ensures, and outstanding city powerline/catenary/content warnings.
5. Review AlMalik interiors, dusk/night, authored local lights and cinematic GPU
   costs; calibrate HDR on the target display. Configuration checks alone do not
   certify the final image.
6. Complete the remaining AI/Tales/navigation/UI/editor audit and release tests.
   A compiled TDAServer target still needs a server-capable engine. Existing
   packaged Game server-mode and listen-server tests do not replace that gate.

The full findings register is the
[complete re-audit](COMPLETE_REAUDIT_2026-09-05.md). Earlier figures and topology
descriptions below are historical evidence from the September 5 baseline, not
current build receipts or a claim that all release gates are closed.

## Implemented baseline and prior verification

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
- Current-source verification on 2026-09-05 built the UE 5.7 Game and `TDAEditor` Development
  targets and passed all 213 `TerritoryFramework.*` tests. The automation log contains no failed
  test, Blueprint Runtime Error, Accessed None, assertion, fatal, or Territory error.
- The reusable Narrative Dialogue AAA shot pack now supplies seven project-owned Level Sequences.
  Every Cinecam is an explicit spawnable with a Spawn track and Camera Cut, and Story Capture
  validation rejects incomplete camera bindings before runtime.
- The Editor Utility Blueprint HDR Scene Maker configures the Narrative Ultra Dynamic Sky bridge,
  a tagged Lumen Post Process Volume, quality presets, tooltip guidance, and a loaded-scene memory
  estimate without changing global scalability, OS HDR output, or Narrative vendor assets. Its
  final read-only audit now checks 15 scene/rendering contracts, detects competing unbound volumes,
  publishes editor notifications, and reports the loaded map ready only when no warning or error
  remains.
- All nested active-assault, diplomacy, strategic-directory, economy, and evaluation fields now
  participate in Unreal's `SaveGame` archive. A packaged game created an active assault, saved it,
  reloaded it once, exited, and a second independent packaged process restored the same live
  assault once from the same slot.
- A packaged dedicated-listener process admitted two independent clients while a real immediate
  Bandit-versus-Heroes assault was live. Both clients were welcomed and created their own server
  pawn. Three authored PlayerStarts now remove the previous second-client origin-spawn failure.
- A later two-client pass showed both clients in the world, the contested Territory HUD,
  the remote player, and the vehicle assault. It also exposed two project Blueprint timing defects
  hidden by the shorter gate: duplicate assault activity cleanup after death and Narrative local UI
  initialization before possession created `GameplayHUD`. The controller now waits on Narrative's
  real HUD lifecycle instead of a fixed delay. The refreshed package admitted both clients with no
  HUD/LoadingMenu None errors, early null-ASC/divide-by-zero warnings, or server-side readiness
  timeout.
- Hard difficulty now produces two complete sedan squads for the authored eight-person assault:
  each car has one driver and three passengers, and car two waits until every member of car one has
  resolved. A single casualty never consumes a new vehicle as a partial top-up.
- A no-asset-registry-cache cook of `HopDistrictTest` and a fresh stage/package/archive completed
  with zero errors. The invalid Fire cue, missing mobile touch interface, missing player appearance
  materials, and project-owned Narrative demo loadout leaks were repaired.
- Latest scoped editor verification compiled 72 Territory Blueprints and validated 113 assets
  under the project/plugin Territory paths plus `HopDistrictTest` and the selected soundtrack,
  with zero errors. Four explicit authoring warnings remain: two prototype NPC appearances,
  Farm dialogue's missing cinematic shot, and its zero blend-out time. See the
  [rendered follow-up](RENDERED_FOLLOWUP_2026-09-05.md).
- A current `/Game/HopDistrictTest` headless runtime smoke loaded the map and player/HUD, then
  exited normally with no Blueprint runtime, Accessed None, assertion, ensure, fatal, or Territory
  warning/error.
- Command Center typography now uses one tested 11-30 px hierarchy. Generated text survives
  CommonUI's final synchronization at its requested size, tab/button labels use title case, and
  player-facing Benefits no longer expose raw Gameplay Tags or Blueprint class prefixes.
- Command Center tabs now maintain exactly one selected item after any number of clicks. Normal,
  hover, selected, and selected-hover states are visually distinct, and Intelligence filters use
  the same state contract while filtering the real report query.
- Player counterattack reports now use localizable status/outcome labels instead of C++ enum
  identifiers, Gameplay Tag names follow Narrative Pro's friendly-name settings, and the District
  Management panel shares the Command Center's compact typography and interaction states.
- Every Definition now controls its exact passive gameplay HUD card without muting notifications,
  POIs, map/compass markers, Command Center intelligence, or management. Cities default quiet;
  Districts and Places default visible.
- A rendered follow-up verified the HUD policy and the Command Center inside the real player
  menu, removed the authored row's default button caption, and corrected Blueprint vehicle-seat
  discovery in data validation. Blacksmith's Claimed soundtrack uses the selected Unity in the
  Ashes track through a project-owned Narrative music set.
- The documentation learning path is uniquely numbered from 00 to 36. Reports and tutorials
  are separate appendices.

## Release verification status

These are verification jobs, not permission to invent a second gameplay authority.

1. **Dedicated topology — admission and live Hard assault passed.** A packaged Game executable ran
   as a real listener with two separate client processes and a live immediate assault.
   Both joins, pawn spawns, Territory HUDs, remote-player presentation, forward road ingress, and
   dismount were verified. The refreshed package also proved the lifecycle fixes and two complete
   four-seat waves. Still manual: capture a Place, recruit one guard,
   patrol, resolve the assault, and observe exact-once capture/XP presentation on both clients.
2. **Live save slot — passed.** A finite active assault survived save/reload once in-process and
   restored once again after a complete packaged-process restart. Both strict gates reported the
   exact one-item live set with zero unexpected or duplicate IDs. The 211-test suite also covers
   the nested archive contract.
3. **World Partition map — partly verified; release blockers remain.** An isolated AlMalik
   fixture now verifies physical Place/post streaming and capture-enabled assault save/load
   with appearance assets already loaded. Cold appearance initialization, defeat after a
   restored wave, and a returning client still need successful verification. HopDistrictTest
   is not World Partition-enabled. See the September 10 checkpoints above.
4. **Packaged cook/game — passed for the installed engine.** A clean no-cache Windows cook plus
   stage, package, and archive completed with zero errors. Epic's installed UE 5.7 build cannot
   produce separate Client/Server target binaries; a source engine or installed Server support is
   required for that optional target split. The packaged Game binary successfully exercised the
   dedicated-server topology.
5. **Authored road playtest — partly passed.** The authored assault vehicle claimed its Narrative
   driver, followed ten ZoneGraph guide points, stopped, dismounted, and completed takeover in the
   existing runtime gate. Still manual: reverse boss pursuit, deliberate traffic blocking,
   damage-triggered abandonment, cleanup timing, player carjacking, and final-fight camera framing.
6. **Project cleanup — project-owned defects fixed; vendor cue warnings remain.** The registered
   damage cue and touch-interface configuration are valid, the Territory melee ability no longer
   emits Narrative's firearm `GameplayCue.Weapon.Fire`, missing player appearance overrides were
   removed, all three Territory NPC definitions grant only the Territory weapon, three PlayerStarts
   are present, and the map owns a RecastNavMesh. Narrative's generic unarmed-impact cue still
   dereferences a null Niagara decal result on a dedicated server, and its damage cue can read a
   not-yet-created Character Visual on a replicated client. Those graphs live in Narrative Pro;
   Territory Framework must not patch the vendor assets.

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
