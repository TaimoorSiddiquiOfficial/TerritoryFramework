# Character LightRig complete system audit — 2026-09-14

## The element is already used

The profile does not force one Blueprint class. `UTerritoryCinematicLightRigProfile::RigClass` accepts an actor class implementing `TerritoryCinematicLightRig`. TDA selects the UDS adapter because it supplies Narrative's visual, camera and day/night context to the original rig.

```text
DA_NarrativeCharacterLights_UDS
  -> BP_TerritoryUDSCharacterLightRig
     inherits BP_TerritoryCharacterLightRig
       inherits BP_LightRig_Actor_Base
         -> SyncPresetElements
           -> ChildActorComponent
             -> BP_LightRigElement (one per character element)
```

The last arrow is a real spawned child, not just a reference. HopDistrictTest PIE produced four exact `BP_LightRigElement_C` children for `Day_Studio`, then five for `Night_Moonlight`. Every child's parent, settings, globals, camera and actual light-component settings passed checks. Stopping Native sequence playback removed the root and all children.

`BP_LightRigElement` implements the pack's smaller `BPI_LightRigElement` contract. It needs character meshes, skeleton, camera, element settings and globals. The later [direct element adapter](DIRECT_LIGHT_ELEMENT_PROFILES.md) supplies those inputs when the original element is selected in Rig Class. Whole-rig profiles can still use a full light-preset row to choose the element arrangement.

## Coverage and authority

This pass inspected all 50 assets in the requested Core, Data and UI folders, plus the original `EUW_LightRig_ControlPanel`. The 15 pack Blueprints contain 81 graphs and 2,236 nodes before these panel fixes. All graph data was inspected; this is source coverage, not a claim that every visual combination was rendered. Three project Blueprints and the profile/editor bridge were also inspected.

The reference is installed Narrative Pro's `UNarrativeDialogueSequence` and `ANarrativeLevelSequenceActor`, including their sequence player, camera binding and termination paths. Territory observes those public APIs through `UTerritoryCinematicLightRigComponent` and `UTerritoryDialogueShot`. The pack extension points are `ApplyPreset`, `SyncPresetElements`, `BPI_LightRigElement::Setup`, `UpdateGlobals` and `UpdateLight`.

Narrative owns playback, the local viewer and the saved/replicated clock. The original rig owns its light children. The editor panel owns preview selection. This batch changes only the panel's editor callbacks. It adds no replicated state, save records, gameplay authority, capture behavior or streaming registration.

## Core: what each part does

| Asset | Actual responsibility | Current Territory use |
|---|---|---|
| BP_LightRig_Actor_Base | Validates character/camera/skeleton references, applies full presets, creates children and updates them | Used through inheritance |
| BP_LightRigElement | Rect/Spot/Point lights and matching wrap lights; placement, targeting, color, shadows and visibility | Used for every character-light element |
| FL_LightRigMath | Bone sampling, axis/direction math, camera reflection, floor clamp, occlusion, prone/back-face blending and wrap transforms | Called by the element |
| BPI_LightRigElement | Setup, globals and per-update light calls | Used by the parent and camera rebinding |
| BP_LightRig_Editor_Base | Editor tick wrapper around a rig | Editor preview only |
| BP_LightRig_Element_Background | Finds tagged level ALight actors and edits their light components | Filtered out of whole-rig presets; supported explicitly by the later direct background profile |
| BP_PostProcess | Editor helper plus an external unbound PostProcessVolume | Separate editor feature; not part of the runtime adapter |
| BPI_LightRig | SetNewCamera interface | No asset referencers or calls found in inspected pack graphs |

The element's ordinary actor Tick is unconnected because its parent drives it. That is not an unused implementation. The element supports Face/Body, Eyes and Hair targets; component/channel controls; camera-relative direction; floor and occlusion corrections; smoothing; and separate wrap positioning. These settings travel through the inherited pack implementation rather than a Territory replacement.

## Data: full looks, single lights and image presets are different

