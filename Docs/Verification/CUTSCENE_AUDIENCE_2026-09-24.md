# Territory bounds its cutscene's cinematic sweep to the audience the sequence names

Status: **fix applied and verified red and green headlessly.** `TerritoryFramework.Presentation.Cutscenes`
passes 24 under both allocators and the full suite passes 429 under both. The isolated listen-server
harness was run, is green, and is recorded in full below - **and it does not exercise the release**,
because in every run of it the fixture's audience resolved to the authority world's only local
controller. That gap is stated, not papered over; what the harness does prove, and what it cannot, is
the subject of its own section.

Adversarial-audit finding, claim 7: replication audience does not bound local server effects.

## The mechanism

Two engine facts decide the shape of any fix, and both were read in the engine source rather than
inferred.

`ULevelSequencePlayer::EnableCinematicMode` walks **every local controller in its own world** and
suppresses it, with no audience input at all (`LevelSequencePlayer.cpp:381-400`). It iterates
`World->GetPlayerControllerIterator()` and tests only `IsLocalController()`. The vendor's
`OwnerControllers` never reaches it: that list gates `IsNetRelevantFor`, i.e. which clients *receive*
the actor, and that is a different question from which local controllers the sequence on this machine
suppresses.

And it runs on the **first update after playback starts**, not at `Play()`: the call comes from the
player's own update reaching `OnStartedPlaying`. So a release issued synchronously from
`UTerritoryPlayCutsceneEvent::ExecuteEvent` would be overwritten one frame later, and would look
correct in any test that only read the flag immediately after starting the cutscene.

The obvious fix - strip the cinematic flags on the authority - is foreclosed by a third fact:
`FMovieSceneSequencePlaybackSettings` **replicates** (`MovieSceneSequencePlayer.cpp:231`), so clearing
the flags on the authority clears them on the audience client too, and the audience client is the one
client that should keep them.

## The fix

`UTerritoryCutsceneTeardownComponent` already owned the actor, the sequence player and the stop
binding, so the bound lives there: no new class, no vendor edit, no new authority over anything.
`ScheduleAfterSequence` now calls `ArmAudienceReconcile()` after the stop bindings are in place, and
the component gains one private per-frame handler and a disarm.

Arming decides, once, whether there is anything to bound:

```cpp
	// A client must never do this: its local controller is the audience for the server's
	// suppression, which arrived over the reliable ClientSetCinematicMode RPC.
	if (!World || World->GetNetMode() == NM_Client) return;

	// An empty audience is the vendor's own "everyone", so the engine's sweep is exactly right.
	const TArray<TObjectPtr<APlayerController>>& Audience = Subject->OwnerControllers;
	if (Audience.IsEmpty()) return;

	// EnableCinematicMode returns before touching a controller unless one of these four is authored.
	const FMovieSceneSequencePlaybackSettings& Settings = Subject->PlaybackSettings;
	if (!Settings.bDisableMovementInput && !Settings.bDisableLookAtInput
		&& !Settings.bHidePlayer && !Settings.bHideHud) return;
```

Then it collects the world's local controllers the audience does not name, excluding any that were
**already** in cinematic mode when it armed - something else put that controller there deliberately
and Territory has no standing to take it back - and returns without subscribing if that set is empty.
The handler itself releases each collected controller only while playback genuinely runs
(`Player->IsPlaying()`, so a paused sequence stays suppressing), only while the controller is still
frozen, and using the actor's own `PlaybackSettings` - the same flags in the same argument order as
the engine's call, so the release is the exact inverse of the suppression being undone.

Three properties are deliberate and each has its own test:

- **Inert by construction in the ordinary cases.** Single player, a client, a dedicated server, an
  audience the vendor left empty, and a sequence with no cinematic flags authored all return at
  arming, taking no subscription and logging nothing.
- **Never releases a suppression it did not make.** A controller already frozen at arming is skipped;
  the class comment on `UncoveredControllers` says why the list is weak (a controller may be destroyed
  mid-cutscene and a release must never resurrect it).
- **Never grants control to a remote client, never spawns, never replicates.** Everything here is
  local-controller bookkeeping on the authority world.

### Deviation 1: the hook is the world's sequence tick, not the component's tick

The plan said *"enable ticking (currently false)"*. That is not available, and the compiler and the
engine agree:

