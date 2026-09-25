# One completeness contract for both hierarchy reducers

Status: **fix applied, linked, and verified red and green.** The suite passes under both allocators,
and the new assertions have been observed failing against a deliberately disabled gate.

Adversarial-audit finding, claim 5: the loaded reducer (`TerritoryHierarchy.cpp`) and the durable
reducer (`ATerritoryWorldState::ReconcileUnloadedHierarchy`) disagreed about what an unknown child
means. One refused a commit; the other committed a default view of it.

## The mechanism

`ATerritoryCity` and `ATerritoryDistrict` derive their owner from their authored children. Two
different reducers serve the two ways a child can fail to be present:

- The **loaded** reducer (`ReduceChildControl` in `TerritoryHierarchy.cpp`) runs on the authority
  against child actors that are currently loaded.
- The **durable** reducer (`ATerritoryWorldState::ReconcileUnloadedHierarchy`) runs against the
  replicated directory rows for children World Partition has unloaded.

Before this change the two had different completeness notions. At the commit this batch builds on
(`13a7919`):

```text
TerritoryHierarchy.cpp:175      int32 UnresolvedChildCount = 0;          // loaded side: had it
TerritoryHierarchy.cpp:502      if (Reduction.UnresolvedChildCount > 0)  // ...and gated on it
TerritoryHierarchy.cpp:625      if (Reduction.UnresolvedChildCount > 0)  // ...in both reducers
```

The durable side had **no completeness notion at all**. Its child loop collapsed three different
situations - resolved-and-matching, absent, and inconsistent - into the same `continue`, then fell
straight through to the mutation block. `TerritoryWorldState.cpp:744`, at that commit:

```cpp
const auto Derived = TerritoryHierarchyPolicy::ReduceControl(Views);
if (Summary->CurrentOwner.IsValid() && Summary->CurrentOwner != Derived.SecuredOwner)
    Summary->FormerOwningFactions.AddTag(Summary->CurrentOwner);   // unconditional
Summary->CurrentOwner = Derived.SecuredOwner;
```

There is no gate between the loop and that append. A child that was absent (streamed out, never
registered, or a reused tag carrying the wrong GUID) reduced to a **default Unclaimed view**, and
the parent then committed that view: it **cleared the parent's owner** and **appended a tenure** for
the owner it appeared to have lost.

The tenure is the part that makes the mistake permanent. It is not derived state - it is *history*,
it is `SaveGame`, and it is exported as part of `SavedStrategicDirectory`. Once written, a later
complete reduction cannot distinguish it from a genuine loss, so the parent is recorded as having
lost a territory it never lost.

The loaded side had the same defect class on its *other* branch. Its duplicate/empty-tag path was a
silent `continue`, deliberately exempted and documented as such
(`TerritoryHierarchy.cpp:182-184` at that commit), on the grounds that counting it "would leave such
a parent unable to reconcile at all". The exemption was itself the defect: the reduction committed a
phantom view of a defective topology - the same failure the unknown case causes - and it hid the
authoring error rather than reporting it. Neither side was clean.

## The contract

One rule, both reducers:

| Reduction | Owner | State | Progress | History | Loss event | Eligibility |
|---|---|---|---|---|---|---|
| **Complete** | derived | derived | derived | appended if genuinely lost | fired if genuinely lost | granted |
| **Incomplete** | preserved | preserved | preserved | untouched | none | denied |

Incompleteness **propagates upward**: a parent is complete only when every authored child resolves
*and* each resolved child's own subtree is complete. A loaded City must not trust an unloaded
District's retained owner while that District has unresolved children of its own.

The distinction the contract preserves: a **known** Unclaimed child is a real loss and is recorded
as one; a **missing** child is an unknown and is not. Only the first may write history.

## What changed

Four parts, all Territory-owned. No vendor file is touched.

**1. The loaded reducer learned the second failure mode.** `FReducedChildControl` gained
`InconsistentChildCount` beside `UnresolvedChildCount`, incremented on the empty/invalid/duplicate
path instead of the silent `continue`; `IsComplete()` is the combined test, and both
`ReconcileDerivedControl` gates use it. The two counts are kept apart because they have different
causes and different fixes - unknown means "not arrived yet", inconsistent means "this asset is
wrong".

**2. The durable reducer gained the whole notion.** `ReconcileUnloadedHierarchy` now classifies each
authored child as resolved-with-identity-match, absent, or inconsistent, and:

- the mutation block is gated on the combined count;
- **the history append moved inside the complete branch.**

