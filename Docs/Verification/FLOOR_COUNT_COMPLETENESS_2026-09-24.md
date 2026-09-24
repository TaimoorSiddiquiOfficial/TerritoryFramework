# A floor whose posts are unloaded reads unknown, not cleared

Status: **fix applied, linked, and verified red and green.** Three red legs were observed against
deliberate one-line breaks, each failing a different assertion; the fix removes all of them; the
suite passes under both allocators. **One verification leg could not be exercised** - the replicated
parity check in PIE is blocked by the arm fixture, recorded below rather than reported as covered.

Adversarial-audit finding, claim 3: an unloaded guard post keeps its authored floor capacity but
contributes zero counts, so a floor reads **cleared** while its defenders are still physically
standing in an unloaded cell.

## The mechanism

`BuildFloorSnapshots` accumulates a floor's capacity from the **Definition** and its counts from the
**live post actors**:

```cpp
// capacity: every authored post, loaded or not
FloorSlotIDs.FindOrAdd(Post.FloorIndex).Add(Post.StableGuardPostGUID);
// counts: every post standing this pass
FloorActive.FindOrAdd(FloorIndex) += SpawnPoint->GetActiveGuardCount();
```

Those two sources disagree exactly while a post's cell is streamed out, and they disagree in the
direction that reads as a victory: authored capacity survives in `MaximumGuards`, while
`ActiveGuards`, `ReserveGuards` and `PendingDeployments` all fall to zero. That is the same reading a
floor has once its last defender dies, and `IsCleared()` said so:

```cpp
return MaximumGuards > 0 && ActiveGuards == 0 && PendingDeployments == 0 && ReserveGuards == 0;
```

Two harms follow, and the second is the one that made this a story defect rather than a read-model
tidy-up.

1. **A spurious clear.** The per-floor objective satisfied, and the floor's authored
   `FloorClearedEvents` fired, while a defender was still standing in the unloaded cell.
2. **A permanently lost beat.** `TryCompleteDefenderDefeat` captures its pre-loss read
   (`FloorsBeforeLoss = GarrisonSnapshot.Floors`, `TerritoryVolume.cpp:2449`) and
   `DispatchClearedFloors` announces a floor only on an **observed transition**
   (`Previous && !Previous->IsCleared()`). With an unloaded post the committed snapshot *and* the
   pre-loss read both said cleared, so the compare found no transition - and because stream-out never
   calls `DispatchClearedFloors` at all, nothing announced it later either. A floor cleared while its
   cell was unloaded lost its clear beat for the rest of the campaign: the gate is what makes the
   first read that can actually see an empty floor a real transition.

## The fix

`FTerritoryFloorSnapshot::bCountsKnown` defaults to **false** and gates `IsCleared()`:

```cpp
return bCountsKnown && MaximumGuards > 0 && ActiveGuards == 0
    && PendingDeployments == 0 && ReserveGuards == 0;
```

The default is load-bearing, not stylistic: a `true` default fails open and silently reintroduces the
defect in the next producer that forgets to set it, so it fails closed instead. The gate is **inside
`IsCleared()`** rather than added as a parallel predicate because the struct's own contract is that
the objective, the floor-cleared event and any Blueprint widget all read this one rule.

`BuildFloorSnapshots` now keeps the authored set and the observed set apart - `FloorSlotIDs` is their
union and cannot answer "is anything missing":

- `AuthoredFloorSlots[Floor]` - every post the Definition declares, loaded or not.
- `LiveFloorPostIDs[Floor]` - the subset observed standing this pass.
- `Entry.bCountsKnown = bCountsPhysicalSlots && Live.Includes(Authored)`.

A declared floor that authors **no** post is complete with `MaximumGuards == 0`: "known to hold
nothing" is a different statement from "unknown", and `IsCleared()` already refuses it on its own
`MaximumGuards > 0` gate. `AggregateOnly` owns no physical counts, so it reports unknown rather than
claiming a completeness it cannot observe.

**Whole-Place extension, closed in the same batch.** The totals have the identical hole and the
identical failure, through `ETerritoryStateTaskObjective::AllDefendersDefeated` with no floor
selected: `GetDefenderCount() == 0` (actor-based) and `PendingDeployments == 0` (post-based) are both
zero for a fully streamed-out Place. `FTerritoryGarrisonSnapshot` carries the same flag, computed
over **every authored post** rather than over the declared floor rows - a post whose `FloorIndex` is
not a declared floor still belongs to the Territory's totals, so the Place flag must not depend on
`Floors` being non-empty. The floorless early return moved below the flag for that reason.

`bCountsKnown` is in both `operator==` overloads. Without that, `RefreshGarrisonSnapshot`
(which publishes and `ForceNetUpdate`s only when the snapshot compares **unequal**) would let a Place
become unknown without a single byte crossing the wire, and the client would keep reading cleared.

