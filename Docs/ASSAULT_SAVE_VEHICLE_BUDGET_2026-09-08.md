# Batch 45: saved vehicle budgets and Native spawn-time saves

Focused continuation of the complete audit. Host evidence directory:
`Saved/Verification/20260908_AssaultSaveBudgetAudit/`.

## Confirmed defect and minimal fix

Started from plugin `65611b6`, host `93127a05`, on
`hoptrendy/territory-complete-audit`. Relevant CounterAttackSubsystem,
AssaultPersistence, WorldState and Narrative character/save source was inspected
before implementation. Existing user maps, dialogue, owner and audio changes are
outside this batch.

`RestorePersistentState` accepted negative saved maximum vehicle deployments,
total used deployments and per-approach used counts. A negative spent count can
pass the fresh-car admission comparison and grant additional deployment credit.
Clamping spent usage to zero alone would also replenish that credit.

The server now cancels an invalid nonterminal record with
`ConfigurationInvalid` before physical checkpoint normalization or reconstruction.
Recorded deaths remain consumed; all remaining planned force becomes withdrawn
once, with zero living or reserve force. Negative displayed vehicle values are
then clamped to zero. Existing terminal outcomes, valid budgets, difficulty,
decision seed and roll remain unchanged. This is bounded malformed-save handling,
not a new planner or an ownership change.

## Authorities and lifecycle

CounterAttackSubsystem remains the scheduling and finite-force authority;
WorldState owns Narrative save integration and client snapshots. Narrative owns
NPC creation, durable actor records and combat. Volume/ControlSubsystem still own
territory state and capture. The lifecycle traced in
[batch 41](CASUALTY_DRIVER_LOSS_2026-09-07.md#authorities-and-full-lifecycle-trace)
was checked against the current schedule, activation, registration, casualty,
resolution and restore call sites. User-selected autonomous activation and
explicit immediate story waves are preserved.

WorldState's existing server import republishes the normalized assault records;
clients receive that read model. No save schema, replicated field, Blueprint
gameplay API, gameplay tag or durable GUID authority changed. Valid saves require
no migration. Invalid nonterminal saves now cancel rather than continue with
negative car credit. No target lookup is required for this rejection, so it also
applies before World Partition actors become available. Physical city streaming
is still a separate unverified gate.

## Spawn-time save false positive

A save inside Narrative's `OnNPCSpawned` callback observes the previous committed
admission: the constructing on-foot NPC remains unspent reserve. The actual
Narrative `UpdateSaveObject` regression confirms that an earlier committed NPC
appears once in the physical roster and WorldState snapshot. Narrative may record
the constructing actor, but its existing `ShouldRespawn` implementation returns
false; it cannot independently respawn beside the finite reserve.

After the callback, admission commits both NPCs once. Restoring the saved boundary
preserves the two-person budget and the committed survivor GUID. No production
spawn change was justified by this suspected issue. The first test fixture lacked
a stable WorldState GUID and was corrected to use the existing save interface;
that failure was not a framework defect.

## Regression coverage

`TerritoryFramework.CounterAttack.SaveLoad.NegativeVehicleUsageCannotGrantDeployments`
serializes actual SaveGame structs for -1 and MIN_int32 in all three vehicle
fields. It verifies invalid-configuration cancellation, preserved deaths and
decision, finite withdrawal, reconstruction refusal, published terminal state,
repeat-load idempotence and unchanged valid budgets. The retained red report
reproduces Active records and unspent reserve before the fix. This test proves
load rejection; it does not spawn an excess pair of physical cars.

`TerritoryFramework.CounterAttack.Regression.NarrativeSpawnRestoreCallbacks`
now includes the actual whole-world Narrative save callback described above.
The editor-only `TerritoryAuditEventProbe` can inject negative used-car credit
into one PIE save record for live load testing. It rejects non-PIE worlds and is
excluded from the runtime module and packaged game.

## Build, validation and live multiplayer evidence

Editor/runtime/UHT and Development Game builds pass. After adding the PIE fixture,
`Build_PIEFixture.log` passes and `Automation_All_Final.json` reports **282/282
passing tests**, zero failed/skipped. `AssetValidation_Batch45.json` records
**77 Blueprint compilations and 128 assets, zero errors/invalid assets and four
existing presentation warnings**.

`NegativeBudget_PIE.json` records the successful 18.8-second listen-server and
two-client test through Native `PrepareForSave` and `Load` interfaces. An active
eight-person assault had four living attackers, four reserves and one charged
car. A client load with injected negative usage cannot change the authoritative
or client-visible record. The server load cancels it as invalid configuration;
all three worlds agree on zero living/reserve/killed and eight withdrawn, with
the same seed and roll. Physical assault NPCs are removed from all three worlds.
A second server prepare/load produces identical records. All six checks pass.
This is transient actor load testing, combined with the native SaveGame archive
regression; it does not alter an on-disk campaign save.

Earlier unsuccessful harness attempts are retained: Python cannot read the
protected SavedAssaults field, and a console-based injection did not change it.
They are not counted as passing verification. A Monolith dry-run lookup also
resolved the map class rather than the actor; the final fixture uses a typed
WorldState actor and checks PIE world type directly. A dirty editor map was
discarded on clean shutdown; both map disk hashes and the user's handover/owner
asset hashes match their starting values. The final successful gameplay window
contains no fatal, assertion, ensure, Blueprint runtime error, Accessed None or
Territory Error-severity entry. All 741 local Narrative source files still match
the installed Fab package.

## Package and final editor state

`CookStage.log` records a successful HopDistrictTest dependency cook and distinct
`Stage_Batch45` package: exit zero, zero cook errors and 30 existing warnings.
The five modular retake assets are explicitly included through the `-PACKAGE`
list. `StagedBinaryHash.json` confirms the packaged executable matches the
verified Development Game build.

`PackagedSmoke_Process.json` records a 90-second localhost Game server-mode
assault smoke, exit zero. The authored vehicle departs, follows its road pursuit
and starts Native dismount; combat and deaths occur. The complete log has zero
Error-severity entries, fatal, assertion, ensure, Blueprint runtime error or
Accessed None. This smoke does not assert terminal force totals and does not
replace a compiled TDAServer test.

The editor is restored to HopDistrictTest, no PIE worlds, play-client setting one
and no dirty packages. The user's disk assets remain unchanged.

## Remaining scope

Negative vehicle counts are addressed. Oversized positive budgets, duplicate
per-approach records and other malformed-save arithmetic still need review.
The earlier unsymbolized AIModule crash, separate editor shutdown crash,
physical AlMalik fixture, production/refund settlement, city content/Chaos
issues, interior/HDR visual review and compiled TDAServer gate remain open.
See the [current roadmap](ROADMAP_AND_REMAINING.md).
