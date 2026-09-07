# Batch 41: surviving passengers after driver loss

Focused continuation of the unfinished complete audit. Host evidence directory:
`Saved/Verification/20260907_CasualtyAudit`.

## Preflight and confirmed defect

Started from plugin `293e3f1`, host `0d88e7e`, branch
`hoptrendy/territory-complete-audit`. Existing host audio assets, editor settings,
SteamIntegrationKit enablement and unsaved HopDistrictTest edits belong to the user.
The map's disk version was backed up before saving its editor state for the rebuild.

The controlled rendered baseline killed a mounted driver through Narrative's ASC.
Its death was correctly charged once, leaving three living passengers and four
pending attackers. All three healthy passengers then remained in vehicle ingress
until timeout and were withdrawn. The next wave waited for that timeout.

`EnsureNarrativeVehicleIngress` watched a driver's dismount/completion flags but
not its retirement flag. A retired driver remains an actor during Narrative's
corpse lifetime, so its weak component reference is still valid. A missing/failed
driver was also incorrectly treated as a passenger failure.

The existing passenger branch now treats retired, removed and failed drivers as
reasons to dismount. It waits for low vehicle speed and uses the existing Native
`UNPCInteractionComponent::StopInteractBehavior(false)` contract. A rejected exit
stays pending; the next update confirms Native slot release before ingress
completion. The server-only bridge does not consume or replenish another force
slot. An unpossessed car receives brake input while waiting to stop. Passengers
cannot override a living driver's controls, and a former driver cannot brake a
car after another controller takes possession.

## Authorities and full lifecycle trace

| Transition | Existing implementation / persistence / presentation |
| --- | --- |
| Capture to grace | `HandleTerritoryControlChanged`, `ScheduleAssault`; `ATerritoryVolume` owns the capture result. |
| Grace and evaluation | `AdvanceAssault`, `EvaluateAssault`; durable cycle, seed, roll and grace timestamps. |
| Warning and activation | `NotifyRelevantPlayers`, `ActivateAssault`, `TryCommitProximityActivation`; commit one activation before spawning. User-selected autonomous profiles and explicit immediate story events remain supported. |
| Physical deployment | `SpawnNextWave`, `SpawnParticipant`; Narrative character subsystem owns NPC creation and identity. Finite admission lives in CounterAttackSubsystem. |
| Arrival and participation | `EnsureNarrativeVehicleIngress`, `CompleteVehicleIngress`, `UpdateParticipation`; Native mount ability owns seats/animation/possession; ControlSubsystem owns capture participants. |
| Death and withdrawal | Native ASC death delegates, assault character `HandleDeath`, participant `Retire`, `NotifyParticipantRemoved`; capture removal precedes casualty publication, with repeated removal guarded. |
| Capture or defeat | Existing ControlSubsystem/volume capture flow, `CompleteRecapture`, `ResolveAssault`; no ownership roll or replacement capture authority. |
| Recovery | Terminal timestamp and recurring cooldown; surviving actor/vehicle cleanup uses existing retirement rules. |
| Save and clients | CounterAttackSubsystem value records and `ATerritoryWorldState` snapshots; `GetPersistentState`, `RestorePersistentState`, `ReconstructParticipants`; Native stable GUID records preserve survivor attributes. |
| Story/UI | `BroadcastStateTransition`, `OnCounterHappened`, relevant-controller notifications and existing Tales/task adapters. |

## Compatibility and coverage

No new persistent or replicated field, Blueprint API, NPC controller, behavior tree,
or Narrative source/content edit. Existing child Blueprints inherit the fix.
Driver references remain transient: restore still elects one surviving passenger
for an already charged car. Target streaming and GUID reconciliation continue
through the existing registry; this batch does not certify all AlMalik streaming.

The native regression `PassengersSurviveDriverLoss` covers a healthy driver,
retirement, missing reference, failed driver, client rejection, Native exit refusal,
zero pending capture pressure, eventual ingress completion and actual Chaos
throttle/brake ownership for possessed and abandoned cars. The existing
casualty, survivor save/load, streaming identity and determinism tests remain gates.
Rendered probes exercise the authored sedan, mount ability and actual ASC deaths.

## Gameplay verification

- `Build_Final.log` and `Build_Game.log`: UE 5.8.2 Editor/runtime/UHT and
  Development Game builds succeed. After the host staging rule changed,
  `Build_Staging_Editor.log` and `Build_Staging_Game.log` also succeed.