Two `BlueprintPure` accessors let presentation distinguish an unloaded floor from an empty one:
`UTerritoryBlueprintLibrary::AreTerritoryFloorGuardCountsKnown(Floor)` and
`AreTerritoryGuardCountsKnown(Territory)`. Neither is a second authority - both read the replicated
snapshot.

## Verification

| | Result |
|---|---|
| `TerritoryFramework.Guards.Floors` - default | 16 passed, 0 failed |
| `TerritoryFramework.Guards.Floors` - stomp | 16 passed, 0 failed |
| `TerritoryFramework.Tales.Tasks` - default | 6 passed, 0 failed |
| `TerritoryFramework` - default | 414 passed, 0 failed |
| `TerritoryFramework` - stomp | 414 passed, 0 failed |

414 is 413 plus the one new editor-module test. The new tests reach the read model through the post's
own `RegisterResolvedGuardSpawnPoint` weak registration - the call the level's streaming path makes -
and then destroy the actor, so the floor loses its counts exactly as it does when a cell unloads.
`AttachPost` cannot stand in for that: `GuardSpawnPoints` holds a **strong** pointer that keeps a
destroyed post readable until GC, which is why no previous floor test could model a stream-out.

### The red legs

Three deliberate single-line breaks, run separately so each assertion's sensitivity is attributable:

| Break | Observed |
|---|---|
| D1 `TerritoryStateTask.cpp`: drop `&& bCountsKnown` from the whole-Place branch | 1 failure, full suite - the whole-Place objective leg |
| D2 `TerritoryTypes.h`: drop `bCountsKnown` from `IsCleared()` | 3 failures, `Guards.Floors` filter - the two runtime legs and the new editor test |
| D3 D2 again, `Tales.Tasks` filter | 1 failure - the per-floor objective leg |

Verbatim, assertion messages exactly as reported:

```text
D1: Expected 'A whole-Place objective cannot complete while its posts are unloaded' to be false.

D2: Expected 'A floor whose post has not streamed in is never cleared, however empty it reads' to be false.
    Expected 'A half-loaded floor is still not cleared' to be false.
    Expected 'The unloaded floor's pre-loss read is not cleared' to be false.
    Expected 'A floor unloaded when its last defender fell still announces its clear' to be 1, but it was 0.
    Expected 'The announcement names the floor that was unloaded' to be 0, but it was -1.
    Expected 'The recovered beat is announced exactly once' to be 1, but it was 0.
    Expected 'A floor cannot read cleared while one of its posts is in an unloaded cell' to be false.
    Expected 'A reloaded floor does not read cleared out of a save' to be false.

D3: Expected 'A floor objective cannot complete on a floor whose posts are unloaded' to be false.
```

The pair in the middle of D2 is the second harm, observed end to end: with the gate missing, a floor
cleared while its post was unloaded announces its clear **zero** times instead of once. D1 and D3 are
independently load-bearing - the whole-Place leg stayed green under D2/D3 and the floor leg stayed
green under D1, so neither break can satisfy the other's assertion.

### A trap this test walked into, recorded because it is the same defect class

The new editor test first read

```cpp
const FTerritoryFloorSnapshot* ReloadedFloor = FindFloor(Fixture.Place->GetGarrisonSnapshot(), 0);
```

`GetGarrisonSnapshot()` returns the read model **by value**, so that pointer targets a temporary that
dies at the end of the full expression, and every later assertion reads freed bytes. The release
allocator kept serving them: the failure that exposed it was an impossible `ActiveGuards == 646` on a
floor whose post had just been streamed in empty, and the same read reported 400 on the previous run.
It is fixed by binding a named snapshot first, with the reason stated at the call. The committed
regression test `TerritoryFramework.Guards.Floors.ProgressReadsItsOwnFloorSnapshot` documents the same
trap for the same accessor; the inline form is safe only while the dereference stays inside the one
full expression.

## Companion tooling change

`Scripts/Territory/verify_floor_staging_pie.py` compares each client's floor read model against the
server's. `counts_known` is added to `floor_model()`, to `floors_in()` and to `FLOOR_FIELDS`, so a
completeness disagreement across worlds cannot pass on the numeric fields alone.

The reflected attribute name was settled from engine source, not guessed:
`PyGenUtil.cpp:1951` `PythonizePropertyName` strips a leading `b` from bool names before snake-casing
("Strip the b prefix from bool names"), so the attribute is `counts_known`, **not**
`b_counts_known`. That was then confirmed at runtime rather than left as a reading:

```text
FLOORSTATE: snapshot counts_known attr=True b_counts_known attr=False
```

**This half is instrument-verified but parity-unexercised.** The comparison itself never ran - see
the blocker below. It is a harness change with a proven attribute name and no observed result.

