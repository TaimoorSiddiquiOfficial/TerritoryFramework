# Audit repair ledger — 2026-10-05

Baseline: `d7553f0ebf22c636bb6289d3570ab843f047e3f1`, Narrative Pro 2.4.2,
UE 5.8.3. Previous automation: 515 success, one Farm authoring failure.

The approved repair targets both TDA and independent Native hosts. Current
autonomous/player-gated assault profiles are preserved; Farm starts Locked;
currency rates must survive streaming. Only the two confirmed Native save defects
have a source-change exception. Existing authorities and durable IDs stay intact.

## Status

| Batch | Status | Evidence |
|---|---|---|
| A: save isolation/dependency | Focused regressions pass; broader gates pending | Unpatched: 2 fail, 0 crashes. Patched: 2 pass, exit 0. Explicit Native module rebuild. Tool apply/refusal/rollback checks pass. |
| B: guards/reserves/AI lifecycle | Focused suites pass; composed multiplayer gate pending | Guards 56 pass, 0 failures/crashes. Post-load perception 1 pass. Actor recreation, both load orders, remaining delay and explicit zeros exercised. |
| C: capture/public queries | Focused regressions pass; network gate pending | Baseline regression failed; repaired Capture suite 15 pass. Rejected recovery remains retriable and client unknown counts are explicit. |
| D: durable currency | Focused regressions pass; cold/network gate pending | Baseline unload changed 50/6 rates to zero. Repaired Economy 9 pass and Production 12 pass. Legacy Native record migration and callback restore tested. |
| E: assaults/treaties/movement | Focused regressions pass; composed gameplay gate pending | Baseline 3 failures reproduced. Repaired CounterAttack 62 pass; latest full isolated host 524 pass, 0 fail, 0 crashes. Includes actual accepted engine path without physical movement, finite withdrawal, final-condition races and saved force estimates. |
| F: Farm/authoring/docs | Full TDA suite passes; multiplayer observation still pending | TDA Farm was Unlocked; corrected authoring seed, both assets validate (2 valid, 0 warnings). Native owner/prerequisite/idempotence/save regression passes. Full TDA: 525 pass, 0 failures/crashes. Nested ownership and stale-inventory regressions pass; 844 editable options match fresh source. |
| G: performance/release | Implementation and verification in progress | 500-row/two-client-world cache regression failed before repair and passes after. Bounded runner rejects zero, partial, failure, crash and timeout. Async readiness and fresh release-host gates under verification. |

An initial TDA build could not link while the editor was open; later fresh builds
and the full 525-test TDA suite passed after its clean shutdown. An isolated host
contains a licensed dependency copy solely for verification; it is not a release
artifact. Pre-migration saves are backed up outside the plugin.

The isolated host's full count includes tests that skip TDA-only story/content
assertions. It is not evidence that TDA content or multiplayer release gates passed.
TDA's old editor was cleanly closed after two read-only checks confirmed no dirty
packages and stopped PIE. Fresh Runtime, Editor, UHT and explicitly patched Native
SaveSystem compilation passed. Full TDA verification uses those binaries.

Batch A exposed an additional failure in UE's general loader: a four-byte corrupt
file entered the ancient unversioned-name fallback and asserted. The narrow Native
correction now uses a bounded decoder for the existing UE5 GVAS container and
configured Native save class. No slot or durable authority was added. Both targeted
tests passed after the correction; this does not certify the composed network run.

The first player fixture lacked Native's required stable-GUID implementation and
crashed before the intended assertion. That fixture was corrected; its crash is
not counted as a product finding. The subsequent unpatched run reproduced both
intended defects without a crash.

## Completion gates

Runtime/Editor/UHT, automation, stopped-PIE data validation, included Blueprint
compilation, dedicated/listen server with two clients, cold save restart, physical
World Partition streaming, late join, Win64 cooking, UE 5.7 and 5.8 release hosts.
The composed scenario must pass three times without reseeding or force-success.
Unperformed or blocked gates remain unresolved; no release upload is authorized.

## Latest verification checkpoint

- TDA full automation: 526 passed, zero failures/crashes, after the streamed
  ancestor-readiness repair. Guard subset: 56 passed. The before-repair composed
  ancestor-last regression failed in all eight added combinations (0 vs 2 guards).
- Diagnostic fresh UE 5.8 Native host: Editor non-unity build and 526 automation
  tests passed. Game Development build exposed two missing direct includes, then
  passed after their correction. This diagnostic host was modified after its
  snapshot; it is not a final release receipt.
- Fresh-host content inspection: 123 assets, 79 Blueprint compilations, no invalid
  assets, no compile errors, no external dependency errors. The first manifest
  check failed because an already-tracked ranged-guard example was omitted from
  the manifest; the manifest now includes it and needs fresh release verification.
  Two warnings identify intentional prototype mannequin appearances in example
  owner NPCs, not certified cinematic/MetaHuman visuals.
- Controlled TDA listen-server/two-client scenario passed capture, recruitment,
  finite physical assault, casualty, Native save, cold process restart, late join
  and defeat replication. It starts the existing quest at a capture checkpoint;
  it is not a complete normal-input playthrough or three clean composed runs.
