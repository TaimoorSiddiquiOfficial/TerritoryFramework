# Use an original light element in a cinematic profile

A Territory Cinematic Light Rig Profile can now spawn either original pack element directly. It does not need a `BP_LightRig_Actor_Base` parent at runtime.

The TDA examples are in `/Game/HOPTRENDY/Cinematics/LightRig`:

| Profile | Rig Class | Element Adapter |
| --- | --- | --- |
| `DA_NarrativeSingleCharacterLight` | `/Game/Character_LightRig/Blueprints/Core/BP_LightRigElement` | `BP_TerritoryLightElementAdapter` |
| `DA_NarrativeBackgroundLights` | `/Game/Character_LightRig/Blueprints/Core/BP_LightRig_Element_Background` | `BP_TerritoryBackgroundLightElementAdapter` |

Child classes of the matching original element are accepted. Duplicate the example profile, expand **Element Adapter**, and edit its **Element Settings** and **Global Settings**. These settings belong to that profile; starting a shot makes a private temporary copy. Select the profile on your Territory dialogue shot, or pass it to **Follow Narrative Sequence** for a full cutscene.

For a character element, also choose a valid **Skeleton Config** row and matching profile mesh requirements. The example uses Narrative's attached MetaHuman visual with `Body` and `FaceMesh`. The adapter resolves the exact row type and both registered meshes before applying the element. A missing visual waits up to the profile's Ready Timeout; a failed setup skips the optional lights. A cold character appearance load may need a longer timeout.

For a background element, put `TerritoryCinematicBackground` in the **Actor Tags** of the scene lights you want to affect. The example uses that same tag in Element Settings. It accepts loaded `ALight` actors, such as Point Light, Spot Light and Rect Light actors, and their Blueprint children. Lights must be Stationary or Movable. A light component inside an arbitrary actor, a component tag, and a GameplayTag are different and are not selected by this pack API. Background profiles can leave Mesh Requirements empty and use an ordinary actor as their cutscene subject.

The background adapter remembers intensity, exact light color, temperature, use-temperature, ray-traced shadow mode and samples per pixel. It restores these on stop, camera loss, setup failure or rig destruction. It checks for newly loaded tagged lights at most twice per second. Removed tags release their lights; destroyed or unregistered lights are excluded. Two local sessions cannot take control of the same light at once. Use dedicated cinematic scene lights when another system also writes light settings; concurrent external changes are not merged into the captured starting values.

**Is Enabled** controls each element. A disabled background element leaves scene lights alone. The background adapter also applies Global Settings > **Use Ray Tracing**, which the original background update does not check. Its example intensity is `500 × 0.35 × 1 = 175`; the last factor is Background Intensity Multiplier. This is an editable example, not a recommended exposure for every scene.

These single-element profiles use a fixed element look. The existing whole-rig profiles retain the full preset arrangement, control panel workflow, and optional UDS day/night selection. The direct profiles leave the editor panel-copy fields empty because that operation copies to a rig Blueprint's defaults, not an instanced adapter. No camera post-process or automatic day/night preset selector is added to the direct adapters in this batch.

## Integration and migration

The design reference is Narrative Pro's instanced `UNarrativeDialogueSequence` and its `ANarrativeLevelSequenceActor` lifecycle. `UTerritoryCinematicLightRigComponent` still follows Narrative's active local camera; it does not create another playback authority. The project adapter calls the pack's existing `BPI_LightRigElement::Setup`, `UpdateGlobals` and `UpdateLight` functions. Character Light Rig continues to own placement, wrap lights, targeting, smoothing and light calculations.

`UTerritoryCinematicLightRigAdapter` is a generic instanced UObject extension. A private copy is prepared before actor construction, activated after components exist, updated during the active camera, and released on teardown. Scene-light leases are weak runtime references. The profile saves only configuration; runtime actor references and light snapshots are transient. There are no new RPCs or campaign records. Dedicated servers and split screen skip optional lights. Streaming discovery covers currently loaded actors, not a second World Partition registry.

Existing profiles with **Element Adapter** empty continue using the Territory whole-rig interface. No asset conversion is required. An original pack element needs its matching adapter; choosing any arbitrary actor is rejected by profile validation. The native plugin has no hard reference to the optional pack. Project example assets require the pack and were authored in UE 5.8; UE 5.7 needs content authored with compatible asset versions.

## Verification

The native regression is `TerritoryFramework.Presentation.Cinematics.DirectLightElementAdapter`. It covers setup order, configuration serialization, camera reuse, per-frame updates, invalid classes, exclusive scene-light ownership, complete restoration, late spawning, tag removal, destroyed lights, and repeated cleanup. Existing whole-rig lifecycle coverage remains enabled.

UE 5.8 and UE 5.7 Editor, Development and Shipping builds pass, including UHT. Both full automation suites pass 340 tests with zero failed or not run; 35 results on 5.8 and 37 on 5.7 retain existing warning logs. The new direct-element regression passes without warnings on both engines. All four runtime example assets validate, and the adapter Blueprints compile without errors or warnings.

HopDistrictTest checks the exact original character class against 128 actual component settings, camera-cut actor reuse, and cleanup. The background test uses a meshless subject, changes actual scene lights, finds a light spawned during playback, excludes a destroyed light, and restores every captured setting. Enable controls and the background global ray-tracing switch pass. After restarting the editor, dedicated PIE plus two clients creates one independent raw element in each client and none on the server. Stopping one client leaves the other active; stopping both removes every element. This background network check covers empty target sets; scene-light mutation/restoration is exercised in the single-world fixture.

`Scripts/Territory/verify_direct_light_elements.py` records real HopDistrictTest PIE actors and component values without changing gameplay. Live playback is driven through the existing `TerritoryLightRigTestDriver` and Narrative sequence APIs. Evidence and remaining limits are recorded in [the verification record](Verification/DIRECT_LIGHT_ELEMENTS_2026-09-14.json).

UE 5.8 cook/stage/package passes. The staged container includes both direct profiles, both adapters and the original element classes; editor panels, preview wrapper and test assets are absent. HopDistrictTest already contains a placed `BP_PostProcess` actor, so its class is also packaged. Neither direct profile has a hard dependency on it, and this batch does not change that map actor. A 60-second packaged Development game in server mode exits 0. Existing headless canvas and Native CutscenePlayerActor warnings remain; this is not a compiled TDAServer or a rendered packaged dialogue test. All 741 Narrative Pro source files still match the installed Marketplace package.

Rendered acceptance in AlMalik, real World Partition travel, an authored packaged dialogue, HDR quality and GPU cost remain separate gates. Spawning and restoring the requested classes does not certify those broader scene requirements.
