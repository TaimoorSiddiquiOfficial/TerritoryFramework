# Defenders staged across floors, with a story cutscene

Receipt: [FLOOR_STAGING_2026-09-24.json](FLOOR_STAGING_2026-09-24.json) — 31 checks, all green,
`unresolved: []`. Probe: `Scripts/Territory/verify_floor_staging_pie.py` (project repo), fixture
hook: `Scripts/Territory/arm_floor_staging_pie_fixture.py`.

The goal is a Far Cry / Max Payne-shaped game: story-driven, with scripted defender staging and
cutscenes, where defenders are staged on specific floors. This record closes the three verified
negatives the story-surface audit opened — no guard-spawned delegate, no explicit floor concept,
and Territory never starting a sequence — and proves them in live gameplay on the real
HopDistrictTest Blacksmith, not in reflection.

## In the game

1. The Blacksmith authors three floors over its seven guard posts: **Ground Floor** (2 posts),
   **Upper Floor** (3), and a third that holds the 2 posts this level cannot deploy to.
2. It registers staffed to 5 and deploys one guard per post. Each arrival fires
   `FOnTerritoryDefenderSpawned(Volume, Guard, FloorIndex)` carrying that post's authored floor.
3. Killing the two guards on the Ground Floor fires floor-cleared **once** for floor 0 and plays
   the `LS_FightCutscene` sequence the floor authors, which suppresses movement and look for the
   audience and releases both when it stops.
4. The Upper Floor is still defended and is *not* reported cleared while it is. Clearing it fires
   floor-cleared **once** and starts **no** sequence, because floor 1 authors no event.
5. A designer reads "is floor 2 cleared" through one shared rule, so a HUD, a quest and this proof
   cannot disagree about it.

## What the run actually put up

This project's editor launches PIE as `PlayNetMode=PIE_Client`, `PlayNumberOfClients=2`,
`RunUnderOneProcess=True`, so a run is **three worlds** — `UEDPIE_0` (server, the only one with a
game mode) plus `UEDPIE_1`/`UEDPIE_2` (clients) — and the late-joining 4th client becomes a fourth
(`UEDPIE_3`). `recon['pie_worlds']` records the shape in the receipt.

**The approved plan asked for a single-player PIE run and then a two-client + late-join run as two
separate steps. They are the same launch in this configuration**, so both are evidenced by this one
run rather than by a second one. The receipt path is `Saved/Verification/FloorStagingPIE/SinglePlayer.json`,
which is the name the plan specified and four earlier runs used; the name is misleading — nothing
here is single-player — and the docstring of the probe, not the filename, is what states the shape.

## Two clients, then a late joiner

- **The cutscene reaches exactly the controllers it named and nobody else.** Audience of exactly
  one (`owner_counts: [1]`), staged in `UEDPIE_1` only; `UEDPIE_2` is a client world with a local
  controller and is correctly left alone. `frames_frozen_outside_its_staging: 0` and
  `no_client_was_frozen_without_a_cutscene_staged_for_it` is green. This matters because the vendor
  gates a sequence on `OwnerControllers`, which is `ExposeOnSpawn` and **not replicated** — the
  client's own copy reports `owners: []` while the authority's reports the real audience
  (`recon['sequence_at_cutscene_end']`), so the authority is the only place the audience can be read.
- **The per-floor read model reaches both clients through the existing snapshot path**, with no new
  RPC and no new replicated member: `replica_gaps` is `[]` for both original clients and both hold
  `{0: cleared, 1: cleared, 2: not cleared}` field-for-field. Asserted **after** the fight, because
  that is the transition that exercises replication rather than its first copy.
- **A late joiner sees the same floors.** `TerritoryAuditEventProbe.request_late_join_for_pie()`
  produced `UEDPIE_3` as a client world (`has_game_mode: false`), and its Blacksmith replica holds
  the server's three floors identically.
