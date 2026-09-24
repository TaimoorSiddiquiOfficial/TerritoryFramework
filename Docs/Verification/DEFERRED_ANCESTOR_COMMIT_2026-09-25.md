# Ancestor events wait for the source transition to finish

Status: **fix applied, linked, and verified red and green.** The suite passes under both allocators,
and the new assertions have been observed failing against a deliberately disabled deferral.

Adversarial-audit finding, claim 4: a parent Territory's Narrative events fired **before** the child
transition that caused them had finished. The parent committed - and ran its `CityLost` /
`CityFullyCaptured` events - against a child that still had its pre-transition upgrade level, its
pre-transition garrison and its own state events still to fire.

## The mechanism

`CommitOwnershipData` publishes the child's replicated summary part-way through itself
(`TerritoryVolume.cpp:1879`), deliberately: the comment above it (`:1877-1878`) records why - the
World Partition read model must be current *before* state events evaluate cross-territory conflict
protection. Publishing drives `ATerritoryWorldState::ReconcileUnloadedAncestors`, which reconciled
every loaded ancestor **immediately**, in the middle of the child's commit.

The nesting is fully synchronous, so the ancestor's commit runs while the child is still half
changed:

```text
CommitOwnershipData(child)
  :1876  OwnershipData = MoveTemp(CommittedData)      <- child's owner is committed
  :1879  PublishCaptureSummary()                       <- ancestor commits HERE
           -> ReconcileUnloadedAncestors
             -> ReconcileDerivedControl(ancestor)      <- ancestor fires its events
  :1891  guard lifecycle (DespawnGuards/SpawnGuards)   <- child's garrison still pre-transition
  :1930  ReconcileAvailabilityDependentSystems()
  :1939  ReconcileOwnershipDependentSystems()          <- child's UpgradeLevel resets to 0 HERE
  :1946  FireStateEvents(child)                        <- the child's own events
```

Everything the child does between `:1879` and `:1946` was invisible to the ancestor, and the two
lines that matter most sit well after the ancestor already fired:
`ATerritoryProperty::ReconcileOwnershipDependentSystems` clears the upgrade level at `:1939`, and the
guard lifecycle replaces the child's garrison at `:1891-1922`.

The existing test `EventsObserveReconciledDependencies` asserts this exact invariant and could not
fail: it uses a **parentless** `ATerritoryProperty`, so there is no ancestor to observe the child
early. The property that was tested was real; the test simply could not reach the case where it
breaks.

## Why the design defers the commit and not the events

Two alternatives were rejected before the chosen one, and both rejections are structural rather than
aesthetic:

- **Queueing the ancestor's *events*** would have created a second execution path for
  `FireStateEvents` - a path that must, forever, reproduce every decision the original one makes
  (which bundle, which context, which conditions, which conflict protection). Two paths that both
  answer "did this fire" is a competing authority over the answer, which is precisely the class of
  defect this audit is about.
- **Forcing loaded ancestors to self-reconcile after the fact** is disproportionate: it needs its own
  ordering argument for availability, guards and locks, and it would make the ancestor reachable from
  two directions.

**Chosen:** defer the ancestor's **volume commit** - nothing else - to the end of the outermost
transition frame, keyed by tag. The ancestor still commits exactly once, through the one existing
path (`ReconcileLoadedAncestor`), with the live transition context, after the child is fully
reconciled. **The queue holds tags, never state or pointers**, so a queued entry cannot dangle and
cannot disagree with the child by the time it drains.

## The one subtle failure mode, and how it is closed

The drain must run while the child's `ActiveTransitionContext` is **still installed**, because
`ReconcileDerivedControl` reads that context off the child
(`TerritoryHierarchy.cpp:663-677`) and hands it to the ancestor's commit. Get this wrong and every
ancestor event fires with a default context, and any cutscene authored on `OnCityLost` finds no
audience - a silent presentation failure with no assertion pointing at it.

The frame scope is therefore declared **after** the context guards
(`TerritoryVolume.cpp:1821-1822` → scope at `:1825`), deliberately: C++ destructors run in reverse
declaration order, so the frame closes *first*, before `ActiveTransitionContext` is restored.

It is RAII rather than paired `Enter`/`Exit` calls because `CommitOwnershipData` has early
`return false` paths between the declaration and its success return. Counted in source rather than
estimated, there are exactly **four**, all at `TerritoryVolume.cpp:1926`, `:1931`, `:1941` and
`:1943`, all of the same shape:

```cpp
if (GarrisonLoadGeneration != CommitLoadGeneration || IsActorBeingDestroyed()) return false;
```

