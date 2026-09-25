# A cinematic suppression is only Territory's to undo while it is still owed

Status: **fix applied and verified red and green.** Every filter run passes under both allocators, and
both halves of the fix have been observed failing against a deliberate break, each isolating one test.

Re-audit findings **P2 #2** and **P2 #3**, raised against the audience reconcile introduced by
`CUTSCENE_AUDIENCE_2026-09-24.md`. They are the same defect seen from two ends: the release had no way
to tell a suppression *this* sequence made from one it did not.

- **#2** - arm-time collection excluded a controller already in cinematic mode, but the per-tick
  release tested only `APlayerController::bCinematicMode`, **a bare bool with no owner identity**. A
  controller in the pool that a *later* sequence legitimately froze was released by this component's
  next tick.
- **#3** - the release was gated on `Player->IsPlaying()`, so a pause landing between the engine's
  sweep and the corrective tick left the excluded controller suppressed for the whole pause.

## The mechanism

`ULevelSequencePlayer::EnableCinematicMode` (`LevelSequencePlayer.cpp:381-400`) has no audience input.
It walks **every local controller in its world** and calls
`SetCinematicMode(bEnable, bHidePlayer, bHideHud, bDisableMovementInput, bDisableLookAtInput)` on each.
It never consults `OwnerControllers`, which the vendor applies only to net relevancy. That is the
defect the audience reconcile was written for, and it is unchanged.

What the reconcile could not previously do was **attribute a release**. Two facts from the engine
decide the shape of the fix, both read from source rather than assumed:

1. `APlayerController`'s own setter has no change guard - it assigns `bCinematicMode` and calls the
   reliable `ClientSetCinematicMode` RPC unconditionally. So "is it cinematic" is the only thing a
   release can read, and it says nothing about who set it.
2. The engine does not sweep once per player. It sweeps once per **play-start**:
   `StartTimeControllerAndBroadcastPlayState` (`:1181`) sets `bPendingOnStartedPlaying = true` and then
   broadcasts `OnPlay` (`:1225-1229`) or `OnPlayReverse` (`:1218`); `PlayInternal` (`:310`) is guarded
   by `!IsPlaying()`, so **a resume from Pause passes the guard and re-sweeps**; and
   `UpdateTimeCursorPosition_Internal` (`:1232`) consumes the flag and fires `OnStartedPlaying()` once
   (`:1250-1254`).

Together those give the release a clock and a debt, without needing an owner field the engine does not
have.

> **Superseded by this record:**
> `CUTSCENE_AUDIENCE_2026-09-24.md` line 186 records a red leg named
> `AudienceReconcileDoesNotReleaseAPausedSequence`, and line 312 states that a controller another
> system freezes "after arming and during the sequence is in `UncoveredControllers` and will be
> released". Both describe the behaviour these findings were raised against. The test is renamed and
> inverted; the second sentence is now false and the pool is only half the rule.

## What changed

The release is bounded **twice**. Both bounds are needed because `bCinematicMode` reports that a
controller is suppressed and never reports by whom.

**1. The pool, decided before any suppression exists (unchanged in kind, re-documented).**
`UncoveredControllers` is now named for what it is: the controllers this component may *ever* release.
Membership is fixed at arming, and a controller already in cinematic mode is excluded - something else
put it there deliberately.

**2. The debt, per play-start (new).** `OwedReleases` is filled from the pool on every play-start and
each controller is dropped the moment its release lands:

```cpp
void UTerritoryCutsceneTeardownComponent::HandleSequenceStarted()
{
	if (bTeardownArmed) return;
	OwedReleases = UncoveredControllers;
}
```

`HandleSequenceStarted` is bound to the player's own `OnPlay` **and** `OnPlayReverse`, through the
existing `friend class FTFTerritoryCutsceneTeardownTestAccess;` seam for the tests. `OnPlay` is the
last thing `StartTimeControllerAndBroadcastPlayState` does before re-arming the suppression it is about
to apply, so the debt is incurred on the exact frame of the sweep rather than a frame later when a poll
of `IsPlaying()` would notice it. The pair is bound rather than `OnPlay` alone because
`bReversePlayback` decides which one `PlayInternal` broadcasts and this component does not own the play
call - it is handed a player by `ScheduleAfterSequence`.

`ArmAudienceReconcile` fills the debt at arming only for a player that is **already** playing: `Play()`
has happened and the sweep it triggered has not, because `bPendingOnStartedPlaying` is consumed by the
player's next position update rather than by `Play()` itself. That case's debt is already incurred and
would otherwise wait on a binding that will never fire again. An unplayed arm incurs nothing.

**3. The playback-status gate is gone from the release.** The engine releases cinematic mode only from
`OnStopped`, so a bystander still owed when a pause lands between the sweep and the handler would stay
frozen for the whole pause - for a cutscene it is not in. Removing the gate cannot release the audience,
because the audience is not in the pool.

