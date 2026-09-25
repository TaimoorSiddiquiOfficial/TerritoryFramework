# The completeness query is the reducer's rule, not a row count

Status: **fix applied, linked, and verified red and green.** Every filter run passes under both
allocators, and the six new assertions have been observed failing against the reverted cardinality
rule.

Re-audit finding **P1**, raised against `13a7919` and confirmed against the current tree: the
hierarchy completeness query could approve a reduction the durable reducer had refused, because the
query counted rows while the reducer compared identities.

This is the follow-up to `HIERARCHY_COMPLETENESS_CONTRACT_2026-09-25.md` (claim 5). That batch
introduced `IsHierarchyReductionComplete` as the one completeness result for reduction *and* its
readers. It gave the readers a rule the reducer did not use.

## The mechanism

`ATerritoryWorldState::IsHierarchyReductionComplete` had no identity test. It found the row for the
tag, returned `true` for a missing row, returned `true` for a Place level, then counted every row
whose `ParentTerritoryTag` and `HierarchyLevel` matched and returned:

```cpp
// before, TerritoryWorldState.cpp:1058
return ResolvedChildren >= Row->TotalChildren;
```

`ReconcileUnloadedHierarchy` classified each **authored** child by exact identity - `TerritoryTag`,
`TerritoryGUID`, `ParentTerritoryTag`, `HierarchyLevel` - and deferred the commit when any authored
child failed it. The two therefore disagreed in exactly the cases where the disagreement is
harmful, and the disagreement has a direction: the count is satisfied by rows the reducer refuses.

The function's own comment justified the missing test on the grounds that "a row that satisfies this
count but not that test is refused there rather than here". That reasoning does not cover the case
that matters, because the eligibility consumer is **the query, not the reducer**.
`GetClaimedDistrictCountForFaction` filtered `Verified` on it, and that list is what grants a faction
staging eligibility. So a District whose Place row carried the right tag and the wrong GUID, or
whose row count reached `TotalChildren` by the wrong means, was read as **verified control** at the
same moment the reducer was refusing to commit it.

Three ways the count is satisfied without the authored children resolving:

1. **A row carrying the right tag and the wrong identity.** `TotalChildren` says two; two rows
   match parent and level; one of them is not the authored child. Count 2 >= 2.
2. **An obsolete row standing in for a missing authored child.** A row left behind by a reparented
   or retired definition has a valid tag, a valid GUID, this parent and this level, and no authored
   child to match. Nothing enumerates the authored children, so nothing can tell it apart.
3. **A `TotalChildren` saved before the authored child list changed.** Registration refreshes
   `TotalChildren` from the current definition, but a row restored by an in-place import keeps
   whatever the build that wrote it recorded, and nothing re-validates it against the authored list.
   One surviving authored child against a stale count of one reads complete.

## What changed

One rule for both sides, taken from the side that has the authored data.

**1. One authored child list.** A file-local helper
`GetDefinitionChildren(const UTerritoryDefinition*, TArray<const UTerritoryDefinition*>&)`
(`TerritoryWorldState.cpp:88`) is now the single place the hierarchy reads a definition's child
array. All three readers go through it: definition registration (`:635`), the durable reducer
(`:847`), and the completeness query (`:1085`). Entries come back exactly as authored, including a
null, because a null child is a topology defect that `UTerritoryDefinition::IsDataValid` reports and
the readers must see it; registration, which hashes each child, filters nulls at its own call site
rather than at the source. The reducer's inline child collection was replaced by the helper, and its
"a Place is a leaf" early return is now expressed as
`GetDefinitionHierarchyLevel(Parent) == ETerritoryHierarchyLevel::Place` (`:842`) - the same
condition as the `else return` it replaces.

**2. The query walks authored children by exact identity.** When a definition is registered for the
tag, `IsHierarchyReductionComplete` (`:1087-1122`) now requires, for every authored child, a row
whose `TerritoryTag`, `TerritoryGUID`, `ParentTerritoryTag` and `HierarchyLevel` all match, and
recurses into each child's own subtree. A child slot that is null, untagged, duplicated, or
parented elsewhere returns `false` - the same topology defect the reducer counts as
`InconsistentChildren`. The authored-leaf early return moved **inside** this branch so it cannot
pre-empt the authored walk: a row that disagrees with its authored definition about its level is a
topology defect, and the authored side is the authority.

