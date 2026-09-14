# UDS day and night character lights

TDA supplies an optional profile at `/Game/HOPTRENDY/Cinematics/LightRig/DA_NarrativeCharacterLights_UDS`. Select it in **Light Rig Profile** on a Territory dialogue shot, or pass it to **Follow Narrative Sequence** for a full cutscene. Existing shots are unchanged. The original `DA_NarrativeCharacterLights` remains the manual look.

Open the new profile's **Rig Class**, `BP_TerritoryUDSCharacterLightRig`. Under **01 UDS Presets**, choose a **Data Table** and **Row Name** for **Day Preset** and **Night Preset**. The starting selections are `Day_Studio` and `Night_Moonlight` in the project `DT_NarrativeLightRigPresets` table beside the profile. Duplicate the Blueprint and profile for a different scene's defaults. **Set UDS Presets** can change both row handles on an active local rig.

Use a complete rig preset table. The adapter applies its global settings and every character-light setting, including light type, color, intensity, shadows, target and wrap light. It removes background elements so a local conversation cannot change tagged lights elsewhere in the level. It does not apply a global post-process preset. The original pack assets are unchanged.

The project rows copy Studio and Moonlight, change their names, set global intensity to 0.25 for day and 0.10 for night, and disable global volumetric scattering. The original Moonlight row washed out the face under HopDistrictTest exposure. The gentler row retains visible face detail in the reviewed shot. These are starting values for scene review, not a global HDR calibration.

Selecting an original pack row applies its authored intensity and volumetric values unchanged. To tune another exact look, edit or duplicate a row in the project Data Table editor, then choose that row. In the control panel's preset table picker, select `DT_NarrativeLightRigPresets` to preview the project rows. The installed panel's existing refresh hook accepts this table. **Use Panel Look in Runtime Rig** copies the fallback look; it does not rewrite the selected day/night table rows. Keep authored table references in the cooked content; the adapter does not load arbitrary table paths from strings.

## Available full presets

| Table folder | Table | Exact row names |
|---|---|---|
| `Preset_Default` | `DT_LightPresets_Default` | `Studio`, `Moonlight`, `Split`, `Portrait`, `Thungsten`, `Fireside`, `TextureBooth`, `RedLantern` |
| `Preset_Starter` | `DT_LightPresets_Starter` | `Drama_Golden`, `Floral_Orchid`, `Fashion_Curtain`, `Nature_Overcast`, `Sunny_Sunset` |

Both tables are under `/Game/Character_LightRig/Blueprints/Data/Tables` and use `S_LightPresetRow`. `Thungsten` is the pack's actual row spelling. Preset names are artistic looks; their names do not cause day/night selection.

## What the other data means

| Asset | Use |
|---|---|
| `E_LightType` | Rect, Spot or Point light |
| `E_LightTarget` | Face/Body, Eyes or Hair |
| `E_BoneAxis` | X, Y, Z, negative X, negative Y or negative Z |
| `S_LightPresetRow` | Complete look: name, thumbnail, globals and light elements |
| `S_GlobalSettings` | Whole-rig intensity, color, distance, ray tracing, collision and light-channel settings |
| `S_LightElement` | One named element, its settings, enabled flag and debug flag |
| `S_LightElementSettings` | Placement, target, light type, color, shadows, wrap light and optional background tag |
| `S_SkeletonConfig` | Mesh component names, bones and direction axes |
| `S_PostProcess` | Separate post-process settings and blend amount |

The two `DT_LightElementPresets_*` tables contain 31 default and 25 starter individual elements. They cannot be selected as complete day/night looks. `DT_PP_Presets_Default` contains Default, Clarity, Cinematic, Softness and Grunge post-process presets; this runtime adapter does not use them. `DT_SkeletonConfig` contains MetaHuman, Mannequin_Manny and Mannequin_Quinn. The TDA rig inherits the project skeleton table with Narrative's `Body` and `FaceMesh` names.

## Time and lifetime

