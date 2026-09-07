# Assault target streaming wait — batch 44

## Confirmed defect and fix

`UTerritoryAssaultParticipantComponent::UpdateParticipation` attempted Narrative
goal initialization before checking whether the target's stable GUID could resolve
through `UTerritoryRegistrySubsystem`. An admitted attacker without an initialized
goal exhausted 40 retries while the target was unavailable. It then withdrew while
alive, reducing the finite force and removing its saved survivor record.

The existing target-unavailable branch now runs before goal initialization, after
the existing vehicle-ingress and optional vehicle-escape handling. It releases
strategic attacker slots, restores Native goal-score overrides, clears the local
capture-registration flag and waits. It preserves already consumed initialization
retries. Returning targets resume the same initialization path; genuine failures
against a loaded target still withdraw after the existing 40-attempt limit.

This is a focused ordering fix. No new subsystem, state, timer, RPC, probability
rule, save field or Blueprint function is introduced. Native combat, death and
goal ownership remain unchanged. Vehicle ingress retains its own timeout policy.

## Authorities, lifecycle and compatibility

- `ATerritoryVolume` owns territory state and `UTerritoryControlSubsystem` owns
  physical capture validation/progress. Registry lookup owns target resolution.
- `UTerritoryCounterAttackSubsystem` owns the finite assault ledger and scheduling;
  `ATerritoryWorldState` remains its persistence and replicated read-model bridge.
- Narrative's character subsystem spawns the NPC; its activity component creates
  and removes the assault goal. The existing ASC death binding is installed by
  `ATerritoryAssaultCharacter::SetNPCDefinition` before Native definition loading,
  so target waiting does not replace or defer casualty accounting.
- Mutations still require the owner actor's server authority. Clients cannot
  consume retry attempts. No first-player-controller context is introduced.
- Save/load preserves the survivor's Native spawn GUID and the original decision.
  There is no schema migration or Blueprint migration. Same-tag actors with a
  different GUID cannot end the wait.

The complete capture → grace/evaluation → warning/activation → Native deployment
→ registration/combat/casualties → capture/defeat → recovery/save/replication trace
was rechecked against source and the
[batch 41 lifecycle record](CASUALTY_DRIVER_LOSS_2026-09-07.md).
`AdvanceAssault` already waits for registry resolution before advancing a streamed
target, and `HandleTerritoryRegistered` resumes it. The defect was the participant's
earlier initialization-failure branch. Autonomous physical assaults and explicit
immediate story waves remain enabled; multiplayer flag capture remains automatic.

## Regression and build evidence

Evidence directory: `Saved/Verification/20260908_AssaultStreamingAudit/`.

`TerritoryFramework.CounterAttack.SaveLoad.UnloadedTargetDoesNotExhaustGoalInitialization`
uses an actual Narrative deferred NPC spawn, a transient definition whose visual
is not yet ready, and the existing Territory registry. It verifies:

- 45 missing-target updates preserve one living attacker, one unspent reserve,
  zero withdrawals and zero capture registration.
- A same-tag/wrong-GUID actor does not substitute for the target.
- Target return resumes initialization; simulated-proxy updates cannot consume
  attempts. A subsequent unload preserves an already consumed attempt.
- A genuinely unready NPC still retires once after 40 loaded-target attempts.
- A SaveGame archive round trip while the target is unavailable retains the exact
  survivor GUID, finite budget, active record, decision roll and zero invented
  withdrawals.

`RedFixtureTests/index.json` reproduces the original withdrawal, retry exhaustion
and missing survivor. `GreenClaimedTests/index.json` passes after the fix. Earlier
receipts are retained: the first cached build omitted the new test; subsequent
fixture compile corrections and a missing runtime defending-owner setup are not
counted as successful validation. The regression intentionally uses an unregistered
transient NPC definition and records the corresponding AssetManager warning.

Editor/runtime/UHT and Development Game builds pass. `Automation_All.json` reports
**281/281 tests passing** in the loaded HopDistrictTest editor.
`AssetValidation_Batch44.json` reports **77 Blueprint compilations, 128 assets,
zero errors/invalid assets and four existing presentation warnings**.

## Live multiplayer verification

`StreamingWait_PIE.json` records a 55-second listen-server/two-client probe. Four
real, visually ready Narrative attackers dismounted before their goals were
removed through Native `RemoveGoal` and their target was unregistered. For more
than 30 seconds they remained alive with no target, assault goal or capture
registration. After registration returned, the same four NPCs acquired new goals
for the original GUID. All three worlds retained the same assault with planned
8, alive 8, reserve 0, killed 0 and withdrawn 0. All four final checks pass.

An invulnerable defending player keeps ordinary recapture from ending this
resumption test. The first probe, retained as
`StreamingWait_PIE_RecapturedFixture.json`, passed the waiting checks but allowed
normal recapture on target return; its later lookup of a removed participant
failed. That harness result is not counted as a successful resumption probe or a
new gameplay defect.

The final gameplay window contains no fatal, assertion, ensure, Blueprint runtime
error, Accessed None or Territory Error-severity log. PIE stops cleanly, the play
client setting returns to one, no packages are dirty and editor shutdown completes.
`UserAssets_Unchanged.json` verifies both maps and the user's existing handover and
owner assets against their starting disk hashes. `NarrativeSourceHashes.json`
compares all 741 vendor source files with the installed Fab package, with no
differences.

## Package verification

`CookStage.log` and `CookStage_Process.json` record a successful HopDistrictTest
dependency cook and distinct `Stage_Batch44` package, exit zero, zero cook errors
and 30 existing content/tooling warnings. The five retake example assets are again
explicitly requested through the cook commandlet's `-PACKAGE` list.

`StagedBinaryHash.json` confirms that the staged executable matches the verified
Development Game build. `PackagedSmoke_Process.json` records a 90-second localhost
Game server-mode assault smoke with exit zero. Vehicles deployed, guards and
attackers fought, and Native deaths occurred. Its log has no Error-severity entry,
fatal, assertion, ensure, Blueprint runtime error or Accessed None. This is a
runtime smoke, not an assertion of terminal force counts or a compiled TDAServer.

## World Partition scope and remaining gates

`CityInventory.json` records 19,726 AlMalik actor descriptors and no Territory
actors found in that descriptor inventory. An authored, isolated assault fixture
is needed before a real city territory stream-out/in test can exercise guards,
routes, returned clients and capture. No Act 1 territory layout is invented here.

The native regression simulates target availability through registry admission;
it does not prove physical World Partition cell streaming. That city gate remains
open, along with the old unsymbolized AIModule crash, the separate editor shutdown
issue, production/refund settlement review, city content/Chaos issues, interior
and HDR visual review, and the compiled TDAServer gate. See the
[current roadmap](ROADMAP_AND_REMAINING.md).
