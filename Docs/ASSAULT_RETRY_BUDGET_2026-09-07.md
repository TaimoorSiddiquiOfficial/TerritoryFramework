# Assault retry budget and clustered casualty audit — 2026-09-07

Batch 43 follows the [conditional dialogue batch](CONDITIONAL_RETAKE_DIALOGUE_2026-09-07.md).
Evidence is under `Saved/Verification/20260907_AssaultCallbackAudit` in TDA.

## Confirmed defect and change

A saved `ConsecutiveSpawnFailures` value of `MAX_int32` passed restore's
nonnegative normalization. The next failed deployment incremented it to
`MIN_int32`, bypassed `MaxConsecutiveSpawnFailures`, and left the assault active
with undeployable finite force. This is a malformed-save boundary defect; it does
not explain an ordinary car waiting behind another car.

The existing `UTerritoryCounterAttackSubsystem` now owns one private saturating
increment, used at all four failure sites: new-wave deployment, physical survivor
reconstruction, legacy vehicle reconstruction and missing legacy approaches.
Ordinary failures still consume one retry. A successful spawn still resets the
consecutive count. Exhaustion uses the existing `ResolveAssault` path to cancel
and withdraw the remaining finite slots without inventing living attackers.

Changed source: `TerritoryCounterAttackSubsystem.h/.cpp`,
`TerritoryAssaultPersistence.cpp`, and the new
`TerritoryAssaultSpawnFailureTests.cpp` regression. No new subsystem, callback,
public API, Blueprint pin, save field, replicated field or migration version is
introduced. Existing saved counts remain readable. The change neither rerolls
decisions nor changes diplomacy, autonomous attacks, explicit immediate story
waves, activation policy, road topology or automatic multiplayer flag capture.

## Existing authorities and integration

CounterAttackSubsystem owns the finite retry/force ledger. `ATerritoryVolume`
and `UTerritoryControlSubsystem` still own territory state and capture. Existing
`ATerritoryWorldState` summaries publish terminal results and provide late-join
state. Native `UNPCDefinition`/`FNPCSpawnInfo`, NPC spawning, activity components,
mount abilities and ASC death handling remain the physical gameplay foundation.
No Narrative Pro source or content is edited.

