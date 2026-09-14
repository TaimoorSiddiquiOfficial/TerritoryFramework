# Optional character lights

The optional bridge and TDA example are implemented. Enable it per shot after reviewing that character and scene; broader story and rendering acceptance remains open below.

Leave **Light Rig Profile** empty on a Territory dialogue shot to use the normal scene lighting. Selecting a profile enables extra lights for that shot. **Light Rig Uses Listener** changes the subject from the speaker to the listener.

For a full cutscene, call **Follow Narrative Sequence** locally with its existing Narrative sequence actor, the intended local player, the subject, and a profile. This observes playback; it does not start a sequence. The same runtime rig follows camera changes. A paused last frame keeps its lights while its camera is displayed. Stopping, returning to gameplay, finishing without a held frame, explicit **Stop Light Rig**, and sequence actor removal release them. Dedicated servers and split screen skip the optional lights.

Create a **Territory Cinematic Light Rig Profile** from Territory Framework > Story & Quests. The profile contains the runtime rig Blueprint and the mesh names and bones it needs. It waits for Narrative's character visual to become ready. A missing or incompatible rig is skipped with a reason rather than retried every frame.

The optional editor section contains two buttons:

1. **Open Light Rig Control Panel** opens the assigned Editor Utility Widget. Play/Simulate must be stopped.
2. **Use Panel Look in Runtime Rig** copies only the configured preset properties from the selected preview to the runtime rig Blueprint. Review that Blueprint and use Save All. Scene actor/component references are rejected before any property is changed.

The panel reference and preview-copy configuration are editor-only. The framework has no dependency on `/Game/Character_LightRig`. A host project supplies its own adapter and third-party assets; these do not belong in the community plugin archive.

## TDA example

For automatic UDS day/night row selection, use the separate project profile
`DA_NarrativeCharacterLights_UDS`. Its rig selects exact full-preset table rows;
the gentler example rows are `Day_Studio` and `Night_Moonlight`. See
[UDS preset setup and verification](UDS_CHARACTER_LIGHT_PRESETS.md).

TDA supplies `/Game/HOPTRENDY/Cinematics/LightRig/DA_NarrativeCharacterLights`. Its runtime Blueprint is a child of the pack's `BP_LightRig_Actor_Base`. Its editor panel is a child of the requested `EUW_LightRig_ControlPanel`, so it keeps the pack's existing preview and preset controls. Use the profile's **Open Light Rig Control Panel** button to open this configured child.

The example skeleton table uses Narrative's attached MetaHuman visual: `Body` and `FaceMesh`. The runtime preset removes the pack's background element and uses four character light elements. The pack can use a main and wrap light for an element. It creates no unbound post-process actor and makes no mesh lighting-channel changes. Its starting intensity scale is 0.25, with global volumetric scattering disabled; this is a starting look for review, not an HDR or day/night quality guarantee.

Use the panel to select a preview rig and edit its preset, global settings and skeleton. Then use **Use Panel Look in Runtime Rig** on the profile and save the runtime Blueprint. Changes affect later instances of that Blueprint. Duplicate the runtime Blueprint and profile when different scenes need different looks. Existing story shots remain unchanged until a profile is selected.

Use a prepared preview character with the component and bone names selected in the panel. This bridge resolves Native visuals during gameplay; it does not create a new editor appearance-preview system for Narrative NPC definitions. Full artist preview acceptance with each authored character is still required.

## Adapter contract

Make a project child of the pack's runtime actor and implement **Territory Cinematic Light Rig**:

- **Prepare Light Rig** assigns the provided visual and camera before construction/BeginPlay. It returns true only when those inputs can be used.
- **Is Light Rig Ready** reports the actual initialization result after BeginPlay.
- **Set Light Rig Camera** updates the existing elements' camera references. It must not recreate the preset's lights.

The rig must be non-replicated. Own each spawned light with ChildActorComponents or provide equivalent actor teardown. Do not create an editor preview wrapper or a global post-process volume in this runtime actor. Avoid changing character lighting channels or level lights without an explicit restoration policy.

