# Character Light Rig: Narrative and Territory compatibility

Reviewed against the installed project on 2026-09-14. The research below informed
the optional implementation described in [Optional character lights](OPTIONAL_CINEMATIC_LIGHTS.md).
Research evidence: `Saved/Verification/20260914_PartyDepartureLightRig` in TDA.
Implementation evidence: `Saved/Verification/20260914_OptionalLightRig`.

## Implemented optional adapter

`UTerritoryCinematicLightRigProfile` and the project runtime Blueprint now supply
the Native visual, exact Body/FaceMesh skeleton configuration and current camera.
`UTerritoryDialogueShot` opts in per shot; full Native cutscenes expose an explicit
local `FollowNarrativeSequence` hook. The requested control panel is reused through
a project child widget and profile buttons that copy only approved preset fields.
All references to the third-party pack remain in TDA content. Existing shots have
no light profile selected. The original pack and Narrative Pro remain unchanged.

The runtime bridge waits for Native appearance readiness, rebinds existing child
lights on camera cuts, retains a displayed paused final frame and removes lights
when the session/camera ends. The project adapter filters background elements and
creates no global post-process actor. Dedicated servers and split screen skip it.
See the implementation guide for verified checks and remaining acceptance work;
the original research checklist below is not a claim that every story is tested.

## Result

`/Game/Character_LightRig` is a folder containing 95 assets. Its runtime rig can
be used with Narrative characters, Territory dialogue shots and Narrative
cutscenes. It needs a project-owned adapter for the current character visual,
current shot camera, preset and cleanup. Inheritance alone does not connect it.

Territory's `UTerritoryDialogueShot` already derives from Narrative's
`UNarrativeDialogueSequence`. A rig connected at that extension point can follow
the normal Narrative dialogue workflow. A full cutscene uses a different Native
player/actor lifecycle and needs the same lighting adapter connected there too.
The existing Territory local-player presentation subsystem currently handles
dialogue HUD state and participant LOD; it does not manage full-cutscene lights.

## What is actually in this asset pack

| Asset under `/Game/Character_LightRig` | Finding |
|---|---|
| `Blueprints/Core/BP_LightRig_Actor_Base` | Runtime `Actor`. BeginPlay calls `INITIALIZE`; Tick calls `UPDATE`. Explicit `CharacterActor`, `TargetCamera`, `SkeletonConfig`, `CurrentPreset` and `GlobalSettings` inputs. No Narrative/Territory interface. |
| `Blueprints/Core/BP_LightRig_Editor_Base` | Derives from `/Script/VPUtilitiesEditor.VPEditorTickableActorBase`. Keep this editor preview wrapper out of the runtime integration. |
| `Blueprints/EUW_LightRig_ControlPanel` | Editor Utility Widget. Useful for authoring a look; it is not the gameplay controller or a CommonUI replacement. |
| `Blueprints/Core/BP_LightRigElement` | Runtime actor with six light components: main and wrap versions of rect, point and spot lights. Native engine light setters apply intensity, shadows, volumetric scattering and lighting channels. `BPI_LightRigElement` supplies its setup contract. |
| `Blueprints/Core/BP_LightRig_Element_Background` | Optional background element created by the preset. Do not assume a portrait background belongs in a live story scene. |
| `Blueprints/Core/BP_PostProcess` | Enabled, unbound PostProcessComponent, priority 1, blend weight 1. Its defaults override minimum and maximum auto exposure. Automatically spawning it would affect the scene beyond the character. |
| `Blueprints/Data/Tables/DT_SkeletonConfig` | Three rows: MetaHuman, Mannequin_Manny and Mannequin_Quinn. These contain component names, bone names and direction axes. |

The runtime actor has no outgoing dependency on the editor wrapper or control
panel in the inspected asset registry. Its references include the runtime light
elements, Blueprint structs/interfaces and skeleton table. This is useful source
evidence. The runtime actor was also explicitly included in a UE 5.8 cook using
the cooker's supported `-PACKAGE` option. The staged IoStore listing confirms
the runtime actor and light element are present, with no editor wrapper, control
panel or unbound post-process actor. This proves dependency packaging; spawning
the rig during a packaged dialogue still needs a runtime integration test.

## Confirmed integration gaps

1. **Character visuals:** `InitializeReference` uses `GetComponentsByClass` on
   one actor and compares component object names with the skeleton row. The
   MetaHuman row expects `body` and `face`; it does not search attached actors.
   Narrative's rendered visual can be attached to the gameplay character.
   Territory's existing LOD bridge already visits those attached visuals. Pass
   the correct visual actor after Native appearance completion; do not assume
   the gameplay pawn itself owns both meshes. Live HopDistrictTest inspection
   confirmed `ANarrativeCharacter::GetCharacterVisual()` returns the attached
   `BP_MetahumanVisual` for the player and Hashir. Their components are `Body`
   and `FaceMesh`: the supplied `face` name will not match `FaceMesh`. Author a
   project-owned skeleton row with those exact names. Hashir's first snapshot
   had no ready head/eye sockets on that visual; the later snapshot had them.
   Actor existence alone is not readiness. Test each authored character.
2. **Camera changes:** the rig caches `TargetCamera` into its child elements
   during setup. Merely changing the root variable is insufficient evidence that
   the children follow a new shot. Native can replace its spawned Cinecam between
   lines or cut to another camera in a sequence. Rebind the element context on
   each actual camera change.