- `AActor::PrimaryActorTick.bCanEverTick` is **false** on `ALevelSequenceActor`, and
  `AActor::SetActorTickEnabled` is gated on that flag, so calling it does nothing at all - silently.
- `AActor::RegisterActorTickFunctions` is **protected**, so no component can re-register it.

The hook is therefore `UWorld::AddMovieSceneSequenceTickHandler`, which is the same delegate
`UMovieSceneSequenceTickManager` subscribes to in order to tick the player. It is the only per-frame
entry point an `ALevelSequenceActor` has, and it happens to be the correct one: the reconcile is only
sound when it runs in the same frame sequence as playback.

### Deviation 2: the guard is per controller, not a "strict subset" test

The plan said to *"guard it to act only when the audience is a strict subset of that world's local
controllers"*. The realised guard is the same condition per controller instead: a controller is
skipped when it is in the audience, skipped when it was already frozen, and the reconcile does not
subscribe at all when nothing survives. The strict-subset form computes the same set and then discards
it; the per-controller form *is* the work list, which is why it can also express the
already-frozen exclusion that the subset test cannot. The property the plan was protecting - the
common cases stay inert without a test - is asserted directly by
`...Cutscenes.AudienceReconcileStaysInertWhenItCoversEveryLocalController` and
`...Cutscenes.AudienceReconcileHoldsNothingWhenThereIsNothingToBound`.

### The frame lag, and why the harness threshold is two

`TMulticastDelegateBase::Broadcast` calls its bound functions in **reverse** registration order
(`MulticastDelegateBase.h:299-300`, and the engine's own comment gives the reason: an instance added
by a callee must not be called inside the same broadcast). This handler registers *after* the tick
manager, so it runs *before* playback ticks - not after, which is the intuitive reading and the wrong
one. The release therefore lands on the frame **following** the engine's suppression.

That is sound only because the reconcile repeats and the engine's flag never re-arms: suppression
happens once, from `OnStartedPlaying`, so a release that arrives one frame late is still the last word
on that controller. It is also why the `bCinematicMode` gate is load-bearing rather than an
optimisation - `APlayerController`'s outer setter has no change guard and calls the reliable
`ClientSetCinematicMode` RPC unconditionally, so an ungated release would send that RPC every frame
for every bystander.

The harness consequently judges over two frames rather than one, in
`every_frozen_local_controller_was_named_by_the_sequence_that_froze_it`: one unnamed frozen frame is
the correct behaviour, and the defect is the frame after that and every one to the end of the
sequence.

### `UTerritoryDialogueShot` is measured, not bounded

Left unchanged by design - a dialogue shot may legitimately be shared, and bounding it is a
dialogue-design question with its own test surface - but its forced flags are now **recorded** as a
receipt rather than left as an unstated residual.
`...Cutscenes.DialogueShotForcedFlagsAreRecordedNotBounded` asserts that a Territory-authored cutscene
forces none of the four flags, that a `UTerritoryDialogueShot` still forces `bHideHud`,
`bDisableMovementInput` and `bDisableLookAtInput`, and - by vendor reflection - that
`UNarrativeDialogueSequence::PlaybackSettings` is `CPF_Edit` and `CPF_BlueprintVisible`. Its comment
says what it is for: *"so that the next reader cannot mistake 'cutscenes are bounded' for 'Territory
is bounded'"*.

## Verification

| Run | Command | Result |
|---|---|---|
| Cutscenes, default | `Tools/Run-Tests.ps1 -Filter TerritoryFramework.Presentation.Cutscenes` | `exit=0 success=24 fail=0 crash=False` **PASSED** |
| Cutscenes, stomp | same, `-Stomp` | `exit=0 success=24 fail=0 crash=False` **PASSED** |
| Full suite, default | `Tools/Run-Tests.ps1 -Filter TerritoryFramework` | `exit=0 success=429 fail=0 crash=False` **PASSED** |
| Full suite, stomp | same, `-Stomp` | `exit=0 success=429 fail=0 crash=False` **PASSED** |

429 is 422 plus the seven tests this batch adds. All seven are new; no pre-existing assertion changed.

### The red observation

One deliberate break, applied to the release gate in `HandleSequenceTick` and reverted immediately:

```cpp
	if (!Controller->bCinematicMode) continue;   // production
	if (Controller->bCinematicMode) continue;    // the break
```