- `AllTests_Final/index.json`: **275 passed**, comprising 259 clean and 16
  fixture-warning tests, zero failed or not run. The focused regression uses real
  Native components and the sedan Blueprint; the rendered probes supply the actual
  mounting, exit animation and death lifecycle that the small native fixture omits.
- `AssetValidation_Batch41.json`: **75 Blueprints compiled, 123 assets checked**,
  zero errors/invalid assets and four existing presentation warnings. The previous
  untracked `DA_QC_NewMission` draft no longer exists in the project; its batch 40
  validation error is historical rather than a current blocker.
- `Casualty_Baseline.json`: the unfixed mounted-driver death left three healthy
  passengers waiting until timeout and then withdrew them. This reproduced a
  passenger failure, not the earlier behavior-tree crash.
- `Casualty_Green_3.json`: final-source rendered listen server plus **two clients**,
  120 seconds. Kill the mounted driver through Native ASC at 1.01 seconds; passengers
  leave, the second squad deploys, and actual ASC deaths remove all eight attackers
  by 32.55 seconds. All three worlds converge on the same stable assault ID and
  `Defeated`, with planned 8, killed 8, withdrawn 0, alive 0 and pending reserve 0.
  Each injected death immediately has zero capture registration.
- `Casualty_SaveReload_1.json`: final-source 120-second rendered run saves and
  reloads to a unique real Native save slot after the driver death. The strict
  `TerritorySaveReloadGate` reports one restored live assault, zero unexpected live
  records and zero duplicate live IDs. It reaches the same finite defeat by 34
  seconds with all eight actual deaths accounted for. It does not certify saves
  taken inside every NPC initialization callback.
- `NarrativeSourceHashes.json`: all **741 Narrative source files match** the
  installed Marketplace version; no vendor patch is part of this change.

## Host packaging integration

The user enabled SteamIntegrationKit 1.9 in `TDA.uproject`. A fresh Game package
then failed before loading the map: exception `0xc06d007e` in the Steam DLL delay
loader called by `UHostMigrationSubsystem` construction. Its SDK DLL was staged
under the plugin source directory, while callback registration occurred before
the plugin module added that directory to the DLL search path.

Placing the plugin's identical Steam SDK 1.64 DLL beside `TDA.exe` made a separate
diagnostic smoke pass. The project-owned `Source/TDA.Build.cs` now declares that
additional Win64 non-editor runtime dependency using the installed `SteamSdk`
module directory. The enabled plugin remains the SDK authority. No Steam or
Narrative vendor source changes, credentials, or SDK-version substitution are
required. This host rule assumes the project's enabled SteamIntegrationKit
dependency remains installed; removing that integration requires removing the rule.

- `Package.log`: fresh final-gameplay-source HopDistrictTest/dependency cook,
  stage and package pass, **zero errors and 30 cook warnings**. Warnings include
  plugin version/deprecation messages, missing Steam tooling credentials and
  existing Native Character Creator tags; they are not all fixed by this batch.
- `Package_Final.log`: after the staging-rule-only change, UAT restages the
  already cooked content to the distinct `Stage_Batch41_Final` directory and
  succeeds. `StagedSteamSDK.json` and the NonUFS manifest prove the runtime rule
  placed the matching DLL beside the executable; no manual copy was used there.
- `PackagedSmoke_Process.json`, `PackagedSmoke.log` and `StagedBinaryHash.json`:
  matching final executable, unique user directory, localhost Game server-mode
  smoke for 90 seconds, exit **0**, no Error-severity log, ensure, assertion or
  Blueprint runtime error. Both four-person vehicle squads deploy, with seven
  ingress completions logged amid real combat. This smoke verifies startup and
  deployment; its terminal casualty totals were not asserted. Steam reports
  unavailable client IPC, so this is not Steam online-session verification or a
  compiled TDAServer test. The earlier failed and manually copied diagnostic
  runs are retained under `MissingSteamDLL` and `DiagnosticDLLCopy` filenames.

## Still open

The earlier behavior-tree crash after clustered combat casualties has not been
attributed to this passenger defect. Matching AIModule debug symbols are not
installed; successful later runs alone cannot close it. Preserve the batch 40
crash context and obtain a symbolized reproduction before changing AI teardown.
The next assault review also needs malformed recurrence counters and spawn/save
callback coverage, followed by actual AlMalik World Partition streaming.
Dedicated-target, broader economy, city content and interior/HDR review gates
remain on the complete audit; this batch does not certify the full framework.