That second point is the fix that matters. `UnresolvedChildCount` alone would have been
insufficient: the durable side had no gate at all, so the append ran on every reduction including
the incomplete ones. Gating the write without moving the append would have left the permanent half
of the bug in place.

**3. A new recursive completeness query.** `ATerritoryWorldState::IsHierarchyReductionComplete` is
**newly introduced by this change** - it is *not* a rename of an earlier symbol. The batch plan
described it as a rename of `IsDirectoryReductionComplete`; that name never existed in any commit of
this repository (`git log -S"IsDirectoryReductionComplete" --all` is empty), and at `13a7919` no
`ReductionComplete` symbol existed under any spelling. Source is authoritative over the plan, so the
record is "introduced", not "renamed". Nothing pre-contract asked a completeness question, because
there was nothing to ask; that is exactly why the durable reducer could commit an unknown.

It is a **query-time** helper, not a stored field. It walks the replicated rows down from a parent,
counting child rows at the level below and comparing against the row's own authored `TotalChildren`,
recursing into each. A `TSet<FGameplayTag> Visited` fails a malformed row set closed instead of
recursing until the stack runs out.

> **Corrected 2026-09-25, by `HIERARCHY_COMPLETENESS_IDENTITY_2026-09-25.md`.** The sentence above
> describes the rule *as this batch introduced it*, and it was wrong: it is what the follow-up
> finding P1 was raised against. Counting rows can be satisfied by a row the reducer refuses -
> the right tag with the wrong GUID, an obsolete row standing in for a missing authored child, a
> `TotalChildren` saved before the authored child list changed - so this query approved reductions
> the reducer had deferred, including for the eligibility consumer
> `GetClaimedDistrictCountForFaction`. The query now walks the **authored** children by exact
> identity, through the one helper that also supplies the reducer and the registration path. The
> counting rule survives only for a row with no registered authored definition, which is documented
> on the branch. Read the follow-up record for the rule as shipped; the table row below for
> `UTerritorySituationProfile` should also be read with its limitation note there.

Deliberately **not** added to `FReplicatedCaptureSummary`: that struct is exported as part of
`SavedStrategicDirectory` (`SaveGame`), so a stored completeness flag would be written into the save,
go stale the moment a child registers or streams in, and become a second authority over a question
the row data can already answer.

**4. The topology defect became a validation gate.** `UTerritoryDefinition::IsDataValid` now reports
an empty child slot, an untagged child, and a tag declared more than once among a Definition's
children - each as `Context.AddError`, so the asset is *Invalid*. This is what answers the objection
that justified the old exemption: refusing an inconsistent slot is only safe because the authoring
error is now reported, so the topology defect fails validation rather than shipping as a permanent
deferral. The pre-existing `if (Context.GetNumErrors() > 0) return EDataValidationResult::Invalid;`
folds the new errors into an invalid asset, and config already treats validation warnings as a
failed run, so error severity is consistent.

## The eligibility consumer sweep

The contract is only as good as its consumers. Every reader of the retained values was swept. The
sweep is the deliverable, not an assumption:

| Reader | Verdict |
|---|---|
| `TerritoryHierarchy.cpp:559` (City), `:680` (District) → `SetDerivedControl` | **Gated.** Both sit behind the `!Reduction.IsComplete()` early return (`:542-555`). |
| `ATerritoryVolume::SetDerivedControl` (`TerritoryVolume.cpp:1666`) | **Deliberately not gated.** It is a *consumer*: it writes `OwningFaction`/`State`/`ControlProgress` from the `SecuredOwner` it is handed. Its only two callers are already gated, so a check here would be a second authority over "was this reduction complete". |
| `TerritoryHierarchy.cpp:147` (row owner read for an unloaded child) | **Gated upstream at `:127`** - `ResolveExpectedChildControl` refuses the child before the row is ever read. |
| `AllDistrictsOwnedBy` `:396`, `GetCityControlPercentage` `:402`, `GetMajorityOwner` `:408`, `IsCitySecured` `:417` | **Inherit by construction** - all route through `BuildExpectedSecureOwners` → `ResolveExpectedChildControl:127`. |
| `ATerritoryWorldState::GetClaimedDistrictCountForFaction` (`TerritoryWorldState.cpp:900`) | **Gated explicitly.** The one gameplay staging gate; `CountClaimedDistrictsForFaction` remains the row-shape rule beneath it. Blueprint-exposed, so the gate is on the exposed path too. |
| `UTerritorySituationProfile` `ReadPlaceHoldings` (`TerritorySituationCondition.cpp:60`, `:64`) | **Gated by its own rule** (`Children == Parent.TotalChildren`, plus a leaf check) feeding `bHoldingsKnown`. Note `:126 Report.CurrentOwner = Target.CurrentOwner` reads a **Place** row - a leaf, so direct state rather than a retained reduction - which is why it correctly needs no completeness gate. |
| UI producers (`TerritoryUIBlueprintLibrary.cpp:715`, `:721`, `:1410`, `:1429`, `:1458`) | **Presentation.** Each row now carries `bReductionComplete`, so a retained owner renders as *last known* rather than as control. |