They are reachable synchronously from a Narrative callback - each sits immediately after a step that
invokes external code (guard retirement, reserve reconciliation, availability reconciliation), and
`UNarrativeSaveSubsystem::LoadActorFromRecord` sets `Ar.ArIsSaveGame = true` before `Actor->Serialize`
(`NarrativeSaveSubsystem.cpp:749-777`), which is exactly what bumps `GarrisonLoadGeneration` at
`TerritoryVolume.cpp:817`. Every one of them must close the frame. A paired `Enter`/`Exit` would have
to be repeated at each, and a single omission would wedge `TransitionFrameDepth` above zero and
silently disable every future ancestor event for the session - a failure with no assertion pointing at
it. RAII makes that omission impossible rather than merely unlikely.

The failure-path test drives one of those four (the guard-retirement path) and asserts that the commit
really did return false, so "the frame still closed" is proven to be the early-return case rather than
inferred from it.

`FTransitionFrameScope` holds a `TWeakObjectPtr` to the world state rather than a raw pointer for the
same reason: a callback can destroy the world state mid-commit.

## What changed

Territory-owned only. No vendor file is touched.

**1. Frame state on `ATerritoryWorldState`** (`TerritoryWorldState.h:431-440`, `:613-631`):
`FTransitionFrameScope` (public, RAII), plus private `ReconcileLoadedAncestor`,
`EnterTransitionFrame`, `ExitTransitionFrame`, `DrainDeferredAncestorReconciles`,
`TransitionFrameDepth`, `bDrainingDeferredReconciles` and
`TArray<TPair<FGameplayTag,FGameplayTag>> DeferredLoadedAncestorReconciles`.

**2. One commit path, two callers.** The inline body of the loaded-volume pass was extracted verbatim
into `ReconcileLoadedAncestor` (`TerritoryWorldState.cpp:713-721`), which is now called both inline
(no frame open) and by the drain (frame closing). This is what keeps "the ancestor commits once,
through the one existing path" true by construction rather than by discipline.

**3. The loaded-volume pass became conditional** (`TerritoryWorldState.cpp:694-707`): with
`TransitionFrameDepth > 0` the pair is queued and the loop `continue`s.

**4. `CommitOwnershipData` opens the frame** (`TerritoryVolume.cpp:1825-1830`).

**5. The drain** (`TerritoryWorldState.cpp:736-781`):

- **Re-entrancy guard.** A commit performed by the drain opens and closes its own frame, so its exit
  re-enters. Returning immediately is correct - the loop keeps popping, and anything the nested exit
  queued is already in the array being walked.
- **Bounded at 64 entries**, logging and dropping the remainder rather than hanging the transition on
  a malformed or self-referential hierarchy. Dropping is recoverable (the next transition rebuilds
  the queue); spinning is not.
- **Authority-gated** like every sibling reconciliation entry point, emptying the queue on a client.
  This is a contract guard rather than the thing that keeps the queue empty: the producer returns
  before queueing without authority.
- **Re-resolved at drain time.** The pair holds tags precisely so the child is resolved to the actor
  that exists now, not the one that existed when the transition began.

## Plan-versus-source corrections

The batch plan was written against a pre-`63aa92d` tree. Three of its statements did not survive
re-verification; source is authoritative, so the record is corrected here rather than silently
diverged from.

**1. The line citations had drifted, and this change moved one of them again.** The plan cites the
context guards at `:1807-1808` (actual: `:1821-1822`) and the read-model-visibility comment at
`:1856` (actual, before this change: `:1868-1869`). The comment and the publish it describes now sit
at `:1877-1878` and `:1879` because **this change inserted nine lines above them**. A citation to
`CommitOwnershipData` line numbers from any earlier commit should be treated as approximate.

**2. The plan's test 1 is topologically impossible as written.** It specifies "a loaded City with a
loaded, Definition-backed child Property... assert `GetUpgradeLevel() == 0` from inside the City
callback". `GetUpgradeLevel` exists only on `ATerritoryProperty` (`TerritoryHierarchy.h:280`,
`:303`), while a City's *authored* children are Districts - a City has no Property child to read an
upgrade level from. The delivered test observes from the **District**, whose authored child is the
Place and which can therefore read `GetUpgradeLevel` on its own child. The property the test proves
is exactly the one the plan intended; only the observation level moved.

