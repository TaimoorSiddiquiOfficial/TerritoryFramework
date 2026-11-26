# 41. Sequencer tracks — the vendor's track library, and what Territory should reuse

The vendor ships a complete **Blueprint authorable sequencer-track system**. Before writing a new
Territory subsystem or a C++ track for anything sequence-shaped, check this library and the engine
contract behind it: most of it already exists, and anything missing is a **Blueprint**, not code.

## 1. The library — `/NarrativePro/Pro/Core/BP/Sequencer/Tracks`

32 assets in nine families. Every family is a triple: `Track` + `Section` + `TrackInst`
(track instance = the runtime half).

| Family | Assets | What the instance does (graph-verified, see §3) |
|---|---|---|
| Dialogue | `BP_DialogueTrack` · `BP_DialogueSection` · `BP_DialogueTrackInst` | plays a looping montage/sequence for the section's frame range |
| GameplayTags | `BP_GameplayTagTrack` · `…Section` · `…TrackInst` | applies the section's `TagsToApply` to the animated actor while the section is active, removes them on exit |
| DistanceRequirement | `BP_DistanceTrack` · `…Section` · `…TrackInst` | per-interval distance check between the player character and a found character |
| Input | `BP_InputTrack` · `…Section` · `…TrackInst` | `DisableInput` / `SetIgnoreLookInput` from the section's `DisableAllInput?` / `DisableLookInput?`, restored on exit |
| Music | `BP_MusicTrack` · `…Section` · `…TrackInst` | `OverrideMusicWithSound` / `ClearOverrideMusicWithSound` with `FadeDuration` |
| MusicVolume | `BP_MusicVolumeTrack` · `…Section` · `…TrackInst` | `AdjustVolume` on the active audio component, restores `OldVolume` |
| QuicktimeEvent | `BP_QTETrack` · `…Section` · `…TrackInst` + `S_QuickTimeEventOption` + `WBP_NarrativeMenu_QTE` + `WBP_QTEButton` | looping anim for the QTE window + `BlendOutOfSequencer` on the character anim instance; ships its own menu/button widgets |
| Skip | `BP_SkipTrack` · `…Section` · `…TrackInst` | pushes the section's end frame into `CutsceneSkipFrame` on the local `BP_NarrativePlayerController` |
| UI | `BP_WidgetTrack` · `…Section` · `…TrackInst` + `WBP_WidgetPopup` + `WBP_ExamplePopup` | `Create Widget` → `AddToViewport` for the section's widget, tears the instance down on exit |

## 2. The engine contract — `CustomizableSequencerTracks`

`Engine/Plugins/MovieScene/CustomizableSequencerTracks` (built into UE 5.8). Three blueprintable
classes; a designer-authored track is a Blueprint of each.

### `USequencerTrackBP` (`SequencerTrackBP.h`)
`UCLASS(Blueprintable, Abstract, DisplayName=SequencerTrack)`, extends `UMovieSceneNameableTrack`.
Every authorable knob is `EditDefaultsOnly` **and `AssetRegistrySearchable`** — that is the whole
discovery mechanism: the Sequencer's *Add Track* menu reads these tags off the registry, so a new
Blueprint appears there with no registration call anywhere.

| Knob | Meaning |
|---|---|
| `TrackType` (`ECustomSequencerTrackType::RootTrack` / `ObjectTrack`) | binds to the sequence, or to an actor |
| `SupportedObjectType` | for `ObjectTrack`: which actor class the track may bind to |
| `DefaultSectionType` / `SupportedSections[]` | which section Blueprints this track accepts |
| `TrackInstanceType` | the `USequencerTrackInstanceBP` spawned at playback |
| `bSupportsMultipleRows` / `bSupportsBlending` / `Icon` | row stacking, easing, and the menu icon |

`CreateNewSection()`, `SupportsType()`, and the `Sections[]` array are implemented by the base class.

### `USequencerSectionBP` (`SequencerSectionBP.h`)
`UCLASS(Blueprintable, Abstract, DisplayName=SequencerSection)` — a plain `UMovieSceneSection` plus
`IMovieSceneEntityProvider`. The **section is the data**: whatever variables the designer adds to the
section Blueprint are the track's authorable fields. Its entity import is what turns an active
section into a track instance input.