The UI field is `bool bReductionComplete = false` (`TerritoryUIBlueprintLibrary.h:113`) - fail-closed,
so a future producer that forgets to set it under-claims rather than over-claims. It is hashed by
`HashHierarchyRow` (`:1832`), which puts it inside the Command Center revision-key blast radius; the
reflection-driven `EveryDisplayedFieldInvalidatesRevision` test probes it automatically as a result.

## The red leg

The gate was disabled in the durable reducer and the filter re-run against the rebuilt binary:

```cpp
if (UnresolvedChildren + InconsistentChildren < 0)   // RED LEG: gate disabled deliberately
```

A one-token change chosen so the break isolates the gate and nothing else: `Views`, both counters and
all production code stay as written, so the counters remain read and the build stays warning-clean.
The result was `exit=0 success=3 successWithWarnings=0 fail=2` - where the same filter is `5/5` with
the gate in place. The two failures were `IncompleteReductionPreservesVerifiedState` (13 assertions)
and `UnloadedAncestorsFollowChildSnapshots` (3 assertions).

The two assertions the plan names as this batch's evidence failed **together**, which is the defect
stated exactly - the owner is cleared *and* the tenure is fabricated:

```text
Error: Case 1: a missing child preserves the District's last verified owner: The two values are not equal.
Error: Expected 'Case 1: a missing child records no tenure on the District' to be 0, but it was 1.
Error: Expected 'Case 1: a missing child records no tenure on the City' to be 0, but it was 1.
Error: Case 1: a missing child preserves the District's last verified state: The two values are not equal.
Error: Case 1: a missing child preserves the City's last verified owner: The two values are not equal.
```

The permanence half is what `Case 3` and `Case 5` prove. With the gate disabled the fabricated tenure
survives a save/load round trip and is then indistinguishable from a real one:

```text
Error: Expected 'Case 5: no tenure is fabricated by the round trip' to be 0, but it was 1.
Error: Case 5: the preserved District state survives a save/load: The two values are not equal.
Error: Expected 'Case 3: restoring the same owner records no tenure' to be 0, but it was 1.
Error: Expected 'Case 3: restoring the same owner records no tenure on the City' to be 0, but it was 1.
```

`Case 2` failed independently, confirming upstream propagation is a separate property and not a
side effect of the case-1 gate:

```text
Error: Case 2: a missing grandchild preserves the City's last verified owner: The two values are not equal.
Error: Expected 'Case 2: a missing grandchild records no tenure on the City' to be 0, but it was 1.
```

The pre-existing test's three flipped assertions failed as well, which is the second, independent
confirmation that the flip was real rather than cosmetic:

```text
Error: Missing child snapshot preserves the last verified owner: The two values are not equal.
Error: Duplicate authored slot defers without clearing the parent: The two values are not equal.
Error: Reused tag with wrong GUID is an unknown, so it defers rather than clearing: The two values are not equal.
```

The full red log is preserved at `Saved/Tests/_redleg_B6_WorldPartition.default.log`. The deliberate
break was then reverted, verified absent (`grep -rn "RED LEG" Source/` is empty), and the binary
rebuilt.

## Verification

Assertion messages are cited rather than reported line numbers: the framework's reported line for a
`TestEqual` was off by up to +6 in claim 1's record, and the addresses shown above split between the
test file and `AutomationTest.h`.

Each filter was run under both allocators. `-Stomp` is not extra severity here - for a change whose
whole subject is a value retained across an unloaded boundary it is the substantive half, since a
read of a destroyed temporary is invisible to the default allocator (see
`FLOOR_SNAPSHOT_LIFETIME_2026-09-24.md`).