**3. A load-bearing premise was only half true - the substantive finding of this batch.**`ReconcileUnloadedHierarchy` returns early for a **loaded** parent
(`TerritoryWorldState.cpp:814-815`, "Loaded parents still commit through their own hierarchy
lifecycle"). So the plan's claim that "the directory pass stays early and unconditional - that is
what preserves the load-bearing read-model visibility" holds for **unloaded** ancestors only. For a
**loaded** ancestor there was no early directory write to preserve: its row was published by
`PublishTerritorySummary(Loaded)` inside the mid-commit `ReconcileLoadedAncestor` - that is, from the
child that had not finished changing.

The row therefore is not merely published early; it is published **wrong**. Before this change, a
loaded District's row during a child capture briefly held a value derived from a half-reconciled
child. After it, nothing publishes the ancestor's row until the ancestor actually commits.

The consequence for save/load is stated honestly rather than papered over: **a save taken inside a
child's transition now records the loaded ancestor's row as of before the transition.** That is the
*correct* paired value - it matches the ancestor actor, which has not committed either, so the save is
internally consistent - and it self-heals at two boundaries: the frame exit publishes it, and
`ImportPersistentState` followed by any actor publication reconciles it from the restored child. The
alternative - writing a row that matches neither the pre- nor the post-transition actor - is what the
old code did.

The child's own row is unaffected and stays early: `PublishCaptureSummary()` at `:1879` was **not**
moved, which is the read-model visibility the comment protects and the input the child's own state
events evaluate conflict protection against.

**4. The plan's count of early returns was an overestimate.** It describes "~10 early `return false`
paths between `:1815` and `:1940`" as the justification for RAII. That count is wrong: the range
contains **four**, all at `:1926`, `:1931`, `:1941` and `:1943`, and all identical. RAII is still the
right choice and for the reason given, but the argument does not need ten repetitions to hold - four
distinct call sites that each invoke external code, in a function whose contract is that no frame may
be left open, is enough. The corrected count is used above.

## The red leg

The deferral was disabled and the filter re-run against the rebuilt binary:

```cpp
if (false) // RED LEG: deferral disabled to observe the new assertions fail
```

A one-token change chosen so the break isolates the deferral and nothing else: the queue, the drain,
the RAII scope and every production statement stay as written, so the build stays warning-clean and
the only thing falsified is the conditional.

The result was `exit=255 success=7 successWithWarnings=0 fail=5` where the same filter is `12/12`
with the deferral in place. **Five of the six new tests failed**, each on the assertions the plan
named:

```text
Error: Expected 'The ancestor event observes the child's cleared upgrade level' to be 0, but it was 2.
Error: Expected 'The ancestor event observes the child's post-despawn guard count' to be 0, but it was 1.
Error: Expected 'The ancestor event observes the same child guard state the transition settles on' to be 0, but it was 1.
```

That trio is the defect stated exactly: the ancestor read the child's **pre-transition** upgrade level
(2, the value the test set) and its **pre-transition** garrison (1 guard, the one the test added), and
disagreed with the garrison the transition actually settled on.

```text
Error: The child's event observes the ancestor still on the old owner: The two values are not equal.
Error: Expected 'The child's events run before the ancestor's, in that order' to be "child|ancestor", but it was "ancestor|child".
```

The ordering inversion is the claim itself, and it is measured two independent ways - a direct read of
the ancestor's owner from inside the child's callback, and a shared trace of execution order.

```text
Error: Expected 'The two-level cascade runs bottom-up, each level once' to be "place|district|city", but it was "city|district|place".
Error: Expected 'Iteration 0 drains the same order' to be "place|district|city", but it was "city|district|place".
  ... (Iterations 1-9 identical)
```

The two-level and determinism tests both failed on order rather than on count: without the deferral
the cascade runs **top-down** (each ancestor reconciling its own parent as it commits), and all three
levels still fire exactly once. That is worth stating precisely, because it means those two tests
prove ordering and not merely cardinality - and it is why their assertion labels say "drains" while
the pre-fix trace is an immediate cascade rather than a drain. The label describes the intended
mechanism; the assertion is valid either way.

```text
Error: A loaded ancestor's row is not published from a half-reconciled child: The two values are not equal.
Error: A loaded ancestor's row is not published from a half-reconciled child: City: The two values are not equal.
Error: The mid-commit save restores the ancestor row as of the save instant: The two values are not equal.
```

This trio is correction 3 above, observed rather than argued: with the deferral disabled the rows
**are** published from the half-reconciled child, and the mid-commit save records that value.

The sixth new test (`DeferredAncestorReconcileFailurePaths`) stayed **green** under the break, and
that is correct rather than a gap: it asserts durability properties (an early return still closes the
frame and still drains; a destroyed ancestor drains into nothing rather than dangling; a superseded
commit leaves the ancestor alone), none of which the deferral's presence or absence changes. Its value
is regression protection for the RAII and tag-keyed design, not discrimination of the fix.

That test gained one assertion **after** the red leg was run: `TestFalse` on the commit result, proving
the nested campaign load really does take an early `return false` path rather than the frame assertions
merely being consistent with one. It is a durability assertion, not a discriminator, so its absence
from the red run costs nothing - but it was verified green under both allocators on the final binary,
and the red-leg counts above are from the binary immediately before it. Stating the order rather than
implying the red leg covered a line that did not yet exist.

The deliberate break was then reverted, verified absent (`grep -rn "RED LEG" Source/` is empty), and
the binary rebuilt before the green runs below.

## Verification

Assertion messages are cited rather than reported line numbers: the framework's reported line for a
`TestEqual` was off by up to +6 in claim 1's record, and addresses split between the test file and
`AutomationTest.h`.

Each filter was run under both allocators. `-Stomp` is not a formality here: the whole subject is a
value (a tag pair, then a resolved actor) held across a window in which a callback can destroy actors,
so a read of freed memory is exactly the hazard the default allocator would hide (see
`FLOOR_SNAPSHOT_LIFETIME_2026-09-24.md`).

| Filter | Default | `-Stomp` | Deferral disabled (red leg, default) |
|---|---|---|---|
| `TerritoryFramework.Capture` | 12/12 | 12/12 | **7/12, fail=5** |
| `TerritoryFramework.Hierarchy` | 6/6 | 6/6 | not run (break does not reach this side) |
| `TerritoryFramework.WorldPartition` | 5/5 | 5/5 | not run |
| `TerritoryFramework.WorldState` | 4/4 | 4/4 | not run |
| `TerritoryFramework.Guards.Floors` | 19/19 | **not run** (see below) | not run |
| `TerritoryFramework.StateRules` | 4/4 | **not run** (see below) | not run |
| `TerritoryFramework.Tales` | 50/50 | not run | not run |
| `TerritoryFramework.UI` | 27/27 | not run | not run |
| `TerritoryFramework.Presentation.Cutscenes` | 24/24 | not run | not run |
| `TerritoryFramework.Presentation.Cinematics` | 9/9 | not run | not run |
| `TerritoryFramework.SaveLoad` | 2/2 | not run | not run |
| `TerritoryFramework.Contract` | 53/53 | not run | not run |

The red leg was run on the default allocator only, deliberately: the break disables a *conditional*
rather than introducing a lifetime hazard, so the failure it must produce is an assertion failure and
the poisoning allocator has no additional purchase on it. The stomp legs were run against the reverted,
corrected binary.

**Two stomp legs are missing, and this is a verification gap rather than a pass.** The stomp sweep was
issuing `StateRules` and `Guards.Floors` when the harness stopped it because the test editor had driven
the machine to critically low memory. Those two filters were therefore never run under `-Stomp`. The
affected code is `ReconcileUnloadedAncestors`, shared by every capture path, so a lifetime defect there
would be expected to surface in `Capture` (12/12 under `-Stomp`) and `WorldPartition` (5/5) first; but
"would be expected to surface elsewhere" is an argument, not a result, and the two cells above are
recorded as unrun.

**The red log was not preserved as a separate artifact.** The red run wrote to the filter's normal log
path and the green re-run overwrote it, so unlike the claim-5 batch there is no
`_redleg_B7_*.log` beside it. Every red-leg message quoted above is a verbatim extraction from that
run; what is lost is the full multi-assertion dump for the five failing tests, not the discriminating
lines.

## Test map

New and changed tests, all in the TerritoryFramework suite (the TDA game module never loads, so a test
placed there would never register):

| Test | Covers |
|---|---|
| `...Capture.Regression.AncestorEventsObserveTheSourcesReconciledState` | The discriminator: a loaded District's event reads its child's cleared upgrade level and post-despawn garrison. Plus the premise that a normal commit leaves depth 0 and an empty queue. |
| `...Capture.Regression.AncestorEventsStillRunExactlyOnceOnADeferredCommit` | Entry events fire once per level, the child's event observes the ancestor still on the old owner, and a shared trace proves `child|ancestor`. |
| `...Capture.Regression.DeferredAncestorReconcileSurvivesANestedTwoLevelCommit` | Place → District → City, all loaded: bottom-up order, each level once, the City observing an already-committed District - and two queued entries naming the City that still commit it once. |
| `...Capture.Regression.DeferredAncestorReconcileFailurePaths` | An early `return false` inside the frame still closes and drains it (the return itself asserted, not inferred); a superseded commit leaves the ancestor alone (the drain re-resolves by tag); an ancestor destroyed by a callback drains into nothing. |
| `...Capture.Regression.DeferredAncestorReconcileIsDeterministic` | Ten alternating captures produce one identical trace and an empty queue; the drain cap is never reached legitimately. |
| `...Capture.Regression.DeferredAncestorCommitKeepsTheReadModelCurrent` | The child's row is current mid-commit; a loaded ancestor's row deliberately is not; all rows are current by return; a mid-commit export restores consistently and a publication reconciles the ancestor. |

**The test seam.** `FTFTransitionFrameProbe`
(`TerritoryFrameworkEditor/Private/Tests/TerritoryTransitionFrameProbe.h`) is a friend of both
`ATerritoryWorldState` and `ATerritoryVolume`. Frame depth, the deferred queue, `bDraining...` and the
authored runtime state configs are private precisely because nothing in production may read or mutate
them that way - the whole point of the fix is that **one** path owns the ancestor commit - so the
assertions that prove the frame opens, closes and drains need an explicit seam rather than a public
accessor that would invite a second reader.

**Why the fixture uses an unbegun world.** `ATerritoryDistrict::BeginPlay`
(`TerritoryHierarchy.cpp:585-604`) registers `Registry->OnTerritoryRegistered` and binds every
`GetProperties()` via `BindToProperty`. In an **unbegun** world nothing binds, so the WorldState's
deferred reconcile is the only path that can commit a loaded ancestor and the test cannot pass by way
of the delegate cascade at `CommitOwnershipData:1974`. The fixture is therefore built in
`UWorld::CreateWorld(EWorldType::Game, false)` and registers the child **before** its ancestor. A
begun world would reach the ancestor by two paths, which would make the tests unable to distinguish
the drain from the cascade.

## Migration and release notes

**No serialized field, no content migration.** The frame state is `Transient`, unsaved and
unreplicated; the queue holds gameplay tags; nothing durable changes shape.

**Gameplay-visible improvements, both intentional:**

- Ancestor events (`OnCityLost`, `OnCityFullyCaptured`, and any authored Territory state event on a
  City or District) now observe a fully reconciled child: the new owner, the cleared upgrade level,
  the replaced garrison. A callback that previously read a half-changed child reads a settled one.
- The cascade runs bottom-up rather than top-down. For a designer-authored sequence of events this
  changes the *order* ancestors observe the change in. Any content that relied on the old top-down
  order - the City reacting before its District - was relying on the defect, since the City's event
  was reading an unreconciled District.

**Mid-transition saves record a pre-transition ancestor row** for a *loaded* ancestor, as described in
correction 3. It is internally consistent, self-heals at frame exit and at load, and is strictly
better than the previous value, which matched neither the pre- nor the post-transition actor. It is
recorded here because a support engineer reading a save taken at that instant will see the ancestor
one transition behind and should not treat it as corruption.

## Known limitations

- **The two-client/late-join leg is not verified, and this batch does not claim it.** The plan's
  verification strategy asks for a hierarchy/directory **parity** receipt in the PIE probe - the check
  that every client's directory rows and every client's derived hierarchy state match the server's
  after a capture. That receipt does not exist: the probe's `late_join()` and `replicate()` helpers are
  **garrison-shaped**, built for the floor/garrison read model, and carry no hierarchy rows. Claim 4 is
  therefore verified on the **authority only**, headlessly, with the assertions above. Writing that
  parity check is the outstanding work item; until it exists, a client-side divergence in the deferred
  ancestor path would not be caught by anything in this repository.
- **The drain cap is a safety valve, not a proof.** 64 entries is far above any authored hierarchy this
  project builds, and the determinism test asserts the cap is never reached legitimately, but a
  hierarchy deeper than 64 levels of ancestors would silently drop deferred commits with a warning.
  The cap trades a theoretical correctness loss for a guarantee against a hung transition, which is the
  right trade for a synchronous gameplay path.
- **A dropped entry does not self-heal until the next transition.** The queue is frame-local, so a
  dropped entry is simply gone; the ancestor's row and events are reconciled by whatever later capture
  touches that subtree. No shipped content approaches the cap.
- **The mid-transition ancestor-row staleness above is a real, if narrow, observable.** A save taken
  from inside an ancestor callback during a child capture will read one transition behind. Fixing it
  would require the exact thing this design rejects - publishing an ancestor row from a child that has
  not finished changing.
- The previous batch (claim 5, `HIERARCHY_COMPLETENESS_CONTRACT_2026-09-25.md`) settles
  `ReconcileUnloadedHierarchy`'s mutation rule, which is what the deferred re-run calls. Its
  known limitation - spurious tenures already written into existing saves are not repaired - is
  unchanged by this batch.