- **A late joiner is not handed a finished cutscene.** `late_joiner_sequences` is
  `{actors: 0, playing: false}` — a client absent from `OwnerControllers` is not given the sequence.
  The positive half of that gate is proven by the audience of one above, so both halves hold.
- **A client replica never broadcasts the clear itself.** The same callable was bound to the
  server's volume and to the late joiner's, and the counts are `{server: 2, clients: {UEDPIE_3: 0}}`.
  The check requires the replica to demonstrably *hold* the cleared floors as well, so a bare zero
  from a client that could never fire is not accepted as evidence.

## The instrument error this run found, and that the previous run corrected

Run 15 (`FLOORSTAGE_REPLICA_LATEJOIN`) went red on two checks with `late_joiner_floors: {}`. That
was **my instrument, not the framework**: a PIE client world context exists before the client has
finished joining, and it starts holding the map's actors without the authority's replicated values,
so reading the newcomer the instant it appeared saw the Blacksmith with an *empty* snapshot. The
same comparison had already passed field-for-field for `UEDPIE_1` and `UEDPIE_2`, and `replica_gaps`
was empty for both.

The read now waits for a non-empty model, bounded by the phase ceiling, and records
`late_joiner_waiting` while it waits so a client that never arrives still reports as itself. Run 16
shows the fix working end to end: `late_joiner_waiting = {seconds: 4.56, model: {}}` then
`late_joiner_joined_after_seconds = 5.17` with the three floors intact. The non-vacuity guard earned
its keep during the failure — it refused to let `clients: 0` count as evidence while the client held
no floors.

Two reds that are instrument error are reported here rather than quietly dropped, because the run
that found them is the reason the fixed run means anything.

## Authority and replication

- `ATerritoryVolume` remains the sole owner of territory state. Floors are **derived from the spawn
  points that already own the real state** and never stored, so no new authority exists and
  renumbering a floor is a pure authoring change.
- `FTerritoryGarrisonSnapshot` is a plain replicated USTRUCT with `OnRep_GarrisonSnapshot` and
  change detection by `operator==`, so per-floor numbers extend an existing read model with zero new
  replication plumbing. `operator==` was extended to compare them; leaving it alone would have
  silently defeated the change detection that makes replication fire at all.
- Floor-cleared is fired from `RefreshGarrisonSnapshot()` by diffing previous and new floor entries
  for a >0 → 0 transition, and is fired **before** any ownership change — the discipline the existing
  `AllDefendersDefeatedEvents` path already uses. `HasAuthority()` still guards the whole path, and
  the client-replica count of zero is the evidence.
- The spawn point is **not** replicated, so a floor authored on a Definition row reaches the server
  only; clients see floors through the volume, which is what the replica checks prove.
- No dedicated-server smoke run, per the chosen proof scope.

## Save/load

No save migration is needed, and that is a property of the design rather than an omission: all floor
state is derived from spawn points, so nothing durable changed shape. Floors are not persisted;
`FTerritoryGarrisonSnapshot` is not `SaveGame`. `FTerritoryGuardPostTemplate` already carries the
`StableGuardPostGUID` that §7 requires, so the floor assignment hangs off an identity that already
existed.

## Narrative Pro reference and reuse

- `ANarrativeLevelSequenceActor::CreateNarrativeLevelSequencePlayer` is the vendor's replicated,
  `Players`-filtered, relevancy-culled cutscene starter, and Territory calls it rather than
  hand-rolling playback or inventing a client RPC path. Narrative stays the presentation authority.
- Input suppression is a **data flag on `PlaybackSettings`, not an API call Territory makes**:
  `ULevelSequencePlayer::EnableCinematicMode` reaches `PC->SetCinematicMode` on every *local*
  controller of the *sequence player's own world*, which is why the suppression readings are taken
  in the audience's world and not the server's.
- `UTerritoryPlayCutsceneEvent` is a `UNarrativeEvent` added to an existing family of eight in
  `TerritoryStoryEvents.h`, placeable in the existing `DefenderDiedEvents` /
  `AllDefendersDefeatedEvents` arrays and in the new `FloorClearedEvents`, so story reactions stay
  authored data gated by their own Narrative conditions.