| Data | Contents | Current path |
|---|---|---|
| DT_LightPresets_Default | 8 complete looks: Studio, Moonlight, Split, Portrait, Thungsten, Fireside, TextureBooth, RedLantern | Full runtime preset selection |
| DT_LightPresets_Starter | 5 complete looks: Drama_Golden, Floral_Orchid, Fashion_Curtain, Nature_Overcast, Sunny_Sunset | Full runtime preset selection |
| DT_LightElementPresets_Default | 31 individual-light templates | No current AssetRegistry referencers |
| DT_LightElementPresets_Starter | 25 individual-light templates | No current AssetRegistry referencers |
| DT_PP_Presets_Default | Default, Clarity, Cinematic, Softness, Grunge | Separate panel post-process list |
| DT_SkeletonConfig | MetaHuman, Mannequin_Manny, Mannequin_Quinn | Pack mesh/bone descriptions |
| DT_NarrativeLightRigSkeletons | TDA's matching Narrative component names | Project rig uses Body and FaceMesh |
| DT_NarrativeLightRigPresets | Day_Studio and Night_Moonlight | Current UDS selections |

The six structs define complete presets, global settings, named elements, 52 element-setting fields, skeleton configurations, and post-process settings. The light enums define type, target and bone axis. UI enums describe button/list content; they do not control gameplay.

An individual-element table cannot replace `S_LightPresetRow`. A post-process row named Cinematic cannot create lights. UDS selection already checks the full row type, row existence and usable character elements, retaining the previous valid look on failure. Better authoring-time table filtering remains useful so the editor prevents this mistake earlier.

## UI and authoring

| Asset | Role |
|---|---|
| EUW_LightRig_ControlPanel | Setup, full-preset and post-process menus; rig/camera/character selection; spawn/delete helpers; skeleton selection; background actor tags |
| WBP_ScrollBox_Base | Generic table/actor/tag list and selection callbacks |
| WBP_Button_Base | Typed click events, row text/thumbnail and selection styling |
| WBP_LabelField | Object name labels and text styling |
| WBP_DropDown_DT | DataTable row dropdown |
| WBP_DropDown_Button | One selectable row |
| WBP_ConfirmDialogue | Confirmation helper with no current AssetRegistry referencers |
| EUW_TerritoryLightRig | Project child of the original panel |

The generic list/button widgets support individual-element rows, but neither individual-element library is connected to the installed panel defaults. No complete element-library editing menu or save-to-selected-UDS-row workflow was found. A no-reference result is evidence of an unconnected asset, not proof that no external dynamic caller could ever load it. None of these helper assets was deleted.

A suspected stale dropdown array was a false positive: `Initialize_ScrollBox` uses a function-local Widgets array, not a persistent member accumulating entries.

### Confirmed panel bugs fixed

1. **Changing the post-process table refreshed the light list.** The PP table property callback called `Refresh_Lists_LR_Preset`. It now calls `Refresh_Lists_PPV_Preset`. Before the fix, one-, two- and zero-row fixtures all left five old PP entries displayed. Afterward, all three show exactly the chosen rows and leave the light table unchanged.
2. **The skeleton dropdown kept its default table.** `Refresh_SkeletonField` updated the selected row label without updating the dropdown's DataTable. It now copies the selected rig's complete skeleton handle, sets the dropdown table, rebuilds the list, then selects the row. With no selected rig it uses the panel's handle. Both paths pass using a unique custom row; before the fix both displayed the three unrelated pack rows.

The panel was compiled and saved locally. The original and project child compile with zero errors and warnings. The small changed graph section was arranged left to right. The earlier Rect-to-Point visibility fix remains separate; see [element repair](LIGHT_RIG_ELEMENT_PRESET_REPAIR.md).

## Features that need additional integration

These findings are not fixed by choosing a different Rig Class.

### Background scene lights

The background element resolves existing **ALight actor tags**, not GameplayTags or all light components inside arbitrary actors. It gathers them during Setup and edits their intensity, color, temperature and ray-traced settings.

Source inspection found no original-value restoration, no ongoing discovery of streamed-in tagged lights, no use of the element IsEnabled flag in its update path, and no application of the global UseRayTracing switch there. Overlapping sessions could write the same scene light. The later direct background adapter addresses these gaps around the original public API, with scoped snapshots, exclusive ownership, enabled/ray-tracing controls and bounded loaded-light discovery. This does not change the original pack's independent editor/runtime use. Real AlMalik World Partition travel remains an acceptance gate.

