# Complete TerritoryFramework re-audit — 2026-09-05

Status: **in progress**. Findings below distinguish confirmed defects from candidates.

Baseline: host `d3db8b7`, plugin `79af71e`, UE 5.7. Work is isolated on
`hoptrendy/territory-complete-audit` in both repositories. The prior 213-test build,
Blueprint validation and cook are baseline evidence, not proof of the new changes.

## Coverage ledger

| Area | Current coverage |
|---|---|
| Inventory | 255 C++ headers/implementations, 84,644 lines including tests |
| Stub/unsafe context sweep | No production TODO/FIXME/stub markers or gameplay first-player lookup found |
| Management requests | Nine server RPC paths inspected; count/sequence/cooldown checks traced into mutations |
| Economy/production | Currency routing, finite arithmetic, recipe transactions, checkpoints and callback boundaries under review |
| Property benefits | Definition payload, runtime GAS grant/revoke and existing tests inspected; regressions being added |
| Capture and hierarchy | Authoritative state transitions, availability and upgrade paths under review |
| Guards and assault | Full lifecycle trace pending; garrison purchase/deployment entry points inspected |
| Save/replication/streaming | WorldState and restoration review pending |
| AI/stealth/Tales/navigation | Detailed review pending |
| UI/editor/CI/packaging | Detailed review pending |

## Findings register

| ID | Finding | Status |
|---|---|---|
| ECON-01 | Production multiplies a valid int64 per-cycle amount by an int32 batch count before checking the result; overflow can wrap to an accepted small quantity. | Fixed; red/green behavioral regression |
| ECON-02 | A zero-cost Property upgrade passes affordability but is rejected by the positive-only currency debit API. | Fixed; red/green behavioral regression |
| ECON-03 | Property income performs upgrade multiplication and final addition in int32, and converts an unchecked capital multiplier to int32. | Fixed; red/green behavioral regression |
| GAS-01 | Property benefits retain ability/effect/tag handles after external removal and do not reconcile them while ownership stays unchanged. | Fixed; real Narrative GAS removal/regrant test |
| GAS-02 | Runtime Property benefits apply Instant effects despite authoring validation rejecting them, so invalid content can repeat permanent modifiers on every refresh. | Fixed; red build applied twice, green applies zero times |
| GAS-03 | Vehicle possession replaces the management pawn even though Narrative retains the player character and its GAS/inventory identity. | Fixed; retained-character/PlayerState fixture passes |
| CAP-01 | Capture registration, progress updates and ticking retain map references/iterators across state callbacks. | Fixed; state callback cancellation and full suite pass |
| CAP-02 | Stealth evidence callbacks can remove an infiltrator, after which stale runtime state still exposes/registers it; decay callbacks also mutate an iterated map. | Fixed; red build emitted stale exposure and restored a contester; green withdrawal/decay tests pass |
| DIP-01 | Invalid/self diplomacy requests and superseded transitions can record successful events despite a rejected/finally different state. | Fixed; red/green callback and invalid-pair tests |
| DIP-02 | Reputation increments can overflow int32. | Fixed; both int32 boundaries tested |
| DIP-03 | Timed trade agreements created without the Narrative clock never receive a usable expiry. | Fixed; reject missing clock/nonfinite timing |
| DIP-04 | Duplicate reversed treaty pairs survive restore and make the chosen state depend on array order. | Fixed in subsystem; newest signed row and stable tie-breaks, expiry preserved; WorldState cache review ongoing |
| AUTH-01 | Upgrade and garrison staffing eligibility do not consistently enforce availability or registered world identity. | Candidate; caller/authority audit ongoing |
| ATOMIC-01 | Upgrade, garrison purchase and production hold mutable state across Narrative inventory or Territory delegate callbacks. | Candidate; re-entry/failure reproductions pending |
| ECON-04 | Member income split can lose a remainder when later accounts are full although earlier accounts have room; ceiling division can overflow at MAX_int32. | Fixed; full/partial/reversed capacity and maximum payout tests use real Narrative inventories |
| ECON-05 | Controller currency requests lose the retained player identity during vehicle possession; debit can claim success when the inventory's owner lacks authority. | Fixed; vehicle, role rejection and Narrative inventory load tests |
| SAVE-01 | WorldState district counting filters duplicate tags but misses duplicate valid GUIDs when both rows also have tags. | Confirmed; regression pending |
| SAVE-02 | Negative history limits can cause invalid removals in Economy restore/tick and WorldState transactions; WorldState RecordTransaction dereferences a missing world. | Confirmed; boundary tests pending |
| SAVE-03 | Negative retained-assault limits can remove the final row and then index the empty array. | Confirmed; full assault lifecycle audit required before changing this path |

