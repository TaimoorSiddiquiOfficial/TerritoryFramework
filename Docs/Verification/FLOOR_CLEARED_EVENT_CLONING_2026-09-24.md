# A floor-cleared event runs this Territory's clone, not the Definition's template

Status: **fix applied, linked, and verified red and green.** The defect reproduced with a deliberate
break, the fix removes it, the new assertions were observed failing against that break and passing
against the fix, and the suite passes under both allocators.

Adversarial-audit finding, claim 2: `DispatchFloorClearedEvents` executed the **Definition's own**
`UNarrativeEvent` objects instead of a per-Territory clone.

## The mechanism

`UTerritoryDefinition` is a data asset. Its `Floors[].FloorClearedEvents` are authored templates -
one object each, shared by every Territory that references the Definition. `ATerritoryVolume`
already treated the sibling arrays (`DefenderDiedEvents`, `AllDefendersDefeatedEvents`) as
per-actor clones, and `RuntimeStateConfigs` carries the rule in a comment:

```cpp
/** Per-actor clones. DataAsset Narrative objects are immutable templates and never execute. */
```

Floor events were the one array that broke that split. Two consequences follow from executing a
shared template:

1. **One object runs once per Territory.** `First->ExecutionCount` cannot be attributed to a
   Territory, and two Territories referencing one Definition interleave into a single counter.
2. **The template resolves no world when the transition context is empty.** Vendor
   `UNarrativeEvent::GetWorld()` walks the outer chain calling `Outer->GetWorld()` at each level.
   A Definition-owned event's outer chain is the authoring DataAsset and then a transient package,
   so it returns null. `TerritoryTales::ResolveWorld(ContextObject, ...)` prefers the live
   Narrative execution context and only falls back to `ContextObject->GetWorld()`; with an empty
   context there is nothing to prefer and the fallback returns null. Every world-dependent beat
   then returns at `if (!World ...) return;` - **with no log line, so the beat fails silently.**

The empty context is not a corner case. A floor's last defender is usually a reserve walking in,
and a reserve death records no killer, so the transition context handed to the floor event is
empty. A floor cleared by its last reserve is the ordinary way a floor empties.

## Live evidence that narrowed the claim

The audit stated the world-resolution failure generally. Re-verification against the live receipts
in `Saved/Verification/FloorStagingPIE/` narrowed it:

- `DiagSequence1.json` records the floor-0 event as a `TerritoryPlayCutsceneEvent` with
  `outer = "DA_Place_Blacksmith"` - Definition-owned, as claimed.
- `DiagSequence5/6/8` show a `NarrativeLevelSequenceActor` **did** exist in the world.

So a Definition-owned floor event *does* resolve a world when the death carries a valid pawn, and
`DiagSequence1` has exactly that (`killer = BP_TerritoryPlayerCharacter_C_0`). The silent failure is
the empty-context case, and `DiagSequence1` also shows floor 0 at `active 0, maximum 2, reserve 2,
pending 2` - its clear was decided by reserve deaths, the case with no killer. The clone fixes both
halves; only the empty-context half was failing in that run.

## The fix

`FTerritoryFloorRuntimeEvents` (`Core/TerritoryTypes.h`) names one floor's runtime event list. It
exists only because UHT forbids a container as a `TMap` value:

```text
Error: The type 'TArray<TObjectPtr<UNarrativeEvent>>' can not be used as a value in a TMap
```

Keeping the key on the outside preserves the one-row-per-floor shape; folding the index into the
struct would allow a repeated row.

`ATerritoryVolume::RuntimeFloorClearedEvents` is a `TMap<int32, FTerritoryFloorRuntimeEvents>` keyed
by floor index. `RebuildRuntimeNarrativeConfiguration` fills it with
`CloneNarrativeArrayForTerritory` - the same seam the defender arrays already use, which is
`DuplicateObject<T>(Template, Territory)` and carries `Instanced` conditions with the event.
`DispatchFloorClearedEvents` reads the clone, still copying the array before iterating.