| Filter | Default | `-Stomp` | Gate disabled (red leg, default) |
|---|---|---|---|
| `TerritoryFramework.WorldPartition` | 5/5 | 5/5 | **3/5, fail=2** |
| `TerritoryFramework.Hierarchy` | 6/6 | 6/6 | not run (break does not reach this side) |
| `TerritoryFramework.UI` | 27/27 | 27/27 | not run (break does not reach this side) |

The UI filter's 27 includes the reflection-driven `EveryDisplayedFieldInvalidatesRevision`, which
picks up the new `bReductionComplete` field automatically because `HashHierarchyRow` hashes it. That
is the control proving the new UI field is inside the revision-key blast radius rather than silently
outside it.

The red leg was run on the default allocator only, deliberately: the break disables a *gate* rather
than introducing a lifetime hazard, so the failure it must produce is an assertion failure, and the
allocator that poisons freed memory has no additional purchase on it. The three stomp legs above
were run against the reverted, corrected binary.

## Test map

New and changed tests, all in the TerritoryFramework suite (the TDA game module never loads, so a
test placed there would never register):

| Test | File | Covers |
|---|---|---|
| `FTFHierarchyCompletenessContract` | `TerritoryFrameworkEditor/.../TerritoryUnloadedHierarchyTests.cpp:146` | The five cases: missing child; missing grandchild; same-owner restore; genuine Unclaimed loss recorded exactly once; save/load during incompleteness. Plus a premise control. |
| `FTFHierarchyChildTopologyValidation` | `.../TerritoryUnloadedHierarchyTests.cpp:344` | `IsDataValid` reports duplicate District tag, duplicate Place tag, null slot and untagged child - and reports nothing on clean assets. |
| `FTFHierarchyDuplicateChildSlot` | `TerritoryFramework/.../TerritoryHierarchyReconcileTests.cpp:360` | The loaded-side duplicate-tag case, previously uncovered, with a phase-2 premise control. |
| `FTFUnloadedHierarchyReconciliation` | `.../TerritoryUnloadedHierarchyTests.cpp:13` | Three assertions flipped from "clears the parent" to "defers without clearing". |

`FTFHierarchyDuplicateChildSlot` is in the runtime module rather than the editor module because
`ATerritoryCity::ReconcileDerivedControl` is private and its friend seam
(`FTFHierarchyTestAccess`, with the `SeedLoadedOwnership` save-load path) already lives there
alongside the fixture helpers. Duplicating that seam into the editor module would have been a second
way to reach the same private state.

The phase-2 premise control in that test is what makes phase 1 mean anything: without it, phase 1
would also pass on a reducer that simply never ran.

## Migration and release notes

**Spurious tenures already in saves are not repaired.** Saves written by the previous code may
already contain tenures recorded from incomplete reductions. This change stops *creating* them; it
does not remove existing ones. Repairing them would mean guessing which entries were spurious, and
that inference would itself become a second authority over history - the same class of mistake the
contract exists to prevent. Existing entries therefore persist and remain indistinguishable from
genuine ones.

This has a concrete, user-visible consequence rather than being a purely theoretical residual:
`TerritorySituationCondition.cpp:127-128` reads `FormerOwningFactions` to compute
`bPreviouslyOwned`, and `:128` turns that into `bRetakeNeeded`. A save carrying a spurious tenure can
therefore report a Place as *previously owned, retake needed* for a faction that never held it. This
is stated as a known residual, not as a fixed behaviour.

**Content with duplicate child tags will now defer and report an error.** An asset with an empty,
untagged or duplicated child slot previously "worked" because the reduction silently skipped the
slot. It now defers reconciliation and fails data validation. This is a content sweep, not a code
migration: the fix is to correct the asset. No shipped asset is known to be affected; the sweep is
the action item.

## Known limitations

- One Place-level read is intentionally ungated: `TerritorySituationCondition.cpp:126` reads the
  target Place's own row owner. A Place is a leaf, so that value is direct state rather than a
  retained reduction, and gating it would refuse valid data. A future authored child under a Place
  would invalidate that reasoning - which is why the UI view asks
  `IsHierarchyReductionComplete` rather than assuming `true` for Places, so the leaf rule has exactly
  one authority.
- The next batch (claim 4, deferred ancestor commits) re-runs `ReconcileUnloadedHierarchy` from a
  deferred drain. Its mutation rule is what this batch settles, which is why this batch precedes it.
- No PIE or two-client leg is required for this claim: the contract is server-authoritative derived
  state over replicated rows, and every assertion above is reachable headlessly. The claim-4 batch
  carries the two-client blocker for the hierarchy surface.
