# One fight announces one floor once, and never walks a live read model

Status: **fix applied and verified red and green.** Three deliberate breaks were observed, each against
a different assertion, one of them a hard crash under the stomp allocator; the fix removes all three.
`TerritoryFramework.Guards.Floors` passes under both allocators (19), and the full suite passes under
both (417). No verification leg is blocked.

Adversarial-audit finding, claim 6: a nested defender death emits a floor-clear reward **twice**.
Re-verification found the same function carries a second, worse defect: the announcement pass walked
the **live** `GarrisonSnapshot.Floors` while a listener could reassign it from inside the pass.

## The mechanism

`OnDefenderDied` broadcasts `OnGuardKilled` (`TerritoryVolume.cpp:2524`) **before** the frame that is
handling the death concludes the fight (`TryCompleteDefenderDefeat` at `:2574`). Everything broadcast
there is arbitrary Blueprint, so a listener may synchronously unregister and kill the next defender -
and that second death concludes the fight **first**, from inside the frame of the first. Both frames
then observe the same real transition, because the floor did empty, so both announced it:

```cpp
OnFloorCleared.Broadcast(this, Floor.FloorIndex);
DispatchFloorClearedEvents(Floor.FloorIndex, TransitionContext);
```

The second half is the same defect class in the same function. The pass range-`for`ed the live member:

```cpp
for (const FTerritoryFloorSnapshot& Floor : GarrisonSnapshot.Floors)
```

and `RefreshGarrisonSnapshot` (`:2868`) reassigns that member (`:2878`) whenever a listener re-reads
the garrison - the most ordinary thing a listener can do - which frees the array the loop is walking.

The whole-Place beat has the identical hole through the same nesting: the nested frame reaches
`RegisteredDefenders.Num() == 0` first, so `OnAllGuardsDefeated()`, `OnAllGuardsDefeatedDelegate` and
the authored `AllDefendersDefeatedEvents` loop all ran twice per fight for one real defeat.

## The fix

Frame-local bookkeeping on `ATerritoryVolume` (`TerritoryVolume.h:972-974`):

```cpp
TSet<int32> FloorsAnnouncedInDefeatCascade;
bool bPlaceDefeatAnnouncedInDefeatCascade = false;
int32 DefeatCascadeDepth = 0;
```

Neither replicated nor saved. The set records only which beats **this cascade** has already broadcast;
it is a second authority over nothing, because "was this floor cleared" is still read from the
snapshot's one `IsCleared()` rule.

**Two scopes, and the placement is load-bearing.** `OnDefenderDied` (`:2425`) and
`TryCompleteDefenderDefeat` (`:2585`) each open a scope:

```cpp
TGuardValue<int32> CascadeDepth(DefeatCascadeDepth, DefeatCascadeDepth + 1);
ON_SCOPE_EXIT
{
    if (DefeatCascadeDepth == 1)
    {
        FloorsAnnouncedInDefeatCascade.Reset();
        bPlaceDefeatAnnouncedInDefeatCascade = false;
    }
};
```

The depth is guarded **first** and the bookkeeping cleared **second** on purpose. Destructors run in
reverse, so the clear runs while the depth still holds this frame's value, and `== 1` there means this
frame was the outermost one - the frame that owns the cascade. The discipline is the one
`bTransitionInProgress` already uses; the reason it matters here is that a set without a clear would
outlive its cascade and silently suppress a later, unrelated fight's beat.

