# 40 — Dialogue Shots and Shot Packs

Territory's cinematic dialogue-camera layer: what a dialogue shot is, how the vendor fallback
resolves, how the studio shot pack is built and applied, and every supported way to extend it.

Related docs: `37_Every_Option_Reference.md` (the `UTerritoryDialogueShot` option list),
`OPTIONAL_CINEMATIC_LIGHTS.md` (character light rig), `ROADMAP_AND_REMAINING.md` (open items),
`Verification/CUTSCENE_AUDIENCE_2026-09-24.md` + `Verification/CUTSCENE_SUPPRESSION_OWNERSHIP_2026-09-25.md`
(cinematic-audience behaviour).

---

## 1. The two layers

| Layer | Class | Lives in |
|---|---|---|
| Vendor cinematic dialogue | `UNarrativeDialogueSequence` (`Tales/NarrativeDialogueSequence.h`) | `Plugins/NarrativePro` — never modified |
| Territory shot | `UTerritoryDialogueShot : UNarrativeDialogueSequence` | `Plugins/TerritoryFramework/Source/.../Public/Cinematics/TerritoryDialogueShot.h` |

`UTerritoryDialogueShot` is `UCLASS(Blueprintable, EditInlineNew)`: it is an *instanced* object. The shot
pack builds it **inline** inside a dialogue (`NewObject` with the dialogue as outer, `RF_Transactional`),
and reuses the existing instance when the outer already matches. A shot may also be authored as its own
data asset or Blueprint subclass.

### 1.1 The vendor fallback chain (authoritative)

```
replayed NPC line : Line.Shot            -> Speaker.DefaultSpeakerShot -> DefaultDialogueShot
player reply pick : LastNode.SelectingReplyShot -> PlayerSpeaker.SelectingReplyShot -> DefaultDialogueShot
playback          : UDialogue::PlayDialogueSequence(Sequence, Speaker, Listener)   (Dialogue.cpp:1924)
                    ALevelSequenceActor* DialogueSequencePlayer + CurrentDialogueSequence
stop              : UDialogue::StopDialogueSequence()
```

So a dialogue has four shot slots, not one:

| Slot | Property | Pack default |
|---|---|---|
| Master | `UDialogue::DefaultDialogueShot` | Two Shot |
| NPC speaker | `FSpeakerInfo::DefaultSpeakerShot` (per entry) | Medium Close-Up |
| Player speaker | `UDialogue::PlayerSpeakerInfo.DefaultSpeakerShot` | Medium Close-Up |
| Player reply selection | `UDialogue::PlayerSpeakerInfo.SelectingReplyShot` | Reaction |

A line-level `Line.Shot` always wins over its speaker slot. Every slot is optional.

### 1.2 The `Cinecam` binding convention

The authored Level Sequence owns composition and camera movement; the shot class configures the camera
that is **tagged `Cinecam`**. `TerritoryDialogueShotEditorLibrary.cpp` defines:

```cpp
namespace TerritoryDialogueShotEditor {
    constexpr int32 ShotDurationFrames = 720;                 // 24 fps -> 30 s per shot
    const FName CinecamBindingTag(TEXT("Cinecam"));
}
```

A valid Territory shot sequence therefore has all four of: a **`Cinecam` spawnable**, an explicit
**full-range Spawn track**, a **full-length Camera Cut**, and role-specific **lens / focus / aperture /
composition**. A sequence missing the `Cinecam` spawnable is what the tool reports as an incomplete shot.

---

## 2. The studio shot pack

Authoring lives in the editor-only module `TerritoryFrameworkEditor`, as
`UTerritoryDialogueShotEditorLibrary` (`Public/Cinematics/TerritoryDialogueShotEditorLibrary.h`).
All three entry points are `UFUNCTION(BlueprintCallable)` and take an optional designer-authored
shot pack spec:

