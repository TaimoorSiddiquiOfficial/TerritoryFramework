# Batch 40: weapon melee, vehicle departure and UDS lighting

This is a focused batch within the unfinished complete audit. Evidence is in the host
project's `Saved/Verification/20260907_MeleeRoadLighting` directory.

## Confirmed defects and authority

1. **Sword execution silently stops near a defender.** Territory NPCs derive from Native
   C++ classes rather than `BP_NarrativeNPC` and lacked its Contextual Animation actor
   component. Narrative's melee ability unpossesses its target before attempting the
   two-actor execution binding. UE rejects a target without that component, leaving no
   swing and no possessed target. Both Territory NPC classes now supply a plugin-owned
   subclass of the existing engine component. It restores the original Narrative
   controller only when an interrupted scene leaves a living, unpossessed server NPC
   and that controller has no other pawn. Native GAS, execution assets, attack tokens,
   animation and activity selection remain authoritative.
2. **Mounted Native initialization replaces the driver's controller.** Narrative calls
   `SpawnDefaultController` from definition/load callbacks while a seated character is
   deliberately unpossessed. The new controller overwrote Narrative's NPC cache while
   the original still possessed the car. The Territory assault override retains a
   valid controller whose existing Native owned-NPC reference identifies this actor.
   It permits server replacement if that controller was destroyed.
3. **The driver's road probe detects its own sword.** Live box sweeps hit
   `BP_DemoSwordVisual.MeleeCollider` at the spawn point, holding throttle at zero and
   brake at one until the bounded on-foot fallback. Ignoring only occupant pawns missed
   their separate visual actors. The existing participant probe now ignores attached
   visuals for occupants assigned to that car. Other squads, departed occupants and
   actual road obstacles remain detectable. No new road graph or movement authority.
4. **The HDR tool competed with UDS exposure.** The existing tool-owned volume now
   yields metering, bias, curve and EV range to UDS when UDS exposure is enabled.
   UDS's public Blueprint controls receive sky/light/fog/interior settings, with its
   construction logic rebuilding derived components. Narrative Game State retains
   gameplay time authority. Existing quality modes budget cloud sampling and Lumen
   updates separately for gameplay and cinematic work.
5. **Existing unbound post-process volumes can assert on spatial-flag migration.**
   HopDistrictTest retained a true serialized spatial flag with unbound enabled.
   The engine locks changes in that state. Migration now clears the flag while bound
   before restoring the final unbound mode. The regression starts from that exact
   legacy configuration rather than only testing freshly spawned volumes.

## Compatibility

- No Narrative Pro source or content changes. All 741 checked vendor source hashes
  match the installed UE 5.8 Marketplace package.
- No custom replicated state or campaign save fields were added. Contextual scene
  replication uses the engine component; Native controller and attachment ownership
  are resolved from live actors after load. No durable pawn/controller references.
- Existing Blueprint classes inherit the component automatically. The existing HDR
  options gain controls; no Blueprint API or class rename is required. Recompile
  project child Blueprints and perform a full cook after the native build.
- Global sky and post-process setup remains in the persistent level. This does not
  prove all city World Partition streaming/load-order scenarios.
- Autonomous physical attacks and explicit immediate story waves remain supported,
  as selected by the user. No probability or capture ownership calculation changed.

## Evidence and limits

- Editor/runtime/UHT and Development Game builds pass. The final full automation
  report contains **274 passed** (257 clean, 17 with warnings), zero failed/not run.
- The fresh HopDistrictTest/dependency cook, stage and package pass with zero errors
  and ten warnings. The matching Development Game executable completes a 90-second
  fresh-user-directory localhost smoke with zero errors/ensures/assertions, two
  four-person squads and seven ingress completions. It logs real casualties,
  including a driver death, and no blocked-arrival fallback. This is a Game executable
  in server mode, not a compiled dedicated server. Its terminal capture/casualty
  records were not asserted; that full assault release gate remains open.
- HopDistrictTest's existing sky and tagged volume were updated and saved. A noon
  preview is stored in the map; Narrative still controls time in gameplay. The setup
  audit reports 17 passes and one interior-review advisory. Corrected running-game
  captures at noon and 22:00 show readable shaded architecture and night surfaces.
  The early dark captures lacked display gamma and the first game-time harness
  froze Narrative before its queued time advance applied; those captures are invalid
  for judging scene exposure. Final captures explicitly use gamma 2.2 and wait for
  Narrative to reach the requested time before freezing the disposable PIE clock.
- AlMalik's existing Narrative sky and tagged post-process external actors were
  updated. Exposure overrides were cleared on its older global post-process actor,
  which had fixed min/max exposure of 1. The bounded Embassy volume was preserved.
  The three modified World Partition external packages have pre-edit backups in
  the verification directory. The audit retains a warning for the additional
  global volume's other authored grading.
- Weapon probe: far/front/very-near sword swings and close/back execution montages
  play after the component fix. A separate interruption test preserves a living
  target and restores its original controller. The first broad interruption probe
  killed its target before cancellation and is not counted as a recovery pass.
- Vehicle probe: both mounted drivers retain controller identity through forced
  Native load callbacks. Both cars move; all eight attackers complete dismount.
  Server plus two clients also observe both cars moving. Do not interpret client
  physics correction velocities as gameplay speed measurements.
- The corrected server/two-client execution probe identifies replicas using their
  replicated home transform. All three worlds enter and leave the same scene, and
  the living target resumes its original server controller after interruption.
  Earlier probes using actor names or editor actor GUIDs were invalid identity
  comparisons and are retained separately, not counted as network failures.
- New native behavioral tests cover mounted load/recovery/client authority,
  actual Narrative sword scene bindings, own/foreign weapon obstacle collision and
  the real UDS Blueprint contract, quality switching and failure paths.
- All 75 included Blueprint compilations pass. The full project-content validation
  scans 124 assets and reports one invalid asset: the user's pre-existing untracked
  `DA_QC_NewMission` draft lacks Objective/Success states, name and description.
  It remains untouched. Excluding that unfinished draft, the 123 baseline assets
  report zero errors and the four previously documented presentation warnings.
- An earlier long diagnostic crashed in the engine behavior-tree decorator search
  after attacker casualties. Its root cause is not established; later successful
  arrivals do not close that crash. Keep the crash context and exact combat repro
  as an open release gate.
- Compiled dedicated-server testing still requires a server-capable UE build.
  The broader production metadata/callback, assault restore, streaming, Manny
  visibility, AI/Tales/navigation/UI and city-asset issues remain on the main audit.

## Lighting use and review

Use **Balanced Gameplay** for the map default and **AAA Cinematic** for controlled
shots. The sky keeps the existing sun intensity scale (10), moon 0.15 and skylight 1;
this is not a conversion of the entire project's lighting to physical lux. Clear
weather fog starts at 0.003, indoor fog at 0.25 of outdoors and indoor exposure bias
at +0.35. Existing weather and time-of-day color authoring remain with UDS/UDW.

Interior adjustments depend on real collision or authored UDS occlusion volumes.
They do not illuminate closed rooms: use authored local lights and inspect window
highlights, enclosed rooms, dusk and night in the target map. A successful setup or
configuration audit is not a claim of perfect lighting or calibrated HDR output.
