# Rendered HUD and authoring follow-up — 2026-09-05

This checkpoint finishes the rendered review of the Definition-owned passive HUD policy
and records two defects found during that review. It also includes the previously accumulated
Territory UI, finite vehicle squad, distraction-item, and Narrative HUD readiness changes.
Their earlier multiplayer and persistence evidence remains in the release verification report.

## Confirmed defects and corrections

- Blueprint-authored District rows displayed Narrative's default **Button Text** behind their
  named labels. Only the native layout builder had cleared it. Common row initialization now
  calls Narrative's public `SetButtonText` for both paths, retaining the real focus target.
- Vehicle validation searched only the native CDO and incorrectly rejected the sedan's four
  Blueprint-authored mount seats. It now uses Unreal's `GetActorClassDefaultComponent`, including
  construction templates and inherited overrides, without spawning actors. Missing seats remain
  an error; an over-capacity request remains a warning about bounded later deployment.
- Blacksmith's Claimed audio requested `Music.Territory.State.Claimed` from a music set without
  that row. The user selected **Unity in the Ashes**. Project-owned `DA_BlacksmithMusic` maps that
  tag to the existing SoundWave. The separate **Horns of War** entry sound and all other state
  fields and events are preserved.

## Authority and compatibility

The Definition still owns HUD/audio authoring. Narrative owns button input, its Menu layer,
music playback, and vehicle mount seats. The new fixes add no gameplay, save, replication,
identity, or Blueprint API authority and require no Blueprint migration. No Narrative Pro source
or asset was modified. Transient PIE changes were discarded; the editor had zero dirty packages.

## Executed checks

- UE 5.7 Win64 Development Editor and Game builds passed. The final editor rebuild includes
  the Blueprint mount-seat correction.
- All **213** `TerritoryFramework.*` automation tests passed: **207 clean**, **6 intentional
  warning fixtures**, **0 failed**, **0 skipped**. This includes the authored-row caption and
  real Narrative sedan seat regressions, plus the existing save/load and authority coverage.
- **72 Blueprints** compiled; **113 assets** across project/plugin Territory content,
  `HopDistrictTest`, and the selected soundtrack validated with **0 errors** and **4 warnings**.
  The warnings identify two prototype mannequin appearances and Farm dialogue's missing shot
  and zero blend-out time. They remain explicit authoring work.
- A fresh Windows `HopDistrictTest` cook with `-NoAssetRegistryCache` completed **6,906 packages**
  with **0 errors** and **5 warnings**. The cooked output contains both `DA_BlacksmithMusic` and
  `Unity_in_the_Ashes`. This was a map cook; a new staged package was not produced in this pass.
  The warnings concern the vendor UI substring redirect, texture-pool setting priority, and
  three missing-tag references in Narrative's Character Creator content.
- In visible PIE, City-only travel collapsed the passive card, Blacksmith displayed it, and
  disabling Blacksmith did not reveal a parent card. Restoring its policy restored the card.
  Menu-layer activation collapsed it. Overview/Places/Garrison selection remained exclusive.
- The real player menu's Territories tab rendered with its normal blurred background, clean
  District labels, readable notifications, and no stray default row caption.
- The new music set loaded through Narrative's existing subsystem; setting the Claimed theme
  returned success and its audio component was playing. This was a direct playback check.
  Entering enemy-held Blacksmith changes it to Contested, whose authored music override is
  disabled; this check does not claim an end-to-end capture soundtrack playthrough.

Local evidence is under the host project's `Saved/Verification/20260905_Resume/`:
`Automation_Final/index.json`, `EditorBuild_Final.log`, `GameBuild.log`,
`AssetValidation_Final.json`, `Cook_Final.log`, and `10_Territories_InPlayerMenu.png`.

## Remaining release gates

The earlier packaged listener/two-client admission, finite Hard assault, and exact active-save
restart proofs were not repeated in this HUD/authoring follow-up. The full two-player
capture/recruit/patrol/assault/XP scenario, a physical World Partition stream-out/in fixture,
and gamepad/accessibility review remain open. This checkpoint is not a claim that those release
gates are complete.