```cpp
FTerritoryDialogueShotPackBuildReport CreateStudioDialogueShotPack(
    const FString& DestinationContentPath,
    const UTerritoryDialogueShotPackSpec* ShotSpec = nullptr);
FTerritoryDialogueShotPackBuildReport ApplyStudioDialogueShotPack(
    UDialogueBlueprint* DialogueBlueprint,
    const FTerritoryDialogueShotPackBuildReport& ShotPack,
    const UTerritoryDialogueShotPackSpec* ShotSpec = nullptr);
FTerritoryDialogueShotPackBuildReport CreateAndApplyStudioDialogueShotPack(
    UDialogueBlueprint* DialogueBlueprint, const FString& DestinationContentPath,
    const UTerritoryDialogueShotPackSpec* ShotSpec = nullptr);
```

Passing a spec makes the tool build from that asset's rows instead of the built-in table; passing
nothing keeps the previous behaviour exactly.

- `CreateStudioDialogueShotPack` *creates **or repairs*** the seven 24 fps Narrative-ready sequences.
- Destination tooltip: **"Use `/Game`, never the Narrative Pro vendor plugin."**
- `ApplyStudioDialogueShotPack` assigns master, speaker and reply-selection defaults, enables cinematic
  bars and movement control, and **preserves every authored line-level shot**.

Report fields: `bSucceeded`, `bDialogueConfigured`, `DestinationContentPath`, `SequenceAssets[]`,
`ShotRoles[]`, `ShotGuidance[]`, `Errors[]`, `Warnings[]`.

`bDialogueConfigured` is the tool's own validation, and is the value to trust:

```cpp
Report.bDialogueConfigured = DialogueBlueprint->Status != BS_Error
    && RuntimeTemplate
    && RuntimeTemplate->DefaultDialogueShot
    && RuntimeTemplate->PlayerSpeakerInfo.DefaultSpeakerShot
    && RuntimeTemplate->PlayerSpeakerInfo.SelectingReplyShot;
```

Reading those slots back from Python returns `None` / "Failed to find property" — the inline objects are
not exposed to Python reflection. **Trust `bDialogueConfigured`.**

### 2.1 The built-in spec table

`namespace TerritoryDialogueShotEditor` holds `FStudioShotSpec`, a `static const TArray<>` of seven rows:

| Role | Asset | Camera location | Rotation | Focal | Aperture | Focus (cm) | Best use |
|---|---|---|---|---|---|---|---|
| Establishing | `LS_Territory_DLG_Establishing` | (-440, -125, 58) | (-4, 0, 0) | 35 | 4.0 | 440 | Open the location, show geography, establish both speakers before coverage. |
| Two Shot | `LS_Territory_DLG_TwoShot` | (-315, -48, 28) | (-2, 0, 0) | 45 | 3.2 | 315 | Default dialogue master shot; keeps speaker geography readable and edits safe. |
| Over Shoulder | `LS_Territory_DLG_OverShoulder` | (-215, 52, 13) | (-1, 0, 0) | 55 | 2.8 | 215 | Coverage with foreground depth; alternate screen side without crossing the 180-degree line. |
| Medium Close-Up | `LS_Territory_DLG_MediumCloseUp` | (-155, 27, 5) | (0, 0, 0) | 65 | 2.8 | 155 | Primary speaking coverage; facial emphasis while preserving shoulders and gesture. |
| Close-Up | `LS_Territory_DLG_CloseUp` | (-108, 13, 3) | (0, 0, 0) | 85 | 2.4 | 108 | Emotional story beat; reserve the shallow depth of field for important lines. |
| Reaction | `LS_Territory_DLG_Reaction` | (-132, -24, 4) | (0, 0, 0) | 75 | 2.5 | 132 | Silent listener response and player reply selection; creates editorial rhythm. |
| Insert | `LS_Territory_DLG_Insert` | (-92, 0, -28) | (8, 0, 0) | 90 | 3.2 | 92 | Story prop, hand action, weapon, contract, or Territory handover detail. |

The camera locations encode one staged conversation axis: each step pulls in (focal 35 -> 90) and opens
up (aperture 4.0 -> 2.4 / focus 440 -> 92 cm). `ConfigureCameraTemplate` writes the row straight onto the
shot's `CineCameraActor` template.