It compiles, and it is the nearest plausible bug - a sign error in the one predicate that decides
whether a controller is released. Observed: `exit=255 success=22 fail=2 crash=False`, and exactly the
two tests that assert a release went red while the five that assert *no* release stayed green, which
is what makes the failures attributable rather than collateral.

Verbatim, assertion messages exactly as reported:

```text
Name={CinematicModeIsReleasedOutsideTheAudience}
Expected 'The bystander is released' to be 2, but it was 1.
Expected 'The bystander's last word is control, not suppression' to be false.
Expected 'The bystander really is out of cinematic mode' to be false.
Expected 'Later frames release nothing, because there is nothing left to release' to be 2, but it was 1.

Name={AudienceReconcileStopsWhenTheSequenceEnds}
Expected 'and the grace period releases nobody further' to be 2, but it was 1.
```

The first three are the release itself being falsified: with the gate inverted, the bystander's last
recorded call is a *suppression*, and it is still in cinematic mode when the sequence ends. The other
two are the idempotence assertions, which is the failure the `bCinematicMode` gate exists to prevent.
Note what the messages do and do not say: they cite the *behaviour* (`released`, `last word`, `grace
period releases nobody further`), not line numbers, and they are the observed text rather than a
paraphrase.

After the revert the editor target was relinked (`Result: Succeeded`) and both allocators re-ran green
at 24 and 429 - the table above.

Earlier in the batch five further breaks were run, each isolating a different predicate, and each was
attributed to its own assertion group before the next was applied: R1 the reconcile disabled by never
resolving a controller (3 failures, including `CinematicModeIsReleasedOutsideTheAudience` and
`AudienceReconcileDoesNotReleaseAPausedSequence`); R2a the already-cinematic exclusion removed
(`A controller another system already froze is not claimed by this reconcile`);
R2b the empty-audience early return removed
(`An audience the vendor left empty covers everyone, so nothing is uncovered`);
R2c the four-flag gate removed; R3a the self-termination branch removed
(`The reconcile hands the world tick back once it has nothing to bound`); R3b the playing-only guard
removed (`A paused sequence releases nobody` 1 → 2, `so the bystander stays in cinematic mode` true →
false); R3c the already-released gate removed (`Later frames release nothing...` 2 → 4). R3's three
breaks were run together because they are disjoint in attribution, and the group produced three
separate assertion failures - one per break - which is what makes the grouping safe.

## The isolated listen-server harness

Per the decision taken for this batch, the repository's default PIE configuration was **not** changed.
The listen-server launch settings (`PlayNetMode=PIE_ListenServer`, `PlayNumberOfClients=2`,
`RunUnderOneProcess=True`) were applied to `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`
for the runs and then restored **byte-identically** from a backup (`cmp -s` reported identical; sha256
prefix `6ee858adcbe504c1` before and after), leaving `PIE_Client` / `PlayNumberOfClients=1` committed.

The cited receipt is `CUTSCENE_AUDIENCE_2026-09-24.json` beside this document: `passed: true`,
`phase: restore`, 39.19s, `unresolved: []`, 33 checks and none failing.

| Reading | Value |
|---|---|
| `pie_worlds` (end of run) | `UEDPIE_0` (has game mode, 1 local controller), `UEDPIE_1`, `UEDPIE_2` - 2 client worlds |
| `launch_settings` | readable, `PlayNetMode=PIE_ListenServer`, `PlayNumberOfClients=2` |
| `cutscene_audience` | `owner_counts [1]`, `staged_client_worlds []`, `frozen_client_worlds []` |
| `authority_audience` | `judgeable_local_controllers` = the authority world's host controller; `frozen_and_named_frames 203`; `frozen_and_not_named_frames 0` |
| `authority_audience_was_exercised` | `true` |
| `no_client_was_frozen_without_a_cutscene_staged_for_it` | pass (`frames_frozen_outside_its_staging: 0`) |
| `the_cutscene_reached_exactly_the_controllers_it_named` | pass |
| `fixture_restored` | `true` |

### What the harness proves, and what it does not

**It proves the audience bound held.** On 203 frames a local controller of the authority world was
frozen, and on every one of them that controller was named by the sequence staged in its own world.
Zero unnamed frozen frames is the property claim 7 is about.