The source reference is the installed `Narrative_UDS_Sky` **Time of Day Animation** override. It reads `UArsenalStatics::GetTimeOfDay`; `ANarrativeGameState` owns the saved clock and replicates accumulated time. The adapter does not write either clock.

The adapter uses UDS's public **Get Ultra Dynamic Sky** and **Is it Daytime?** functions. The latter checks whether the sun is above the horizon. It therefore follows the rendered sky instead of assuming fixed sunrise/sunset hours. Keep one active Narrative UDS sky in the world, as required by the existing environment setup.

The adapter checks before the rig begins play and every 0.5 seconds while that local rig exists. This also catches quest time jumps and client clock updates. It caches the sky, finds it again if it is removed, and compares both table and row before applying a changed selection. Giving day and night the same row does not rebuild lights. A camera cut only rebinds the existing lights. End Play clears the timer; the existing sequence component owns rig and child cleanup.

A missing sky keeps the authored fallback or the last selected look and retries. A missing table, wrong row structure, missing row, or row with no character elements keeps the current look. **Preset Status** shows the reason on the live Blueprint instance. Correcting the selection recovers on the next check. Presets are treated as immutable during a shot; editing the contents of the same row in the editor requires a new rig instance.

The new Blueprint is a project child of the existing adapter. Its shared **Filter Character Preset** and **Prepare Character Light Rig** helpers keep the same Narrative visual/camera setup. Narrative retains dialogue and sequence playback. The rig is transient, local and non-replicated, with no campaign record or RPC. Dedicated servers and split screen are skipped by the existing bridge. A newly created rig reads the current sky instead of restoring stale cosmetic state.

## Verification and remaining review

Evidence is recorded under `Saved/Verification/20260914_LightRigUDSPresets` in TDA. Call `capture_preset_tables()` in `Scripts/Territory/verify_uds_light_presets.py` with PIE stopped before a new run. It validates actual asset records and exports the installed tables. Its `snapshot()` records gameplay without changing it, checking exact globals and every character-element field. The adjacent field schema records the installed pack's property names; a pack schema change must be re-audited. The test-only `DT_InvalidLightPresets` contains empty and background-only rows and is not referenced by runtime assets.

All 20 focused pack/project/test assets pass Unreal validation. Both project rig Blueprints compile without errors or warnings. The existing native optional-rig lifecycle automation passes. All 13 original full presets pass exact-value and child-count checks. Both tuned project rows also pass. Live Narrative clock changes select the day/night rows; each changed row rebuilds elements once while keeping the root. Identical day/night selections across a time jump and ordinary camera cuts do not rebuild lights.

Missing table, wrong table type, missing row, empty row, and background-only row all retain the last valid look. Starting without a sky uses the fallback; a later sky is found and selects the correct row. Removing and replacing the sky also recovers. Listen server plus two clients verifies local rigs, server-clock day/night changes, and cleanup. A held Narrative dialogue with Hashir verifies preset changes and Narrative exit cleanup. These tests do not certify AlMalik World Partition streaming or full campaign save/load.

Dedicated PIE with two clients also passes day/night selection and cleanup; the server creates no local rig. UE 5.8 cook/stage/package passes, and the 60-second packaged Development game in server mode exits 0. This is not a compiled dedicated-server target or rendered packaged dialogue test. The IoStore listing includes the runtime profile, adapter, skeleton and project preset table; the editor panel and failure-fixture table are excluded. The unchanged C++ bridge's previous six builds remain the baseline for this asset-only batch.

See the [verification record](Verification/UDS_LIGHT_PRESETS_2026-09-14.json).

Rendered approval remains separate: review AlMalik exteriors and interiors, sunrise/sunset, grooms, eyes, exposure, Lumen spill, fog and GPU cost. These presets do not fix the city's global lighting. The TDA assets are UE 5.8 project examples and require the installed Character Light Rig and UDS packs. They must not be included in community plugin archives or described as UE 5.7-compatible content.