**Deliberately outside the `IsA<UTerritoryPlaceDefinition>()` gate** that guards the defender arrays.
`Floors` and `FindFloor` live on the base `UTerritoryDefinition` and the dispatch path serves any
definition type, so nesting the clone block inside that gate would silently drop floor beats for a
District or a City that declares floors.

A Definition with no floors leaves the map empty, which is the "nothing to run" case.

## The test that encoded the defect

`FTFTerritoryFloorClearedEvents` authored its events **after** `BuildFixture` had already applied the
Definition, and then asserted on the **templates**:

```cpp
TestEqual(TEXT("..."), Ungated->ExecutionCount, 1);   // the shared object - the defect, passing
```

That is precisely what let the shared-template defect ship. It is rewritten to assert the opposite:
the templates never execute, the clones do, and the condition is flipped on the clone's own
duplicated `Instanced` copy, because flipping the Definition's template would reach a different
object and prove nothing about what executed.

## Verification

| | Result |
|---|---|
| `TerritoryFramework.Guards.Floors` - default | 13 passed, 0 failed |
| `TerritoryFramework.Guards.Floors` - stomp | 13 passed, 0 failed |
| `TerritoryFramework` - stomp (full suite, header change) | 410 passed, 0 failed |

The red leg was the production line storing the Definition's array instead of a clone:

```cpp
RuntimeEvents.Events = Floor.FloorClearedEvents;   // red leg - templates, not clones
```

It failed three tests. Assertion messages, verbatim:

```text
Expected 'A floor cleared with no recorded killer still plays its authored cutscene' to be 1, but it was 0.
Expected 'A floor event clone resolves its Territory's world' to be not null.
Expected 'The clone resolves the very world its Territory stands in' to be true.
Expected 'The two Places do not share one floor event object' to be true.
Expected 'Each clone is outered to its own Territory' to be true.
Expected 'The Definition's ungated cleared event never executes' to be 0, but it was 2.
Expected 'The Definition's gated cleared event never executes' to be 0, but it was 1.
Expected 'The second Place's instance does not run for the first Place' to be 0, but it was 1.
Expected 'The Definition's shared template never executes' to be 0, but it was 1.
```

The first line is the end-to-end consequence: with the shared template, a floor cleared by its last
reserve spawns **zero** sequence actors. With the clone it spawns exactly one. That test builds a
world with a faction viewer (`World->InitializeActorsForPlay` is required before the controller
iterator - and therefore the audience resolver - sees anything) and drives the clear through
`ConcludeFight`, whose context parameter defaults to empty.

## Files changed

| File | Change |
|---|---|
| `Public/Core/TerritoryTypes.h` | `FTerritoryFloorRuntimeEvents` |
| `Public/Core/TerritoryVolume.h` | `RuntimeFloorClearedEvents` member |
| `Private/Core/TerritoryVolume.cpp` | clone loop; dispatch reads the clone; null-branch reset |
| `Editor/.../Tests/TerritoryFloorEventTests.cpp` | rewritten template assertion; two new tests |

## Migration

None. The map is `Transient`: not replicated, not saved, not Blueprint-visible. It is rebuilt
whenever the Definition is applied, and `ApplyToTerritory` is the sole caller.

## Companion tooling fix

`Tools/Run-Tests.ps1`'s staleness guard compared the **runtime** DLL against the newest file under
the whole `Source` tree. Because the tests live in the editor module, every test-only edit reported
a stale runtime binary and refused the run - a false positive that fires on exactly the edits this
work is made of. The guard is now per-module: each `UnrealEditor-<Module>.dll` is compared against
its own `Source/<Module>` directory.

The control was verified to still fire, not weakened: touching
`Source/TerritoryFramework/Public/Core/TerritoryTypes.h` produced
`UnrealEditor-TerritoryFramework.dll is older than TerritoryTypes.h`, and the mtime was restored
afterwards.
