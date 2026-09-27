# Hierarchy identity re-audit — 2026-09-27

A loaded District could consume an unloaded Place row by GameplayTag alone, although the unloaded reducer required the authored GUID, parent and hierarchy level. The child's completeness query did not protect this boundary: an authored leaf has no descendants to be incomplete. A loaded child actor was likewise selected without checking its authored GUID. This made campaign control depend on streaming residency after an identity mismatch.

The shared non-reflected `TerritoryHierarchyPolicy::MatchesAuthoredChildIdentity` now checks authored tag, valid stable GUID, parent and hierarchy level for loaded actors, unloaded-row resolution, durable reduction and completeness queries. Loaded child enumeration and parent lookup refuse a wrong-GUID actor. An exact durable row may still describe the authored child while an unrelated actor is rejected.

Reconciliation also exposed a second path: publishing the unchanged parent recursively registered definitions and replaced a conflicting row's GUID while preserving its political state. Definition registration now leaves a different valid GUID unresolved. Exact actor publication or explicit migration must replace that row. GUID-less legacy rows can still acquire their first authored identity; renaming a tag while retaining its GUID still refreshes the directory. Authored parent/display metadata for the same GUID continues to refresh normally.

## Authority and integration

`ATerritoryVolume` remains the loaded owner/state authority; the existing hierarchy reducer commits aggregate control, and `ATerritoryWorldState` retains the unloaded read model. No Narrative Pro code changed. Narrative's existing `UNarrativeSaveSubsystem::CreateActorRecord` and `LoadActorFromRecord` provide the regression's actual save/restore path, including `INarrativeSavableActor` callbacks. There are no reflected property, serialized layout, Blueprint node, RPC or asset changes in this batch.

Blueprint pure hierarchy/parent queries become stricter for mismatched identity. A campaign that previously relied on a reused tag silently transferring old political state now remains unresolved until identity is corrected or explicitly migrated. That is intentional; a new GUID denotes a different durable territory.

## Added behavioral regressions

- `TerritoryFramework.WorldPartition.Regression.LoadedParentRequiresAuthoredChildIdentity`: a loaded District refuses a wrong-GUID unloaded row; retains verified owner/history; emits no Narrative loss; retains the unresolved identity through real Narrative save/restore; rejects wrong parent/level rows through public queries; refuses a loaded wrong-GUID Place; recovers on exact actor publication; rejects client snapshot mutation.
- `TerritoryFramework.WorldPartition.Regression.IdentityRefreshPreservesGuidRenames`: GUID-less legacy adoption and same-GUID tag rename preserve ownership and do not retain an obsolete directory row.

These join the existing unloaded-ancestor, hierarchy completeness, transition ordering and identity/cardinality regressions. The new tests deliberately exercise actor behavior and Narrative serialization, rather than only the shared predicate.

## Validation status

Implementation and source review are complete. The parent task rebuilt Editor and Win64 Shipping on UE 5.8.3 and passed the 501-test native suite, including these identity regressions. Current dedicated PIE verified authored garrison/floor transport to two clients and a late joiner; it did not test malformed hierarchy identity. These native fixtures model loaded/unloaded actors but do not start a packaged server or an actual World Partition streaming session. Exact hierarchy completeness parity remains a separate release gate because clients still have a weaker count fallback.

Callback-driven save invalidation and City event result gating are separate findings, not addressed by this identity batch.

## First full-suite follow-up

The parent ran the rebuilt 500-test suite: 497 passed and three failed. The newly added identity and callback regressions passed. Two pre-existing test fixtures relied on behavior this repair intentionally rejects:

- `ReserveTotalsRemainMonotonic` applied Definition identity, then assigned different hardcoded GUIDs to the loaded actors. Its fixed fixture authors those GUIDs on the Definitions before application and asserts the applied identities match. Reserve counts and monotonicity expectations are unchanged.
- `UnloadedAncestorsFollowChildSnapshots` injected a wrong GUID and relied on definition refresh to silently overwrite that identity. It now explicitly asserts refresh preserves the mismatch, then publishes the previously saved exact row before continuing its independent streaming scenarios.

These are fixture corrections, with no relaxation of production identity checks. Both passed in the subsequent 501/501 suite. The third failure was the previously observed Blacksmith approach navigation route; its final test follows the actual RoadGuide and the Editor validator now resolves authored defence objectives before BeginPlay. The map integration gate passed in that suite.
