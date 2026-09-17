# Character LightRig preset repair — 2026-09-14

Two separate problems were confirmed in the installed TDA content.

1. `BP_LightRigElement / ApplyLightSettings` hid the Rect lights in game when switching to Point, but did not turn off their visibility flags. A previous Rect main light and Rect wrap light could remain visible in the editor preview. The fix connects those two existing Rect getters to the existing **Set Visibility (false)** nodes. This element repair changes only those connections; later editor-panel repairs are recorded in the [complete pack audit](CHARACTER_LIGHT_RIG_FULL_SYSTEM_AUDIT.md).
2. The project UDS rig had both day and night set to `DT_PP_Presets_Default / Cinematic`, and its fallback had no elements. That table uses `S_PostProcess`, not `S_LightPresetRow`. Runtime correctly rejected it, leaving no lights. Day and night now select `DT_NarrativeLightRigPresets / Day_Studio` and `Night_Moonlight`. The manual Studio fallback is restored. The user's separate 0.35 global intensity is preserved for that fallback; selected day/night rows use their own 0.25 and 0.10 values.

`BP_LightRigElement` is one light element, including its optional wrap light. Select a whole-rig preset on the rig or control panel. **Cinematic** in the post-process table is a camera/image look; it does not describe light elements. Use a full light preset table in **01 UDS Full Light Presets**. Do not clear the fallback if the rig must work while UDS is unavailable.

The existing Native reference is `ANarrativeLevelSequenceActor` and its sequence player/camera lifecycle, observed by `UTerritoryCinematicLightRigComponent` and `UTerritoryDialogueShot`. The existing pack extension points are `ApplyPreset`, `BPI_LightRigElement::Setup`, `UpdateGlobals`, and `UpdateLight`. The parent supplies globals before updating each element. This patch preserves that flow and the public Unreal light-component setters.

## Verification

`Scripts/Territory/verify_light_rig_elements.py` checks the actual six light components, including their visibility flags, intensity, color, attenuation, temperature, shadows, volumetric scattering, dimensions, textures and channels. It tests 65 character elements from all 13 pack rows plus both project rows, then six repeated type/wrap changes on the same actor. All 71 cases pass after the fix. Before the fix, both Rect-to-Point repetitions failed the main and wrap Rect visibility checks. Color is compared by channel values with Unreal's sRGB quantization; comparing Python struct wrapper identity would give a false failure.

The editor test uses transient actors, the pack's public Setup/UpdateGlobals flow, and guaranteed cleanup. It runs with PIE stopped. `validate_uds_configuration()` additionally rejects wrong table types, missing/empty selections and an empty fallback, beyond Unreal's generic asset validator. The existing `capture_preset_tables()` now invokes this configuration check.

HopDistrictTest PIE verifies actual component settings during Narrative day/night changes, camera cuts without rebuilding lights, rejection of the post-process table while retaining the current look, recovery after correcting the rows, Hashir's held Native dialogue, and dialogue exit with zero remaining rigs/elements. With the PIE sky removed, a fresh dialogue uses four fallback lights at the preserved 0.35 intensity. The authored level sky was not removed. No gameplay mutations were made through the Python snapshot recorder.

The element and UDS Blueprints compile with no errors or warnings. Five directly affected assets and the existing 20-asset preset audit validate without warnings. Native `TerritoryFramework.Presentation.Cinematics.OptionalLightRigLifecycle` passes without warnings. Evidence and the reviewed screenshot are under `Saved/Verification/20260914_LightRigElementFix`.

## Ownership and delivery

These are local cosmetic asset changes. Narrative retains playback, camera binding and clock authority. There are no RPC, replicated-state, save-record, owner/capture or World Partition registration changes. The existing local-rig lifetime remains unchanged. This pass did not repeat full engine builds, dedicated/two-client runs, campaign save/load, World Partition travel or packaged gameplay; prior integration evidence remains separate. It does not certify global HDR, interiors, exposure or groom quality.

The Character LightRig pack is ignored by the project repository and must not be added to the community plugin release. The corrected local asset is saved and its original binary is backed up in the evidence folder. `Scripts/Territory/light_rig_element_visibility_patch.json` records the two Blueprint connections without distributing the pack. After updating/reinstalling the pack, run the component regression. If it fails, inspect the installed graph and apply the manifest through Monolith only if its node/component contract still matches; never apply old node IDs blindly. Keep both existing Spot targets when adding the Rect targets.

The repaired UDS adapter remains a UE 5.8 project asset. No Narrative Pro source was changed. No core plugin API or Blueprint caller migration is required.