Keep it separate from the current character-only adapter until a scoped binding/restore policy is implemented. AlMalik World Partition and overlapping local cutscenes need explicit coverage.

### Post-process and exposure

The panel's SpawnPPV creates a BP_PostProcess helper and a separate unbound PostProcessVolume. The helper also has its own unbound PostProcess_Default component at priority 1, blend 1, with both exposure overrides enabled and MinEV100/MaxEV100 set to 1. Its ConstructionScript applies those exposure values separately from the chosen S_PostProcess row.

The UI's DeletePPV deletes both actors. No wrapper EndPlay cleanup of the external volume was found for manually deleting the wrapper. Automatically spawning this editor arrangement during dialogue could impose scene-wide exposure and leave conflicting volumes.

A runtime adapter should apply optional image settings through the active Native camera's supported post-process path, define blending/ownership, and restore its own contribution on cuts and exit. UDS and existing world exposure must retain their roles. The five PP presets remain available in the editor; they are not yet integrated into Territory shot playback.

### Per-shot authoring and defensive checks

The profile's **Use Panel Look in Runtime Rig** copies `CurrentPreset`, `GlobalSettings` and `SkeletonConfig` to the configured rig Blueprint defaults. It does not save into DayPreset/NightPreset rows and does not copy PP data. Editing one shared rig default changes every profile using that class.

Today, duplicate the rig/profile for a separate look, or author and select separate full day/night rows. A clearer next authoring batch is a typed per-shot look with explicit day/night rows and an action that writes the preview into the selected project row. Keep pack references in the optional project adapter rather than the standalone plugin.

Source inspection also found first-row indexing in SpawnLightRig/SpawnPPV without an explicit empty-table guard, and Get/SetElementsSettings index access without an explicit bounds check. These paths need focused failure fixtures before a fix; they are not covered by the table-refresh regressions.

## Verification and remaining gates

- All 51 pack assets pass actual Unreal validation with PIE stopped: zero invalid assets and zero warnings.
- All 18 pack/project Blueprints compile without errors or warnings. The modified original panel and project child were compiled again after the final fix.
- Five panel behavior cases pass: PP lists of 1/2/0 rows and a custom skeleton with/without a selected rig.
- Fresh HopDistrictTest Native cutscene checks pass: day has four original element children; night has five; all nine element component records match their inputs; stop leaves zero roots/elements.
- PIE is stopped, the temporary audit tab is closed, no test rig remains in the editor world, and no dirty packages remain.
- Earlier 71 component cases and broader optional-rig network/package evidence are recorded in their own reports; they were not rerun as this editor-panel change's release gates.

Evidence: `Saved/Verification/20260914_LightRigFullSystem` in TDA. `AuditIndex.json` records every asset, graph overview and compile result. Panel before/after reports reproduce both defects. Day/night chain reports record exact actor classes and parents.

See the versioned [verification record](Verification/LIGHT_RIG_FULL_SYSTEM_2026-09-14.json).

The current work does not certify background-light restoration, camera post-process integration, AlMalik streaming, campaign save/load, rendered HDR quality or a new packaged build. No C++ or Narrative Pro source changed in this batch.

## Delivery and migration

The corrected pack panel binary is saved locally and backed up in the evidence folder. The pack is ignored by Git and is not community-plugin content. `Scripts/Territory/light_rig_panel_patch.json` records the changes and before/after hashes without redistributing that binary. `verify_light_rig_panel.py` provides repeatable editor regressions and restores/removes its fixtures.

After a pack update, inspect its graph first and rerun the tests. Apply the semantic patch only if the defect remains; do not trust old node IDs. No core API, caller, save or network migration is needed.

The next coherent batch is per-shot light/PP authoring and safe camera-local post-process integration. Scoped background lights, streaming/restoration and the remaining editor failure paths follow. The current adapter is a working character-light integration, not complete coverage of every pack feature.
