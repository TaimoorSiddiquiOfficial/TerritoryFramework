# A floor objective reads a guard snapshot that is still alive

Status: **fix applied, linked, and verified red and green.** The defect reproduced under the stomp
allocator before the fix and does not after it; the suite passes under both allocators; and the new
test has been observed failing against a deliberately broken floor lookup as well as passing
against the fix.

Adversarial-audit finding, claim 1: `UTerritoryStateTask` took a pointer into a garrison snapshot
that `ATerritoryVolume::GetGarrisonSnapshot()` had already destroyed.

## The mechanism

`TerritoryVolume.h:553`:

```cpp
FTerritoryGarrisonSnapshot GetGarrisonSnapshot() const { return GarrisonSnapshot; }
```

The accessor returns the read model **by value**. The first version of the floor reader therefore
kept a floor entry from a temporary:

```cpp
// before
return Territory.GetGarrisonSnapshot().Floors.FindByPredicate(...);
// and at the call sites:
const FTerritoryFloorSnapshot* Floor = FindTargetFloor(*Territory);
... Floor->IsCleared() ...        // the temporary died at the end of the previous statement
```

The `FindByPredicate` call itself is safe - the temporary is still alive inside that full
expression. The pointer it returns is not: the temporary's `TArray<FTerritoryFloorSnapshot>` frees
its heap block when the return statement ends, and every caller dereferenced the returned pointer
afterwards.

## Reproduced before the fix

Same filter, two allocators, against the pre-fix binary
(`UnrealEditor-TerritoryFramework.dll`, 06:48:56, older than the fix):

| Allocator | Command | Result |
|---|---|---|
| Stomp | `-stompmalloc` on `TerritoryFramework.Tales.Tasks.FloorGarrisonObjectives` | `RequestExitWithStatus(1, 3)`, **zero** `Test Completed` lines |
| Default | the same filter, unchanged | `Test Completed. Result={Success} Name={FloorGarrisonObjectives}` |

The fault, verbatim from `Saved/_stomp_run.log`:

```text
LogWindows: Error: Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address 0x000003cc68e13ff4
LogWindows: Error: [Callstack] UnrealEditor-TerritoryFramework.dll!UTerritoryStateTask::IsObjectiveSatisfiedBy() [TerritoryStateTask.cpp:164]
LogWindows: Error: [Callstack] UnrealEditor-TerritoryFramework.dll!FTFTerritoryFloorObjectives::RunTest() [TerritoryFloorStagingTests.cpp:491]
```

The default allocator is the reason this shipped: it served the freed block intact, so the same
test reported green. **This defect class is invisible to an ordinary test run.** The stomp pass is
not extra severity - it is the only reading that can see it.

## The three call sites

All three had the identical pattern, in `Private/Tales/TerritoryStateTask.cpp`:

| Line | Function | Read |
|---|---|---|
| 164 | `IsObjectiveSatisfiedBy` | `Floor->IsCleared()`, `Floor->ActiveGuards` |
| 276 | `EvaluateCurrent` | `Floor->ActiveGuards` for `ReachDesiredGarrison` progress |
| 347 | `HandleGarrisonChanged` | `Floor->ActiveGuards` for `ReachDesiredGarrison` progress |

`HandleGarrisonChanged` already received its snapshot by value and did not need the copy that
`EvaluateCurrent` did; it called the same reader, so it dangled the same way.

## The fix

`FindTargetFloor` no longer reaches for the Territory. It takes the floor array it is given:

```cpp
const FTerritoryFloorSnapshot* UTerritoryStateTask::FindTargetFloor(
    const TArray<FTerritoryFloorSnapshot>& Floors) const
```

Each caller names a snapshot that outlives the read - a local copy at the two accessor sites, and
the delegate's own by-value parameter at the third. The helper is `private` with a single friend,
so this changes no reflected signature and no Blueprint asset.

## Coverage added

`TerritoryFramework.Guards.Floors.ProgressReadsItsOwnFloorSnapshot`
(`Private/Tests/TerritoryFloorStagingTests.cpp`).

It drives both paths that read the floor for progress - `BeginTask` into `EvaluateCurrent`, and the
real `OnGarrisonChanged` broadcast into `HandleGarrisonChanged` - and asserts the value that was
read, because the crash is not the only thing that can go wrong here:

| Leg | Ground floor reads | Upper floor reads |
|---|---|---|
| Activation (`BeginTask`) | 1 | **3** |
| Garrison delegate | 4 | **5** |

The two rows hold different counts and the Place-wide staffing target is a third number, so a
stale, a neighbouring or a Place-wide read cannot produce these values. Legs 1 and 2 asserting
non-zero is also what keeps the third leg honest: an undeclared floor must read 0, and 0 only means
"read an absent floor row" because the same reader demonstrably commits non-zero for a declared one.

The whole-Place (`-1`) reading is deliberately not re-asserted here:
`FTFTerritoryFloorObjectives` already pins it, and this fixture never commits ownership, so its
staffing target is zero and could not discriminate.