### `USequencerTrackInstanceBP` (`SequencerTrackInstanceBP.h`)
`UCLASS(Blueprintable, Abstract, DisplayName=SequencerTrackInstance)`, extends
`UMovieSceneTrackInstance`. The runtime hooks (all `BlueprintImplementableEvent`):

| Hook | Fires |
|---|---|
| `OnInitialize` | instance created |
| `OnUpdate` | every frame (`OnAnimate`) |
| `OnBeginUpdateInputs` / `OnEndUpdateInputs` | around one input pass |
| `OnInputAdded(FSequencerTrackInstanceInput)` | a section became **active** |
| `OnInputRemoved(FSequencerTrackInstanceInput)` | a section **stopped being active** |
| `OnDestroyed` | instance destroyed |

Queries: `GetAnimatedObject()` (the bound object), `GetInputs()`, `GetNumInputs()`, `GetInput(i)`.
`FSequencerTrackInstanceInput` carries `{ USequencerSectionBP* Section; FMovieSceneContext Context; }`
— so a hook gets both the authored data (cast the section to your Blueprint) and the evaluation
context (time/range) without any extra plumbing.

## 3. What each vendor track does at runtime (verified from the graphs)

Dumped with `blueprint_query:get_graph_summary` on each `BP_*TrackInst`. The pattern is uniform:
**`OnInputAdded` applies, `OnInputRemoved` restores**, and all data comes off the section.

- **Dialogue** — `Break Sequencer Track Instance Input` → `GetStartFrameSeconds` / `GetEndFrameSeconds`
  → `Set Play Range (Seconds)` + `Set Play Range (Frames)` → `SetPlaybackPosition` →
  `Make Movie Scene Sequence Playback Params` → `PlayLooping`, gated by a `Set Active?` / `Branch`
  pair, reading the character mesh's anim instance to start the clip.
- **GameplayTag** — `GetAnimatedObject` → `Cast To Actor` → `Cast To BP_GameplayTagSection` →
  `Get TagsToApply` (plus a `GetDebugStringFromGameplayTagContainer` print path).
- **Skip** — `Cast To BP_NarrativePlayerController` → `Set LocalController` →
  `Set CutsceneSkipFrame ← GetEndFrame`.
- **QTE** — the Dialogue shape, then `Cast To NarrativeCharacterAnimInstance` →
  `BlendOutOfSequencer`.
- **Music** — `Get NarrativeMusicSubsystem` → `OverrideMusicWithSound` / `ClearOverrideMusicWithSound`,
  with `Max(Float)` and `FadeDuration` off the section.
- **MusicVolume** — `GetActiveAudioComponent` → `AdjustVolume` ← `VolumeMultiplier`; exit restores
  `OldVolume`.
- **Widget** — `Create Widget` → `AddToViewport`; exit clears `WidgetRef` and resolves the owning
  controller first.
- **Distance** — `Cast To BP_DistanceSection` → `Set Timer by Event` → custom event `Check Dist` →
  `NarrativeCharacterSubsystem::FindCharacter` → `GetDistanceTo`.
- **Input** — `Cast To BP_NarrativePlayerController` → `DisableInput` / `SetIgnoreLookInput`, each
  behind a `Branch` on the section's `DisableAllInput?` / `DisableLookInput?`.

## 4. Authoring a Territory track (no C++)

1. `blueprint_query:create_blueprint` — parent `SequencerTrackBP`, three assets in one folder
   (`…Track`, `…Section`, `…TrackInst` with parents `SequencerTrackBP`, `SequencerSectionBP`,
   `SequencerTrackInstanceBP`).
2. On the **track** CDO: `DefaultSectionType` = your section, `TrackInstanceType` = your instance,
   `TrackType` (`ObjectTrack` + `SupportedObjectType` when the track binds to an actor),
   `bSupportsMultipleRows` / `bSupportsBlending` / `Icon`.
3. On the **section**: `add_variable` for each authorable field. (Add the variable, then re-issue
   `set_variable_type` with the same type so the generated class really has it, then set the default.)
4. On the **instance**: `add_event_node` for `OnInputAdded` / `OnInputRemoved` — **never**
   `override_parent_function`, which leaves the Blueprint in an error state that still compiles clean
   and silently breaks the event. Then `Get Animated Object` → `Break Sequencer Track Instance Input`
   → cast the section → apply; `OnInputRemoved` restores.
5. `compile_blueprint` (0 errors / 0 warnings) → `validate_blueprint` → `save_asset`.