- Per-floor objectives reuse `UTerritoryStateTask` with an optional floor filter; no new Narrative
  node type.
- **No Narrative Pro source file was modified.**

## Sequence-actor teardown: implemented, and no longer a limitation

This section **replaces** an earlier "Known limitation: the sequence actor is never destroyed",
which recorded the plan's step-5 assertion "the sequence actor is cleaned up" as **NOT satisfied**
and measured `sequence_actors_after_the_quiet_floor = 2`. That was accurate for the batch it
described. It is not accurate now, and the old text is stated here rather than deleted so the change
is auditable. The teardown was implemented as its own coherent batch — exactly as that section said
it had to be ("a deliberate teardown policy ... plus its own test and mutation control") — and the
same probe now measures the count at **0**.

What changed: `UTerritoryPlayCutsceneEvent::ExecuteEvent_Implementation` ends by calling
`UTerritoryCutsceneTeardownComponent::ScheduleAfterSequence(SequenceActor, TeardownGraceSeconds)`, a
new Territory-owned component attached to the actor it will destroy. It binds the sequence player's
`OnStop`, which is the only signal covering all three ways a cutscene ends: a natural finish reaches
`StopInternal` through `FinishPlaybackInternal`, an explicit `Stop()` calls `StopInternal` directly,
and a skip-to-end (`GoToEndAndStop`) also calls `StopInternal` directly and therefore never
broadcasts `OnFinished` at all. `OnFinished` is bound as well, behind an `IsPaused()` guard, because
`StopInternal` skips the whole `OnStop` broadcast when the entity system runner declines to queue
its final update (`MovieSceneSequencePlayer.cpp:540`), and a sequence that genuinely stopped would
otherwise leak.

Three details are load-bearing, and each has its own test:

- **`IsPaused()` can never reject a genuine stop.** `StopInternal` assigns
  `Status = EMovieScenePlayerStatus::Stopped` inside the same `if (IsPlaying() || IsPaused())` block
  that broadcasts `OnStop` (`MovieSceneSequencePlayer.cpp:466-529`), so by the time the handler runs
  the player is not paused. On the `OnFinished`-after-`Pause()` path the status really is `Paused`,
  and that guard is what keeps a live dialogue-shaped shot from being destroyed.
- **Grace `<= 0` destroys directly, never `SetLifeSpan(0)`.** `AActor::SetLifeSpan` takes its
  clear-the-timer branch for any value `<= 0` (`Actor.cpp:6577-6604`), which would leave the actor
  alive with no timer at all — the silent version of the very leak being fixed.
- **Authority only.** A client must not locally destroy a replicated actor; the client's copy is
  removed by the destruction replicating.

The grace (authored `TeardownGraceSeconds`, default 1.0s) exists because the destruction replicates:
a client whose own copy is a fraction of a second behind must not have its cutscene cut off mid-shot.
**A client that lags longer than the grace would still be cut short** — that is the remaining,
deliberate limit of this policy, and the authored field is the escape hatch.

The two diagnostics that located the leak stay committed beside this record rather than in a session
transcript, and they are what made the fix targetable: `Scripts/Territory/_probe_cutscene_sequence_actor.py`
(finds the actor, reads the floor-0 event array at runtime, and calls the event directly to separate
"never ran" from "ran and started nothing") and `Scripts/Territory/_probe_authored_cutscene_event.py`
(reads the instanced event back off the live Definition, which the receipt cannot distinguish from an
event that ran). Both are diagnostic-only and neither is part of the delivered path.

### Measured, same probe, same fixture

Marker `FLOORSTAGE_TEARDOWN`: harness `ok: true`, `lifecycle: teardown-complete`, `warnings: []`,
`total_matches: 0`; the probe itself `passed: true`, **32 checks, `unresolved: []`, 47.8s**.