## Architecture constraints

Territory actors remain state authority; Control owns capture; Economy owns rates and settlement;
Narrative inventory owns actual balances/items; Narrative GAS owns grants/effects; WorldState owns
durable global snapshots. No vendor source/assets will be modified. Each fixed batch must record
its behavioral tests, save/replication effects, migration requirements and remaining runtime gates.

## Verified batches

- **Batch 1:** Editor build/UHT passed. Full suite: 215 tests, 209 clean, six expected-warning
  tests, zero failures/skips. The pre-fix build reproduced a wrapped 16-item production order,
  a rejected free upgrade, and income of -2. Evidence: `Saved/Verification/20260905_DeepAudit/Batch1_*`.
- **Batches 2–3:** Editor build/UHT passed. Full suite: 217 tests, 211 clean, six expected-warning
  tests, zero failures/skips. Added real GAS grant/revoke, external removal, nested refresh,
  vehicle identity, load reconciliation, client-authority rejection, state callback withdrawal,
  stealth withdrawal and decay callback tests. Evidence: `Batch3_RedTests` and `Batch3_Tests`.
  Two preliminary vehicle fixtures hit Narrative's intentionally unimplemented native stable-GUID
  contract; corrected to unspawned native identity fixtures. No Narrative changes were made.
- **Effects:** No save schema, replicated property or Blueprint signature changes. GAS still owns
  grant replication; handles remain transient and are reconciled against restored state. Capture
  remains Control-owned and Volume snapshots remain the replicated/persistent authority. Invalid
  cross-world capture targets are rejected. Game build, package/cook, live multiplayer and streaming
  release gates have not yet been rerun for this audit.
- **Batches 4–5:** Editor build/UHT passed. Full suite: 220 tests, 214 clean, six expected-warning
  tests, zero failures/skips. Red/green evidence covers diplomacy event truthfulness, signed
  reputation bounds, duplicate treaty restoration, timed-treaty startup rejection, member payout
  capacity/overflow, vehicle currency and inventory authority. Narrative inventory prepare/load
  preserves the settled balance. Evidence: `Batch4_*`, `Batch5_*` in the same verification folder.
  Treaty migration has no schema change: valid rows are sorted by canonical faction pair, newest
  signed time, then state/permanence/expiry tie-breaks; invalid/self/None/nonfinite rows are ignored.
  Development Game build also passed (`Batch5_GameBuild.log`, 38 seconds).

## Pending audit follow-up

- Complete the upgrade/garrison/production callback transaction review; existing debit callbacks
  occur before final gameplay commits. No atomicity completion is claimed for those paths yet.
- Reconcile migrated treaties back into the server WorldState replicated cache. Audit all save
  and streaming identity boundaries, including duplicate district GUIDs and negative history caps.
- Registry/spatial review found unbounded 3D cell enumeration for enormous bounds, repeated
  registration broadcasts, and `UpdateTerritoryBounds` accepting unregistered actors. Reproduction
  tests and minimum compatible fixes are pending. Client movement/reindex behavior needs validation.
- Trace the full physical assault lifecycle before editing counterattack code, then complete
  guard/AI/Tales/navigation/UI/editor review and the live release gates.