## 5. Territory vs the vendor — duplication audit

Read from both sides: Territory's `Public/Cinematics/*` and the vendor's
`NarrativeArsenal` cinematic/Tales headers.

| Territory feature | Vendor equivalent | Verdict |
|---|---|---|
| `UTerritoryDialogueShot : UNarrativeDialogueSequence`, the 7 shot sequences, `UTerritoryDialogueShotPackSpec` | **Base class only** (`UNarrativeDialogueSequence`); no shot concept in the track library | **Keep** — extending the vendor base is the correct reuse |
| Cinematic **bars** | **Yes, and ours is the deprecated route**: `NarrativeLevelSequenceActor.h:82` marks `bShowCinematicBars` *"DEPRECATED — the better way … is the `Narrative.State.Player.WantsCinematicBars` tag … using GameplayTag track"* | **Migrate** — drive bars with the tag (and `BP_GameplayTagTrack`), per-dialogue via the `UDialogue::WantsCinematicBars_Implementation` BlueprintNativeEvent |
| HUD hide, sequencer control, invulnerability during a cinematic | **Yes** — `TagsToApplyWhilstBound` defaults apply `Narrative.State.Player.WantsHideHUD`, `…SequencerControlled`, `…Invulnerable` to a bound character | **Use the vendor's** — do not re-implement |
| `UTerritoryCinematicPresentationSubsystem` (LODSync + Groom + skinned-mesh forced to LOD 0, shared-speaker ref counting, `IsInCutscene` reading the vendor's registry) | **None** — no vendor feature pins MetaHuman LOD/groom per dialogue | **Keep** — genuinely Territory's, and it reads vendor state instead of duplicating it |
| `UTerritoryCutsceneTeardownComponent` (destroys the sequence actor the vendor factory hands back; bounds `ULevelSequencePlayer::EnableCinematicMode`'s world-wide sweep to the audience the sequence names; authority-only) | **None** — the vendor factory "spawns the sequence actor and hands it back through `OutActor` with no self-cleanup of its own", and `EnableCinematicMode` never consults `OwnerControllers` | **Keep** — both gaps are real; the second is an engine limitation, not a vendor omission |
| Reply countdown / timed replies (`UTerritoryReplyTimeoutProfile`, party component) | **None** — `UDialogueNode_Player` has no time field | **Keep** |
| Tagged-dialogue single authority (`UTerritoryDiplomacyDialogueComponent::RequestTaggedDialogue` → `PlayTaggedDialogueLine`) | vendor `PlayTaggedDialogue` is **live**: `BP_NarrativeNPC` implements `ExecutePlayTaggedDialogue` with `Check Can Play Tagged Dialogue` (in-dialogue / alive / `MaxDistance` overlap / per-listener cooldowns / `AllListenersWantDialogue` → `TalesComponent::BeginDialogue`), and `BP_Hashir` + `BP_TerritoryGuard` override the same event | **Keep** — Territory's path stays the *code* call site's authority (tag → row → server checks), but the vendor path is not broken and covers sequence/montage triggers |
| QTE, cutscene skip, input windows, distance gating, music override/volume, UI popups from a sequence | **All present** as track families above | **Use the vendor's** — do not build a Territory QTE, skip system, or in-sequence popup |

#### Audit of the remaining Territory cinematic-adjacent classes

Every Territory class whose job touches a sequence, a cutscene or dialogue presentation, checked against
the vendor side (`Public/Cinematics`, `Public/Tales`, `Public/Social` + the vendor's cinematic and Tales
headers):

| Class | Vendor counterpart | Verdict |
|---|---|---|
| `UTerritoryDialogueShot`, `UTerritoryDialogueShotPackSpec`, `TerritoryShotRoleTags` | base class only (`UNarrativeDialogueSequence`) | keep |
| `UTerritoryCinematicLightRig` / `…Adapter` / `…Component` / `…Profile` | none | keep — per-shot character lighting is Territory's |
| `UTerritoryCinematicPresentationSubsystem` | none (no LOD/groom pinning) | keep — see §5 |
| `UTerritoryCutsceneTeardownComponent` | none | keep — see §5 |
| `UTerritoryAmbientDialogue : UDialogue` | `UDialogue` is the base | keep — it only defaults `bShowCinematicBars = false`, which is the right default for ambient chat |
| `UTerritoryDialogueLifecycleComponent`, `UTerritoryDialogueRecipe` | none | keep |
| `UTerritoryCutsceneCondition` (`UNarrativeCondition`) | `BP_DistanceTrack` gates distance *inside* a sequence; nothing gates *whether* a cutscene may start | keep |
| `UTerritoryDiplomacyDialogueComponent` + `PlayTaggedDialogueLine` | vendor `PlayTaggedDialogue` works (see the row above) | keep |

| `UTerritoryNarrativePartyComponent` reply countdown | none | keep |
| **`UTerritoryPlayCutsceneEvent`** | **vendor Tales task `BPT_PlayCutsceneAndWait` + `BP_Trigger_Cutscene*`** | **review** — a Territory event that spawns and plays a sequence may be re-doing the vendor's cutscene task; confirm its extra behaviour (audience bounding, teardown) before keeping both |
| `UTerritoryModifyReputationEvent`, `UTerritorySetDiplomacyEvent`, `UTerritoryLockEvent` / `UTerritoryUnlockEvent` | none — but `BP_GameplayTagTrack` can toggle a tag mid-sequence | keep the events; where a lock only has to last for the length of a sequence, a `BP_GameplayTagTrack` section is the cheaper authoring path |
| `UTerritoryAIObservationTask` and the other Territory tasks / conditions / goals | none | keep |

**Corrected 2026-10-15:** the earlier claim in this doc that the vendor's tagged-dialogue playback was an
unimplemented event was **wrong**. The vendor ships `BP_NarrativeNPC::ExecutePlayTaggedDialogue` (helper
`Check Can Play Tagged Dialogue` — in-dialogue / alive / `MaxDistance` overlap / per-listener cooldowns /
`AllListenersWantDialogue`), and both project NPC BPs override the same event.
`Docs/24_Stealth_Infiltration.md` had it right all along — that doc is the authority for the native/BP chain.

**Cinematic bars, settled:** `UDialogue::bShowCinematicBars` is **not** the deprecated field — it is the
dialogue's own switch, and `UDialogue::WantsCinematicBars_Implementation()` returns it
(`Dialogue.cpp:637`), after which Narrative itself applies and removes the loose
`Narrative.State.Player.WantsCinematicBars` tag (`Dialogue.cpp:722` / `:836`). Territory's party dialogue
already hands that tag from one owner to the next (`TerritoryPartyDialogue.cpp:39-45`). The **deprecated**
flag is the one on `ANarrativeLevelSequenceActor`'s bound-character settings (`:82`). What was wrong was
**the shot-pack tool forcing the dialogue's flag on** (`TerritoryDialogueShotEditorLibrary.cpp` used to
set `bShowCinematicBars` / `bAutoStopMovement` / `bFreeMovement`); it now sets only
`bAutoRotateSpeakers` and `DialogueBlendOutTime` and leaves presentation policy to the designer.

## 5b. Ours vs the vendor, field by field

Both sides read from the editor (`get_cdo_properties include_parent_defaults=true`, `get_variables`,
`get_graph_summary`). Vendor side = all nine families.

### Track knobs

| Knob | Vendor (×9) | Territory track | Action |
|---|---|---|---|
| `TrackType` | 7× `ObjectTrack`; Music and MusicVolume are `RootTrack` | `ObjectTrack` | matches |
| `bSupportsBlending` | `false` everywhere | `false` | matches |
| `DefaultSectionType` / `TrackInstanceType` | own section + own instance | same | matches |
| `bSupportsMultipleRows` | **`true` everywhere** | was `false` | **changed to `true`** — several dialogue events can now sit side by side on one track |
| `SupportedObjectType` | `Actor`, or narrowed to `BP_NarrativePlayerController` for Dialogue / Input / QTE | was `Actor` | **changed to `/Script/NarrativeArsenal.NarrativeNPCCharacter`** — a tagged line needs an NPC speaker, exactly the way the vendor narrows its player-bound tracks |
| `Icon` | set, with a tint (Dialogue and QTE green `0,218,113`) | class default (white) | optional: give it a Territory tint |

### Section = the data

| Vendor section | Variables | Territory section |
|---|---|---|
| `BP_GameplayTagSection` | `TagsToApply : GameplayTagContainer` | `EventTag : GameplayTag` |
| `BP_DialogueSection` | `DialogueOptions : array<NarrativeCineDialogueLine>` | — |
| `BP_DistanceSection` | `CharacterToCheck : CharacterDefinition`, `Distance : double`, `BlendOutFrame : string` | — |
| `BP_SkipSection` / `BP_InputSection` / `…` | one or two scalars each | — |

Open decision: make `EventTag` a **`GameplayTagContainer`** like the vendor's `TagsToApply`. It is the
vendor's idiom and allows several events per section, but the current wiring
(`Get EventTag → Request Tagged Dialogue.Tag`) takes a single tag, so that change also re-wires the graph.

### Instance = the runtime — the one structural difference

| | Vendor | Territory |
|---|---|---|
| Nodes in the graph | 26 (`BP_GameplayTagTrackInst`) | 9 |
| Where the work lives | **in the instance**: cast the animated object, cast the section, read the section's variables there, act there | **in a section function** `ApplySequenceEvent(Speaker)`: the instance only casts and calls it |
| Why | — | MCP cannot author a read of a variable **off another object**, so the section exposes its own action as a function; this one builds script-side and keeps the instance to four nodes |
| Behaviour | `OnInputAdded` applies, `OnInputRemoved` restores | `OnInputAdded` fires the line; `OnInputRemoved` deliberately empty (a server-owned line has no local state to restore) |

Moving the reads into the instance would match the vendor's shape exactly (a minute of drag-drop in the
editor); keeping the section function means a future track family is authored by editing one section and
never its instance. Both are valid — the choice is structural, not behavioural.

