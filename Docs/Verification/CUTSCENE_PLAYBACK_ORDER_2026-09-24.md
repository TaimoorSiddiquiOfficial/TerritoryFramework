# Territory starts the cutscene, after teardown is armed

Status: **fix applied and verified red and green.** Three deliberate breaks were observed, each against a
different assertion, and the third one is the empirical evidence for the one design deviation this
batch had to take from the plan. `TerritoryFramework.Presentation.Cutscenes` passes under both
allocators (17), and the full suite passes under both (422). No verification leg is blocked.

Adversarial-audit finding, claim 8: an immediate cancellation can precede teardown binding, so the
sequence actor leaks.

## The mechanism

`ANarrativeLevelSequenceActor::CreateNarrativeLevelSequencePlayer` spawns the actor with
`SpawnParams.bDeferConstruction = true`, and the comment on that line says why: *"Defer construction
for autoplay so that `BeginPlay()` is called"*. So the vendor's own `BeginPlay` runs **inside the
factory**:

```cpp
	LevelSequenceActor->InitializePlayer();
	OutActor = LevelSequenceActor;
	LevelSequenceActor->FinishSpawning(DefaultTransform);   // -> BeginPlay -> autoplay
```

and that `BeginPlay` is where the autoplay lives:

```cpp
	if (PlaybackSettings.bAutoPlay)
	{
		GetSequencePlayer()->Play();
	}
```

`PlayInternal` reaches `StopInternal` synchronously when a sequence ends the moment it starts - a
`StopTags` tag, a zero-length sequence, a producer cancelling the shot as it begins - and
`StopInternal` broadcasts `OnStop`, then `OnFinished`. Territory armed its teardown *after* the factory
returned, so those two signals were delivered to a player with no bindings, and the actor nobody else
owns survived for the rest of the session.

The tests could not see any of this. The shared `PlayCutscene` helper called `Player->Play()` **after**
`ExecuteEvent` returned, because Auto Play is a setting the actor consumes from a game loop an isolated
test world does not have - and that hand-play is precisely what put arming before playback in every
test, hiding the ordering the production code got wrong.

## The fix

**(a) Territory owns the play moment** - `TerritoryStoryEvents.cpp`:

```cpp
	Settings.bAutoPlay = false;
	Settings.bPauseAtEnd = false;
	...
	UTerritoryCutsceneTeardownComponent* Teardown =
		UTerritoryCutsceneTeardownComponent::ScheduleAfterSequence(
			SequenceActor, TeardownGraceSeconds);

	Player->Play();
	if (Teardown)
	{
		Teardown->ReconcileAfterPlaybackRequest();
	}
```

`bAutoPlay = false` is what closes the window, and the ordering is the whole fix: force the settings,
spawn, arm, play. It is safe for the audience because the client does not autoplay at all - it follows
the server's status through the replicated sequence player (`PlaybackSettings` and `NetSyncProps` both
replicate, and `PostNetReceive` starts local playback from the server's `LastKnownStatus`). Forcing
Auto Play off removes one start path on the authority and removes nothing on a client.

**(b) The class, not just the autoplay path** - `UTerritoryCutsceneTeardownComponent::
ReconcileAfterPlaybackRequest()`, called by the event immediately after `Player->Play()`. It returns
without acting when the player is playing or paused (the binding will deliver that ending), and
otherwise hands the stop to the private `HandleSequenceStopped()`, so the authored grace, the paused
policy and the arming idempotence all stay in one place.

The shared test helper lost its hand-play, which is part of the fix rather than tidying: it is the seam
that masked claim 8.

### The deviation from the plan, and why it is forced

The plan's (b) was *"an already-stopped branch in `TerritoryCutsceneTeardown.cpp:41` using the public
`GetPlaybackStatus()` before subscribing"*. **Both halves of that are wrong, and the compiler and the
engine agree.**

1. `GetPlaybackStatus()` is **protected** on `UMovieSceneSequencePlayer`. The first build of this batch
   failed with `error C2248: cannot access protected member`. It is not "the public `GetPlaybackStatus()`"
   and no caller can read it at all; `IsPlaying()` and `IsPaused()` are the whole of what a caller has.

2. Those two predicates cannot answer the question (b) wants to ask. `ULevelSequencePlayer` does not
   initialise `Status` in its constructor, so a player that has never played and a player that played
   and stopped answer **identically**: not playing, not paused. `StopInternal` is also a no-op unless
   the player is playing or paused. So "it is not playing" cannot be read as "this sequence has already
   ended".

Placed in `ScheduleAfterSequence` as the plan specified, (b) would therefore fire for **every** armed
cutscene, because under (a) arming always happens before playback starts. That is not a hypothetical:
red leg **R3** implements exactly the plan's placement, and the observed result is `Expected 'The
cutscene starts' to be not null` - the actor is destroyed at arming, and the cutscene never plays.

So the reconcile moved to the caller, which is the only party that knows it has just asked this
specific player to play. `ReconcileAfterPlaybackRequest()` is public, `HandleSequenceStopped()` stays
private, and the ordering knowledge that makes the call sound lives in the one line that calls it.
`...Cutscenes.ArmingBeforePlaybackDoesNotTearDown` is the test that pins the deviation shut: it fails
if a status check is ever moved back into arming.

## Verification

| | Result |
|---|---|
| `TerritoryFramework.Presentation.Cutscenes` - default | 17 passed, 0 failed |
| `TerritoryFramework.Presentation.Cutscenes` - stomp | 17 passed, 0 failed |
| `TerritoryFramework` - default | 422 passed, 0 failed |
| `TerritoryFramework` - stomp | 422 passed, 0 failed |