## Existing authorities and compatibility

References: installed Narrative Pro `UNarrativeDialogueSequence::BeginPlaySequence`, `UDialogue::PlayDialogueSequence`, `ANarrativeLevelSequenceActor`, `ULevelSequencePlayer::OnCameraCut/OnStop/OnFinished`, and `ANarrativeCharacter::GetCharacterVisual`.

`UTerritoryDialogueShot` remains a Native dialogue sequence. Native decides whether a repeated shot restarts and assigns its current shot after BeginPlaySequence returns. The optional component reconciles after that assignment. It does not add a second sequence player, change the camera, advance a quest, or alter gameplay tags.

Lights are local presentation only. Their session component and spawned actor are transient and non-replicated; there is no campaign save record or RPC. Streamed visual replacement releases the old rig and checks the new visual. Missing bones wait for a bounded interval. Missing cameras and disabled camera cuts produce no rig. Existing shot assets remain disabled without migration because the new profile reference defaults to empty.

## Verification record

Evidence: `Saved/Verification/20260914_OptionalLightRig` in TDA. The read-only gameplay recorder is `Scripts/Territory/verify_optional_light_rig_pie.py`.
The compact [verification record](Verification/OPTIONAL_LIGHT_RIG_2026-09-14.json) is kept with the plugin.

- UE 5.8 and UE 5.7 Editor, Development and Shipping builds pass. Both full automation suites pass 339/339, with zero failed or not run. Native warning-level logs remain in 35 results on 5.8 and 37 on 5.7. The new lifecycle test passes without warnings.
- The four project assets validate with zero warnings. The runtime Blueprint and child editor panel compile with zero errors or warnings.
- The actual child panel opens. Copying an edited intensity succeeds. A later batch containing a scene actor reference is rejected before its otherwise-valid intensity change is applied. Original preview values were restored and the temporary preview was removed.
- Listen host plus two clients: a local Native cutscene creates one rig only for its viewer. Two clients can each run their own rig while the host has none. Camera cuts reuse the same root and four child elements. A naturally paused final frame keeps them. Stop removes every root and child.
- Dedicated PIE server plus two clients: a request without a local viewer is refused on the server. Each client creates its own rig and four elements; the server creates none. Both clients' roots and children disappear on stop. This checks the local cutscene hook, not every authored replicated sequence-start path.
- A held Native dialogue fixture binds Hashir and the player through Native speaker APIs. Replaying the same shot preserves its root, children and camera. Listener mode selects the player's actual visual. Native dialogue exit removes every rig and child. This fixture does not change the Hashir greeting asset or certify its complete authored story.
- All 741 Narrative Pro source files match the installed Marketplace package.
- UE 5.8 cook/stage passes with the project profile explicitly included. The staged IoStore listing contains the profile, runtime adapter and project skeleton table, with no original/child editor control panel, editor preview wrapper or pack global post-process actor. The base pack's background class remains a cooked dependency of its parent Blueprint; the adapter filters it out of runtime instances. A first cook collided with a concurrent test tool's HTTP port; the isolated retry passed.
- The 60-second packaged Development game server-mode smoke exits 0. It is not a compiled TDAServer target or a packaged rendered dialogue test. Existing project/optional Native CutscenePlayerActor warnings remain tracked.

The TDA adapter assets were authored in UE 5.8. UE 5.7 builds/tests verify the generic bridge without this third-party project pack; they do not prove backward compatibility of UE 5.8 `.uasset` files. Reauthor or migrate project examples using supported content versions for each engine.

Rendered acceptance is still open. The initial dialogue screenshots show a hair/LOD change between captures, so they are not a controlled lighting comparison. Review warm and cold appearance, grooms, eyes, clothing, AlMalik UDS day/night and interiors, Lumen spill and GPU cost. Also verify an authored packaged dialogue, cutscene chains, travel/World Partition, respawn, disconnect and continuing-party camera ownership. Default-disabled compatibility does not certify those paths.