The complete authored, saved and replicated lifecycle remains traced in
[batch 41](CASUALTY_DRIVER_LOSS_2026-09-07.md#authorities-and-full-lifecycle-trace). The changed
branches sit inside physical activation and survivor reconstruction; terminal
resolution still reconciles capture participants, forces, events and summaries.
World Partition identity and rebind rules are unchanged. This batch's native
suite covers stable target rebind; a physical AlMalik streaming run remains open.

## Behavioral regression

`TerritoryFramework.CounterAttack.SaveLoad.SpawnFailureBudgetSurvivesRestore`
serializes real SaveGame archives, restores through the subsystem and attempts
new-wave, physical-survivor and legacy deployments with an absent authored route.
Each path covers counts zero, `MAX_int32 - 1` and `MAX_int32`.

The test proves saturated counts, zero living force after failed deployment,
unchanged decision roll, finite cancellation/withdrawal, the existing WorldState
terminal projection and persistence of terminal state through another reload.
It also proves that reloading after an ordinary failure does not reset the
three-attempt budget, and that a simulated-proxy target cannot consume a wave or
physical-reconstruction retry. That role test is not described as a network RPC
test; the live three-world probe is separate evidence.

Before the fix, `RedTests/index.json` records the test failing with 13 assertions,
including the exact `2147483647 -> -2147483648` wrap on all three deployment paths.
After the fix, `EditorAllTests.json` records **280 passed, zero failed/skipped**.
Sixteen tests emit fixture warnings. Both Editor/runtime/UHT and Development Game
builds succeeded (`Build_Editor_Green.log`, `Build_Game_Green.log`).

The first headless full-suite run started on `/Engine/Maps/Entry`: 279 tests
passed, including the new regression, but `CounterAttackMapConfiguration` failed
because loading the HopDistrictTest package alone did not create its ZoneGraph
subsystem. The subsequent complete suite ran with HopDistrictTest loaded in the
editor and passed all 280. The initial report is retained as environment-limited
evidence, not silently counted as a clean pass.

## Crash investigation limits

The earlier crash `UECC-Windows-5173E9724B0CB0DFAD2E7DB61C823938_0000`
occurred during behavior-tree execution after clustered attacker casualties.
Matching AIModule debug symbols are absent. Export-address inspection identifies
decorator search/execution frames but does not identify the failing node or an
exact source line. No speculative Narrative AI teardown change is made.

The initial 75-second three-world probe killed four dismounted attackers and
removed their capture participation immediately. Four remaining attackers were
still in vehicle ingress at the deadline, so its finite-defeat check is false;
that partial run neither proves terminal defeat nor reproduces the old crash.

A separate crash occurred during editor shutdown, after PIE had stopped and no
dirty packages remained: `UECC-Windows-4F85709746F88D3674287FAE7B3B1F8A_0000`.
Its UnrealEd/Slate painting stack and read at `0x58` differ from the old AIModule
crash. It remains an editor shutdown issue to investigate, not a fixed gameplay
defect or a successful shutdown gate.

The final `ClusteredCasualties_Final_3.json` probe observed a listen server plus
two clients for 150 seconds. Four dismounted attackers died together through
Native ASC at 18.51 seconds; the remaining four died after dismount by 25.53
seconds. All worlds reached the same assault ID with planned 8, killed 8, alive
0, reserve 0, withdrawn 0 and `Defeated`. Every injected casualty immediately
stopped contributing capture pressure. All four final probe checks pass. The
first cluster was in `TerritoryAssaultActivity` before death; it does not reproduce
the old combat/decorator stack. The gameplay window contains no fatal, assertion,
ensure or Blueprint runtime error.

After PIE stopped, an attempted Python `ReloadPackages` cleanup caused a separate
tooling fatal (`UECC-Windows-65B27CD94CE73866CB94918D285099EC_0000`). The log
explicitly identifies a Python-held `/Game/HopDistrictTest` package reference
preventing GC, followed by EditorServer's world-memory-leak check. Do not reload
the active map through that Python package API. This failure occurred after the
completed gameplay probe, and is not evidence of an assault runtime crash.
`UserAssets_Unchanged.json` verifies that the map, the user's handover dialogue
and the Blacksmith owner asset retain their pre-verification disk hashes.

## Asset and package verification

`AssetValidation_Batch43.json` reports **77 Blueprint compilations, 128 assets,
zero errors/invalid assets and four existing presentation warnings**: two
prototype owner appearances and Farm dialogue's missing shot/zero blend-out.
`NarrativeSourceHashes.json` compares all 741 local vendor source files with the
installed Fab package, with no differences. The fresh validation session has no
dirty packages and shuts down normally (`Editor_Validation.log`).

`CookStage.log` and `CookStage_Process.json` record a successful HopDistrictTest
dependency cook and distinct `Stage_Batch43` package, exit zero, with zero cook
errors and 30 existing content/tooling warnings. The five retake example assets
are explicitly requested again through the cook commandlet's `-PACKAGE` list.
`StagedBinaryHash.json` verifies that the staged Game executable matches the
compiled source executable.

`PackagedSmoke_Process.json` records a 90-second localhost Development Game
server-mode smoke, exit zero. An immediate Hard assault deployed and combat ran.
The log contains no Error-severity entry, fatal, assertion, ensure, Blueprint
runtime error or Accessed None. Offline Steam initialization and existing Native
gameplay diagnostics remain warnings. This is startup/deployment smoke coverage,
not a compiled TDAServer test or an assertion of packaged terminal casualty counts.

Broader remaining work is tracked in the [roadmap](ROADMAP_AND_REMAINING.md).