### What the track actually does — the intended difference

Every vendor family pairs with a helper the engine/narrative already has: tags applied, montages played,
distance checked, skip frame written, music overridden, widget shown, QTE anim blended, input disabled.
Territory's track calls the **tagged-dialogue authority** instead
(`UTerritoryDiplomacyDialogueComponent::RequestTaggedDialogue`, `BlueprintAuthorityOnly`), and the
vendor's own Dialogue track does not do that — it plays montages, not dialogue lines. That is the reason
this track exists.

### Conventions both sides share

`BP_<Family>Track` / `…Section` / `…TrackInst` naming; the section holds the data; the instance is the
runtime; `OnInputAdded` applies and `OnInputRemoved` restores; overrides are the `K2_*`
`BlueprintImplementableEvent`s; the track is discovered by `AssetRegistrySearchable` defaults and never
registered in code.

Remaining cosmetic/convention gaps: a per-family **sub-folder** (the vendor groups each family, ours sits
flat in `Tracks/`), the `Icon` tint, and the section's `DisplayName`.

## 6. Rules that follow

1. **Search this library first.** A sequence-shaped feature that already exists as a track family is
   not a Territory feature.
2. **Prefer a section variable over a new C++ field.** The section *is* the authoring surface.
3. **Player-facing cinematic state is a tag, not a flag.** Bars, HUD hide, sequencer control and
   invulnerability are `Narrative.State.*` tags applied to the player — the vendor's own deprecation
   note says so, and a tag can be added and removed mid-sequence by `BP_GameplayTagTrack`.