### 2.2 Applying to a dialogue

`ApplyStudioDialogueShotPack` maps roles to dialogue slots through one inline-shot helper:

```cpp
UTerritoryDialogueShot* CreateInlineShot(UDialogue* Dialogue, UTerritoryDialogueShot* ExistingShot,
    const TCHAR* ObjectStem, ULevelSequence* Sequence, const FText& DisplayName,
    const ETerritoryDialogueShotRole Role, const EAnchorOriginRule AnchorRule,
    const EShotTrackingRule TrackingRule, const float FocalLength, const float Aperture,
    const bool bUse180DegreeRule = true);
```

It reuses the inline instance when `Shot->GetOuter() == Dialogue`, otherwise makes a uniquely named
`UTerritoryDialogueShot` on the dialogue and calls `ConfigureEditorShot(...)`. Wiring is:

- `Dialogue->DefaultDialogueShot` = inline **Two Shot**, `AOR_ConversationCenter` + `STR_Speaker`, 45 mm, f/3.2
- `Speakers[i].DefaultSpeakerShot` = inline **Medium Close-Up** (`Territory_NPC_<i>_MCU`)
- `PlayerSpeakerInfo.DefaultSpeakerShot` / `SelectingReplyShot` = **Medium Close-Up** / **Reaction**

The pack is rejected if `TwoShot`, `MediumCloseUp` **or** `Reaction` is missing:

```cpp
"The shot pack must contain Two Shot, Medium Close-Up, and Reaction sequences."
```

At the end the blueprint is compiled (`FKismetEditorUtilities::CompileBlueprint`) and the slots are read
back off the generated class's dialogue template. When `Speakers` is empty the tool warns:

> "This Dialogue has no explicit NPC speaker entries. NPC lines use the configured Territory default
> two-shot fallback; line-level shots still override it."

### 2.3 Verified state (2026-10-15)

```
folder   /Game/TerritoryFramework/Cinematics/Dialogue/Shots
         LS_Territory_DLG_{Establishing,TwoShot,OverShoulder,MediumCloseUp,CloseUp,Reaction,Insert}
before   the seven shots were incomplete — the tool reports
         "Existing Level Sequence '{0}' has no Cinecam spawnable."
after    CreateStudioDialogueShotPack -> bSucceeded true, Errors [], Warnings []
         saved; then applied to six Territory dialogues, every one bDialogueConfigured true:
           DBP_TerritoryGuard_WarnHolsteredWeapon, DBP_GuardNews, DBP_BlacksmithHandover,
           DBP_FarmHandover, DBP_BlacksmithRetakeHandover, DBP_BlacksmithRetakePlanning
```

Notes: a stale duplicate `LS_Territory_DLG_Establishing1` may linger; the tool writes the canonical
`LS_Territory_DLG_Establishing`. The pack leaves every shot's `LightRigProfile` empty, which means normal
scene lighting (see `OPTIONAL_CINEMATIC_LIGHTS.md`). Always persist afterwards —
`unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)` — because the seven sequences and
every configured dialogue become dirty.

---

### 2.4 Designer-authored specs

`UTerritoryDialogueShotPackSpec` (`Public/Cinematics/TerritoryDialogueShotPackSpec.h`, runtime module,
`UPrimaryDataAsset`) carries the same rows as editable data:

```cpp
USTRUCT(BlueprintType) struct FTerritoryStudioShotSpec {
    ETerritoryDialogueShotRole Role;      // same role taxonomy as the built-in table
    ETerritoryDialogueShotSlot Slot;      // None | Master | SpeakerCoverage | ReplySelection
    FString AssetName; FText DisplayName; // asset to create or repair, and its designer name
    FVector CameraLocation; FRotator CameraRotation;
    float FocalLength; float Aperture; float ManualFocusDistance;
    FText BestUse;                        // guidance returned in the pack report
};
UCLASS(BlueprintType) class UTerritoryDialogueShotPackSpec : public UPrimaryDataAsset {
    TArray<FTerritoryStudioShotSpec> StudioShots;
    int32 ShotDurationFrames = 0;         // 0 = the built-in 720-frame default
};
```