**3. The cardinality branch survives, narrowed and documented.** It is reached only when no
authored definition is registered for the tag (`:1124-1152`), which covers a row published by a
runtime-only integration carrying no Definition (`SetCaptureSummary`'s `bDefinitionBacked=false`
path) and a row whose authored tree was never registered. On a server `RefreshStrategicDirectory()`
runs in `BeginPlay` over every `CampaignCities` tree, so a Definition-backed campaign row takes the
authored branch.

The `>=` is kept there rather than tightened to `==`, deliberately. Without authored data an extra
row cannot be told from an obsolete one, and refusing on the equality would deny a parent
permanently: the reducer has no definition for that tag either, so it cannot clean the row up and
nothing would ever resolve it. Where the reducer has no opinion there is no reduction for this query
to disagree with, and that - not the count - is the property that makes the two one rule.

## The red leg

The authored branch was disabled by binding `Definition` to `nullptr`, so the query fell through to
the cardinality rule exactly as it read before this change:

```cpp
const UTerritoryDefinition* Definition = nullptr; // RED LEG: cardinality rule, reverted on purpose
```

`TerritoryFramework.WorldPartition.Regression.CompletenessIsIdentityNotCardinality` then failed with
**exactly the six predicted assertions and no others** - the three "the query refuses it" readings
and the three eligibility readings:

```text
Error: Expected 'Case 1: the query refuses a child carrying the wrong GUID' to be false.
Error: Expected 'Case 1: a row the reducer refused is not staging eligibility' to be 0, but it was 1.
Error: Expected 'Case 2: an obsolete row does not resolve the missing authored child' to be false.
Error: Expected 'Case 2: an obsolete row grants no staging eligibility' to be 0, but it was 1.
Error: Expected 'Case 3: a saved count cannot approve a reduction the authored list denies' to be false.
Error: Expected 'Case 3: a stale count grants no staging eligibility' to be 0, but it was 1.
```

The three "the retained owner is the one the reducer kept" assertions stayed **green** under the
break, which is the agreement evidence rather than the fix evidence: the reducer deferred in all
three cases both before and after this change, and the defect was that the query did not agree with
it. `fail=1` on the test, `exit=255` on the run - the whole-test failure and the six assertion
failures are the same event.

The full red log is at
`Saved/Tests/TerritoryFramework.WorldPartition.Regression.CompletenessIsIdentityNotCardinality.default.log`
(01:36:41). The break was then reverted, the binary rebuilt, and the filter re-run green.

## Verification

Assertion messages are cited rather than reported line numbers, for the reason
`FLOOR_SNAPSHOT_LIFETIME_2026-09-24.md` gives.

Every filter was run under both allocators, one per invocation. Both allocators are reported for
each, and `-Stomp` is substantive here rather than ceremonial: the fixture drops rows out of the
replicated array and re-reads them, which is a lifetime question as much as a logic one.

| Filter | Default | `-Stomp` | Cardinality rule restored (red leg) |
|---|---|---|---|
| `TerritoryFramework.WorldPartition` (6) | PASS | PASS | 1 failing filter, 6 assertions |
| `TerritoryFramework.Hierarchy` (6) | PASS | PASS | not run |
| `TerritoryFramework.Capture` (12) | PASS | PASS | not run |
| `TerritoryFramework.UI` (27) | PASS | PASS | not run |
| `TerritoryFramework.Dialogue` (3) | PASS | PASS | not run |
| `TerritoryFramework.StateRules` (4) | PASS | PASS | not run |
| `TerritoryFramework.CounterAttack` (59) | PASS | PASS | not run |
| `TerritoryFramework.Assault` | **not run** | **not run** | - |
| `TerritoryFramework.Situation` | **not run** | **not run** | - |