A scope **only** in `TryCompleteDefenderDefeat` would not have been enough, which is why the plan
placed one in `OnDefenderDied`. The outer death's own conclusion and the nested one are **sibling**
calls when the nesting comes from `OnGuardKilled` (the outer death's conclusion has not started yet),
so the nested frame's exit would clear the set and the outer frame would announce the floor again. A
second scope is still required in `TryCompleteDefenderDefeat` for the deathless entry: a post that
abandons its queued reserves concludes a fight with no death to open a scope, and would otherwise
record announcements with nothing to clear them.

**The pass** (`:2662-2695`):

```cpp
const TArray<FTerritoryFloorSnapshot> ClearedCandidates = GarrisonSnapshot.Floors;
const bool bInDefeatCascade = DefeatCascadeDepth > 0;
```

The copy closes the re-entrant reassignment and keeps every announcement in one pass decided against
one consistent read. The set is written and read **only** while a cascade is open (`bInDefeatCascade`),
which makes the function self-guarding: a caller that opens no scope is not a conclusion this frame
owns, so it announces exactly as it did before. That gate is not cosmetic - the editor harness's
`ConcludeFight` calls the dispatch **directly**, and without the gate that call would leave a floor in
the set with nothing to reset it and suppress the next test's legitimate announcement.

**The whole-Place beat** (`:2612`) is guarded the same way. Guarding the **broadcast** is safe because
`OnAllGuardsDefeated_Implementation` is idempotent (`OwnershipData.DefenderCount = 0;
ForceNetUpdate();`); the delegate and the authored events are not, which is what the guard is for.

## Verification

| | Result |
|---|---|
| `TerritoryFramework.Guards.Floors` - default | 19 passed, 0 failed |
| `TerritoryFramework.Guards.Floors` - stomp | 19 passed, 0 failed |
| `TerritoryFramework` - default | 417 passed, 0 failed |
| `TerritoryFramework` - stomp | 417 passed, 0 failed |

417 is 414 plus the three new editor tests. The trace is the assertion that carries the weight: it
proves not only how many announcements happened but **which frame** made them. The nesting is driven
through the real death path (`KillDefender` -> `OnDefenderDied`) with a hook bound to the real
`OnGuardKilled` delegate, so the cascade is produced by production code rather than by a test calling
the dispatch twice.

### The red legs

Three deliberate single-line breaks, run separately so each assertion's sensitivity is attributable:

| Break | Observed |
|---|---|
| R1 `TerritoryVolume.cpp`: drop the once-per-cascade `Contains` skip | 2 failures, `Guards.Floors` filter - count and executions `2` |
| R2 `TerritoryVolume.cpp`: drop the whole-Place announcement guard | 2 failures, `Guards.Floors` filter - the whole-Place beat twice |
| R3 `TerritoryVolume.cpp`: walk the live member instead of a copy | **crash**, `Guards.Floors` filter under `-Stomp` |

Verbatim, assertion messages exactly as reported:

```text
R1: Expected 'One fight concludes one floor once' to be 1, but it was 2.
    Expected 'The announcement names the floor that emptied' to be 0, but it was -1.
    Expected 'The authored floor reward runs once for one fight' to be 1, but it was 2.
    Expected 'The innermost frame announces and the outer frame stays silent' to be
      "killed>cleared:0>all-defeated", but it was "killed>cleared:0>all-defeated>cleared:0".
    Expected 'Each floor is announced once for one cascade' to be 2, but it was 4.
    Expected 'Both floors are announced, in authored order' to be "cleared:0>cleared:2>all-defeated",
      but it was "cleared:0>cleared:0>cleared:2>all-defeated>cleared:2".
    Expected 'The floor the nested frame reached first is still announced' to be 2, but it was 0.

R2: Expected 'The whole-Place defeat is announced once for one fight' to be 1, but it was 2.
    Expected 'The authored all-defenders reward runs once for one fight' to be 1, but it was 2.
    Expected 'The innermost frame announces and the outer frame stays silent' to be
      "killed>cleared:0>all-defeated", but it was "killed>cleared:0>all-defeated>all-defeated".
    Expected 'The whole-Place defeat is announced once for one cascade' to be 1, but it was 2.

R3: LogWindows: Error: Fatal error!
    LogWindows: Error: Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address 0x00000240b5baefc0
    [Callstack] UnrealEditor-TerritoryFramework.dll!ATerritoryVolume::DispatchClearedFloors()
      [.../TerritoryVolume.cpp:2695]
    [Callstack] UnrealEditor-TerritoryFramework.dll!ATerritoryVolume::TryCompleteDefenderDefeat()
      [.../TerritoryVolume.cpp:2606]
    [Callstack] UnrealEditor-TerritoryFrameworkEditor.dll!FTFTerritoryFloorReentrantRefreshSurvives::RunTest()
      [.../TerritoryFloorEventTests.cpp:795]
```

R3 is the clearest evidence of the pair: the crash is inside the announcement pass and only under
`-Stomp`, which is the allocator that poisons freed memory. The default allocator kept serving the
freed bytes, so the same walk completed with the right answer **by luck** - and would have shipped.

The two guards are independently load-bearing: the floor assertions stayed green under R2, and the
whole-Place assertions stayed green under R1.

## Honest limitation

The cascade rule is **first-wins**: the frame that observes a floor's transition first announces it.
When a cascade spans two floors and the outer death empties a floor the outer frame has not reached
yet, the inner frame announces it - so that announcement carries the **inner** death's transition
context. First-wins is the deliberate choice: the alternative (deferring every announcement to the
outermost frame) has the mirror-image defect, because the outermost frame's pre-loss read is the
stalest one and the nested death is the one that actually emptied the floor. The two tests in this
batch pin both directions: which frame announces when the outer frame has not reached the floor
(`NestedDeathAnnouncesAFlooredFightOnce`), and that a rule which suppressed everything the cascade had
touched would lose the other floor's beat (`NestedConclusionKeepsAnotherFloorsBeat`).

## Files changed

| File | Change |
|---|---|
| `Public/Core/TerritoryVolume.h` | `FloorsAnnouncedInDefeatCascade`, `bPlaceDefeatAnnouncedInDefeatCascade`, `DefeatCascadeDepth` |
| `Private/Core/TerritoryVolume.cpp` | conclusion scopes on both entries; the value copy and the cascade gate in `DispatchClearedFloors`; the whole-Place guard |
| `Source/TerritoryFrameworkEditor/Private/Tests/TerritoryFloorEventProbe.h` | `GuardKilledCount`, `AllGuardsDefeatedCount`, the ordered `Trace`, and the two re-entrancy hooks |
| `Source/TerritoryFrameworkEditor/Private/Tests/TerritoryFloorEventTests.cpp` | `NestedDeathAnnouncesAFlooredFightOnce`, `NestedConclusionKeepsAnotherFloorsBeat`, `ClearedDispatchSurvivesReentrantGarrisonRefresh`; `CompleteDefeat` / `KillDefender` seams |

The plan named the members `FloorsAnnouncedInDeathCascade` / `DefenderDeathCascadeDepth`. They ship as
`...InDefeatCascade` / `DefeatCascadeDepth` because the scope also covers the **deathless**
abandoned-reserve conclusion, and a name that said "death" would describe only one of the two entries
that open a cascade.

## Migration

None. Frame-local, never replicated, never saved: the bookkeeping exists only while a conclusion scope
is open and is cleared by the frame that owns the cascade, so it cannot become durable state or a
second authority over floor ownership.

Gameplay-visible, and it belongs in the release note: a floor-clear reward and the authored
all-defenders-defeated events now run **once** per fight. Content that had come to rely on the second
execution - a reward granted twice, a quest step that counted both - will observe one.