4. **A new Territory track is a Blueprint.** `AssetRegistrySearchable` knobs put it in the Add Track
   menu; there is nothing to register in C++.

## 7. Why Territory's own track was removed (task c, reversed)

Three BPs were built here — `BP_TerritorySequenceEvent{Track,Section,TrackInst}`, firing a Territory
tagged line from a sequence section. They have been **deleted**: the vendor already covers the capability,
and its own triggers are live in this project.

- `FNarrativeCineDialogueLine` (used by `BP_DialogueSection` through `BP_DialogueTrack`) already carries
  `Conditions[]`, `OptionText`, `HintText`, `DestinationSequence` and `bBlendAnimsBetweenSequences` —
  in-sequence interactive dialogue with branching, strictly more than our single tag.
- The vendor ships the trigger assets `AN_PlayTaggedDialogue` (anim notify) and `BTS_PlayTaggedDialogue`
  (behaviour-tree service), and they end in
  `ANarrativeNPCCharacter::ExecutePlayTaggedDialogue` — a `BlueprintImplementableEvent`.
- **That event is already implemented in this project.** `BP_Hashir` and `BP_TerritoryGuard` both carry
  `Event ExecutePlayTaggedDialogue` (pins `Dialogue`, `DialogueInstigator`), and the guard BP also owns
  `Get TaggedDialogue`, `Get TaggedDialogueCooldowns`, `Get TaggedDialogueLastPlayTimes`,
  `Get BlockTaggedDialogueTags`, `Event OnEnterDialogue` and `Event OnEndDialogue`. The earlier belief
  that this event was unimplemented here was **wrong** — the vendor path is live.