## Repeatable control

`Tools/Run-Tests.ps1 -Filter <name> [-Stomp]` runs one filter headless and refuses to run against a
DLL older than the sources, because a run after a failed build silently re-tests the previous code
and reports green. It resolves this project's engine through the `EngineAssociation` GUID, which is
a custom-install key rather than a launcher version.

## The compile, and why the game target could not run it

Verified: the four production edits and the new test **compile and link**, and the test is present
in the produced binary.

```text
Build.bat TDA Win64 Development        ->  Result: Succeeded (39.17s)
grep -ac ProgressReadsItsOwnFloorSnapshot Binaries/Win64/TDA.exe   ->  1
```

The game target was used because the running editor's Live Coding session blocks the editor target
(`Unable to build while Live Coding is active`), and a live-coding compile of these edits failed.
The changed code is not editor-gated, so this is a real compile of the change - but it is **not**
the editor link the automation run needs, and it is **not** a test.

That binary cannot run this suite, and the reason is in the suite itself rather than in the build:
all 251 tests in `Private/Tests` are declared `EAutomationTestFlags::EditorContext`, and a game
target supplies `GameContext`. Running the game binary headless confirms it and closes the route:

```text
TDA.exe -ExecCmds="Automation RunTests TerritoryFramework.Guards.Floors.\
ProgressReadsItsOwnFloorSnapshot; Quit" ... -TestExit="Automation Test Queue Empty"
  -> LogAutomationCommandLine: Error: No automation tests matched '...ProgressReadsItsOwnFloorSnapshot'
  -> RequestExitWithStatus(1, 255) - zero `Test Completed` lines

TDA.exe -ExecCmds="Automation List; Quit"
  -> 2123 tests available; not one of them is a TerritoryFramework test
```

The test's name string is nevertheless in `TDA.exe`, so the file was genuinely compiled and linked
for the game target. Present in the binary and reachable by the automation runner are two different
things here. **The editor link is therefore not a convenience - it is the only route that can run
this suite at all**, which is why no amount of game-target building closes this record.

## The runs

With the host editor closed, the editor target linked - `Build.bat TDAEditor Win64 Development`,
`Result: Succeeded`, 101.08 s, `UnrealEditor-TerritoryFramework.dll` written 10:50:06, newer than
every source - and `Tools/Run-Tests.ps1` then produced three readings:

| # | Filter | Allocator | Result |
|---|---|---|---|
| 1 | `TerritoryFramework.Tales.Tasks.FloorGarrisonObjectives` | `-stompmalloc` | `exit=0 success=1 fail=0 crash=False` - **PASSED** |
| 2 | `TerritoryFramework.Guards.Floors` | `-stompmalloc` | `exit=0 success=11 fail=0 crash=False` - **PASSED** |
| 3 | `TerritoryFramework.Guards.Floors` | default | `exit=0 success=11 fail=0 crash=False` - **PASSED** |

Reading 1 is this defect's red/green pair. It is the identical filter, on the identical allocator,
that exited 3 with zero results and an access violation at `IsObjectiveSatisfiedBy` against the
pre-fix DLL. Reading 2 is the whole suite containing the new test, under the allocator that poisons
freed memory. Reading 3 is the control: the default allocator's result is unchanged, so the repair
traded no regression for itself.

The new test's own line, from reading 3's log:

```text
Test Completed. Result={Success} Name={ProgressReadsItsOwnFloorSnapshot}
  Path={TerritoryFramework.Guards.Floors.ProgressReadsItsOwnFloorSnapshot}
```

## The red observation

A green test is not evidence until it has been seen failing, so the new test was made to fail on
purpose. `FindTargetFloor` was broken to return the first floor row regardless of `TargetFloor` -
the nearest plausible bug to the real one, since it drops the filter rather than the snapshot - the
editor target was relinked, and reading 3's filter was run again:

```text
Result={Fail} Name={ProgressReadsItsOwnFloorSnapshot}
Expected 'A floor task reads its own floor's guards' to be 3, but it was 1.
Expected 'A floor task follows its own floor's new guard count' to be 5, but it was 4.
Expected 'A floor the Place does not declare reads no guards' to be 0, but it was 4.
```

Three of the four value assertions fired and the two ground-floor assertions stayed green, which is
the correct shape: a lookup that always returns floor 0 hands a ground task the right answer, so
those legs *should* pass. The legs therefore discriminate the floor each is named after rather than
failing wholesale, and the absent-floor zero is a real reading rather than a zero that survives any
implementation. The break was then reverted, the editor target relinked, and both allocators re-run
as the final state: `TerritoryFramework.Guards.Floors` reports 11/11 under `-stompmalloc` and 11/11
under the default allocator, no failures and no faults in either.

The framework reported these three at lines 646, 653 and 666 while the `TestEqual` calls sit at 645,
652 and 660. The messages name the assertions unambiguously, so this record cites the messages and
not the reported lines.