`TerritoryFramework.Assault` and `TerritoryFramework.Situation` both returned
`exit=255 success=0 fail=0`, which is the runner's "no test matched this filter" result - those
namespaces do not exist. They are recorded as **unrun, invalid filter name**, never as passes. The
real namespaces are `TerritoryFramework.CounterAttack` and `TerritoryFramework.Dialogue`, both run
above; `ReadPlaceHoldings`, the one function in those files this change touches, is covered by
`TerritoryFramework.Dialogue.Behavior.SituationHoldingsAndUnknownData`.

The red leg was run on the default allocator only, deliberately: the break restores a counting rule
rather than introducing a lifetime hazard, so the failure it must produce is an assertion failure,
and the allocator that poisons freed memory has no additional purchase on it. All stomp legs above
were run against the corrected binary.

## Test map

| Test | File | Covers |
|---|---|---|
| `FTFHierarchyCompletenessIdentity` | `TerritoryFrameworkEditor/.../TerritoryUnloadedHierarchyTests.cpp:350` | The three mechanisms, each with the same three assertions (query refuses, retained owner agrees, eligibility denied) plus a per-case recovery, and a closing leg proving the refusals were deferrals rather than stalls. |
| `FTFHierarchyCompletenessContract` | `.../TerritoryUnloadedHierarchyTests.cpp:146` | Unchanged behaviour, re-run. One assertion message reworded: it named the count as the rule, which it no longer is for a Definition-backed row. |
| `FTFUnloadedHierarchyReconciliation` | `.../TerritoryUnloadedHierarchyTests.cpp:13` | Unchanged, re-run. It already carried the right-GUID case as a *reducer* assertion (`Reused tag with wrong GUID is an unknown, so it defers rather than clearing`); the new test adds the query-side assertion it lacked. |

The fixture needs mutable access to `ReplicatedCaptureSummaries` to remove a row the way streaming
out would, so `friend class FTFHierarchyCompletenessIdentity;` joins the existing friend list at
`TerritoryWorldState.h:597` - the same seam the two neighbouring hierarchy tests already use.

## Migration and release notes

**No serialized field changed.** Nothing was added to `FReplicatedCaptureSummary`, and this batch
writes no save data. The behaviour change is confined to what the query reports.

**The change only narrows eligibility.** A row set that previously read complete under the count and
still reads complete under the authored walk is unaffected, so no legitimate content loses staging
eligibility. What changes is that eligibility is now denied in the three malformed-row-set cases
above - and in each of them the reducer was already deferring, so the query was disagreeing with the
state the rest of the system was acting on. No content sweep is implied: a row set reaches these
cases through a reused tag, an orphaned row or a stale saved count, not through correct authoring.

## Known limitations

- **A report-only reader still counts.** `UTerritorySituationProfile::ReadPlaceHoldings`
  (`TerritorySituationCondition.cpp:67`) requires `Children == Parent.TotalChildren`, which counts
  rows by parent without an identity test and so has the same blind spot: an obsolete row can
  satisfy the equality while an authored child is missing, and the report then names the obsolete
  Place instead of the authored one in `AvailablePlaces` and `FactionSharePercent`. This is recorded
  rather than repaired. It is a `BlueprintPure` report (`InspectSituation`) with no ownership,
  eligibility or control effect, and `ReadPlaceHoldings` is a static helper handed rows only - it
  has no definition to walk, so the authored rule is not available to it without a signature and
  contract change. The blind spot is now named at the site so the next reader finds it from the
  code. The claim-5 record's verdict for this reader ("gated by its own rule") remains true; it is
  the strictness of that rule, not its presence, that this note refines.
- **A row whose authored tree was never registered keeps the weaker reading.** The narrowing is by
  design, and this is the residual: such a row is read by count. On a server the window is closed by
  `BeginPlay`, but a client, a test fixture, or a WorldState that never ran `BeginPlay` is not
  covered by the authored branch. The comment on the branch states this rather than implying the
  authored rule always applies.
- **The two-client leg is not in scope.** This is server-authoritative derived state over
  replicated rows; every assertion above is reachable headlessly, and no replication path changed.
  The hierarchy surface's two-client blocker belongs to the claim-4 batch and is unchanged by this
  one.