So a sequence plays a tagged line by putting a **montage carrying `AN_PlayTaggedDialogue`** on the bound
actor's track (or using the vendor's Dialogue track for interactive choice), with **no Territory asset
involved**.

Deletion was verified safe: `find_package_referencers_for_asset` returned **no referencers at all** for the
track, and the section and instance referenced only each other. They are not in git, so rebuilding means
re-running the §4 recipe plus the `blueprint_query` calls — `create_blueprint` with the three parents,
`set_cdo_properties` for the knobs, `add_variable`, `add_event_node` with the `K2_` names, `inputs` for a
function parameter, and a `BreakStruct` node for the input struct.

**Two lessons that survive the deletion**, because they apply to any custom track:

- The engine hooks are `K2_OnInputAdded` / `K2_OnInputRemoved`. `add_event_node {event_name: "OnInputAdded"}`
  silently creates a **custom event** that nothing calls, while `compile` still reports 0/0 and
  `validate_blueprint` stays clean. Read back the node's `class` — it must be `SequencerTrackInstanceBP`
  with title `Event OnInputAdded`.
- A track instance cannot be authored to read a variable **off its section** (the property node comes back
  with a `Value` output and no `self`), so the workable shape is a **void function on the section**
  (`set_function_params` with an `inputs` array), which the instance casts to its section and calls.

## 8. Verified state

- Library enumerated from the editor: 32 assets, 9 track instances dumped, behaviour quoted above.
- Engine contract read from the three headers of `CustomizableSequencerTracks` (UE 5.8).
- Duplication audit cross-checked against `NarrativeLevelSequenceActor.h`, `Dialogue.cpp`
  (`WantsCinematicBars`, `:637/:722/:836`), `NarrativeGameplayTags.cpp:156`, and Territory's
  `TerritoryCinematicPresentationSubsystem.h` / `TerritoryCutsceneTeardown.h` doc comments.

## 10. QTE — in-sequence prompts, and the project's working fight scene

The vendor QTE family (`Tracks/QuicktimeEvent/`): `BP_QTETrack` · `BP_QTESection` · `BP_QTETrackInst` ·
`S_QuickTimeEventOption` · `WBP_NarrativeMenu_QTE` · `WBP_QTEButton`.

### Data model (read off the assets)

`BP_QTESection` carries **`QuicktimeOptions : array<S_QuickTimeEventOption>`** and `LoopQTESection?`.
One option is:

| Field | Meaning |
|---|---|
| `TriggeringInput` (`DataTableRowHandle`) | the key/button row the prompt listens for. Rows come from **`/NarrativePro/Pro/Core/UI/ControllerData/DT_NarrativeInputActions`** (CommonUI input actions); the project also ships `BP_NarrativeInputData` |
| `OptionText` (`FText`) | the prompt label |
| `OptionDestination` (`ULevelSequence`) | **branch on success** — a different sequence to continue into (empty = carry on) |
| `Offset` (`Vector2D`) | where the button sits in the menu |
| `BlendAnimsBetweenSequences` / `KeepPlaybackSettings` / `PlaybackSettings` | how the hand-off to `OptionDestination` behaves |

`BP_QTETrackInst` spawns `WBP_NarrativeMenu_QTE` while a section is active, reads the bound
`NarrativeLevelSequenceActor` + local controller, and derives `EventDuration` from the section length. Like
the other actor-bound families it is an **`ObjectTrack` narrowed to `BP_NarrativePlayerController`**.

### The project's fight scene: `LS_Territory_Fight_QTE`

`/Game/TerritoryFramework/Cinematics/QTE/LS_Territory_Fight_QTE` started from the vendor's working Craty
example, but is now a Territory-owned authoring asset; the vendor assets remain untouched. Its verified
contract is:

- playback `0..223` at 30 fps, with continuous camera coverage `0..77` and `77..223`;
- one `BP_QTETrack_C` section on the Narrative player-controller binding, `64..124` (a two-second
  telegraph/input window), with one `Interact` option labelled `Dodge` and looping disabled;
- `OptionDestination` is the local
  `/Game/TerritoryFramework/Cinematics/QTE/LS_Territory_Fight_QTE_Dodge`, never the vendor demo branch;