The `bCinematicMode` test inside the loop stays, and is now load-bearing for a second reason: it is the
test for "this one has not been answered yet". It is what makes the debt a **state** rather than a
one-shot, so a release is not spent on the frame between `Play()` and its sweep - a frame that is every
frame in production, because the reconcile runs *before* the playback tick
(`TMulticastDelegateBase::Broadcast` invokes in reverse registration order, `MulticastDelegateBase.h:298-300`)
and the sweep is consumed by a position update inside that same broadcast.

`DisarmAudienceReconcile` drops both start bindings **before** the world tick, deliberately: the release
is one-way, so a start arriving after the reconcile has handed its work back must not re-open a debt
nothing will answer.

No new class, no new subsystem, no vendor edit, and no per-tick actor enumeration. The two signals the
engine already emits were sufficient.

## The red legs

Two breaks, run separately so each new assertion set is attributable to exactly one half of the fix.
Both on the default allocator: neither break introduces a lifetime hazard, so the failure each must
produce is an assertion failure, and the poisoning allocator has no additional purchase on it. All
`-Stomp` legs were run against the corrected binary.

**Red leg A - the playback-status gate restored** (`#3`):

```cpp
if (!Player->IsPlaying()) return; // RED LEG A: the released playback-status gate, restored on purpose
```

`exit=255 success=25 fail=1` - **one** failing test,
`AudienceReconcileReleasesABystanderOfAPausedSequence`, with its whole causal chain:

```text
Error: Expected 'A paused sequence still releases the controller it is not for' to be 1, but it was 0.
Error: Expected 'so the bystander gets control back while the shot is still on screen' to be false.
Error: Expected 'So the resumed sequence releases it again' to be 2, but it was 1.
Error: Expected 'and a frame with nothing owed releases nothing' to be 4, but it was 3.
```

The last two are downstream of the same missing release rather than independent noise: the pause leg
never released, so the resume leg produced the first `NumberLeaving()` of the run and the call count
ends one short.

**Red leg B - the debt kept instead of paid** (`#2`):

```cpp
// RED LEG B: the debt kept instead of paid - the released behaviour, restored on purpose.
// OwedReleases.RemoveAt(Index);
```

`exit=255 success=25 fail=1` - one failing test,
`AudienceReconcileLeavesALaterSuppressionAlone`, failing on exactly the three assertions the finding is
about:

```text
Error: Expected 'The later suppression is left exactly as it was found' to be 3, but it was 4.
Error: Expected 'so the bystander is still in cinematic mode' to be true.
Error: Expected 'and the reconcile has nothing left to pay, so it is owed nothing' to be 0, but it was 1.
```

The full red log is preserved at `Saved/Tests/_redleg_B8_later_suppression.default.log`. Both breaks
were reverted, `grep -rn "RED LEG" Source/` verified empty, the binary rebuilt, and every filter re-run
green.

## Verification

Assertion messages are cited rather than reported line numbers, for the reason
`FLOOR_SNAPSHOT_LIFETIME_2026-09-24.md` gives: the framework's reported line for a `TestEqual` has been
observed off by up to +6.

Every filter was run under both allocators, one per invocation.

| Filter | Default | `-Stomp` | Red leg A | Red leg B |
|---|---|---|---|---|
| `TerritoryFramework.Presentation.Cutscenes` (26) | 26/26 | 26/26 | **25/26, fail=1** | **25/26, fail=1** |
| `TerritoryFramework.Tales` (50) | 50/50 | 50/50 | not run | not run |
| `TerritoryFramework.Dialogue` (3) | 3/3 | 3/3 | not run | not run |
| `TerritoryFramework.WorldPartition` (6) | 6/6 | 6/6 | not run | not run |
| `TerritoryFramework.Capture` (12) | 12/12 | not run | not run | not run |

`Tales` is where `UTerritoryPlayCutsceneEvent` lives and is the caller that arms the teardown;
`WorldPartition` is where the floor-cleared cutscene path arms it through the same static; `Dialogue`
covers `UTerritoryDialogueShot`, which forces three of the four cinematic flags and shares this
component's subject matter as a recorded residual (see the receipt test below). `Capture` is a cheap
integration check that cutscenes fired from gameplay transitions still behave.

The cutscene filter went from 24 to 26 tests: one existing test rewritten under a new name, two new
tests, and no test deleted.

## Test map