| Reading | Before | Now |
|---|---|---|
| `sequence_actors_after_the_quiet_floor` | 2 | **0** |
| `sequence_actors_at_clear1` | — | 0 |
| `quiet_sequence_peak` | — | 0 |
| `quiet_teardown_waited_seconds` | — | 0.03 |

`quiet_teardown_waited_seconds = 0.03` is the reading that makes the zero mean something. The phase
dwells 3.0s before sampling, and the teardown had **already finished** by the time that dwell
elapsed — the actor was gone at `clear1` as well. The zero is a prompt teardown observed, not a
drain that happened to complete inside the observation window.

Two checks that were green before are re-asserted, because this batch changes actor lifetime:
`control_returned_when_the_cutscene_stopped` — the engine releases input before any finish/stop
delegate fires, so destroying the actor cannot strand the player, now proven in live gameplay rather
than by argument — and `cutscene_stopped_rather_than_paused`.

The quiet phase's own check was **rewritten, not merely re-baselined**, and this is the trap that
would have looked like a regression. `floor_without_an_authored_event_starts_no_sequence` used to
compare the live count against a number captured at `clear1`. Once the leak was fixed, the teardown
can complete *between* `clear1` and the quiet phase, so that equality would have gone **red purely
because the fix worked**. It now reads "the count never rose across the phase"
(`quiet_sequence_peak <= quiet_start_count`), which is what "starts no sequence" always meant, and
`the_cutscene_actor_is_destroyed_once_its_sequence_stops` is the new check for the count reaching
zero.

### Mutation control for this batch

Each new native test was seen to fail against a deliberately broken production value before it was
counted, with the reds attributable to one policy each and the unaffected tests left green as the
specificity control:

| Mutation | Tests seen red |
|---|---|
| call site removed (the exact bug) | after-it-stops, skip-to-end, zero-grace, paused (at its control line) |
| `OnStop` binding dropped | after-it-stops, skip-to-end, zero-grace, paused (at its control line) |
| `SetLifeSpan` armed unconditionally | zero-grace **only** — "No cutscene actor is left behind" expected 0, got 1 |
| authority gate dropped | server-authoritative **only** |
| `IsPaused()` guard dropped | paused, at its guard assertion **only** |

The drop-`OnStop` run is what justifies binding `OnStop` at all: `SequenceActorIsDestroyedOnSkipToEnd`
exists precisely because `GoToEndAndStop` never broadcasts `OnFinished`, and it goes red when the
binding is removed. Note the paused test carries **two** assertions, and the runs reddened them
separately — its control line by the dropped binding, its guard line only by the dropped guard.
All 12 tests then re-ran green: `**** TEST COMPLETE. EXIT CODE: 0 ****`.

## Level-authoring constraints this proof surfaced

These are requirements on authors, not defects, and each is enforced rather than hidden:

1. **Every post on a clearable floor must be staffed.** A floor reports cleared only when every post
   on it has spent its reserve, and a reserve leaves a post only through the slot that post's own
   guard vacates by dying. A post the Territory never staffs keeps its reserve for the whole run and
   pins its floor uncleared forever, so that floor's `FloorClearedEvents` never fire. The fixture
   therefore raises `InitialGuardCount` to the deployable post count;
   `the_staffing_target_covers_every_post_on_a_floor_the_run_clears` and
   `every_post_on_a_floor_the_run_clears_is_staffed` are the gates that fail loudly if this drifts.
2. **A post whose guard cannot be placed is refused after the spawn and silently reduces the
   garrison.** `TrySpawnSingleGuard` spawns the guard and then discards it when
   `TerritoryGuardSpawnValidation::IsPlacementAcceptable` fails — the settled capsule must stay
   within 10cm of the resolved transform. Both HopDistrictTest posts at X=-2630 resolve about 17cm
   too high and are refused with an engine Error naming expected and actual transforms. One spawn
   point = one active guard (`GetEffectiveMaxGuards()` returns a hardcoded `1`), so a floor's
   capacity *is* its post count and data cannot exceed physical reality. The fixture gives those two
   posts a floor of their own rather than nudging them to hide the problem; correcting it is level
   authoring (the ground surface at that X).