- the `BP_InputTrack_C` section covers the complete main shot (`0..223`);
- `OpponentCine` attacks with Narrative core `Punch_Left` over `84..124`; the miss/failure reaction uses
  Narrative core `MM_HitReact_Front_Med_01` over `124..170`;
- special Narrative binding tags are `Player`, `PlayerController`, and one local `Opponent` binding.
  `OpponentCine` is a **possessable**, following the vendor's `CratyChar` pattern: gameplay owns the real
  Narrative actor and the sequence never spawns a duplicate combat/NPC authority. The copied `Craty`
  tag, crate/stand-in bindings, broken experimental opponent spawnable, and demo event tracks were removed;
- cameras are named `ActionCamera` and `WideCamera`; the cinematic opponent's transform evaluates for the
  complete shot rather than an empty `0..0` section. Both camera transforms and focal lengths were
  reauthored around the local fight origin rather than the Craty demo set.

The success branch is the Territory-owned
`/Game/TerritoryFramework/Cinematics/QTE/LS_Territory_Fight_QTE_Dodge`: playback `0..105`, one continuous
camera cut and input section, player roll `Roll_Combat_L_90_Seq` over `0..31`, and deliberately no QTE
track (so the branch cannot recurse).

Play the main sequence through Narrative's cutscene task/trigger or Territory's
`UTerritoryPlayCutsceneEvent`. The Territory event owns explicit audience selection and bounded teardown;
the Narrative sequence actor/player still owns special-tag binding, replicated audience playback and the
local QTE menu. Before playback, the intended live Narrative opponent must resolve the sequence's
`Opponent` binding tag (the same contract as the vendor demo's `Craty` character tag). Do not replace it
with a Sequencer-spawned NPC: spawning, definition readiness, death, GAS and combat authority stay with
Narrative/gameplay.

### Authoring your own beat

1. Open the sequence, find the `Quicktime Event Track` on the player-controller binding.
2. Right-click the track → add a QTE section; drag its edges — **the section's length is the prompt's
   window**.
3. On the section, add rows to `QuicktimeOptions`: pick `TriggeringInput` from `DT_NarrativeInputActions`,
   write `OptionText`, optionally set `OptionDestination` (another sequence) and nudge `Offset`.
4. Set `LoopQTESection?` if a miss should re-offer the prompt instead of moving on.
5. For a cinematic that is ours rather than the vendor demo, use a tagged **possessable** for every live
   fighter and give the gameplay actor the matching Narrative/actor identity before playback. Replace the
   per-binding skeletal-animation tracks in Sequencer; never add a duplicate NPC spawnable to make the
   preview look populated.

**Authoring limit:** editor Python can read and export `QuicktimeOptions`, but writing an element of this
Blueprint user-defined-struct array is not exposed reliably. Set `OptionDestination` through the Sequencer
section Details panel, then read it back through reflection. Animation params *are* writable, but the
modified struct must be assigned back to the section after changing it.

### Verification (2026-10-15)

- both QTE assets passed `Scripts/Territory/editor_validation.py`: 2 checked, 2 valid, 0 invalid,
  0 warnings;
- the read-only asset audit verified tags, bindings, event-track cleanup, playback/camera/input ranges,
  the non-spawning tagged-opponent contract, all three animation assets and ranges, the local branch
  destination and no recursive QTE track;
- `TerritoryFrameworkEditor` compiled and linked successfully after adding the asset regression;
- `TerritoryFramework.Presentation.Cutscenes`: 26/26 passed;
- `TerritoryFramework.Presentation.Cinematics`: 10/10 passed;
- `TerritoryFramework.Editor.Cinematics.FightQTEAssetContract`: 1/1 passed and now protects this exact
  authored contract, including the possessable `Opponent` binding and the absence of an opponent spawn
  track.

The automated Narrative-factory PIE smoke covered playback and the timeout/failure path without Blueprint
runtime errors. It did not synthesize a focused CommonUI `Interact` press, so final game-specific visual
sign-off of the success click still belongs in PIE with the real, definition-ready player and opponent.
The success destination and the non-recursive branch are protected structurally by the asset audit and the
native contract above.

This cinematic is presentation only. It does not own damage, rewards, capture progress or territory
ownership. Put those effects in the existing server-authoritative Tales/Territory transition, not in a
Sequencer event copied from demo content.