| Test | Covers |
|---|---|
| `...Cutscenes.AudienceReconcileReleasesABystanderOfAPausedSequence` | **Renamed and inverted** from `AudienceReconcileDoesNotReleaseAPausedSequence`. A pause lands, the engine's sweep is reproduced, and the bystander **is** released while the audience stays cinematic - then the sequence is genuinely resumed (`Player->Play()`, asserting `IsPlaying()`, so the real `OnPlay` binding is what re-opens the debt) and released again. Red under leg A. |
| `...Cutscenes.AudienceReconcileLeavesALaterSuppressionAlone` | **New.** The bystander is released, then frozen again by a later call with the engine's own arguments. Two further ticks leave it exactly as found. Red under leg B - the #2 finding in one assertion. |
| `...Cutscenes.AudienceReconcileHoldsAReleaseUntilItsSweepLands` | **New.** A guard on the design rather than a red leg: a play-start owes a release before anything is suppressed, a frame where the sweep has not landed releases nobody and the debt survives it, and a sequence armed but never played holds no debt and releases nothing another system froze. This is the leg that would fail against a one-shot implementation of the same fix. |
| `...Cutscenes.AudienceReconcileStaysInertWhenItCoversEveryLocalController` | Extended: no play-start binding is taken either. |
| `...Cutscenes.AudienceReconcileHoldsNothingWhenThereIsNothingToBound` | Extended: same, on both inert legs (empty audience, non-suppressing settings). |
| `...Cutscenes.AudienceReconcileLeavesAControllerAnotherSystemAlreadyFroze` | Extended: same, for the arm-time bound. |

Two assertions in the rewritten test changed shape deliberately. `Watcher->Num()` was replaced by
`Watcher->NumberEntering() == 2` and `Watcher->NumberLeaving() == 0`, because the engine's reproduced
sweep touches the audience on every play-start - twice in that test - and asserting a total count
described the fixture rather than the claim. The claim is that the reconcile never *releases* the
audience, and it is now stated that way.

Test seams added to `FTFTerritoryCutsceneTeardownTestAccess`: `SequenceStarted` (drives one play-start
directly, for the leg that must open a debt without the engine also applying the suppression that the
same call triggers) and `OwedCount` / `HasPlayStartBinding`. `HasPlayStartBinding` uses
`TMulticastScriptDelegate::Contains(const UObject*, FName)` with `GET_FUNCTION_NAME_CHECKED`, matching
the existing `HasTickSubscription`.

## Migration and release notes

**No serialized field, no replicated field, no API change.** `OwedReleases` is a transient member of a
transient component, and the new binding is on a `RF_Transient` sequence player. The component keeps
`SetIsReplicatedByDefault(false)` and remains authority-only, so nothing here reaches a client except
through the `ClientSetCinematicMode` RPC that `APlayerController` already sends.

**The change is strictly narrower on two axes and strictly wider on one.** Narrower: a suppression
arriving after this sequence's own has been answered is no longer released. Wider: a bystander owed a
release is now released even while the sequence is paused. The second is the point of #3 - a paused
cutscene has not stopped, and holding a non-audience controller frozen for the whole of someone else's
pause is the harm.

## Known limitations

- **A controller frozen by something that is not another Territory sequence inside the sweep window is
  still released.** The window is between a play-start and the frame the engine's sweep is consumed -
  one frame, or a whole pause if the pause lands inside it. There is no owner field to test, and the
  engine's `SetCinematicMode` has no change guard, so Territory's own sweep is the last writer in that
  window and releasing is the correct attribution there. This is the residual the design accepts rather
  than an untested case: closing it would need the per-world staged-sequence registry the batch plan
  rejected as disproportionate under AGENTS.md §12.
- **`UTerritoryDialogueShot` is still unbounded.** It forces three of the four cinematic flags
  (`TerritoryDialogueShot.cpp:51,59,60`), so it remains the unconditional instance of the original
  audience defect. It is recorded by
  `...Cutscenes.DialogueShotForcedFlagsAreRecordedNotBounded` as a receipt, not as coverage. A shot's
  audience is the conversation's and the dialogue system stops the sequence itself, so bounding it is a
  dialogue-design question with its own test surface. **Cutscenes are bounded; Territory is not.**
- **The live listen-server harness was not re-run.** The audience bound's engine-side premise - that
  the sweep really does reach every local controller - was proven in live gameplay in
  `CUTSCENE_AUDIENCE_2026-09-24.md` and is unchanged by this batch. What this batch adds is what the
  reconcile does with each controller it finds, which is reachable headlessly: the engine's own call is
  reproduced verbatim by `SuppressCinematics` with its own argument order, and the play-start under test
  is the engine's real `OnPlay` (`Player->Play()` reaches `StartTimeControllerAndBroadcastPlayState`
  synchronously, because `PlayInternal`'s `NeedsQueueLatentAction` is `IsEvaluating()` - false outside an
  evaluation callback). The one thing the fixture cannot do is produce the position update that consumes
  `bPendingOnStartedPlaying`, which is why the tests drive the sweep explicitly and why the debt is
  asserted against a sweep rather than against the frame the engine would have chosen.