It is a **runtime** class on purpose: a `UPrimaryDataAsset` saved under `/Game` cannot be editor-only.
The project authors it at `/Game/TerritoryFramework/Cinematics/Dialogue/DA_TerritoryStudioShotPack`.

`ApplyStudioDialogueShotPack` resolves each dialogue slot from the spec first and falls back to the
built-in role lookup: a row with `Slot == Master`, `SpeakerCoverage` or `ReplySelection` supplies that
slot's sequence, role, focal length and aperture. The required-slot check therefore becomes "master,
speaker-coverage and reply-selection must each resolve", so a pack is rejected only when one of those
three cannot be produced.

Following the repository's authoring contract, the class is registered in the Territory content-browser
menu (`Private/TerritoryFrameworkEditor.cpp`) with a named factory
(`UTerritoryDialogueShotPackSpecFactory` in `Private/Assets/TerritoryAssetFactories.h`); the
`TerritoryFramework.Editor.Authoring.AllDataAssetMenusAndFactories` test fails without both.

Verified 2026-10-15: the spec asset built and applied the pack with `succeeded true`, zero errors and
zero warnings, and the seven sequences it produced were the same seven the built-in table produces.

#### Shots are opt-in: nothing plays a shot on its own

Native resolves a shot per NPC line (`Dialogue.cpp:1758-1779`) as
`Line.Shot` → `Speaker.DefaultSpeakerShot` → `DefaultDialogueShot` → **otherwise `StopDialogueSequence()`**,
and per reply prompt (`Dialogue.cpp:1093-1103`) as
`LastNode.SelectingReplyShot` → `PlayerSpeakerInfo.SelectingReplyShot` → `DefaultDialogueShot`.

A dialogue-level or speaker-level shot therefore plays on **every** line of that dialogue, so a shot pack
must never be left behind on a dialogue by default — a gameplay bark with the built-in pack (720 frames at
24 fps) would cut to a thirty-second camera in the middle of a fight, and `UDialogue` freezes the listener
while it plays. Territory's rule is therefore: **a shot plays only where the designer placed one.**
`ApplyStudioDialogueShotPack` still writes the pack when it is invoked for a dialogue by name, but the first
pass had been applied across the whole dialogue set and was reverted. Verified slot by slot:
`default_dialogue_shot`, `PlayerSpeakerInfo.DefaultSpeakerShot` and `PlayerSpeakerInfo.SelectingReplyShot`
read `None` on all eleven dialogue blueprints, and no dialogue line carries a line-level `Shot`.

Where to place a shot yourself, in increasing scope:

| Scope | Field | Plays |
|---|---|---|
| One line | `FDialogueLine::Shot` (per NPC line in the dialogue editor) | only that line |
| One speaker | `FSpeakerInfo::DefaultSpeakerShot` | every line that speaker speaks |
| Whole dialogue | `UDialogue::DefaultDialogueShot` | every NPC line, plus the reply prompt when the reply slots are empty |
| Reply prompt | `PlayerSpeakerInfo.SelectingReplyShot`, or a reply node's own `SelectingReplyShot` | while the player is choosing |

Leaving all of them empty is the vendor's own "no shot" state: it calls `StopDialogueSequence()` and plays the
line on the normal gameplay camera. Re-applying the pack to a dialogue you want cinematic is a single call —
`apply_studio_dialogue_shot_pack(DialogueBlueprint, DA_TerritoryStudioShotPack)`.

#### Movement during a line

`UDialogue` freezes the listener by default — `bAutoStopMovement = true` and `bFreeMovement = false` are
private defaults (`Dialogue.cpp:56,58`) applied at `Dialogue.cpp:655` — unless the caller overrides them
through `FDialoguePlayParams` (`Dialogue.h:118-148`). Native's own quest path asks for free movement
(`Quest.cpp:26-27`), and so does Territory's ambient guard conversation
(`UTerritoryConversationListenerComponent::ResolvePlayParams`). The tagged-line path was the one place that
did not: `UTerritoryBlueprintLibrary::PlayTaggedDialogueLine` now sets the free-movement pair (and the
stop-movement pair) from its `bKeepMovementDuringLine` parameter, which defaults to true, so a guard warning
or a bandit taunt no longer roots the player mid-fight. Story dialogues that genuinely want a freeze keep
passing `bStopMovement = true` (see `UTerritoryStoryOwnerSpawner`).