## The blocked leg: replicated parity in PIE

Running `verify_floor_staging_pie.py` requires arming the level's fixture, which asserts it is the
only authority on the asset:

```python
assert not original_floors and not any(original_row_floors), (
    'The Blacksmith Definition already authors floors; this fixture would overwrite them.')
```

The project-side asset now authors floors and every post row has a floor index, so the guard fires -
and it fires on **both** halves of the condition. The PIE run returned `python_ok: false` with that
`AssertionError` at `arm_floor_staging_pie_fixture.py:86` before any world was staged, so no parity
comparison was produced.

Reading the plugin's dead `Content/` copy instead of the project asset answers "no floors authored"
for an asset that authors floors, which is how this blocker was first mis-reported as disproven. The
corrected probe reads the project asset and reports, verbatim:

```text
FLOORSTATE: floors=2
FLOORSTATE:   floor index=1 desired=2 events=1
FLOORSTATE:   floor index=0 desired=0 events=0
FLOORSTATE: initial_guard_count=7
FLOORSTATE: guard_posts=7
FLOORSTATE:   post id=BP_TerritoryGuardSpawnPoint_C_1 floor=0
FLOORSTATE:   post id=BP_TerritoryGuardSpawnPoint_C_2 floor=0
FLOORSTATE:   post id=BP_TerritoryGuardSpawnPoint_C_3 floor=0
FLOORSTATE:   post id=BP_TerritoryGuardSpawnPoint_C_0 floor=0
FLOORSTATE:   post id=BP_TerritoryGuardSpawnPoint_C_4 floor=0
FLOORSTATE:   post id=BP_TerritoryGuardSpawnPoint_C_5 floor=1
FLOORSTATE:   post id=BP_TerritoryGuardSpawnPoint_C_6 floor=1
```

That probe was a temporary read-only instrument (a headless `UnrealEditor-Cmd.exe -ExecCmds="py
<abs unquoted path>.py"` run, the script quitting the editor itself because `py` swallows the rest of
`-ExecCmds`) and is removed again at the end of the batch; the output above is its receipt.

Two consequences that belong to the asset, not to this fix:

- The arm fixture can no longer re-run the floor proof as written. Making it usable again means
  either authoring the floors **before** the guard reads them (the fixture would then have to
  reconcile with authored content rather than assert its absence) or re-authoring the asset's floors
  as a deliberate, reviewed content change. Neither is in this batch.
- Floor 1 carries the only `events=1` row in the asset, and its two posts are `C_5`/`C_6` - the two
  the floor staging proof recorded as **undeployable**. This is the previously-raised open finding,
  unchanged and still open: the authored floor reward may be unreachable in the live level.

## Known gap in the harness, unchanged

`floor_model()` records `pending`, but `pending` has never been in `FLOOR_FIELDS` nor in `floors_in()`,
so the client/server comparison has never covered `PendingDeployments`. This is pre-existing and
deliberately left alone here: fixing it is a separate parity question (`pending` is where the reserve
timing lives) and changing it silently inside this batch would widen the harness's claims without a
run behind them.

## Files changed

| File | Change |
|---|---|
| `Public/Core/TerritoryTypes.h` | `bCountsKnown` on both snapshots; gates `IsCleared()`; in both `operator==` |
| `Private/Core/TerritoryVolume.cpp` | authored/live slot sets; per-floor completeness; whole-Place completeness before the floorless return |
| `Private/Tales/TerritoryStateTask.cpp` | whole-Place `AllDefendersDefeated` requires `bCountsKnown` |
| `Public/Core/TerritoryBlueprintLibrary.h` / `Private/.../.cpp` | two `BlueprintPure` completeness accessors |
| `Private/Tests/TerritoryFloorStagingTests.cpp` | `IncompleteFloorReadsNotCleared`, `StreamedOutPostCannotClearItsFloor`, `DefenderObjectivesWaitForTheirPostsToLoad`; contract test gains the two reflection legs and the completeness sensitivity legs |
| `Editor/.../Tests/TerritoryFloorEventTests.cpp` | `StreamedOutFloorKeepsItsClearBeat` - the lost-beat half, through the resolved-post streaming path |
| `Scripts/Territory/verify_floor_staging_pie.py` (host) | `counts_known` in the floor comparison |

## Migration

None. No serialized field: `bCountsKnown` is `CPF_SaveGame` false on both structs, asserted by the
contract test, because it is derived state recomputed from the loaded posts. It **is** in the
replicated snapshot, which is the only way the flag can mean anything on a client.

Gameplay-visible, and it belongs in the release note: a floor objective and a whole-Place objective
no longer complete while the posts holding the defenders are in an unloaded cell. That is the
intended behaviour, but it is a behaviour change for any content that happened to rely on the
spurious read.