- The original TDA test map is not World Partition. A separate temporary converted
  copy exposed the ancestor-readiness defect; original map assets were not changed.
- UE 5.7, final fresh receipts, physical streaming composition, dedicated server,
  cook/package and measured performance gates remain pending. No public upload.

## Physical streaming corrections and proof

The converted TDA test map exposed two additional composed defects: late
ancestor registration omitted the initial garrison, and the WorldState actor
itself was spatially loaded. Network `AlwaysRelevant` did not keep its directory
resident. The latter changed known currency rates from 600/350 to 0/0 when all
territory cells unloaded. Both failures were recorded before correction.

Current TDA source passes 527 automation tests, zero failures/crashes. A first
rate-observation test failed because its fixture replaced GameInstance after
subsystem initialization and ticked a different timer manager; the same natural
observation assertion now runs in the existing streaming fixture without that
replacement. The payout/transaction assertion remains in place.

Physical World Partition smoke now passes with listen server plus two clients:
all two Places and seven posts unloaded, WorldState remained present, currency
rates stayed 600 income / 350 upkeep, and reload retained owner, Territory GUID,
seven guards and seven reserves. No manual actor recreation or rate recalculation
was used. Plugin and TDA WorldState Blueprint defaults were resaved, and the
temporary map's external WorldState actor descriptor was resaved with the same
GUID. Original TDA map geometry was not changed.

The UE 5.8 G3 diagnostic snapshot passed Editor, Game Development and Shipping
compilation. Its first Shipping launch conflicted with a queued verification
build; the final release driver now waits on the UBT mutex. G3 predates these
latest residency/rate changes, so its results cannot certify the current source.
Exact-source resumes now avoid copying matching file hashes again.

## Current checkpoint after compatibility repair (supersedes counts above)

- Main TDA and isolated UE 5.7 hosts: **528 passed, zero failures/crashes**, using
  the repaired source and backported content. The latest registration test checks
  Native primary-asset identity, including rejection of an unregistered NPC copy.
- Seventeen UE 5.8-saved assets were properly reconstructed and cold-reloaded in
  UE 5.7. Semantic comparison found zero unexpected differences; see the separate
  content-backport report, including one inherited localization-key migration.
  UE 5.7 validation: 123 assets, 79 Blueprints, zero errors, two appearance warnings.
- Three clean TDA composed runs passed defeat, treaty cancellation and physical
  recapture. They include Native player wallets, finite casualties, a process
  restart, natural reserve-timer recovery, real World Partition unload/reload and
  late join. These runs precede the content backport; current automation and the
  graph-semantic comparison are separate evidence, not a substitute for rerunning
  a changed gameplay scenario.
- Real driving exposed overlap-only triggers incorrectly blocking the vehicle
  probe. A before/after regression now verifies that overlaps are ignored while
  a blocking obstacle behind them is still detected. A second reproduction fixed
  withdrawal of living attackers already occupying the target when a deeper
  authored waypoint was off navigation. Physical TDA recapture then succeeded.
- Fresh Native setup exposed missing primary-asset scan registration. The host
  builder now extends Native's existing scan types and registers the template's
  missing damage tag/road class. No additional vendor source was modified.
- Fresh Native composed runs, dedicated-server, Win64 cooked-game and measured
  performance gates are still in progress at this checkpoint. Earlier package
  receipts identify older snapshots and do not certify these latest inputs.
- The standalone example quest differs from TDA's named post-capture story wave.
  Its acceptance fixture observes ordinary autonomous scheduling without changing
  launch probabilities, grace periods, finite budgets or currency settlement.

The pre-migration save/content/dependency backups remain outside release payloads.
No public upload or push has been performed. Final evidence must name the exact
snapshot and may not convert an unperformed gate into a passing result.

## Current checkpoint after measured transport and arrival repairs

- Latest TDA Editor/UHT build and full automation: **530 passed, zero failures**.
  A 500-row socket test exposed a 94,568-byte WorldState bunch exceeding the
  engine's 65,536-byte limit. Transaction history now uses a bounded lossless
  snapshot and explicit client readiness; see the history-transport report.
  Native inventory and saved ledger remain the authorities. The actual cold/warm
  multiplayer performance/late-join test is still in progress.
- An added movement regression failed on the previous watchdog: a reinforcement
  at a destination accepted by Native MoveTo was withdrawn by a smaller 3D test.
  The repair uses the same path-following arrival predicate. The full 530-test run
  includes that regression and existing blocked-route/finite-withdrawal checks.
- UE 5.8 G6 release pipeline passed Editor, Development, Shipping, 528 tests and
  content validation (123 assets / 79 Blueprints / zero errors / two appearance
  warnings), but predates the two repairs above and is historical evidence only.
- A real dedicated-server/two-client TDA scenario passed save, cold restart,
  natural reserves, physical streaming, late join and treaty cancellation before
  the latest transport change. It does not certify the latest network snapshot.
- Fresh UE 5.7 setup exposed CRLF handling in template configuration extraction.
  The corrected script has a failing-before/passing-after LF/CRLF regression.
  A fresh current-source release build is still required on both engine versions.
- Fresh Native composed acceptance, current-source release receipts, Win64
  cook/package and measured performance remain open. No public release is ready.
