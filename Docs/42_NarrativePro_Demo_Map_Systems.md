# 42. Narrative Pro's OpenWorld demo map — the vendor's complete system showcase

`/NarrativePro/Pro/Demo/Maps/OpenWorld/L_DemoMap_OpenWorld` — the vendor's own demo level, **2403 actors**,
~1.5 MB, no World Partition (every actor is inside the `.umap`). Sibling `L_DemoMap_Old.umap` is the older
layout. Nothing in it is Territory's; it is the reference for how Narrative expects each of its systems to be
wired, and several of its chains are worth copying outright.

Read it with `EditorLoadingAndSavingUtils.load_map(path)` (check `list_dirty_packages` first — this load
replaces the open level).

## What is in it, by system

| System | Actors / assets in the map | The implementation |
|---|---|---|
| **Cutscene trigger** | 2 only: `BP_Trigger_Cinematic2` (30787, −10429, 593) and **`BP_Trigger_Cinematic3`** (35879, −14507, 523) — both `BP_Trigger_Cutscene_C` with a `SphereComponent` | `BP_Trigger_Cutscene` (4 vars: `Level Sequence`, `DestroyOnFinished?`, `Settings:FNarrativeSequencePlaybackSettings`, `SequencePlayerActor:NarrativeLevelSequenceActor`). Graph: `Event Triggered By Character` / `ActorBeginOverlap` → `Get Level Sequence` → `Set SequencePlayerActor` → `Cast To BP_NarrativePlayerController` → **`Play Cutscene`** → `OnFinished` → `Branch(DestroyOnFinished?)` → `Destroy Actor` |
| **QTE** | the two Craty sequences (`LS_Craty_Kick`, `LS_Craty_Dodge`) | vendor QTE family: `BP_QTETrack` / `BP_QTESection` / `BP_QTETrackInst` + `S_QuickTimeEventOption` + `WBP_NarrativeMenu_QTE` / `WBP_QTEButton`. Options are `Dodge` and `Punch_Left`, each with an `OptionDestination` = the next sequence (the branch). Bound to `BP_NarrativePlayerController` with the input track alongside it |
| **QTE fail → damage/death** | `LS_Craty_Kick` and `LS_Craty_Dodge` | each sequence carries a **DirectorBP** with **Sequencer Events** (`CrateHitPlayer`, `SequenceEvent__ENTRYPOINT…`). The event calls `GetNarrativePlayerCharacter` and applies **`GE_Damage_SetByCaller`** (`SetByCaller.Damage`) — miss the QTE and the player takes the hit and dies. Both use `Narrative.State.Player.WantsHideHUD` for the HUD |
| **Quest / encounter** | `BRE_PlayerRobbery` (Cinematics/PlayerRobbery) | 72-node blueprint and the best quest reference in the pack: 7 vars (`WalkSpec:FActiveGameplayEffectHandle`, `NarrativeSequencePlayer`, `MoveToGoal:FInstancedNPCGoalItem`, `NodeSelector:FDialogueNodeSelector`, `WalkDownAlleySequence`, `RobberySequence`, `RobberySettings`). Flow: encounter/overlap → `Cast To NarrativePlayerCharacter` → `Set Triggerer` → **`Show Narrative HUD Notification`** → **`CreateNarrativeLevelSequencePlayer`** → get `NPCSpawn_Trickster`'s spawned NPC → `ApplyGameplayEffectToSelf` (the walk spec) → **`PlayNarrativeSequence`** → `GetActivityComponent` |
| **NPC population** | 17 `NPCSpawner` actors — `NPCSpawner_Craty`, `_MeleeRange{,2,3}`, `_Shootout{,2}`, `_Market`, `_MainHall`, `_SecretEvilMerchant`, `_MagicRange`, `_BanditPatrolTest{,2}`, `NPC_Felix` | native **`ANPCSpawner : ASpawnerBase`** (`Spawners/NPCSpawner.h`): `GetSpawnedNPCs(TArray<ANarrativeNPCCharacter*>&)` (BlueprintPure) and `CreateNPCSpawner()` (editor). The demo drives one per encounter |
| **Interaction** | 5 `BP_InteractableSeat_Couch`; 5 `BP_NarrativeDemoButton_SpawnDynamic` | the seat is native-driven (`BP_InteractableSeat_Couch` has no BP logic of its own). The demo button has `Spawn:SpawnerBase` and runs `On Interacted (NarrativeInteractable)` → `SpawnActors` → `NarrativeCharacterSubsystem::DestroyNPC` → `RemoveActors` + `NarrativeSaveSubsystem::RemoveSingleActor` |
| **POI / fast travel** | 9 `POIActor` | native **`APOIActor : ATargetPoint`** (`Navigation/POIActor.h`): `Narrative.POIs`-category tag, `POIIcon`, and a **`FastTravelCapsule`** + arrow component — a POI *is* a fast-travel point |
| **Combat** | 30 `BP_Cover`; 3 `BPV_Sedan_C`; 243 `BP_Spear` pickups | `BP_Cover` carries `Claimed?` / `ShouldCrouch?` and only a debug sphere in BP — the AI activity (`AI/Activities/Attacks/ShootFromCover`) owns the behaviour |
| **Loot / items** | 69 `BP_DefaultItemPickup` | native-driven (`Pro/Core/BP/World/BP_DefaultItemPickup`, no BP variables) |
| **Traversal** | 58 `LevelBlock_Traversable` | the vendor's traverse blocks, used with ladders and cages (`BP_Cage`, `BP_Ladder`) |
| **Presentation / demo furniture** | 44 `BP_DemoDisplay`, 44 `TextRenderActor`, `BP_DemoRoom`, `BP_DayCounter`, `BP_TOD*` | the demo's own info boards and a time-of-day demo corner — the reason the map is self-explaining |

## The one chain worth copying verbatim

```text
player walks into the sphere on BP_Trigger_Cinematic3
  -> BP_Trigger_Cutscene: Play Cutscene (on BP_NarrativePlayerController) with its Level Sequence
       -> the sequence has a QTE section (Dodge / Punch_Left, each with an OptionDestination)
            success -> the option's destination sequence plays (e.g. LS_Craty_Kick -> LS_Craty_Dodge)
            miss    -> the sequence's DirectorBP fires a Sequencer Event
                         -> GetNarrativePlayerCharacter -> GE_Damage_SetByCaller (SetByCaller.Damage)
                              -> the player takes the hit; enough of it and the player is dead
  -> OnFinished -> Branch(DestroyOnFinished?) -> Destroy Actor
```

## How this maps onto Territory

- **Cutscene playback** — Territory already owns a richer equivalent: `UTerritoryPlayCutsceneEvent`
  (server-only, exact target player, owns the sequence actor's lifetime through
  `UTerritoryCutsceneTeardownComponent`, releases movement/look at the end). Use the vendor trigger for
  authoring reference, Territory's event for project work.
- **QTE** — use the vendor's family as-is (`Docs/41` §10); Territory has nothing of its own here, and the
  fail path is a **DirectorBP sequencer event applying a damage GameplayEffect**, which is a project-side
  authoring pattern, not a system to rebuild.
- **Spawners, POI, interaction, loot, cover** — all vendor-native or vendor-BP; check `ANPCSpawner`,
  `APOIActor` and `BP_InteractableSeat_*` before writing anything Territory-side.
- **Quest/encounter authoring** — `BRE_PlayerRobbery` is the pattern to follow: a HUD notification, a
  sequence player created per encounter, an NPC goal, a walk-spec GameplayEffect and an activity hand-off.