3. **Reinitialization cost:** `SyncPresetElements` destroys every existing child
   element and recreates the set. Do not call it every frame to follow a camera.
   Separate a preset rebuild from a camera/subject reference update in the
   project adapter, using the pack's existing element setup interface.
4. **Missing inputs:** the runtime rig retries initialization from its update
   loop when references are invalid. Missing character/mesh configuration can
   print errors repeatedly. A bridge should wait for readiness, stop when its
   session ends and report a missing required input once.
5. **Exposure ownership:** UDS/gameplay exposure and Territory's camera-local
   studio look already exist. Keep the pack's unbound post process opt-in for a
   deliberately authored studio scene. Character fill lights do not require a
   new global exposure controller.
6. **Distribution:** `/Game/Character_LightRig` is project content. A reusable
   Territory plugin must not hardcode these paths or bundle this third-party
   pack in community release artifacts. Put pack references in TDA content;
   keep the plugin's optional integration contract independent of the pack.
7. **Multiplayer:** this is cosmetic local presentation. A dedicated server
   does not need lights. Separate client processes have separate light worlds;
   split-screen players share one world and need an explicit policy for shared
   subjects/cameras. Ordinary lighting channels are not per-player visibility.

The existing rig permits channel configuration. Epic documents that light and
mesh channels must overlap and that their isolation covers direct lighting on
opaque materials. Do not assume channels isolate Lumen indirect light, hair,
translucent materials or every renderer path without a rendered test.
[Epic: Lighting Channels](https://dev.epicgames.com/documentation/unreal-engine/using-lighting-channels-in-unreal-engine).

## Integration pattern to implement

Use one project Blueprint adapter around the runtime rig and reuse it for both
entry points. Narrative remains the playback authority; the rig owns only lights.

**Dialogue shots:** extend `UTerritoryDialogueShot`, retaining
`UNarrativeDialogueSequence::BeginPlaySequence`, `PlaySequence`, `OnStop` and
`EndSequence`. Acquire the confirmed Cinecam after Native starts the selected
sequence, resolve Speaker/Listener to their ready visual actors, then initialize
the optional rig. Release it at shot/session stop, rejected replacement and
world teardown. Preserve Native's no-restart behavior for an unchanged shot.
Free-movement dialogues with no cinematic camera must skip camera-driven rig
creation or use an explicitly authored alternative.

**Full cutscenes:** keep `ANarrativeLevelSequenceActor` and
`UNarrativeLevelSequencePlayer`, including Native character binding, readiness,
tags and playback replication. Obtain the local player and bound character from
that existing sequence context. Observe the sequence player's camera cuts and
stop/finish lifecycle; update the same rig adapter's explicit inputs. A chain to
the next cutscene must transfer or release the old light ownership deliberately.
Do not replace Native's sequence player with another cinematic manager.

For a fixed authored cinematic, a rig can instead be a Sequencer spawnable with
explicit actor/camera bindings. Sequencer owns the temporary lifetime. Avoid
External spawn ownership unless cleanup is authored; it deliberately leaves
the actor alive after the sequence.
[Epic: Spawnables and Possessables](https://dev.epicgames.com/documentation/en-us/unreal-engine/spawn-temporary-actors-in-unreal-engine-cinematics).

Expose plain-English options for enable/disable, preset, subject selection,
intensity scale, background enable and lighting channels. Default to no rig when
the optional project adapter is unset. Preserve existing gameplay light settings.
Temporary mesh-channel changes require exact previous-value restoration and
shared-subject ownership, following Territory's existing LOD override pattern.

## Verification required before enabling it by default

- Compile and validate the project adapter with the runtime pack; test cooking
  without the editor wrapper/utility widget in its dependency path.
- Hashir and the player MetaHuman: correct body, face, groom and eye bone context;
  delayed appearance completion; pawn respawn; missing camera; missing preset.
- Dialogue: speaker/listener switch, repeated same shot, rejected replacement,
  free movement, leave, skip and normal completion. No abandoned light actors.
- Cutscene: start, camera cuts, pause, skip, finish, stop, chain, destruction and
  World Partition travel. No lingering lights or global post-process changes.
- Standalone, listen host, remote-only party, two clients and dedicated server;
  separately evaluate split-screen shared-world behavior.
- Rendered AlMalik day/night and interior comparisons with UDS, Lumen, shadows,
  fog, HDR output and frame time. A successful Blueprint call is not visual proof.

The inspection found all 95 asset records valid. Nine focused core
Blueprint/data-table assets passed Unreal validation with zero warnings and PIE
stopped. All eight core Blueprints compile UpToDate without errors or warnings.
Graph snapshots record the actual wiring and defaults; two live visual snapshots
prove the component-name and delayed-appearance findings above. The read-only
recorder is `Scripts/Territory/audit_character_light_rig.py`. Automatic rig
playback, rendered quality and full integration acceptance remain open.

## Party continuation decision

The user chose **continue the dialogue for remaining members** when a member
leaves. This requires member-local alias/camera/input cleanup, release of that
member's Native player-speaker tags, safe leader context and shared local-view
handling. Calling the party's `ExitDialogue` or deinitializing its shared server
`UDialogue` would violate that choice. Atomic transfer and the known immediate
join/start replication race remain distinct lifecycle gates.