#### Role tags and optional lights

Two further fields make the pack data-driven where it used to need code:

- `ShotRoleTag` (`FGameplayTag`) names the shot's editorial role. `TerritoryShotRoleTags`
  (`Cinematics/TerritoryShotRoleTags.h`) defines the seven native tags
  `Territory.Cinematic.Shot.{Establishing,TwoShot,OverShoulder,MediumCloseUp,CloseUp,Reaction,Insert}`
  and `FromLegacyRole()` maps the enum onto them. A row that leaves the tag empty gets the tag derived
  from its `Role`; a row may also carry a tag that no enum value represents, so a designer can name a new
  editorial role without extending `ETerritoryDialogueShotRole`. The resolved tag is written onto every
  inline shot (`UTerritoryDialogueShot::ShotRoleTag`, read through `GetShotRoleTag()`), so a quest or a
  validation pass can key off the role.
- `LightRigProfile` plus `bLightRigUsesListener` are copied onto each inline shot, which already carried
  the runtime lighting fields. Leaving them empty keeps the shot on normal scene lighting.

Verified in the saved assets: the three dialogue slots of a configured dialogue carry
`Territory.Cinematic.Shot.TwoShot`, `…MediumCloseUp` and `…Reaction`, and the shots reference
`DA_NarrativeCharacterLights_UDS`.

#### Speakers, and why the tool needs a helper

`ApplyStudioDialogueShotPack` only gives a speaker its own coverage when the dialogue actually has that
speaker entry in `Dialogue->Speakers`; Narrative matches a line to a speaker by id, so an empty
`Speakers` array means every NPC line falls back to the single master shot. The engine exposes
`FSpeakerInfo::SpeakerID` as read-only to Python, so speakers cannot be authored from a script; the
tool therefore publishes `EnsureDialogueSpeaker(DialogueBlueprint, SpeakerID, NPCDefinition)`, which adds
one entry when it is missing and never modifies an existing one. Run it before applying the pack.

Verified 2026-10-15 on five story dialogues (`DBP_Hahsir`, `DBP_Hahsir_Greet`, the three
`DBP_HopBanditLeader_*`): each gained its speaker entry, its own shot, and its
"no explicit NPC speaker entries" warning disappeared.

## 3. Extending the shots

### 3.1 What the vendor base class already gives you (no code)

`UNarrativeDialogueSequence` is `Blueprintable, EditInlineNew` and already exposes:

| Property | Effect |
|---|---|
| `SequenceAssets[]` | more than one sequence per shot — e.g. 180-degree left/right variants |
| `AvatarToTrack` (`EShotTrackingRule`) | who the camera tracks (speaker / listener / custom) |
| `AnchorOriginRule` + anchor offsets | anchor the shot to the conversation centre, a speaker, or a custom point |
| `bUse180DegreeRule` + framing | screen-side discipline |
| camera settings + manual focus distance | lens / depth-of-field control |

Most framing, tracking and anchoring requests are satisfied here, purely in the editor.

### 3.2 Extension levels

| Level | What you add | Cost |
|---|---|---|
| 0 — Designer | unlimited inline `UTerritoryDialogueShot`s on any dialogue / speaker / line / reply slot; bind any Level Sequence, including the vendor `Shot_*` library and the CityStreets `Shot_01..48` sequences | none, editor only |
| 1 | a new shot in the pack | one `FStudioShotSpec` row + re-run the tool |
| 2 | a new **role** (e.g. Dolly Zoom, Crane) | **no code needed**: give the spec row a `ShotRoleTag` (§2.4). Extend the enum only when the role must also own a dialogue slot |
| 3 | a new **studio look** | a value in `ETerritoryCinematicStudioLook` + a case in `GetResolvedStudioSettings` / `ApplyStudioLookToCamera` |
| 4 | new shot **behaviour** (shake, bespoke tracking, multi-sequence edits) | subclass `UTerritoryDialogueShot`, override `BeginPlaySequence` |
| 5 — **implemented** | designer-authored packs | `FTerritoryStudioShotSpec` rows in a `UTerritoryDialogueShotPackSpec` asset — no code per shot (see §2.4) |