**It does not exercise the release, and no run of it in this configuration can.** The fixture authors
a faction audience with no explicit controllers, so the vendor resolves it by fallback; on a listen
server that fallback lands on the authority world's only local controller - the host. The receipt says
so directly: `local_controllers_by_world` is 1 for `UEDPIE_0`, and the sequence's owners and the
judgeable local controller are the same one name. With the audience covering the world's only local
controller, `ArmAudienceReconcile` collects nothing and returns before subscribing. This is a
derivation from recorded facts rather than an observation - the reconcile's own log line is what would
have made it an observation, and the PIE logs were overwritten by later sessions - so it is stated as
a derivation.

The consequence is the part that matters: **because the engine's sweep was already correct in this
configuration, these runs do not distinguish pre-fix from post-fix behaviour.** The run at 19:17
(`Saved/Verification/FloorStagingPIE/ListenServer_Claim7_2026-09-24.json`, `passed: false`) was
produced *with the fix already built*; its single failure was the harness's own strict clause, not the
feature. The pre/post-fix distinction for claim 7 rests entirely on the headless tests and the red
observation above, and the plan's requirement to *"verify each controller individually, including the
excluded local host"* was not satisfiable with this fixture: the excluded local host never existed in
it.

What a fixture would need is a **second local controller in the authority world** (split screen) or an
**explicit-controller audience aimed at a remote client** - either makes the host a controller the
sequence suppresses and does not name, which is the case the reconcile exists for. Neither is authored
here, and writing one is its own piece of work rather than a tweak to this probe.

**What the harness does exercise** is that the guard is inert where it must be: the audience covered
every local controller and nothing was released. That is
`AudienceReconcileStaysInertWhenItCoversEveryLocalController`'s behaviour confirmed in live gameplay.

### The two shapes, and why the receipt records which one it was

The same probe is run in two configurations, and they differ in exactly the place this feature lives.
Under `PIE_Client` the authority world holds no local controller at all, so nothing there can be
wrongly frozen and the work is done by the client-world checks; under `PIE_ListenServer` the host
plays in the authority world, which is the only controller the engine's audience-blind sweep can
wrongly freeze.

A green bound in the first shape is silent about the second, so
`authority_audience_was_exercised` is **recorded, not checked** (false in the client shape by
construction, true here) and `pie_config` records the launch settings that were asked for beside the
worlds that actually came up. The client-shape receipt was produced before the listen-server runs and
was **superseded and not regenerated**; its values were read as `passed: true`, 33 checks,
`staged_client_worlds == frozen_client_worlds == ["/Game/UEDPIE_1_HopDistrictTest"]` across two client
worlds, and `authority_audience_was_exercised: false` with `judgeable_local_controllers: []`. Those
are cited as read from a receipt that no longer exists on disk, not as a file.

`the_cutscene_reached_exactly_the_controllers_it_named` was reformulated in this batch from
`len(staged_client_worlds) == 1` to `<= 1`, and the receipts show why the relaxation is a no-op in the
client shape (`len` was 1 there) and the only satisfiable form here (`len` is 0, because the audience
is the host, so no client world stages anything). The property the clause protects - one cutscene must
not reach several clients - is what `<= 1` still states, and two staged client worlds is exactly the
duplication it catches. What moved is carried by
`every_frozen_local_controller_was_named_by_the_sequence_that_froze_it`, which reads the authority
world's own controllers and therefore sees the host that a client-world clause cannot.

### Two instrument defects found and fixed in the probe

**A dead field.** `pie_config` carried a `net_mode_by_world` reading that could only ever say
`"unavailable"`: `unreal.GameplayStatics` has no `get_net_mode` in this build and `UWorld` exposes
none either. A receipt claiming a net mode it never read is worse than no field, so it was replaced by
`launch_settings()`, which reads the ini the editor wrote - documented as *the request, not the
result* - with the live world facts (which worlds exist, which hold a local controller, which has a
game mode) recorded beside it.

**A boot-time race.** `record_worlds()` runs the moment the first authority world resolves, and PIE
holds a world as `/Temp/Untitled_N` while its map is still loading and renames it `UEDPIE_N`
afterwards - so one run's inventory listed a placeholder package as a client world. The checks were
never affected (`client_worlds()` recomputes live and never reads the stored list), so this was a
receipt defect only, fixed by re-recording the inventory in `finish()` and preserving the first reading
as `pie_worlds_at_boot`. The cited receipt shows both: `pie_worlds_at_boot` is
`UEDPIE_0` + `/Temp/Untitled_5`, and `pie_worlds` is the three real worlds.