422 is 417 plus the five new tests. All five are new; one pre-existing assertion was inverted (below).

### The red legs

Three deliberate breaks, run separately so each assertion's sensitivity is attributable:

| Break | Observed |
|---|---|
| R1 `TerritoryStoryEvents.cpp`: restore autoplay, drop the explicit play and the reconcile | 6 failures - the settings, the playback, and every teardown test |
| R2 `TerritoryCutsceneTeardown.cpp`: empty the reconcile body | 2 failures - exactly the two reconcile tests |
| R3 `TerritoryCutsceneTeardown.cpp`: the plan's literal status check inside arming | 6 failures - including `The cutscene starts` null |

Verbatim, assertion messages exactly as reported:

```text
R1: Expected 'Auto Play is forced off, because the engine starting the sequence is the window this
      closes' to be false.
    Expected 'Auto Play is forced off, so the factory cannot start the cutscene itself' to be false.
    Expected 'The event started the sequence itself, once teardown was armed' to be true.
    Expected 'The cutscene is playing even though Auto Play is off' to be true.
    Expected 'The cutscene is playing before it is stopped' to be true.
    Expected 'The cutscene is playing before it is skipped' to be true.
    Expected 'Stopping the cutscene arms the teardown with the authored grace' to be true.
    Expected 'Skipping to the end still arms the teardown' to be true.
    Expected 'Zero grace destroys the actor as soon as the sequence stops' to be true.

R2: Expected 'Reconciling destroys a cutscene whose sequence has already ended' to be true.
    Expected 'Reconciling an already-ended sequence applies the authored grace, not an immediate
      destroy' to be true.
    Expected 'No cutscene actor is left behind' to be 0.

R3: Expected 'Arming arms no lifespan of its own' to be 0.000000,
    Expected 'Arming alone leaves the already-ended cutscene alive' to be true.
    Expected 'A cutscene that is playing has had no lifespan armed' to be 0.000000,
    Expected 'The cutscene starts' to be not null.
```

R1's teardown failures are not collateral damage, they are the evidence for the helper change: with
Auto Play off and the helper's hand-play removed, nothing plays at all, so `Stop()` is a silent no-op
(`StopInternal` acts only on a playing or paused player) and no teardown can arm. That is what makes the
event's explicit `Play()` load-bearing rather than cosmetic.

R2 left all 15 other tests green, which shows the reconcile is independently load-bearing: nothing else
in the feature was covering for it.

### The inverted assertion, and the churn the plan predicted

`...Cutscenes.PauseAtEndIsForcedOff` asserted *"Auto Play is forced on, because an event that never
plays is not a cutscene"*. That is the old contract exactly: the event relied on the vendor's `BeginPlay`
to start playback, which is the window claim 8 is about. It now asserts Auto Play is forced **off**, and
the contract it was really about is asserted directly instead - the same test now also asserts
`Player->IsPlaying()`, so the inversion cannot degenerate into "the settings are safe because nothing
ever plays".

## Honest limitations

**The begun-world ordering is proven by construction, not by a runtime trace.** Vendor autoplay fires
only inside the factory's `FinishSpawning` in a *begun* world, and there is no engine hook between the
factory's `InitializePlayer()` and that `BeginPlay` to interpose a probe on:
`OnActorPreSpawnInitialization` fires before the player object exists, and `OnActorSpawned` is broadcast
from `OnActorFinishedSpawning` (which is itself gated behind `UE::Gameplay::CVars::bDelayOnActorSpawned
UntilFinishedSpawning`), i.e. after `BeginPlay`. This was checked in the engine source rather than
assumed. So the ordering is pinned by the value the vendor's `BeginPlay` branches on - asserted on the
actor the factory built - plus the absence of any other start path, and not by observing the window
close. A begun-world fixture for the cutscene tests would be the way to observe it directly and is not
written here.

**The reconcile covers endings that happen before arming, not endings that happen between arming and
`Play()`.** Nothing can: `Play()` starts synchronously for a fresh player (`NeedsQueueLatentAction()` is
false unless playback is started from inside that player's own evaluation), so there is no frame in
between. That is a property of the fix's ordering, not a gap in it.

**`UTerritoryDialogueShot` still forces three cinematic flags**, and that is the unconditional instance
of the audience question claim 7 owns. It is unchanged here, and it is recorded as a receipt in B5
rather than as coverage.

## Files changed

| File | Change |
|---|---|
| `Private/Tales/TerritoryStoryEvents.cpp` | Auto Play forced off; teardown armed, then `Player->Play()`, then the reconcile |
| `Public/Cinematics/TerritoryCutsceneTeardown.h` | the new public `ReconcileAfterPlaybackRequest()`, with the "why arming cannot decide this" note |
| `Private/Cinematics/TerritoryCutsceneTeardown.cpp` | `ReconcileAfterPlaybackRequest()` implementation |
| `Public/Tales/TerritoryStoryEvents.h` | class comment and the `PlaybackSettings` tooltip now name both overridden fields |
| `Source/TerritoryFrameworkEditor/Private/Tests/TerritoryCutsceneEventTests.cpp` | the helper's hand-play removed; the autoplay assertion inverted; five new tests |

## Migration

None to content. The behaviour change a designer can observe is that an authored **Auto Play** on a
Territory cutscene is now ignored - the cutscene plays either way - so nothing that "worked" stops
working. It belongs in the release note alongside the tooltip, which now says so in the editor.

No durable state is involved: the sequence actor is `RF_Transient`, the teardown component is
server-only and unsaved, and the reconcile changes nothing that replicates. The client path is
untouched, because a client never autoplayed and still follows the server's replicated status.