Level 5 is implemented (see §2.4): the pack rows live in a `UPrimaryDataAsset`, so this plugin's C++ is
out of the shot-authoring loop and shot authoring happens in the editor alongside the rest of the
cinematics. The plan that produced it is
`.hermes/plans/2026-10-15_023638-dialogue-shot-pack-data-driven.md`.

### 3.3 Authoring a single new shot today (Level 0)

1. Create (or pick) a Level Sequence with a `Cinecam` spawnable, a full-range Spawn track, a full-length
   Camera Cut, and the camera you want.
2. Add the shot inline wherever it belongs — dialogue master, speaker, a replayed line (`Line.Shot`), or
   a reply (`PlayerSpeakerInfo.SelectingReplyShot`).
3. Set `ShotRole`, `AnchorOriginRule`, `AvatarToTrack`, `bUse180DegreeRule`, lens / aperture / focus.

No C++ and no pack rebuild is involved. Line-level shots always win, and `ApplyStudioDialogueShotPack`
never deletes them.

---

## 4. Pitfalls

- The pack installs **inline** shots. Python read-back of the dialogue slots returns `None`; use
  `bDialogueConfigured` as the signal.
- A shot sequence without a `Cinecam` spawnable is incomplete — the tool says so explicitly.
- Keep `LightRigProfile` empty unless the shot genuinely needs character lights; empty means normal scene
  lighting.
- Dialogues with no explicit NPC speaker entries fall back to the Territory default two-shot for NPC
  lines; add speaker entries to give each NPC its own coverage.
- The destination path must be under `/Game`; never write into the Narrative Pro vendor plugin.
- Re-running either entry point is safe: creation repairs, application reuses the inline instances and
  preserves line-level shots.
- A new Territory DataAsset class is not finished until it is registered in the content-browser menu and
  given a named factory: `TerritoryFramework.Editor.Authoring.AllDataAssetMenusAndFactories` derives its
  list from every non-abstract `UDataAsset` in the runtime module, so an unregistered class fails the suite.

---

## 5. Symbols referenced

`UTerritoryDialogueShot`, `ETerritoryDialogueShotRole`, `ETerritoryCinematicStudioLook`,
`ConfigureEditorShot`, `BeginPlaySequence`, `GetResolvedStudioSettings`, `ApplyStudioLookToCamera`,
`UTerritoryDialogueShotEditorLibrary::{CreateStudioDialogueShotPack, ApplyStudioDialogueShotPack,
CreateAndApplyStudioDialogueShotPack}`, `FTerritoryDialogueShotPackBuildReport`,
`FStudioShotSpec`, `ConfigureCameraTemplate`, `CreateInlineShot`, `CinecamBindingTag`,
`UDialogue::{DefaultDialogueShot, PlayerSpeakerInfo, Speakers}`,
`UNarrativeDialogueSequence::{SequenceAssets, AvatarToTrack, AnchorOriginRule, bUse180DegreeRule}`.

Added by the data-driven pack work: `FTerritoryStudioShotSpec`, `ETerritoryDialogueShotSlot`,
`UTerritoryDialogueShotPackSpec`, `UTerritoryDialogueShotPackSpecFactory`,
`UTerritoryDialogueShot::ShotRoleTag` + `GetShotRoleTag()`,
`TerritoryShotRoleTags::{Establishing,TwoShot,OverShoulder,MediumCloseUp,CloseUp,Reaction,Insert}` +
`FromLegacyRole()`, `UTerritoryDialogueShotEditorLibrary::EnsureDialogueSpeaker`.