**A known limitation of the audience classification.** `state['client_worlds']` is the last boot-frame
reading, so a client world that arrives later - here the late joiner, `UEDPIE_2` - never enters the set
the audience checks classify against. It is harmless in this shape, where nothing stages in a client
world, but it is a limitation of the instrument and not of the feature, and it is recorded rather than
left to be discovered.

## Honest limitations

**The live release is unexercised**, for the reason above. The headless
`CinematicModeIsReleasedOutsideTheAudience` proves the reconcile's logic by reproducing the engine's
private sweep and driving the handler through the test seam; the seam's own docstring states that
whether the world's delegate is what invokes the handler in play, and the registration-order lag that
follows from it, are *not* shown by it. Neither gap is closed anywhere in this batch.

**A controller suppressed on a bystander after arming is released.** The exclusion list is a snapshot
taken at arming: a controller that was already in cinematic mode is left alone, but one that something
else freezes *after* arming and during the sequence is in `UncoveredControllers` and will be released.
The window is small and no producer in this plugin opens it, but it is a real edge rather than a
theoretical one - the honest bound is "Territory releases a suppression it did not make, if that
suppression began while its cutscene was already playing".

**The launch used `-unattended -nosplash`**, which every run of this harness uses; it is not specific
to this batch and is recorded only so that a reader does not take the runs for interactive ones.

### Instrument findings this batch surfaced

Two harness problems were found by running the listen-server shape and are now controlled by the
fixture rather than left to chance. Both are documented in
`Scripts/Territory/arm_floor_staging_pie_fixture.py`, not repeated here: the **authored floors** (the
Blacksmith asset ships two floors of its own and puts one - with its only Narrative event, a currency
reward - over the two posts this level cannot deploy, so the fixture takes the array over and the probe
restores it from exported text, which round-trips an instanced event by object path), and the
**priority residue** (each post's Priority is copied from its authored row, so an unpinned priority
decides the deploy order - with the asset as it stands the run placed the undeployable post third and
left another unstaffed). The fixture pins Priority per floor so the floor split and the deploy order
necessarily agree, and `fixture_restored: true` in the cited receipt is the evidence that the authored
Definition was returned to its own values.

## Files changed

| File | Change |
|---|---|
| `Private/Cinematics/TerritoryCutsceneTeardown.cpp` | `ArmAudienceReconcile` / `HandleSequenceTick` / `DisarmAudienceReconcile`; the tick subscription; the reverse-registration-order note |
| `Public/Cinematics/TerritoryCutsceneTeardown.h` | the audience bound in the class comment; `UncoveredControllers`, `ReconciledWorld`, `SequenceTickHandle`; the seam and the three new private members |
| `Public/Tales/TerritoryStoryEvents.h` | the "must never suppress or release input itself" note now names the component that owns the bound |
| `Source/TerritoryFrameworkEditor/Private/Tests/TerritoryCinematicAudienceProbe.h` | new: `ATerritoryCinematicRecordingController`, a local controller that records every `SetCinematicMode` transition it is put through |
| `Source/TerritoryFrameworkEditor/Private/Tests/TerritoryCutsceneEventTests.cpp` | the seam's `SequenceTick`/`UncoveredCount`/`HasTickSubscription`; seven new tests |

The recording controller exists because `bCinematicMode` is a single bool: "was suppressed and then
released" and "was never touched" are the same reading taken afterwards. Only a recording of the calls
tells them apart, which is why every release assertion is made against the call history and not the
final flag. It overrides the five-argument `SetCinematicMode` that the engine's own
`EnableCinematicMode` calls and still runs the real path underneath, so the gate under test sees the
same `bCinematicMode` production would.

## Migration

None. The sequence actor is spawned `RF_Transient`, the teardown component is server-only and created
with `RF_Transient`, and nothing in this batch is saved, replicated or durable: `SetIsReplicatedByDefault(false)`
is unchanged and the reconcile touches only local-controller bookkeeping on the authority world. No
authored content changes meaning, and no Blueprint contract changes - the new members are private.

The one behaviour a designer can observe is that a cutscene staged for a specific audience no longer
suppresses a *different* local player on the same machine, which is the fix rather than a migration.