3. **`UTerritoryDefinition::IsDataValid` requires every post's `FloorIndex` to name a declared
   floor**, so a post cannot be left off the floors entirely to dodge the above. The same validation
   rejects an authored `DesiredGuards` above that floor's post count as an error rather than clamping
   it, because a quota that cannot be achieved is a level-design bug and silently clamping it would
   hide it.

The seven HopDistrictTest posts all sit at Z=50, so this is a data-level split of one storey, not
physical multi-storey geometry. What the run proves is the path from the authored row to the post,
the per-floor read model, the delegates and the cutscene; a level that actually places posts on
upper floors is a level-authoring task, not a code one.

## Deviations from the approved plan

1. **Floor-clear is dispatched from `TryCompleteDefenderDefeat` with `FloorsBeforeLoss`**, not from
   `RefreshGarrisonSnapshot` as the plan's Part 4 described. The plan's site would have had to
   reconstruct the previous floor entries; the defeat path already holds them.
2. **The plan's 6b step 4 `OnFinished` + `Stop()` binding is replaced by forcing
   `Settings.bPauseAtEnd = false`** on the created sequence. Same outcome (control returns, and the
   check that catches the trap is green), simpler. The actor-lifetime consequence this briefly left
   behind — the sequence actor surviving the session — is closed by its own later batch; see the
   teardown section below, which also records why the original `Stop()`-then-destroy shape was not
   the right vehicle for it.
3. **"Single-player PIE" and "two-client PIE" are one launch in this project's editor
   configuration**, as described under "What the run actually put up". The receipt says so rather
   than claiming a standalone single-player run.
4. **The earlier claim that `SetCinematicMode` "is called by nothing" is right about direct calls
   and wrong about reachability.** The engine reaches it indirectly through
   `ULevelSequencePlayer::EnableCinematicMode` on every sequence whose playback settings request
   cinematic mode, including Territory dialogue shots. `Docs/TERRITORY_STORY_SURFACE_2026-09-23.md`
   carried that claim at line 220 and has been corrected.
5. **The sequence actor was not destroyed by this batch**, where the plan's step 5 asserted it was
   cleaned up. Closed afterwards, by its own batch — see the teardown section below.

## Mutation control

`recon['observer_self_test']` is the control for the clear observer itself: on the client world's
controller it recorded `raised: [true, true]` and `cleared: [false, false]` with `works: true`,
proving the observer can be *seen* to go red before its zero on the replica counts as evidence. Per
`test-after-every-step`, each new native test was likewise seen to fail against a deliberately broken
value before it was counted.

## How to re-run

```
editor_query run_pie_smoke:
  map: /Game/HopDistrictTest          duration: 115
  marker: <yours>                     on_compile_errors: refuse
  log_patterns: [Blueprint Runtime Error, Accessed None, LogChooser, LogPython: Error, Traceback]
  stages.pre_pie.python: exec arm_floor_staging_pie_fixture.py, then verify_floor_staging_pie.py
```

The probe self-drives on a slate post-tick callback, writes
`Saved/Verification/FloorStagingPIE/SinglePlayer.json`, and restores the Definition it was armed on
before finishing — including on every failure path (`fixture_restored` is green, and the originals
are saved to `Saved/Verification/FloorStagingPIE/DefinitionOriginals.json`). The Definition is
modified **in memory only**; nothing authored is written to disk.

`poll_pie_smoke` returns `ok: false` while a session is still `running`; poll until
`status == "complete"` before reading the verdict. Run 16's harness verdict: `complete`,
`lifecycle: teardown-complete`, `ok: true`, `warnings: []`, `missing_must_present: []`,
`total_matches: 0`, every must-absent count 0.
