# Assault appearance readiness and AlMalik verification

## Confirmed readiness defect

Territory's readiness query checked for a visual actor and individual asset-load
handles. Narrative also waits for the base appearance to finish and for the
character-definition load handle to clear. A newly created visual with no active
mesh handles could therefore pass Territory's check too early.

The existing `IsNarrativeSpawnReady` query now uses Narrative's
`IsCharacterPendingLoad` after checking its definition, controller and activity.
No new loading system is added. Unarmed NPCs remain valid. The existing bounded
initialization timeout remains in effect. Its error now identifies the definition,
visual, base appearance, pending visual loads, controller and activity component.

`WaitForNativeBaseAppearance` exercises the real Native visual before and after
base initialization, missing definition/visual failures, an unarmed ready NPC,
and side-effect-free client queries. The passenger recovery fixture explicitly
marks its simulated base appearance complete, matching that test's stated setup.

## Ownership and compatibility

Narrative owns definition loading, appearance creation, NPC spawning, activities
and death. Territory's counterattack subsystem owns the finite force ledger;
its participant adapter waits for Native readiness before starting a goal.
This is server AI readiness: remote clients normally have no NPC AI controller.
They receive the visual and assault read models through existing replication.
Capture participation remains in the existing control subsystem. WorldState
persists and replicates the existing assault records.

The source trace includes owner change, grace/evaluation, warning or explicit
immediate activation, physical deployment, registration, death/withdrawal,
capture/defeat, and WorldState save/replication. Autonomous attacks and immediate
story waves remain supported. No save schema, replicated property, RPC, faction
identity, Blueprint signature or migration changes are introduced.

## Live findings and limits

Evidence lives in the TDA host under
`Saved/Verification/20260910_AssaultVisual`.

Both UE 5.7 and UE 5.8 pass all 307 automation tests with zero failures or skipped
tests. The new readiness regression passes without warnings. The original red
test reproduced the premature-ready result; its Native-query assertion also
needed editor script execution enabled in the fixture before the final run.
All 741 Narrative Pro source files still match the installed vendor.

Editor, Development and Shipping builds pass on both engines. TDA asset
validation checks 244 assets and compiles 147 Blueprints with zero errors and
eight existing appearance/dialogue-camera warnings.
The UE 5.8 cook, stage and package pass, followed by a 60-second Development Game
server-mode startup with exit zero. The existing optional cutscene-player warning
remains. This is not a compiled TDAServer or an AlMalik gameplay certification.

The missing-visual failure reproduced after a fresh UE 5.8 editor restart. Both
waves withdrew at the existing initialization limit. All four attackers had a
definition, controller and activity, but no visual and no completed base
appearance. The project Blueprint has no definition/appearance event override,
and its NPC definition resolves to the expected primary asset. Narrative's asset
load trace left the assault activity and Manny appearance in progress. Other
Native driver appearances also showed pending loads.

A diagnostic blocking load of Manny completed in 16 milliseconds. After that
load, a new assault initialized and passed the streaming check below. This is
evidence of a load-progress problem, not proof of its cause. No synchronous-load
workaround or longer timeout was added. Normal async budgets were present; a
later verbose trace did not establish garbage-collection starvation. Temporary
console changes were restored. The cold-load problem remains a release blocker.

With the appearance already loaded, the capture-enabled AlMalik fixture passed target unload, a Native save while the
target was absent, and target return. Two living attackers, two pending reserves,
the original AssaultID and decision roll survived. Capture participation stopped
while the target was missing and resumed after return. Two guards returned on
the server and both clients.

An actual Native Load also restored ready survivors. The first death-test attempt
left the fixture's invulnerability effect active; Native's Instakill applies
damage and does not bypass that protection. That attempt is retained as a failed
harness result, not evidence of broken casualty accounting.

The corrected harness removed its invulnerability effect before applying Native
damage. The editor crashed after the first restored wave died, before the final
defeat checks could complete. The saved minidump is
`UECC-Windows-7702552C4D6096707EAB16B71F585858_0000`. Its stack includes behavior-tree
decorator evaluation and execution processing. Engine PDBs were unavailable;
nearest-export names are approximate and do not establish the responsible NPC,
node, or root cause. The full post-load defeat test is failed, not passed.

The late-join test is not passed. One original client was disconnected with
`MissingLevelPackage` for a temporary `/Memory/L_AlMalik_...` streaming package.
The new client received two visible attackers, but that partial result cannot
certify the whole topology. Recheck with a persisted isolated World Partition
fixture; do not bypass Unreal's level-package validation in gameplay code.

## Reproduction and next action

Use a fresh editor process on AlMalik. The retained `setup_fixture.py` and
`prepare_nav_layers.py` describe the temporary Place, independent guard-post
layers and route. Start a server with two clients and run `capture_assault.py`
before reading or loading the appearance asset. Preserve the load log and verify
real visuals and capture registration, not just the finite ledger.

`CaptureAssaultColdFailure.json` and `VisualTraceColdFailure.json` retain the cold
failure. `CaptureAssault.json` is the later pass after diagnostic asset loading.
`LoadAndDefeat_InvulnerableHarness.json` is the earlier harness failure; it must
not be mistaken for the subsequent native crash. The test assets and save files
are retained in the verification directory for reproduction, outside game
content and normal save slots. Original map hashes remain unchanged.
The editor is restored to HopDistrictTest with PIE stopped, one client selected
and background CPU throttling enabled.

Next, isolate native appearance loading on the same map and reproduce the
post-load behavior-tree crash with engine debug symbols. Then rerun finite
defeat and use a persisted isolated World Partition map for the returning-client
gate. The readiness correction does not close any of these three blockers.
