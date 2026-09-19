# 37 — Every Option Reference

**Complete, generated reference for every option a designer can set from the editor.**

This file is **generated from the plugin's C++**, not written by hand. Every option that carries
`EditAnywhere` in `Source/TerritoryFramework` appears here, with its real name, its real type, the
default the code actually initialises it to, and the description the author wrote into `meta`.
Nothing in the tables is invented — if a description is missing, the table shows it as missing
rather than filling the gap with a guess.

| | |
|---|---|
| Options documented | **787** |
| Classes / structs covered | **117** |
| Source areas | **9** |
| Options whose author wrote a description | **389** |
| Options with no description in source | **398** |
| Options with a default written in code | **487** |

**What this file deliberately does not cover.** It lists options, not behaviour. For *what the
system does* and *why*, read the plain-English guide that pairs with this one, and the numbered
system docs (00–36). This file answers one question well: *"this option in the Details panel —
what is it, what type is it, and what does it start as?"*

**Scope note.** Only `Source/TerritoryFramework` is scanned. The separate
`Source/TerritoryFrameworkEditor` module is editor-only tooling and its options are not shipped to
a game build, so they are out of scope here.

---

## How to read a row

| Column | Meaning |
|---|---|
| **Option** | The C++ property name. This is what appears in the Details panel unless a display name is set. |
| **Shown as** | The `DisplayName` the author set. Blank means the panel shows the C++ name. |
| **Type** | The declared type, exactly as written in the header. |
| **Default** | The value the code initialises it to. `—` means no initialiser is written, so the type's zero/empty value applies (0, false, empty array, null pointer). |
| **What it does** | The author's own `ToolTip` text, verbatim. **“no description in source”** is printed when none was written — that is a real gap in the plugin, shown honestly rather than guessed. |
| **Rules** | Numeric clamps and `EditCondition` gates, as declared. |

---

## Index

**AI — NPC behaviour, diplomacy, perception** — 10 option(s) in 3 class(es)

- `FTerritoryFactionDialogueProfile` — 2
- `UTerritoryDiplomacyDialogueProfile` — 7
- `UTerritoryPatrolGoal` — 1

**Cinematics — presentation and shot settings** — 34 option(s) in 4 class(es)

- `FTerritoryCinematicStudioSettings` — 13
- `FTerritoryLightRigMeshRequirement` — 2
- `UTerritoryCinematicLightRigProfile` — 7
- `UTerritoryDialogueShot` — 12

**Combat — assaults, counterattacks, targeting** — 121 option(s) in 10 class(es)

- `FTerritoryAssaultApproach` — 16
- `FTerritoryCounterAttackQuestRule` — 4
- `FTerritoryDifficultyVehicleCount` — 2
- `FTerritoryFactionAssaultConfig` — 32
- `FTerritoryPlayerPowerTier` — 2
- `FTerritoryStoryPursuitOptions` — 16
- `UBTService_TerritoryAssaultPermission` — 3
- `UBTTask_ReleaseTerritoryPermission` — 1
- `UBTTask_RequestTerritoryPermission` — 2
- `UTerritoryCounterAttackProfile` — 43

**Core — definitions, districts, places, guards** — 297 option(s) in 23 class(es)

- `ATerritorySavableData` — 1
- `ATerritoryWorldState` — 2
- `FTerritoryCapturePointTemplate` — 6
- `FTerritoryFactionGuardDefinition` — 2
- `FTerritoryGuardBehaviorTemplate` — 12
- `FTerritoryGuardPatrolTemplateNode` — 3
- `FTerritoryGuardPostTemplate` — 21
- `FTerritoryManagementPointTemplate` — 7
- `FTerritoryNotificationSettings` — 9
- `FTerritoryPatrolNode` — 4
- `FTerritoryPropertyGameplayBenefit` — 7
- `FTerritoryStateAudioConfig` — 10
- `FTerritoryStateConfig` — 3
- `FTerritoryStateGameplayRules` — 10
- `FTerritoryStoryOwnerTemplate` — 8
- `UTerritoryCityDefinition` — 2
- `UTerritoryDefinition` — 36
- `UTerritoryDeveloperSettings` — 78
- `UTerritoryDisguiseProfile` — 13
- `UTerritoryDistrictDefinition` — 4
- `UTerritoryGuardPostDefinition` — 13
- `UTerritoryPlaceDefinition` — 7
- `UTerritoryStealthProfile` — 39

**Economy — production, resources, currency** — 39 option(s) in 7 class(es)

- `FTerritoryProductionNotifications` — 9
- `FTerritoryProductionRule` — 12
- `FTerritoryProductionStockCap` — 3
- `FTerritoryProductionStockCondition` — 4
- `FTerritoryResourceRate` — 3
- `UTerritoryFactionResourceAccountComponent` — 6
- `UTerritoryProductionProfile` — 2

**Interaction** — 3 option(s) in 1 class(es)

- `UTerritoryDistractionComponent` — 3

**Navigation** — 28 option(s) in 4 class(es)

- `ATerritoryRoadGuide` — 8
- `FTerritoryVehicleAwarenessSettings` — 12
- `FTerritoryVehicleRetirementSettings` — 3
- `UTerritoryMapMarker` — 5

**Tales — narrative tasks, quest cascades, story events** — 251 option(s) in 62 class(es)

- `ATerritoryNarrativeQuestStarter` — 6
- `FTerritoryDialogueRecipeNode` — 8
- `FTerritoryQuestCascadeBranch` — 7
- `FTerritoryQuestCascadeState` — 6
- `FTerritoryQuestRuntimeOverrideRule` — 7
- `UTerritoryAIObservationTask` — 8
- `UTerritoryActivateDisguiseEvent` — 1
- `UTerritoryAssaultCondition` — 8
- `UTerritoryAssaultTask` — 4
- `UTerritoryCancelEnemyWavesEvent` — 4
- `UTerritoryCaptureEligibilityCondition` — 7
- `UTerritoryCaptureEvent` — 5
- `UTerritoryCaptureTask` — 3
- `UTerritoryCharacterActionTask` — 3
- `UTerritoryClearExposureEvent` — 2
- `UTerritoryCombatProgressTask` — 5
- `UTerritoryConditionGroup` — 2
- `UTerritoryControlProgressCondition` — 5
- `UTerritoryDialogueRecipe` — 3
- `UTerritoryDiplomacyCondition` — 3
- `UTerritoryDisguiseCondition` — 3
- `UTerritoryDisguiseIdentityCheckEvent` — 2
- `UTerritoryDisguiseTask` — 3
- `UTerritoryEventContextCondition` — 5
- `UTerritoryExecuteResourceRecipeEvent` — 5
- `UTerritoryExposureCondition` — 2
- `UTerritoryFactionDistrictHoldingCondition` — 4
- `UTerritoryGameplayStateTask` — 7
- `UTerritoryGarrisonCondition` — 4
- `UTerritoryHierarchyStoryOverrideEvent` — 5
- `UTerritoryLockEvent` — 2
- `UTerritoryModifyReputationEvent` — 4
- `UTerritoryNarrativeCheckpointEvent` — 3
- `UTerritoryNarrativeConditionTask` — 4
- `UTerritoryNarrativeDataTask` — 3
- `UTerritoryOwnerHandoverEvent` — 2
- `UTerritoryOwnershipCondition` — 5
- `UTerritoryPresenceCondition` — 2
- `UTerritoryProductionStatusCondition` — 3
- `UTerritoryQuestCascadeRecipe` — 12
- `UTerritoryQuestStateCondition` — 2
- `UTerritoryReportDistractionEvent` — 2
- `UTerritoryReputationCondition` — 4
- `UTerritoryResourceCondition` — 4
- `UTerritoryRevealInfiltratorEvent` — 1
- `UTerritoryScheduleEnemyWaveEvent` — 7
- `UTerritorySetDiplomacyEvent` — 10
- `UTerritorySetDisguiseCoverEvent` — 3
- `UTerritorySetGarrisonTargetEvent` — 2
- `UTerritorySetNarrativePlayerFactionsEvent` — 3
- `UTerritorySetStealthOverrideEvent` — 3
- `UTerritorySituationCondition` — 7
- `UTerritorySituationProfile` — 3
- `UTerritoryStartBossChaseEvent` — 3
- `UTerritoryStateCondition` — 5
- `UTerritoryStateTask` — 3
- `UTerritoryStealthEvidenceCondition` — 3
- `UTerritoryStealthPolicyCondition` — 2
- `UTerritorySuspicionCondition` — 2
- `UTerritoryUnlockEvent` — 2
- `UTerritoryUpgradePropertyEvent` — 1
- `UTerritoryWaitTimeCondition` — 2

**UI — Command Center, HUD, journal, theme hooks** — 4 option(s) in 3 class(es)

- `UTerritoryDistrictRowWidget` — 1
- `UTerritoryEconomyWidget` — 2
- `UTerritoryProductionSiteRowWidget` — 1

---

## AI — NPC behaviour, diplomacy, perception

### `FTerritoryFactionDialogueProfile`

*struct* · `Source/TerritoryFramework/Public/AI/TerritoryDiplomacyDialogue.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `DialogueProfile` | — | `TObjectPtr<UTerritoryDiplomacyDialogueProfile>` | `—` | Relationship dialogue profile for this exact faction. | — |
| `Faction` | — | `FGameplayTag` | `—` | Exact current NPC faction that selects this profile. | `Categories` |

### `UTerritoryDiplomacyDialogueProfile`

*profile DataAsset* · `Source/TerritoryFramework/Public/AI/TerritoryDiplomacyDialogue.h` · 7 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `AllianceDialogue` | — | `TSubclassOf<UDialogue>` | `—` | *no description in source* | — |
| `CeasefireDialogue` | — | `TSubclassOf<UDialogue>` | `—` | *no description in source* | — |
| `NeutralDialogue` | — | `TSubclassOf<UDialogue>` | `—` | Dialogue when no treaty exists. Example: cautious but not hateful. | — |
| `NonAggressionDialogue` | — | `TSubclassOf<UDialogue>` | `—` | *no description in source* | — |
| `SameFactionDialogue` | — | `TSubclassOf<UDialogue>` | `—` | Dialogue for an interactor who shares an exact Narrative faction tag with the NPC. | — |
| `TradeAgreementDialogue` | — | `TSubclassOf<UDialogue>` | `—` | *no description in source* | — |
| `WarDialogue` | — | `TSubclassOf<UDialogue>` | `—` | Dialogue used while the NPC and interactor factions are at War. | — |

### `UTerritoryPatrolGoal`

*class* · `Source/TerritoryFramework/Public/AI/TerritoryPatrolGoal.h` · 1 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `TerritoryPatrol` | — | `TArray<FTerritoryPatrolNode>` | `—` | *no description in source* | — |

## Cinematics — presentation and shot settings

### `FTerritoryCinematicStudioSettings`

*struct* · `Source/TerritoryFramework/Public/Cinematics/TerritoryDialogueShot.h` · 13 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `BloomIntensity` | — | `float` | `0.15f` | Camera-local bloom. Keep below 0.25 for the supplied looks. | `ClampMin` `ClampMax` |
| `Contrast` | — | `float` | `1.f` | Uniform RGBA contrast multiplier. Keep close to 1.0 and preserve MetaHuman skin and Groom detail. | `ClampMin` `ClampMax` |
| `ExposureCompensation` | — | `float` | `0.f` | Shot-local exposure compensation in stops. This layers over the gameplay exposure authority only while the Cine Camera is active. | `ClampMin` `ClampMax` |
| `FilmGrainIntensity` | — | `float` | `0.02f` | Subtle camera-local film grain. Set to zero for clean Groom and facial-quality review. | `ClampMin` `ClampMax` |
| `LocalExposureBlurredLuminanceBlend` | — | `float` | `0.5f` | Blend between bilateral and blurred luminance. 0.5 is a stable midpoint for Lumen scenes. | `ClampMin` `ClampMax` |
| `LocalExposureDetailStrength` | — | `float` | `1.f` | Local-exposure detail strength. 1.0 is the neutral recommended value. | `ClampMin` `ClampMax` |
| `LocalExposureHighlightContrast` | — | `float` | `0.8f` | Local-exposure highlight contrast. Epic's recommended working range is 0.6 to 1.0. | `ClampMin` `ClampMax` |
| `LocalExposureShadowContrast` | — | `float` | `0.8f` | Local-exposure shadow contrast. Higher values preserve deeper night silhouettes. | `ClampMin` `ClampMax` |
| `MotionBlurAmount` | — | `float` | `0.25f` | Camera-local motion blur. 0.25 is a restrained cinematic preview; Movie Render Queue temporal sampling owns final offline blur quality. | `ClampMin` `ClampMax` |
| `Saturation` | — | `float` | `1.f` | Uniform RGBA saturation multiplier. Keep close to 1.0; never enter different per-channel values for this studio control. | `ClampMin` `ClampMax` |
| `VignetteIntensity` | — | `float` | `0.12f` | Camera-local vignette. Use restrained values so gameplay-to-dialogue cuts do not look filtered. | `ClampMin` `ClampMax` |
| `WhiteBalanceTemperature` | — | `float` | `6500.f` | Camera white balance in Kelvin. Warm Dusty Day uses 6000 K; Moonlit Blue Night uses 4300 K. | `ClampMin` `ClampMax` `EditCondition` |
| `bOverrideWhiteBalance` | — | `bool` | `false` | Override white balance on this Cine Camera only. Leave disabled for a neutral shot controlled entirely by Narrative Ultra Dynamic Sky. | — |

### `FTerritoryLightRigMeshRequirement`

*struct* · `Source/TerritoryFramework/Public/Cinematics/TerritoryCinematicLightRig.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ComponentName` | — | `FName` | `—` | Exact skeletal mesh component name on the character visual. For Narrative MetaHumans use Body or FaceMesh. | — |
| `RequiredSockets` | — | `TArray<FName>` | `—` | Bones or sockets this rig needs. Lights wait until every listed socket exists on this component. | — |

### `UTerritoryCinematicLightRigProfile`

*profile DataAsset* · `Source/TerritoryFramework/Public/Cinematics/TerritoryCinematicLightRig.h` · 7 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `AuthoringPanel` | — | `TSoftObjectPtr<UObject>` | `—` | Optional editor control panel for this rig pack. This reference is removed when cooking the game. | — |
| `ElementAdapter` | Element Adapter | `TObjectPtr<UTerritoryCinematicLightRigAdapter>` | `—` | Optional setup for a pack's original element classes. Configure its element settings here. Each local cutscene gets its own temporary copy. Leave empty for an existing Territory whole-rig Blueprint. | — |
| `MeshRequirements` | — | `TArray<FTerritoryLightRigMeshRequirement>` | `—` | Required component names and sockets. Match the rig's skeleton configuration. Character lights need at least one mesh. Background-only adapters can leave this empty. | — |
| `PreviewPropertiesToCopy` | — | `TArray<FName>` | `—` | Only these editable preset properties are copied from the preview to the runtime rig defaults. Do not list character or camera references. | — |
| `PreviewRigProperty` | — | `FName` | `—` | Name of the panel variable holding its preview rig. Used by Use Panel Look in Runtime Rig. Leave empty if the panel does not support copying a preview. | — |
| `ReadyTimeout` | — | `float` | `10.f` | How long to wait for a streamed character visual and its bones. A failed rig is skipped for this subject, with one warning. | `ClampMin` `ClampMax` |
| `RigClass` | — | `TSubclassOf<AActor>` | `—` | Runtime actor to create: a whole rig or an individual light element. Whole rigs use the Territory Cinematic Light Rig interface. Original pack elements need the matching Element Adapter below. | — |

### `UTerritoryDialogueShot`

*class* · `Source/TerritoryFramework/Public/Cinematics/TerritoryDialogueShot.h` · 12 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Aperture` | — | `float` | `2.8f` | *no description in source* | `ClampMin` `ClampMax` `EditCondition` |
| `CustomStudioSettings` | — | `FTerritoryCinematicStudioSettings` | `—` | Editable camera grade used when Studio Look is Custom. | `EditCondition` |
| `FocalLength` | — | `float` | `65.f` | *no description in source* | `ClampMin` `ClampMax` `EditCondition` |
| `FocusSmoothingSpeed` | — | `float` | `8.f` | *no description in source* | `ClampMin` `ClampMax` `EditCondition` |
| `LightRigProfile` | — | `TObjectPtr<class UTerritoryCinematicLightRigProfile>` | `—` | Optional extra character lights for this shot. Leave empty to use normal scene lighting. The profile holds a project runtime rig and its character requirements. | — |
| `ShotRole` | — | `ETerritoryDialogueShotRole` | `ETerritoryDialogueShotRole::MediumCloseUp` | *no description in source* | — |
| `StudioLook` | — | `ETerritoryCinematicStudioLook` | `ETerritoryCinematicStudioLook::Neutral` | Neutral, warm daylight, moonlit night, or custom camera-local look. It never edits the level-wide gameplay grade. | `EditCondition` |
| `StudioLookBlendWeight` | — | `float` | `1.f` | Blend weight for the Cine Camera post process. 1.0 applies the selected profile fully during the shot. | `ClampMin` `ClampMax` `EditCondition` |
| `bApplyCinematicStudioLook` | — | `bool` | `true` | Keeps gameplay on the neutral global HDR baseline and applies this look only to the spawned Narrative Cine Camera. | — |
| `bApplyLensOverride` | — | `bool` | `true` | *no description in source* | — |
| `bLightRigUsesListener` | — | `bool` | `false` | Light the listener instead of the speaker. Only one subject rig is created for this shot. | `EditCondition` |
| `bSmoothTrackingFocus` | — | `bool` | `true` | *no description in source* | — |

## Combat — assaults, counterattacks, targeting

### `FTerritoryAssaultApproach`

*struct* · `Source/TerritoryFramework/Public/Combat/TerritoryCounterAttackTypes.h` · 16 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ApproachID` | Approach ID | `FName` | `—` | Stable unique ID on this Territory, for example Blacksmith_WestRoad. Blank IDs are auto-filled in the editor. | — |
| `EntryType` | — | `ETerritoryAssaultEntryType` | `ETerritoryAssaultEntryType::OnFoot` | Choose On Foot for a normal Narrative NPC route, or Narrative Vehicle for a road arrival that reuses Narrative's vehicle, seat, interaction ability, controller possession, and ZoneGraph road network. | — |
| `MaxWaveSize` | — | `int32` | `4` | Largest part of one wave allowed to use this approach. This never creates infinite reserves. | `ClampMin` |
| `MaximumVehicleDeployments` | — | `int32` | `1` | Maximum Narrative vehicles deployed by this approach during one assault. Later finite attackers use the drop-off as an on-foot entry. | `ClampMin` `ClampMax` `EditCondition` `EditConditionHides` |
| `RelativeSpawnTransform` | — | `FTransform` | `—` | Spawn point relative to the Place actor. Example: 2,000 cm west of the Blacksmith entrance. | — |
| `RelativeVehicleDropOffTransform` | — | `FTransform` | `—` | Vehicle park and dismount transform relative to this Place. Put it on a ZoneGraph road with a NavMesh walk route into the Place. | `EditCondition` `EditConditionHides` |
| `RoadGuideID` | — | `FName` | `—` | Stable ID of a placed Territory Road Guide. Blank looks for a guide whose ID matches this Approach ID. When found, its spline start/end replace the fallback spawn/drop-off transforms. | `EditCondition` `EditConditionHides` |
| `RoadLaneSide` | — | `ETerritoryRoadLaneSide` | `ETerritoryRoadLaneSide::Right` | Directional lane on the Road Guide. Reverse story pursuits automatically mirror left/right so both directions stay on the correct side. | `EditCondition` `EditConditionHides` |
| `Type` | — | `ETerritoryAttackApproachType` | `ETerritoryAttackApproachType::Road` | Meaning of this Place ingress route for UI and project rules. It does not replace navigation validation. | — |
| `VehicleAwareness` | — | `FTerritoryVehicleAwarenessSettings` | `—` | Centre/left/right collision probes used by the possessed Narrative mission car. Narrative Mass traffic continues using its own obstacle grid. | `EditCondition` `EditConditionHides` |
| `VehicleClass` | — | `TSoftClassPtr<ANarrativeVehicleBase>` | `—` | Narrative Vehicle class spawned on the server. It must derive from ANarrativeVehicleBase and provide usable mount seats. | `EditCondition` `EditConditionHides` |
| `VehicleIngressTimeoutSeconds` | — | `float` | `120.f` | Maximum real seconds allowed to mount, follow the ZoneGraph road, park and dismount. A timed-out driver withdraws safely instead of leaving a broken controller or car. | `ClampMin` `ClampMax` `EditCondition` `EditConditionHides` |
| `VehicleMaximumDriveSpeed` | — | `float` | `0.f` | Desired AI road speed in cm/s. Zero uses Territory's safe default of 1,400 cm/s. The vehicle still uses Narrative possession and its Chaos movement component. | `ClampMin` `EditCondition` `EditConditionHides` |
| `VehicleOccupantCapacity` | — | `int32` | `4` | Maximum Territory assault participants assigned to one vehicle, including its driver. The actual team is capped by the vehicle's authored Narrative mount seats, the remaining finite force, and this approach's wave limit. Example: 4 creates one driver and up to three passengers. | `ClampMin` `ClampMax` `EditCondition` `EditConditionHides` |
| `VehicleRetirement` | — | `FTerritoryVehicleRetirementSettings` | `—` | Safe cleanup policy for this temporary reinforcement or pursuit car. | `EditCondition` `EditConditionHides` |
| `bEnabled` | — | `bool` | `true` | Disabled approaches are never selected or used for physical spawning. | — |

### `FTerritoryCounterAttackQuestRule`

*struct* · `Source/TerritoryFramework/Public/Combat/TerritoryCounterAttackProfile.h` · 4 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Action` | — | `ETerritoryCounterQuestRuleAction` | `ETerritoryCounterQuestRuleAction::BlockWhenMatched` | Easy example: Block + In Progress prevents normal counters during a stealth quest. Require + Succeeded unlocks counters only after a story consequence. | — |
| `PlayerScope` | — | `ETerritoryCounterQuestPlayerScope` | `ETerritoryCounterQuestPlayerScope::DefendingFactionPlayers` | Which online players may satisfy this rule. Exact Narrative faction tags are used. | — |
| `QuestClass` | — | `TSubclassOf<UQuest>` | `—` | Narrative Quest read from each scoped player's Tales component. | — |
| `QuestState` | — | `ETerritoryQuestStateRequirement` | `ETerritoryQuestStateRequirement::InProgress` | *no description in source* | — |

### `FTerritoryDifficultyVehicleCount`

*struct* · `Source/TerritoryFramework/Public/Combat/TerritoryCounterAttackTypes.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Difficulty` | — | `ENarrativeGameplayDifficulty` | `ENarrativeGameplayDifficulty::Medium` | Narrative Pro gameplay difficulty selected in Game User Settings. Easy example: Hard can send two Bandit cars while Easy sends one. | — |
| `MaximumCars` | — | `int32` | `1` | Maximum cars in the complete finite assault, not per wave. Zero makes this faction arrive on foot at this difficulty. | `ClampMin` `ClampMax` |

### `FTerritoryFactionAssaultConfig`

*struct* · `Source/TerritoryFramework/Public/Combat/TerritoryCounterAttackTypes.h` · 32 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ActivityConfigurationOverride` | — | `TObjectPtr<UNPCActivityConfiguration>` | `nullptr` | Optional Narrative activity configuration override. Leave empty to use the NPC Definition's normal configuration. | — |
| `AttackerDefinition` | — | `TObjectPtr<UNPCDefinition>` | `nullptr` | Narrative Pro NPC Definition used for every physical attacker. Territory never saves the spawned pawn pointer. | — |
| `EconomyReadiness` | — | `float` | `1.f` | 0 means the faction cannot afford a response; 1 means fully ready. | `ClampMin` `ClampMax` |
| `EnemyLevelOffset` | — | `int32` | `1` | Added after player power is resolved. Use 1 for an enemy one level above the player. | `ClampMin` `ClampMax` `EditCondition` |
| `Faction` | — | `FGameplayTag` | `—` | Exact Narrative faction allowed to launch this force. Example: Narrative.Factions.Bandits. | `Categories` |
| `FinalFightDialogueTag` | — | `FGameplayTag` | `—` | Optional Narrative tagged dialogue played when a chase target abandons a damaged or blocked car and begins the final on-foot fight. | — |
| `MaximumScaledEnemyLevel` | — | `int32` | `100` | Largest Narrative NPC level produced by adaptive scaling. | `ClampMin` `EditCondition` |
| `MaximumScheduledAssaults` | Maximum Scheduled Assaults | `int32` | `3` | Example: 3 means the first finite battle plus at most two later finite counterattacks. | `ClampMin` `ClampMax` `EditCondition` `EditConditionHides` |
| `MilitaryPower` | — | `float` | `100.f` | Strategic strength used for planning probability. Example: 200 is twice a baseline force of 100; it never captures by itself. | `ClampMin` |
| `MinimumScaledEnemyLevel` | — | `int32` | `1` | Smallest Narrative NPC level produced by adaptive scaling. | `ClampMin` `EditCondition` |
| `PlannedForce` | — | `int32` | `6` | Total finite attackers in one assault. Example: 6 means at most six lives, never automatic infinite replacement. | `ClampMin` |
| `PlayerPowerTiers` | — | `TArray<FTerritoryPlayerPowerTier>` | `—` | Optional tag-to-level mappings. Narrative perk Gameplay Effects can grant these replicated tags when skills unlock. | `EditCondition` |
| `PowerScalingEffect` | — | `TSubclassOf<UGameplayEffect>` | `—` | Optional Gameplay Effect whose scalable modifiers use the resolved enemy level. Leave empty when the Narrative NPC configuration already has level curves. | `EditCondition` |
| `PowerScalingMagnitudePerEnemyLevel` | — | `float` | `0.f` | Value sent through Narrative Pro's existing SetByCaller.AttackDamage tag for each level above one. Example: 1.5 gives a level 6 enemy +7.5 Attack Damage. Narrative's AttackDamage, AttackRating, Armor, material and friendly-fire rules remain authoritative. | `ClampMin` `EditCondition` |
| `RecentMomentum` | — | `float` | `0.f` | Recent performance: -1 losing badly, 0 neutral, 1 winning strongly. | `ClampMin` `ClampMax` |
| `RecurringCounterCooldownGameTime` | — | `float` | `900.f` | Time between finite counterattack attempts. Example: 900 means a defeated wave cannot immediately reroll every subsystem update. | `ClampMin` |
| `ReserveWaveAlertDialogueTag` | — | `FGameplayTag` | `—` | Optional Narrative dialogue tag used to alert a nearby player as a reserve wave arrives. Example: Territory.Dialogue.ReservesArriving. | — |
| `ScheduleMode` | — | `ETerritoryCounterScheduleMode` | `ETerritoryCounterScheduleMode::UnlimitedSeries` | Easy example: Finite Series + 3 allows the faction to attempt at most three separate battles during this response series. Unlimited Schedule keeps trying after cooldowns while every gameplay rule still passes. | — |
| `SignatureVehicleClass` | Faction Signature Vehicle | `TSoftClassPtr<ANarrativeVehicleBase>` | `—` | Narrative Vehicle Blueprint used by this faction on every vehicle approach. Easy example: Bandits use a rusty pickup while the Regime uses a black sedan, so the player recognizes the attacker from far away. Leave empty to use the vehicle stored on each Approach. | — |
| `StagingRequirement` | — | `ETerritoryAssaultStagingRequirement` | `ETerritoryAssaultStagingRequirement::OwnsSecureDistrict` | Normal strategic admission rule. Recommended: require one loaded, unlocked District fully secured through its authored Places, so a defeated faction with no operational holding cannot keep launching counters. | — |
| `SupplyReadiness` | — | `float` | `1.f` | 0 means no supply/proximity support; 1 means fully supplied. | `ClampMin` `ClampMax` |
| `TakeoverStartedDialogueTag` | — | `FGameplayTag` | `—` | Optional Narrative tagged dialogue played once by the first attacker that enters the target and registers takeover pressure. Easy example: 'The market belongs to us now!' | — |
| `TerritorialInfluence` | — | `float` | `0.5f` | Local political/logistics reach: 0 distant and weak, 1 deeply established. Higher values shorten response timing. | `ClampMin` `ClampMax` |
| `TimePolicy` | — | `ETerritoryCounterTimePolicy` | `ETerritoryCounterTimePolicy::AnyTime` | Use Narrative / Ultra Dynamic Sky Time Window when attacks should begin only at night, dawn, or another story-friendly period. | — |
| `TimeWindowEnd` | Window End (Narrative Time) | `float` | `500.f` | Example: Start 1800 and End 0500 permits night attacks across midnight. Equal values mean all day. | `ClampMin` `ClampMax` `EditCondition` `EditConditionHides` |
| `TimeWindowStart` | Window Start (Narrative Time) | `float` | `1800.f` | Example: 1800 starts the window at 6 PM. | `ClampMin` `ClampMax` `EditCondition` `EditConditionHides` |
| `TriggerSetOverrides` | — | `TArray<TSoftObjectPtr<UTriggerSet>>` | `—` | Optional Narrative TriggerSets added to the spawned attackers. | — |
| `VehicleCountsByDifficulty` | — | `TArray<FTerritoryDifficultyVehicleCount>` | `—` | Optional per-difficulty replacement values. Add only the difficulties you want to change; missing rows use the safe built-in values. | `EditCondition` `EditConditionHides` |
| `WaveSize` | — | `int32` | `3` | Maximum attackers attempted per reserve wave, still limited by Planned Force and approach capacity. | `ClampMin` |
| `bAllowStoryPursuitWithoutStagingDistrict` | — | `bool` | `false` | Allows this force to be used by a Story Pursuit / Boss Chase event without owning a District. The event must also explicitly select Story Pursuit. Normal counters never use this exception. | — |
| `bScaleLevelToRelevantPlayerPower` | — | `bool` | `false` | Off by default. When enabled, newly spawned attackers use the strongest nearby player's Narrative level or configured power-tier tag, then add Enemy Level Offset. Narrative difficulty and attack-token counts are unchanged. | — |
| `bScaleVehicleCountByNarrativeDifficulty` | Scale Car Count With Narrative Difficulty | `bool` | `true` | Recommended. Uses Narrative Pro's current gameplay difficulty when the assault is evaluated. Empty overrides use Easy 1, Medium 1, Hard 2, Insane 3, always clamped by the authored road approaches and finite force. | — |

### `FTerritoryPlayerPowerTier`

*struct* · `Source/TerritoryFramework/Public/Combat/TerritoryCounterAttackTypes.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `PlayerPowerLevel` | — | `int32` | `1` | Campaign power level represented by the tag. Example: tier 3 can map to player power level 6. | `ClampMin` |
| `PlayerPowerTag` | — | `FGameplayTag` | `—` | Exact replicated Gameplay Tag granted to the player by a Narrative perk or story reward. Example: Territory.Power.Tier.3. | — |

### `FTerritoryStoryPursuitOptions`

*struct* · `Source/TerritoryFramework/Public/Combat/TerritoryCounterAttackTypes.h` · 16 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `AttackerDefinitionOverride` | — | `TSoftObjectPtr<UNPCDefinition>` | `—` | Optional Narrative NPC Definition for the capo, underboss, hunter or escort. Empty reuses the attacking faction definition from the Counter Attack Profile. | — |
| `ChaseDistanceGraceSeconds` | — | `float` | `10.f` | How long every player may remain outside Maximum Chase Distance before the target escapes. This protects the mission from a single short road separation. | `ClampMin` |
| `Direction` | — | `ETerritoryStoryPursuitDirection` | `ETerritoryStoryPursuitDirection::EnemyChasesPlayer` | *no description in source* | — |
| `GracePeriodOverrideGameTime` | — | `float` | `0.f` | Negative uses the profile grace period. Zero begins evaluation immediately. Positive values provide an authored story delay in Narrative campaign time. | `ClampMin` |
| `MaximumChaseDistance` | — | `float` | `9000.f` | Maximum distance from the closest participating player to a fleeing target vehicle. Zero disables the distance failure rule. | `ClampMin` |
| `MissionTrafficVehicleCountOverride` | — | `int32` | `-1` | Negative uses the Road Guide traffic count. Zero clears controlled ambient traffic; positive values request that many Narrative Mass vehicles. | `ClampMin` `ClampMax` |
| `OpposingFaction` | — | `FGameplayTag` | `—` | Required opponent for Owner Reinforcements Before Handover. The sender must own the Place and be at war with this exact faction. Ignored by other launch modes. | `Categories` |
| `PlannedForceOverride` | — | `int32` | `0` | Zero reuses Planned Force from the profile. One creates a single kill-or-escape target; larger values create a finite escort or hunter group. | `ClampMin` `ClampMax` |
| `ScenarioID` | — | `FName` | `—` | Optional stable, non-localized identifier used by saves, logs and story outcome handling. Example: CastleHill_UnderbossEscape. | — |
| `StoryFocusLocation` | — | `FVector` | `FVector::ZeroVector` | Optional player or story focus captured when the event starts. Zero uses the normal Place objective. This is a value, not a saved actor reference. | — |
| `VehicleAbandonHealthFraction` | — | `float` | `0.35f` | Narrative vehicle health fraction that triggers the authored abandonment/final-fight handoff. | `ClampMin` `ClampMax` |
| `WaveSizeOverride` | — | `int32` | `0` | Zero reuses the profile Wave Size. The effective value is always clamped to the finite planned force. | `ClampMin` `ClampMax` |
| `bAbandonDamagedVehicleForFinalFight` | — | `bool` | `true` | When enabled, a fleeing target whose Narrative vehicle falls below the health threshold dismounts and moves to the Road Guide final-fight point instead of being declared escaped. | — |
| `bActivateRoadMissionTraffic` | — | `bool` | `true` | Activate the Road Guide's referenced Narrative Quest Road Controls while this pursuit is active. Use it for authored slow traffic, intersections, and chase pressure. | — |
| `bAllowsTerritoryCapture` | — | `bool` | `false` | When false, this story encounter may fight and chase but can never register capture pressure or hand the Place to the hostile faction. Useful for an optional capo encounter during District capture. | — |
| `bUseStrategicDecisionRoll` | — | `bool` | `false` | When false, the explicit Narrative Event always launches after validation. Enable only when the authored story intentionally wants the profile's strategic probability roll. | — |

### `UBTService_TerritoryAssaultPermission`

*class* · `Source/TerritoryFramework/Public/Combat/BTService_TerritoryAssaultPermission.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `PermissionGrantedKey` | — | `FBlackboardKeySelector` | `—` | *no description in source* | — |
| `TerritoryKey` | — | `FBlackboardKeySelector` | `—` | *no description in source* | — |
| `bPreferGuardOwningTerritory` | — | `bool` | `true` | *no description in source* | — |

### `UBTTask_ReleaseTerritoryPermission`

*class* · `Source/TerritoryFramework/Public/Combat/BTTask_ReleaseTerritoryPermission.h` · 1 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `TerritoryKey` | — | `FBlackboardKeySelector` | `—` | *no description in source* | — |

### `UBTTask_RequestTerritoryPermission`

*class* · `Source/TerritoryFramework/Public/Combat/BTTask_RequestTerritoryPermission.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `TerritoryKey` | — | `FBlackboardKeySelector` | `—` | *no description in source* | — |
| `bPermissionGrantedKey` | — | `FBlackboardKeySelector` | `—` | *no description in source* | — |

### `UTerritoryCounterAttackProfile`

*profile DataAsset* · `Source/TerritoryFramework/Public/Combat/TerritoryCounterAttackProfile.h` · 43 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ActivationRadius` | — | `float` | `5000.f` | *no description in source* | `ClampMin` |
| `AttackerPowerWeight` | — | `float` | `0.30f` | *no description in source* | `ClampMin` |
| `BaseLaunchProbability` | — | `float` | `0.15f` | *no description in source* | `ClampMin` `ClampMax` |
| `DamagingEnemyMemorySeconds` | — | `float` | `20.f` | *no description in source* | `ClampMin` `ClampMax` |
| `DefenceDeterrenceWeight` | — | `float` | `0.45f` | *no description in source* | `ClampMin` |
| `DefendingPlayerEngagementPadding` | Defending Player Engagement Padding | `float` | `800.f` | How far outside a Place bound a defending player may stand and still be treated as part of the fight. Easy example: 800 lets guards fight a player just outside the gate, but not chase them across the city. | `ClampMin` `ClampMax` `EditCondition` |
| `FactionForces` | — | `TArray<FTerritoryFactionAssaultConfig>` | `—` | *no description in source* | — |
| `GracePeriodGameTime` | — | `float` | `300.f` | *no description in source* | `ClampMin` |
| `InfluenceWeight` | — | `float` | `0.20f` | *no description in source* | `ClampMin` |
| `MaxConsecutiveSpawnFailures` | — | `int32` | `5` | *no description in source* | `ClampMin` `ClampMax` |
| `MaxStalledMovementRetries` | — | `int32` | `8` | *no description in source* | `ClampMin` `ClampMax` |
| `MaximumApproaches` | — | `int32` | `3` | *no description in source* | `ClampMin` `ClampMax` |
| `MaximumLaunchProbability` | — | `float` | `0.95f` | *no description in source* | `ClampMin` `ClampMax` |
| `MinimumInfluenceTimingScale` | — | `float` | `0.25f` | *no description in source* | `ClampMin` `ClampMax` |
| `MinimumLaunchProbability` | — | `float` | `0.01f` | *no description in source* | `ClampMin` `ClampMax` |
| `NotificationRadius` | — | `float` | `12000.f` | *no description in source* | `ClampMin` |
| `ParticipantSpacing` | — | `float` | `220.f` | *no description in source* | `ClampMin` `UIMin` |
| `PreferredCameraEdgeDot` | — | `float` | `0.55f` | Preferred absolute view dot. Around 0.55 places arrivals near the left or right camera edge instead of directly in front or behind. | `ClampMin` `ClampMax` `EditCondition` |
| `QuestRules` | — | `TArray<FTerritoryCounterAttackQuestRule>` | `—` | *no description in source* | — |
| `ReadinessWeight` | — | `float` | `0.10f` | *no description in source* | `ClampMin` |
| `ReserveMaximumPlayerDistance` | — | `float` | `5500.f` | Authored approaches farther than this receive a presentation penalty but may still be used when they are the only valid route. | `ClampMin` `EditCondition` |
| `ReserveMinimumPlayerDistance` | — | `float` | `1200.f` | Authored approaches closer than this are strongly discouraged to avoid unfair pop-in. | `ClampMin` `EditCondition` |
| `ReservePreferredPlayerDistance` | — | `float` | `2600.f` | Ideal distance for an arriving reserve wave. Example: 2600 cm leaves time for the alert line. | `ClampMin` `EditCondition` |
| `SameFloorHeightTolerance` | — | `float` | `500.f` | Maximum height difference considered the same floor. Use authored Rooftop, Stair, or Custom approaches plus Nav Links for multi-floor Places. | `ClampMin` `EditCondition` |
| `SpawnPlacementAttemptsPerParticipant` | — | `int32` | `4` | *no description in source* | `ClampMin` `ClampMax` |
| `StalledMovementRetryInterval` | — | `float` | `1.5f` | *no description in source* | `ClampMin` `ClampMax` |
| `StrategicValueWeight` | — | `float` | `0.15f` | *no description in source* | `ClampMin` |
| `UnattendedRecaptureDelayGameTime` | Unattended Recapture Time | `float` | `30.f` | Time available for a defending player to return. Entering the Place or parent District stops the countdown and requires a fight. | `ClampMin` `EditCondition` |
| `UnguardedLaunchProbability` | — | `float` | `1.f` | *no description in source* | `ClampMin` `ClampMax` |
| `WaveStrategy` | — | `ETerritoryAssaultWaveStrategy` | `ETerritoryAssaultWaveStrategy::Legacy` | *no description in source* | — |
| `bCapConcurrentAttackersToNarrativeDifficulty` | — | `bool` | `true` | Recommended. Effective concurrent participants are min(Territory Max Concurrent Attackers, Narrative attack tokens for the current difficulty). | — |
| `bConcedeWhenDefendingPlayerDies` | Concede Cleared Place On Defending Player Death | `bool` | `true` | If attackers have cleared the defenders and a defending player inside the Place or District dies, ownership immediately transfers before respawn. | — |
| `bContinueFiniteWavesAfterActivation` | — | `bool` | `true` | Recommended. After activation, finite reserve waves continue without the player. Disable to pause reserve deployment until a relevant player returns. | — |
| `bDamageRetaliationOverridesDefenderPriority` | Damage Retaliation Beats Guard Priority | `bool` | `false` | Off (recommended): a living registered guard is attacked before any non-guard, including a player who just shot this NPC. On: the most recent attacker outranks the guard for the damage memory window. This restores the older behaviour, where a distant shooter could pull attackers off a local guard. | `EditCondition` |
| `bDistributeParticipantsAcrossObjectives` | — | `bool` | `true` | Uses each participant's stable save GUID to distribute a wave across reachable defence objectives instead of making every attacker crowd the same point. | — |
| `bGarrisonTriggersActivation` | A Garrison Also Triggers Activation | `bool` | `false` | If the Place already has a living registered defender (a guard assigned to it), the counterattack starts on that guard without waiting for a player. Leave off when the attack must be staged on the player's arrival. | `EditCondition` |
| `bNotifyDefendingFactionOnly` | — | `bool` | `true` | *no description in source* | — |
| `bPrioritizeTerritoryTakeover` | Prioritize Territory Takeover | `bool` | `true` | Recommended. Strategic attackers fight registered guards and defending players inside the local defence area, then move into the Place and hold it. A distant player outside that area cannot freeze capture. Story Pursuit / Boss Chase remains free to leave Territory bounds. | — |
| `bRequirePlayerProximityForActivation` | Require Player Proximity To Activate | `bool` | `false` | Disable for autonomous counterattacks. Attackers will deploy after the warning and fight Territory guards without waiting for the player. | — |
| `bRequireReinforcementCapabilityForStrategicCounterattacks` | Require Reinforcement Capability | `bool` | `true` | Recommended. A strategic counterattack can be prepared only while the attacking faction owns a Territory whose active State Config grants Territory.Capability.Reinforcements. Easy example: losing the Radio District removes the Bandits' ability to prepare another counterattack. A Story Pursuit ignores this strategic perk. | — |
| `bUseNavigationAwareObjectives` | — | `bool` | `true` | Recommended for multi-floor Places. Attackers choose only complete reachable NavMesh paths when one exists; stairs or Nav Links must connect floors. | — |
| `bUsePlayerRelativeReserveStaging` | — | `bool` | `true` | Recommended. Prefer valid authored approaches near the player, on the same floor, and near the left or right edge of the camera. Navigation and route validation still decide whether spawning is legal. | — |
| `bUseUnattendedRecaptureHandover` | Allow Unattended Recapture Countdown | `bool` | `true` | Recommended. If the defence is cleared while no living defending player is inside the Place or its District, start a countdown instead of capturing instantly. | — |

## Core — definitions, districts, places, guards

### `ATerritorySavableData`

*actor* · `Source/TerritoryFramework/Public/Core/TerritorySavableData.h` · 1 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `SavableDataGUID` | Savable Data GUID (auto-generated) | `FGuid` | `—` | *no description in source* | — |

### `ATerritoryWorldState`

*actor* · `Source/TerritoryFramework/Public/Core/TerritoryWorldState.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `CampaignCities` | Campaign City Definitions | `TArray<TObjectPtr<UTerritoryCityDefinition>>` | `—` | *no description in source* | — |
| `WorldStateGUID` | World State GUID (auto-generated) | `FGuid` | `—` | *no description in source* | — |

### `FTerritoryCapturePointTemplate`

*struct* · `Source/TerritoryFramework/Public/Core/TerritoryDefinition.h` · 6 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ActorClass` | — | `TSoftClassPtr<ATerritoryCapturePoint>` | `—` | *no description in source* | `EditCondition` `EditConditionHides` |
| `CaptureRadius` | — | `float` | `350.f` | *no description in source* | `ClampMin` `EditCondition` `EditConditionHides` |
| `RelativeTransform` | — | `FTransform` | `FTransform::Identity` | *no description in source* | `EditCondition` `EditConditionHides` |
| `bAutomaticCapture` | — | `bool` | `true` | Normal domination/multiplayer progress. Story Capture From Bounds disables automatic progress. | `EditCondition` `EditConditionHides` |
| `bEnabled` | — | `bool` | `false` | *no description in source* | — |
| `bHideWhileUnavailable` | — | `bool` | `true` | *no description in source* | `EditCondition` `EditConditionHides` |

### `FTerritoryFactionGuardDefinition`

*struct* · `Source/TerritoryFramework/Public/Core/TerritoryTypes.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Faction` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `NPCDefinition` | — | `TObjectPtr<class UNPCDefinition>` | `—` | *no description in source* | — |

### `FTerritoryGuardBehaviorTemplate`

*struct* · `Source/TerritoryFramework/Public/Core/TerritoryDefinition.h` · 12 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ClosestHostilePlayerGoalScoreBonus` | — | `float` | `0.75f` | *no description in source* | `ClampMin` `ClampMax` `EditCondition` |
| `CombatTargetFactions` | — | `FGameplayTagContainer` | `—` | *no description in source* | `Categories` |
| `DialogueProfile` | — | `TObjectPtr<UTerritoryDiplomacyDialogueProfile>` | `—` | *no description in source* | — |
| `FactionDialogueProfiles` | — | `TArray<FTerritoryFactionDialogueProfile>` | `—` | *no description in source* | — |
| `PatrolAvoidanceConsiderationRadius` | — | `float` | `500.f` | *no description in source* | `ClampMin` `ClampMax` `EditCondition` |
| `PatrolAvoidanceWeight` | — | `float` | `0.5f` | *no description in source* | `ClampMin` `ClampMax` `EditCondition` |
| `PatrolGoalClass` | — | `TSubclassOf<UTerritoryPatrolGoal>` | `—` | *no description in source* | — |
| `bAllowPersonalRetaliation` | — | `bool` | `true` | *no description in source* | — |
| `bDefendAgainstExposedEnemies` | — | `bool` | `true` | *no description in source* | — |
| `bEnablePatrolCrowdAvoidance` | — | `bool` | `true` | *no description in source* | — |
| `bEngageAtWarInClaimedTerritory` | — | `bool` | `false` | *no description in source* | — |
| `bPrioritizeClosestHostilePlayer` | — | `bool` | `true` | *no description in source* | — |

### `FTerritoryGuardPatrolTemplateNode`

*struct* · `Source/TerritoryFramework/Public/Core/TerritoryDefinition.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ActivityTag` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `RelativeTransform` | — | `FTransform` | `FTransform::Identity` | *no description in source* | — |
| `WaitTime` | — | `float` | `2.f` | *no description in source* | `ClampMin` |

### `FTerritoryGuardPostTemplate`

*struct* · `Source/TerritoryFramework/Public/Core/TerritoryDefinition.h` · 21 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ActivityConfigurationOverride` | — | `TObjectPtr<UNPCActivityConfiguration>` | `—` | *no description in source* | — |
| `ActorClass` | — | `TSoftClassPtr<ATerritoryGuardSpawnPoint>` | `—` | *no description in source* | — |
| `FactionOverride` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `GuardPostDefinition` | — | `TObjectPtr<UTerritoryGuardPostDefinition>` | `—` | *no description in source* | — |
| `GuardPostID` | Guard Post ID | `FName` | `—` | *no description in source* | — |
| `NPCDefinitionOverride` | — | `TObjectPtr<UNPCDefinition>` | `—` | *no description in source* | — |
| `PatrolRoute` | — | `TArray<FTerritoryGuardPatrolTemplateNode>` | `—` | *no description in source* | — |
| `Priority` | — | `int32` | `50` | *no description in source* | `ClampMin` `UIMin` `UIMax` |
| `RelativeTransform` | — | `FTransform` | `FTransform::Identity` | *no description in source* | — |
| `ReserveCameraAvoidanceRetryLimit` | — | `int32` | `3` | *no description in source* | `ClampMin` `ClampMax` |
| `ReserveMinimumPlayerDistance` | — | `float` | `500.f` | *no description in source* | `ClampMin` |
| `ReserveOwnershipPolicy` | — | `EReserveOwnershipPolicy` | `EReserveOwnershipPolicy::RefillOnOwnerChange` | *no description in source* | — |
| `ReserveSlots` | — | `int32` | `1` | *no description in source* | `ClampMin` `UIMin` `UIMax` |
| `ReserveSpawnCandidateCount` | — | `int32` | `12` | *no description in source* | `ClampMin` `ClampMax` |
| `ReserveSpawnDelay` | — | `float` | `3.f` | *no description in source* | `ClampMin` |
| `ReserveSpawnRadius` | — | `float` | `600.f` | *no description in source* | `ClampMin` |
| `ReserveSpawnRetryInterval` | — | `float` | `2.f` | *no description in source* | `ClampMin` |
| `ReserveTotalRetryLimit` | — | `int32` | `10` | *no description in source* | `ClampMin` `ClampMax` |
| `TriggerSetOverrides` | — | `TArray<TSoftObjectPtr<UTriggerSet>>` | `—` | *no description in source* | — |
| `bAutoSpawnReserves` | — | `bool` | `true` | *no description in source* | — |
| `bLoopPatrol` | — | `bool` | `true` | *no description in source* | — |

### `FTerritoryManagementPointTemplate`

*struct* · `Source/TerritoryFramework/Public/Core/TerritoryDefinition.h` · 7 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ActorClass` | — | `TSoftClassPtr<ATerritoryDistrictManagementPoint>` | `—` | *no description in source* | `EditCondition` `EditConditionHides` |
| `InteractionDistance` | — | `float` | `600.f` | *no description in source* | `ClampMin` `EditCondition` `EditConditionHides` |
| `ManagedDistrictOverride` | — | `FGameplayTag` | `—` | *no description in source* | `EditCondition` `EditConditionHides` `Categories` |
| `RelativeTransform` | — | `FTransform` | `FTransform::Identity` | *no description in source* | `EditCondition` `EditConditionHides` |
| `WidgetClass` | — | `TSoftClassPtr<UTerritoryDistrictManagementWidget>` | `—` | *no description in source* | `EditCondition` `EditConditionHides` |
| `WidgetLayer` | — | `FGameplayTag` | `—` | *no description in source* | `EditCondition` `EditConditionHides` `Categories` |
| `bEnabled` | — | `bool` | `false` | *no description in source* | — |

### `FTerritoryNotificationSettings`

*struct* · `Source/TerritoryFramework/Public/Core/TerritoryDeveloperSettings.h` · 9 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `EconomyHUDNotificationDuration` | — | `float` | `5.f` | *no description in source* | `ClampMin` `ClampMax` |
| `MinimumMoneyForHUDNotification` | Minimum Money For HUD Notification | `int32` | `1` | *no description in source* | `ClampMin` |
| `MinimumResourceUnitsForHUDNotification` | Minimum Resource Units For HUD Notification | `int32` | `1` | *no description in source* | `ClampMin` |
| `bRecordMoneyTransactions` | Record Money Transactions In Command Center | `bool` | `true` | *no description in source* | — |
| `bRecordResourceProduction` | Record Resource Production In Command Center | `bool` | `true` | *no description in source* | — |
| `bShowBlockedProductionOnHUD` | Show Blocked Production On HUD | `bool` | `true` | *no description in source* | — |
| `bShowMoneyEarningsOnHUD` | Show Money Earnings On HUD | `bool` | `true` | *no description in source* | — |
| `bShowMoneyExpensesOnHUD` | Show Money Expenses On HUD | `bool` | `false` | *no description in source* | — |
| `bShowResourceEarningsOnHUD` | Show Resource Earnings On HUD | `bool` | `true` | *no description in source* | — |

### `FTerritoryPatrolNode`

*struct* · `Source/TerritoryFramework/Public/Core/TerritoryGuardSpawnPoint.h` · 4 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ActivityTag` | Activity Tag | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `Location` | Location | `FVector` | `FVector::ZeroVector` | *no description in source* | — |
| `Rotation` | Rotation | `FRotator` | `FRotator::ZeroRotator` | *no description in source* | — |
| `WaitTime` | Wait Time | `float` | `2.f` | *no description in source* | `ClampMin` `UIMin` `UIMax` |

### `FTerritoryPropertyGameplayBenefit`

*struct* · `Source/TerritoryFramework/Public/Core/TerritoryDefinition.h` · 7 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `BenefitTag` | — | `FGameplayTag` | `—` | Stable capability tag granted by Narrative's Ability System while this Property benefit is active. | `Categories` |
| `Description` | — | `FText` | `—` | Player-facing explanation shown in the Territory BENEFITS tab. | — |
| `DisplayName` | — | `FText` | `—` | *no description in source* | — |
| `GrantedAbilities` | — | `TArray<TSubclassOf<UNarrativeGameplayAbility>>` | `—` | Narrative Gameplay Abilities granted to the owning player's character. Use each ability's Input Tag (for example Narrative.Input.Throw) for input binding. | — |
| `GrantedGameplayEffects` | — | `TArray<TSubclassOf<UGameplayEffect>>` | `—` | Persistent Narrative Gameplay Effects applied while the Property remains owned. Infinite effects are recommended so Territory can remove them on loss. | — |
| `RequiredUpgradeLevel` | — | `int32` | `0` | Minimum Property upgrade level required. Use zero for an ownership benefit and one or higher for a purchased upgrade. | `ClampMin` |
| `UnlockedWeaponItems` | — | `TArray<TSubclassOf<UWeaponItem>>` | `—` | Weapon item classes unlocked or serviced by this tier. Territory exposes them in UI; Narrative inventory/vendor assets remain purchase authority. | — |

### `FTerritoryStateAudioConfig`

*struct* · `Source/TerritoryFramework/Public/Core/TerritoryAudioTypes.h` · 10 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `MusicSetOverride` | — | `TSoftObjectPtr<UTaggedMusicSet>` | `—` | Optional Narrative Tagged Music Set for this Territory state. Easy example: Blacksmith Claimed can use a forge ambience set. Empty uses the current world/default Narrative music set. | `EditCondition` |
| `MusicTheme` | — | `FGameplayTag` | `—` | Theme inside the selected Narrative Tagged Music Set. Easy examples: Music.Ambient for calm control, Music.Combat for Contested, or a custom Music.Territory.* theme. | `EditCondition` `Categories` |
| `StateEffectPitch` | — | `float` | `1.f` | Pitch multiplier for this row's state-entered and state-exited sounds. | `ClampMin` `ClampMax` |
| `StateEffectVolume` | — | `float` | `1.f` | Volume multiplier for this row's state-entered and state-exited sounds. | `ClampMin` `ClampMax` |
| `StateEnteredSound` | — | `TSoftObjectPtr<USoundBase>` | `—` | Optional local state-start sound. Easy examples: alarm when Contested begins, victory cue when Claimed begins, or a lock mechanism when Locked begins. | — |
| `StateExitedSound` | — | `TSoftObjectPtr<USoundBase>` | `—` | Optional local state-end sound. Easy example: play an all-clear cue when Contested ends. | — |
| `bImmediateThemeChange` | — | `bool` | `false` | Immediate is useful for a sudden ambush. Leave disabled for professional cross-fades between ambient and combat music. | `EditCondition` |
| `bOverrideNarrativeMusic` | Override Narrative Music | `bool` | `false` | When enabled, a local player inside this Territory uses the Music Theme below. The most specific configured Place wins over its District and City. This is cosmetic and never changes replicated gameplay state. | — |
| `bPlayEnteredSoundOnPlayerArrival` | — | `bool` | `false` | Disabled avoids replaying capture or alarm cues every time the player returns. Enable for an arrival cue that should play on every entry. | — |
| `bPlayExitedSoundOnPlayerDeparture` | — | `bool` | `false` | Disabled means the sound represents a real state transition only. Enable when it should also act as a departure cue. | — |

### `FTerritoryStateConfig`

*struct* · `Source/TerritoryFramework/Public/Core/TerritoryTypes.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Audio` | Narrative Music And State Effects | `FTerritoryStateAudioConfig` | `—` | Optional local audio for this state. Easy example: Contested selects Music.Combat and plays an alarm; Claimed selects Music.Ambient and plays a short victory cue. Empty keeps the parent Territory or current world music. | — |
| `FactionOverrides` | — | `TMap<FGameplayTag, FTerritoryStateGameplayRules>` | `—` | Exact Narrative owner faction overrides. A matching row replaces common conditions, events, capabilities, income and counterattack policy. Entry uses the incoming owner; exit uses the outgoing owner. Unlisted factions use common rules. Audio and stealth stay on the state row. | — |
| `StealthProfileOverride` | Stealth Profile Override | `TObjectPtr<UTerritoryStealthProfile>` | `—` | Optional stealth rules while this state is active. Easy example: assign Rescue Mission Stealth to the Claimed row so entering the enemy Place does not start War until a guard confirms the player. | — |

### `FTerritoryStateGameplayRules`

*struct* · `Source/TerritoryFramework/Public/Core/TerritoryTypes.h` · 10 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `AllowedAttackingFactions` | — | `FGameplayTagContainer` | `—` | *no description in source* | `Categories` |
| `CounterAttackPolicy` | — | `ETerritoryStateCounterAttackPolicy` | `ETerritoryStateCounterAttackPolicy::CaptureTriggered` | *no description in source* | — |
| `EntryConditions` | Entry Conditions | `TArray<TObjectPtr<class UNarrativeCondition>>` | `—` | Every condition must pass before the state can begin. Claimed means capture completed. Contested begins only after gameplay registers a valid contest; walking through a Place is not enough unless Story Capture From Bounds intentionally makes that player an attacker. A real Faction A to Faction B capture evaluates the Claimed row even if the political enum was already Claimed; a same-owner reset does not. | — |
| `EntryEvents` | Entry Events | `TArray<TObjectPtr<class UNarrativeEvent>>` | `—` | Narrative events fired once after the state is committed. The Claimed row also fires for a real Faction A to Faction B handover even when the political enum remains Claimed. The same handover first runs this row's Exit Events for the old owner. Same-owner resets do not refire. Contested fires once when a valid contest begins, not every progress tick. Each event runs only when all inherited conditions pass, including Narrative's Not option. | — |
| `ExitConditions` | Exit Conditions | `TArray<TObjectPtr<class UNarrativeCondition>>` | `—` | Every condition must pass before this state can end, and something must still request the change. These conditions are a gate, not a trigger: nothing polls them, so the state stays put until a quest event or TryUnlock asks for the transition. Example: a Locked Territory opens when a quest event requests the unlock after the gate quest is complete. | — |
| `ExitEvents` | Exit Events | `TArray<TObjectPtr<class UNarrativeEvent>>` | `—` | Narrative events fired after this state ends. Every inherited condition inside each event must pass. Example: advance the quest when the District unlocks, but only while reputation is at least 50. | — |
| `GrantedCommandCapabilities` | Granted Command Capabilities | `FGameplayTagContainer` | `—` | Controls this Territory gives its current owner while this state is active. Easy example: in a District's Claimed row add Territory.Capability.GuardStaffing. The player's faction can add guards while it holds that District; losing the District removes the control immediately. Leave this empty when the state gives no strategic perk. | `Categories` |
| `bAllowCapitalCaptureReward` | — | `bool` | `true` | Allows the authored capital capture bonus. Other rewards belong in this faction's Narrative Entry Events. | — |
| `bAllowPeriodicIncome` | — | `bool` | `true` | Allows this state's current owner to earn periodic currency from this Place. Guard upkeep remains payable. | — |
| `bAllowResourceProduction` | — | `bool` | `true` | Allows this state's current owner to run Property resource recipes. Blocked cycles expire without later backpay. | — |

### `FTerritoryStoryOwnerTemplate`

*struct* · `Source/TerritoryFramework/Public/Core/TerritoryDefinition.h` · 8 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ActorClass` | — | `TSoftClassPtr<ATerritoryStoryOwnerSpawner>` | `—` | *no description in source* | `EditCondition` `EditConditionHides` |
| `DialogueOverride` | — | `TSoftClassPtr<UDialogue>` | `—` | *no description in source* | `EditCondition` `EditConditionHides` |
| `DialogueStartFromID` | — | `FName` | `—` | *no description in source* | `EditCondition` `EditConditionHides` |
| `InteractionDistance` | — | `float` | `300.f` | *no description in source* | `ClampMin` `ClampMax` `EditCondition` `EditConditionHides` |
| `NPCDefinition` | — | `TObjectPtr<UNPCDefinition>` | `—` | *no description in source* | `EditCondition` `EditConditionHides` |
| `RelativeTransform` | — | `FTransform` | `FTransform::Identity` | *no description in source* | `EditCondition` `EditConditionHides` |
| `bBeginDialogueOnActivation` | — | `bool` | `true` | *no description in source* | `EditCondition` `EditConditionHides` |
| `bEnabled` | — | `bool` | `false` | *no description in source* | — |

### `UTerritoryCityDefinition`

*definition DataAsset* · `Source/TerritoryFramework/Public/Core/TerritoryDefinition.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `CapitalCaptureReward` | — | `int32` | `1000` | *no description in source* | `ClampMin` |
| `Districts` | — | `TArray<TObjectPtr<UTerritoryDistrictDefinition>>` | `—` | *no description in source* | — |

### `UTerritoryDefinition`

*definition DataAsset* · `Source/TerritoryFramework/Public/Core/TerritoryDefinition.h` · 36 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `AllDefendersDefeatedEvents` | — | `TArray<TObjectPtr<UNarrativeEvent>>` | `—` | *no description in source* | — |
| `CapturePoint` | — | `FTerritoryCapturePointTemplate` | `—` | Optional physical point, flag, or interaction actor. Automatic progress is for domination/multiplayer mode and is automatically turned off when Story Capture From Whole Place Bounds is enabled. | — |
| `CounterAttackApproaches` | — | `TArray<FTerritoryAssaultApproach>` | `—` | *no description in source* | — |
| `CounterAttackProfile` | — | `TObjectPtr<UTerritoryCounterAttackProfile>` | `—` | *no description in source* | — |
| `DefaultGuardDefinition` | — | `TObjectPtr<UNPCDefinition>` | `—` | *no description in source* | — |
| `DefaultStealthProfile` | — | `TObjectPtr<UTerritoryStealthProfile>` | `—` | Optional reusable stealth policy. Empty preserves legacy story bounds: entering immediately starts Contested. Easy example: assign a Rescue Mission profile so the player can enter Claimed enemy bounds while undetected. | — |
| `DefenderDiedEvents` | — | `TArray<TObjectPtr<UNarrativeEvent>>` | `—` | *no description in source* | — |
| `DisplayName` | — | `FText` | `—` | *no description in source* | — |
| `FactionGuardDefinitions` | — | `TArray<FTerritoryFactionGuardDefinition>` | `—` | *no description in source* | — |
| `FortificationStrength` | — | `float` | `0.f` | *no description in source* | `ClampMin` |
| `FriendlyPriceMultiplier` | — | `float` | `0.85f` | *no description in source* | `ClampMin` `EditCondition` |
| `GuardBehavior` | — | `FTerritoryGuardBehaviorTemplate` | `—` | *no description in source* | — |
| `GuardPosts` | — | `TArray<FTerritoryGuardPostTemplate>` | `—` | *no description in source* | — |
| `GuardQuality` | — | `float` | `1.f` | *no description in source* | `ClampMin` |
| `GuardRecruitmentCost` | — | `int32` | `50` | *no description in source* | `ClampMin` |
| `GuardUpkeepPerCycle` | — | `int32` | `50` | *no description in source* | `ClampMin` |
| `InitialAvailability` | Initial Availability | `ETerritoryAvailability` | `ETerritoryAvailability::Unlocked` | Locked keeps this Territory silent until its Narrative Locked exit conditions pass. Ownership is preserved. Applied only when a brand-new campaign starts; an existing save keeps its saved availability, so changing this appears to do nothing while testing on a save you already have. Easy example: lock the Farm Place until the story unlocks it, then test on a new campaign. | — |
| `InitialGuardCount` | — | `int32` | `3` | *no description in source* | `ClampMin` |
| `InitialOwningFaction` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `InitialState` | — | `ETerritoryInitialState` | `ETerritoryInitialState::Automatic` | *no description in source* | — |
| `ManagementPoint` | — | `FTerritoryManagementPointTemplate` | `—` | *no description in source* | — |
| `MaxConcurrentAttackers` | — | `int32` | `3` | *no description in source* | `ClampMin` |
| `NearbyAlliedSupport` | — | `float` | `0.f` | *no description in source* | `ClampMin` |
| `NeutralPriceMultiplier` | — | `float` | `1.f` | *no description in source* | `ClampMin` `EditCondition` |
| `PeriodicIncome` | — | `int32` | `100` | *no description in source* | `ClampMin` |
| `PostCaptureGarrisonPolicy` | — | `ETerritoryPostCaptureGarrisonPolicy` | `ETerritoryPostCaptureGarrisonPolicy::PlayerChooses` | *no description in source* | — |
| `QuestRuntimeOverrides` | Quest Runtime Overrides | `TArray<FTerritoryQuestRuntimeOverrideRule>` | `—` | Assign Quests that temporarily control this Territory. When the Quest ends, the normal rules continue from the current live state; skipped state rewards/events are not replayed. | — |
| `RelativeTransform` | — | `FTransform` | `FTransform::Identity` | *no description in source* | — |
| `StateConfigs` | State Rules (All Runtime States) | `TMap<ETerritoryState, FTerritoryStateConfig>` | `—` | Always contains four rows: Locked availability, Unclaimed, Contested, and Claimed. Claimed is the stable ownership row after capture completes. Contested Entry Events run once whenever gameplay really enters Contested, not every capture tick. | — |
| `StrategicValue` | — | `float` | `1.f` | *no description in source* | `ClampMin` |
| `TerritoryActorClass` | — | `TSoftClassPtr<ATerritoryVolume>` | `—` | *no description in source* | — |
| `TerritoryTag` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `WarPriceMultiplier` | — | `float` | `1.5f` | *no description in source* | `ClampMin` `EditCondition` |
| `bAttitudeAffectsPrices` | Attitude Affects Prices | `bool` | `false` | *no description in source* | — |
| `bShowGameplayHUD` | Show Passive Gameplay HUD Card | `bool` | `true` | Show the compact Territory location/capture card while the player is inside this exact Territory. Turn this off for broad ambient City or District volumes. Live notifications, POIs, map markers, Command Center, and management are not hidden. | — |
| `bStoryCaptureFromBounds` | Story Capture From Whole Place Bounds | `bool` | `false` | Story mode uses the full multi-floor Place volume for contesting and explicit owner handover. Enabling this automatically turns off physical Capture Point progress, because both modes must never compete for capture authority. | — |

### `UTerritoryDeveloperSettings`

*settings class* · `Source/TerritoryFramework/Public/Core/TerritoryDeveloperSettings.h` · 78 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `AlliedReputationThreshold` | Allied Reputation Threshold | `int32` | `50` | Easy example: set 50 so a long chain of helpful quests earns an alliance without a scripted treaty. | `EditCondition` |
| `CaptureProgressDecayPerSecond` | — | `float` | `0.05f` | *no description in source* | `ClampMin` `UIMin` `UIMax` |
| `CaptureProgressPerSecond` | — | `float` | `0.1f` | *no description in source* | `ClampMin` `UIMin` `UIMax` |
| `CaptureTickInterval` | — | `float` | `0.1f` | *no description in source* | `ClampMin` `UIMin` `UIMax` |
| `CounterAttackCampaignSeed` | — | `int32` | `1337` | *no description in source* | — |
| `CounterAttackUpdateInterval` | — | `float` | `2.f` | *no description in source* | `ClampMin` `ClampMax` |
| `DebugVerbosityLevel` | — | `int32` | `5` | *no description in source* | `ClampMin` `ClampMax` `UIMin` `UIMax` `EditCondition` |
| `DefaultNarrativeButtonClass` | — | `TSoftClassPtr<UNarrativeCommonButtonBase>` | `—` | *no description in source* | — |
| `DefaultPlayerFaction` | Player Faction Fallback (Optional) | `FGameplayTag` | `—` | Optional fallback for a player with no Narrative faction. Easy example: choose your project's Player faction while prototyping, then leave this empty once Narrative assigns factions. This is never used for NPCs and never overrides a player's live Narrative faction. | `Categories` |
| `DefaultRoadTrafficEntityConfig` | — | `TSoftObjectPtr<UMassEntityConfigAsset>` | `—` | *no description in source* | — |
| `DefaultTerritoryButtonStyle` | — | `TSoftClassPtr<UCommonButtonStyle>` | `—` | *no description in source* | — |
| `DefaultTerritoryTextStyle` | — | `TSoftClassPtr<UCommonTextStyle>` | `—` | *no description in source* | — |
| `EconomyTickIntervalSeconds` | — | `float` | `300.f` | *no description in source* | `ClampMin` `UIMin` `UIMax` |
| `HostileReputationThreshold` | Hostile Reputation Threshold | `int32` | `-50` | Easy example: set -50 so robbing a faction three times turns it hostile. | `EditCondition` |
| `MaxConcurrentAssaultsPerFaction` | — | `int32` | `2` | *no description in source* | `ClampMin` |
| `MaxConcurrentScheduledAssaults` | — | `int32` | `8` | *no description in source* | `ClampMin` |
| `MaxLiveCounterAttackNPCs` | — | `int32` | `24` | *no description in source* | `ClampMin` |
| `MaxRetainedAssaultRecords` | — | `int32` | `100` | *no description in source* | `ClampMin` |
| `Notifications` | — | `FTerritoryNotificationSettings` | `—` | *no description in source* | — |
| `ProductionCycleObservationIntervalSeconds` | Campaign Production Check Interval (Seconds) | `float` | `1.f` | *no description in source* | `ClampMin` `UIMin` `UIMax` |
| `RoadGuideBlueprintClass` | Road Guide Blueprint Class | `TSoftClassPtr<ATerritoryRoadGuide>` | `—` | *no description in source* | — |
| `RoadTrafficControlsBlueprintClass` | — | `TSoftClassPtr<ATerritoryRoadTrafficControls>` | `—` | *no description in source* | — |
| `RoadTrafficSpawnerBlueprintClass` | — | `TSoftClassPtr<AMassVehicleSpawner>` | `—` | *no description in source* | — |
| `SpatialCellSize` | — | `float` | `2000.f` | *no description in source* | `ClampMin` `UIMin` `UIMax` |
| `TerritoryActionButtonStyle` | Territory Action Button Style | `TSoftClassPtr<UCommonButtonStyle>` | `—` | *no description in source* | — |
| `TerritoryCommandPanelFillColor` | Command Center Panel Colour | `FLinearColor` | `FLinearColor(0.04f, 0.06f, 0.08f, 0.92f)` | *no description in source* | — |
| `TerritoryCommandPanelOutlineColor` | Command Center Panel Accent Colour | `FLinearColor` | `FLinearColor(0.18f, 0.52f, 0.48f, 0.42f)` | *no description in source* | — |
| `TerritoryCommandScreenFillColor` | Command Center Screen Colour | `FLinearColor` | `FLinearColor(0.f, 0.f, 0.f, 0.f)` | *no description in source* | — |
| `TerritoryHUDCardAlertExtraHeight` | HUD Capture Card Alert Extra Height | `float` | `38.f` | *no description in source* | `ClampMin` `UIMin` |
| `TerritoryHUDCardFillColor` | HUD Capture Card Colour | `FLinearColor` | `FLinearColor(0.04f, 0.07f, 0.09f, 0.62f)` | *no description in source* | — |
| `TerritoryHUDCardSize` | HUD Capture Card Size | `FVector2D` | `FVector2D(328.f, 108.f)` | *no description in source* | — |
| `TerritoryHeadingTextStyle` | — | `TSoftClassPtr<UCommonTextStyle>` | `—` | *no description in source* | — |
| `TerritoryInterfaceFont` | Territory Interface Font | `TSoftObjectPtr<UFont>` | `—` | *no description in source* | — |
| `TerritoryMutedTextStyle` | — | `TSoftClassPtr<UCommonTextStyle>` | `—` | *no description in source* | — |
| `TerritoryPanelTexture` | Territory Panel Texture | `TSoftObjectPtr<UTexture2D>` | `—` | *no description in source* | — |
| `TerritoryProgressFillTexture` | — | `TSoftObjectPtr<UTexture2D>` | `—` | *no description in source* | — |
| `TerritoryProgressFrameTexture` | — | `TSoftObjectPtr<UTexture2D>` | `—` | *no description in source* | — |
| `TerritoryScreenBackgroundTexture` | Command Center Background Texture | `TSoftObjectPtr<UTexture2D>` | `—` | *no description in source* | — |
| `TerritoryTabButtonStyle` | Territory Tab Button Style | `TSoftClassPtr<UCommonButtonStyle>` | `—` | *no description in source* | — |
| `TerritoryTextScale` | Territory Text Scale | `float` | `1.f` | *no description in source* | `ClampMin` `ClampMax` `UIMin` `UIMax` |
| `TerritoryTitleTextStyle` | — | `TSoftClassPtr<UCommonTextStyle>` | `—` | *no description in source* | — |
| `TreatyExpirationCheckInterval` | — | `float` | `10.f` | *no description in source* | `ClampMin` `UIMin` `UIMax` |
| `bDebugAvailabilityHierarchy` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugBT` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugCapture` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugCaptureAttempts` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugCombat` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugCounterAttacks` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugDiplomacy` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugEconomyTicks` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugFactionAttitudes` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugGuardDeaths` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugGuardSpawning` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugInteraction` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugMapMarkers` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugOwnershipChanges` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugProduction` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugRegistry` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugSaveLoad` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugSpatialIndex` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugStateTransitions` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugStealth` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugTales` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugTransactions` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugUI` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDebugWorldState` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDrawCaptureProgress` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDrawGuardSpawnPoints` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDrawOwnershipOverlay` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDrawSpatialGrid` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bDrawTerritoryBounds` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bEnableDebug` | Enable Debug System (Master Gate) | `bool` | `false` | *no description in source* | — |
| `bHideTerritoryHUDDuringNarrativeDialogue` | Hide Territory HUD During Narrative Dialogue | `bool` | `true` | *no description in source* | — |
| `bIncludeAttachedActorsInCinematicLOD` | — | `bool` | `true` | *no description in source* | — |
| `bReputationDrivesDiplomacy` | Reputation Declares Diplomacy | `bool` | `false` | Choose a campaign faction with Set Reputation Subject Faction on the server, then enable this to let reputation declare War or Alliance. Without an explicit subject, scores change but treaties do not. Off means quests decide how reputation affects the story. | — |
| `bTerritoryCommandScreenUseBackgroundTexture` | Command Center Uses Background Texture | `bool` | `false` | *no description in source* | — |
| `bTerritoryHUDCardUsePanelTexture` | HUD Capture Card Uses Panel Texture | `bool` | `false` | *no description in source* | — |
| `bUseCinematicParticipantLODDuringDialogue` | Use Cinematic Participant LOD During Dialogue | `bool` | `true` | *no description in source* | — |

### `UTerritoryDisguiseProfile`

*profile DataAsset* · `Source/TerritoryFramework/Public/Core/TerritoryDisguiseProfile.h` · 13 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ActivatedEventTag` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `ClearanceTags` | — | `FGameplayTagContainer` | `—` | Optional access tags. Easy example: add Territory.Disguise.Clearance.Officer so the uniform can pass an officer-only headquarters check. | — |
| `CompromisedEventTag` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `IdentityCheckFailedEventTag` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `IdentityCheckPassedEventTag` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `PerceivedFaction` | — | `FGameplayTag` | `—` | Faction shown to Territory guards while the disguise is intact. Easy example: a Bandit uniform makes a Heroes player look like Narrative.Factions.Bandits without changing the player's real faction. | `Categories` |
| `Quality` | — | `float` | `1.f` | How convincing the disguise is. 1.0 means a perfect uniform for normal checks; restricted Places may still require clearance. | `ClampMin` `ClampMax` |
| `RemovedEventTag` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `RestoredEventTag` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `bCompromiseFromScriptedReveal` | — | `bool` | `true` | A scripted Reveal Infiltrator or failed identity check burns the disguise. | — |
| `bCompromiseWhenDealingDamage` | — | `bool` | `true` | *no description in source* | — |
| `bCompromiseWhenDefenderKillIsSeen` | — | `bool` | `true` | *no description in source* | — |
| `bCompromiseWhenFiringWhileSeen` | — | `bool` | `true` | *no description in source* | — |

### `UTerritoryDistrictDefinition`

*definition DataAsset* · `Source/TerritoryFramework/Public/Core/TerritoryDefinition.h` · 4 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `CapitalCaptureReward` | — | `int32` | `500` | *no description in source* | `ClampMin` |
| `CapitalIncomeMultiplier` | — | `float` | `2.f` | *no description in source* | `ClampMin` |
| `Places` | — | `TArray<TObjectPtr<UTerritoryPlaceDefinition>>` | `—` | *no description in source* | — |
| `bIsCapital` | — | `bool` | `false` | *no description in source* | — |

### `UTerritoryGuardPostDefinition`

*definition DataAsset* · `Source/TerritoryFramework/Public/Core/TerritoryGuardPostDefinition.h` · 13 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ActivityConfiguration` | — | `TObjectPtr<UNPCActivityConfiguration>` | `—` | *no description in source* | — |
| `DisplayName` | — | `FText` | `—` | *no description in source* | — |
| `FactionOverride` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `NPCDefinition` | — | `TObjectPtr<UNPCDefinition>` | `—` | *no description in source* | — |
| `PatrolRoute` | — | `TArray<FTerritoryPatrolNode>` | `—` | *no description in source* | — |
| `ReserveMinimumPlayerDistance` | — | `float` | `500.f` | *no description in source* | `ClampMin` `UIMin` `UIMax` |
| `ReserveSlots` | — | `int32` | `1` | *no description in source* | `ClampMin` `UIMin` `UIMax` |
| `ReserveSpawnCandidateCount` | — | `int32` | `12` | *no description in source* | `ClampMin` `ClampMax` `UIMin` `UIMax` |
| `ReserveSpawnDelay` | — | `float` | `3.f` | *no description in source* | `ClampMin` `UIMin` `UIMax` |
| `ReserveSpawnRadius` | — | `float` | `600.f` | *no description in source* | `ClampMin` `UIMin` `UIMax` |
| `ReserveSpawnRetryInterval` | — | `float` | `2.f` | *no description in source* | `ClampMin` `UIMin` `UIMax` |
| `TriggerSetOverrides` | — | `TArray<TSoftObjectPtr<UTriggerSet>>` | `—` | *no description in source* | — |
| `bLoopPatrol` | — | `bool` | `true` | *no description in source* | — |

### `UTerritoryPlaceDefinition`

*definition DataAsset* · `Source/TerritoryFramework/Public/Core/TerritoryDefinition.h` · 7 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `GameplayBenefits` | — | `TArray<FTerritoryPropertyGameplayBenefit>` | `—` | Abilities, persistent Gameplay Effects, benefit tags, and weapon catalog entries unlocked by owning and upgrading this Property. | — |
| `IncomeBonusPerLevel` | — | `int32` | `25` | *no description in source* | `ClampMin` |
| `MaxUpgradeLevel` | — | `int32` | `3` | *no description in source* | `ClampMin` |
| `ProductionProfile` | — | `TObjectPtr<UTerritoryProductionProfile>` | `—` | *no description in source* | — |
| `PropertyRoleTag` | — | `FGameplayTag` | `—` | What this Property provides. Example: Territory.Property.Role.ArmsShop for a blacksmith or gunsmith. | `Categories` |
| `StoryOwner` | — | `FTerritoryStoryOwnerTemplate` | `—` | *no description in source* | — |
| `UpgradeCostPerLevel` | — | `int32` | `500` | *no description in source* | `ClampMin` |

### `UTerritoryStealthProfile`

*profile DataAsset* · `Source/TerritoryFramework/Public/Core/TerritoryStealthProfile.h` · 39 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `BreakStealthGameplayEventTag` | — | `FGameplayTag` | `—` | Sent to the exposed player's Ability System. A temporary stealth ability may listen for this event and cancel itself; passive equipment is not removed. | `Categories` |
| `BulletImpactSuspicion` | — | `float` | `0.2f` | *no description in source* | `ClampMin` `ClampMax` |
| `CorpseSuspicion` | — | `float` | `0.5f` | *no description in source* | `ClampMin` `ClampMax` |
| `EscalationScope` | — | `ETerritoryStealthEscalationScope` | `ETerritoryStealthEscalationScope::FactionWar` | *no description in source* | — |
| `GuardDetectionMultiplier` | — | `float` | `1.f` | Multiplies Narrative's sight strength before Stealth Rating is applied. | `ClampMin` |
| `GunshotSuspicion` | — | `float` | `0.35f` | *no description in source* | `ClampMin` `ClampMax` |
| `ImmediateSightExposureThreshold` | — | `float` | `0.8f` | Effective Narrative sight at or above this value immediately confirms the player. | `ClampMin` `ClampMax` |
| `InvestigationAcceptanceRadius` | — | `float` | `150.f` | *no description in source* | `ClampMin` |
| `InvestigationActivityClass` | — | `TSubclassOf<UTerritoryInvestigationActivity>` | `—` | *no description in source* | — |
| `InvestigationDuration` | — | `float` | `12.f` | *no description in source* | `ClampMin` |
| `InvestigationRadius` | — | `float` | `5000.f` | *no description in source* | `ClampMin` |
| `MaximumInvestigators` | — | `int32` | `2` | *no description in source* | `ClampMin` `ClampMax` |
| `MaximumStealthRating` | — | `float` | `100.f` | Narrative Stealth Rating is interpreted on this scale. A rating of 50 on a scale of 100 halves effective sight. | `ClampMin` `ClampMax` |
| `MinimumDisguiseQuality` | — | `float` | `0.5f` | Minimum profile quality accepted here. Easy example: use 0.5 for public streets and 0.9 for a guarded headquarters. | `ClampMin` `ClampMax` |
| `MinimumSightEvidence` | — | `float` | `0.2f` | Sight below this value is ignored. Narrative's normal attack goal uses 0.2 as its useful minimum. | `ClampMin` `ClampMax` |
| `PointBlankSightExposureDistance` | — | `float` | `300.f` | Maximum guard-to-player distance for unavoidable direct-sight exposure. Narrative AI Perception must still have valid sight and the Narrative Invisible tag is still respected. Easy example: 300 means three metres. | `ClampMin` |
| `RequiredDisguiseClearanceTags` | — | `FGameplayTagContainer` | `—` | All of these access tags must exist on the uniform. Easy example: an officer-only floor can require Territory.Disguise.Clearance.Officer. | — |
| `ShotCorrelationWindow` | — | `float` | `0.3f` | Muzzle and impact sounds from the same actor inside this window are treated as one missed shot. | `ClampMin` `ClampMax` |
| `SightSuspicionGainPerSecond` | — | `float` | `2.f` | Suspicion added per second while partial sight remains active. | `ClampMin` |
| `StealthAbilityTagsToCancel` | — | `FGameplayTagContainer` | `—` | Ability tags canceled on confirmed exposure. Defaults cover Narrative Pro crouch stealth and any Territory-aware stealth ability. Add your own ability tag here when using a custom stealth ability. | — |
| `StealthGameplayEffectsToRemove` | — | `TArray<TSubclassOf<UGameplayEffect>>` | `—` | Temporary Gameplay Effect classes removed on confirmed exposure. Easy example: add Narrative Pro GE_CrouchStealth here. Do not add permanent Sneak perk or equipment effects; those are capability bonuses and should survive detection. | — |
| `SuspicionDecayPerSecond` | — | `float` | `0.2f` | Suspicion removed per second after every guard loses sight. | `ClampMin` |
| `ThrowableDistractionSuspicion` | — | `float` | `0.25f` | *no description in source* | `ClampMin` `ClampMax` |
| `bAllowFactionDisguises` | — | `bool` | `true` | Enables faction uniforms in this Territory. The player's real Narrative faction and diplomacy are never changed. | — |
| `bAllowStealthInfiltration` | — | `bool` | `true` | When enabled, entering story bounds registers an undetected infiltrator instead of immediately starting Contested. Easy example: use this for a rescue quest inside an enemy Place. | — |
| `bAnonymousEvidenceCanExpose` | — | `bool` | `false` | *no description in source* | — |
| `bCancelActiveStealthAbilitiesOnExposure` | — | `bool` | `true` | Immediately cancels active Gameplay Abilities matching Stealth Ability Tags To Cancel when a guard confirms the player. Narrative Pro uses Abilities.Crouch for its built-in crouch stealth; Territory.Ability.Stealth is provided for a dedicated project stealth ability. | — |
| `bCompromiseDisguiseOnFailedIdentityCheck` | — | `bool` | `true` | A failed scripted identity check burns the disguise for the checking faction. Disable this for a warning-only checkpoint. | — |
| `bDamageImmediatelyExposes` | — | `bool` | `true` | *no description in source* | — |
| `bFireWhileSeenExposes` | — | `bool` | `true` | *no description in source* | — |
| `bFireWhileUnseenStartsInvestigation` | — | `bool` | `true` | *no description in source* | — |
| `bPointBlankSightAlwaysExposes` | — | `bool` | `true` | When enabled, a guard that already has Narrative sight of the player exposes them inside Point Blank Exposure Distance even when Stealth Rating would otherwise reduce sight below the evidence threshold. This prevents walking directly in front of a guard while remaining hidden. | — |
| `bRemoveActiveStealthEffectsOnExposure` | — | `bool` | `true` | Removes the configured temporary stealth Gameplay Effects when exposure is confirmed. This is needed for instant toggle abilities, such as Narrative Pro crouch, whose ability has already ended while its infinite stealth effect remains active. | — |
| `bRequireOwningFactionDisguise` | — | `bool` | `true` | Recommended for enemy bases. A Bandit Place accepts a Bandit uniform; a Civilian uniform does not automatically pass. | — |
| `bRespectNarrativeInvisibleTag` | — | `bool` | `true` | *no description in source* | — |
| `bRespondToOutsideThreats` | Accept Outside Threat Evidence | `bool` | `true` | *no description in source* | — |
| `bSeenDefenderKillExposes` | — | `bool` | `true` | *no description in source* | — |
| `bSendBreakStealthGameplayEvent` | — | `bool` | `true` | *no description in source* | — |
| `bUnseenDefenderDeathStartsInvestigation` | — | `bool` | `true` | *no description in source* | — |

## Economy — production, resources, currency

### `FTerritoryProductionNotifications`

*struct* · `Source/TerritoryFramework/Public/Economy/TerritoryProductionProfile.h` · 9 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `BlockedMessage` | — | `FText` | `—` | *no description in source* | `EditCondition` |
| `BlockedTitle` | — | `FText` | `—` | *no description in source* | `EditCondition` |
| `SuccessMessage` | — | `FText` | `—` | *no description in source* | `EditCondition` |
| `SuccessTitle` | — | `FText` | `—` | *no description in source* | `EditCondition` |
| `bEnabled` | — | `bool` | `true` | *no description in source* | — |
| `bNotifyAtStockLimit` | — | `bool` | `false` | *no description in source* | `EditCondition` |
| `bNotifyOnSuccess` | — | `bool` | `true` | *no description in source* | `EditCondition` |
| `bNotifyWhenBlocked` | — | `bool` | `true` | *no description in source* | `EditCondition` |
| `bRecordInFeed` | — | `bool` | `true` | *no description in source* | `EditCondition` |

### `FTerritoryProductionRule`

*struct* · `Source/TerritoryFramework/Public/Economy/TerritoryProductionProfile.h` · 12 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `DisplayName` | — | `FText` | `—` | *no description in source* | — |
| `Inputs` | — | `TArray<FTerritoryResourceRate>` | `—` | *no description in source* | — |
| `InventoryStopConditions` | — | `TArray<FTerritoryProductionStockCondition>` | `—` | *no description in source* | — |
| `MinimumUpgradeLevel` | — | `int32` | `0` | *no description in source* | `ClampMin` |
| `Notifications` | — | `FTerritoryProductionNotifications` | `—` | *no description in source* | — |
| `OutputStockCaps` | — | `TArray<FTerritoryProductionStockCap>` | `—` | *no description in source* | — |
| `Outputs` | — | `TArray<FTerritoryResourceRate>` | `—` | *no description in source* | — |
| `Priority` | — | `int32` | `0` | *no description in source* | — |
| `RuleTag` | — | `FGameplayTag` | `—` | *no description in source* | — |
| `bEnabled` | — | `bool` | `true` | *no description in source* | — |
| `bPauseWhileContested` | — | `bool` | `true` | *no description in source* | — |
| `bRequiresClaimedState` | — | `bool` | `true` | *no description in source* | — |

### `FTerritoryProductionStockCap`

*struct* · `Source/TerritoryFramework/Public/Economy/TerritoryProductionProfile.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ItemClass` | — | `TSubclassOf<UNarrativeItem>` | `—` | *no description in source* | `EditCondition` |
| `MaximumQuantity` | — | `int32` | `300` | *no description in source* | `ClampMin` `EditCondition` |
| `bEnabled` | — | `bool` | `true` | *no description in source* | — |

### `FTerritoryProductionStockCondition`

*struct* · `Source/TerritoryFramework/Public/Economy/TerritoryProductionProfile.h` · 4 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Comparison` | Stop When | `ETerritoryIntegerComparison` | `ETerritoryIntegerComparison::AtLeast` | *no description in source* | `EditCondition` |
| `ItemClass` | — | `TSubclassOf<UNarrativeItem>` | `—` | *no description in source* | `EditCondition` |
| `Quantity` | — | `int32` | `300` | *no description in source* | `ClampMin` `EditCondition` |
| `bEnabled` | — | `bool` | `true` | *no description in source* | — |

### `FTerritoryResourceRate`

*struct* · `Source/TerritoryFramework/Public/Economy/TerritoryProductionProfile.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ItemClass` | — | `TSubclassOf<UNarrativeItem>` | `—` | *no description in source* | — |
| `QuantityPerCycle` | — | `int32` | `0` | *no description in source* | `ClampMin` |
| `QuantityPerUpgradeLevel` | — | `int32` | `0` | *no description in source* | `ClampMin` |

### `UTerritoryFactionResourceAccountComponent`

*component* · `Source/TerritoryFramework/Public/Economy/TerritoryFactionResourceAccountComponent.h` · 6 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `AccountPriority` | — | `int32` | `0` | *no description in source* | — |
| `BindingMode` | — | `ETerritoryResourceAccountBinding` | `ETerritoryResourceAccountBinding::FixedFaction` | *no description in source* | — |
| `Faction` | — | `FGameplayTag` | `—` | *no description in source* | `EditCondition` `EditConditionHides` `Categories` |
| `MaxRegistrationAttempts` | — | `int32` | `30` | *no description in source* | `ClampMin` `EditCondition` |
| `RegistrationRetryInterval` | — | `float` | `1.f` | *no description in source* | `ClampMin` `EditCondition` |
| `bAutoRegister` | — | `bool` | `true` | *no description in source* | — |

### `UTerritoryProductionProfile`

*profile DataAsset* · `Source/TerritoryFramework/Public/Economy/TerritoryProductionProfile.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `DisplayName` | — | `FText` | `—` | *no description in source* | — |
| `Rules` | — | `TArray<FTerritoryProductionRule>` | `—` | *no description in source* | — |

## Interaction

### `UTerritoryDistractionComponent`

*component* · `Source/TerritoryFramework/Public/Interaction/TerritoryDistractionComponent.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Loudness` | — | `float` | `1.f` | *no description in source* | `ClampMin` |
| `MaximumRange` | — | `float` | `3000.f` | *no description in source* | `ClampMin` |
| `bReportOnFirstHit` | — | `bool` | `true` | Automatically report once when the owning Narrative projectile hits an actor or surface. | — |

## Navigation

### `ATerritoryRoadGuide`

*actor* · `Source/TerritoryFramework/Public/Navigation/TerritoryRoadGuide.h` · 8 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `FinalFightLocation` | — | `FVector` | `FVector::ZeroVector` | Local-space location used after a chased vehicle is disabled or abandoned. The target dismounts and Narrative navigation moves it here for the final fight. | — |
| `LaneCenterOffset` | — | `float` | `150.f` | Distance from the spline centre to each directional lane. Narrative's default Road profile uses two 300 cm lanes, so 150 cm is a useful starting point. | `ClampMin` |
| `MaximumZoneGraphDistance` | — | `float` | `650.f` | Maximum distance from each sampled mission point to a Narrative ZoneGraph lane. | `ClampMin` |
| `MissionTrafficVehicleCount` | — | `int32` | `5` | Requested Narrative Mass vehicle count while a mission uses this road. A mission option may override it. | `ClampMin` `ClampMax` |
| `NarrativeTrafficControls` | — | `TSoftObjectPtr<AQuestRoadControls>` | `—` | Optional Narrative Quest Road Controls actor. Territory only activates/deactivates it; Narrative remains responsible for Mass traffic spawning, lane occupancy, obstacle avoidance, and intersection rules. | — |
| `RoadGuideID` | — | `FName` | `—` | Stable level-wide ID referenced by a Territory approach. By convention use the Approach ID, for example Blacksmith_WestRoad. | — |
| `SampleSpacing` | — | `float` | `300.f` | Distance between drive points sampled from the spline. Smaller values follow sharp bends more closely. | `ClampMin` |
| `bRequireNarrativeZoneGraphCoverage` | — | `bool` | `true` | Reject this helper when samples are not close to Narrative's ZoneGraph. Keep enabled for traffic, reinforcement, and pursuit roads; disable only for a deliberate off-road mission spline. | — |

### `FTerritoryVehicleAwarenessSettings`

*struct* · `Source/TerritoryFramework/Public/Navigation/TerritoryRoadTypes.h` · 12 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `AbandonAfterBlockedSeconds` | — | `float` | `12.f` | After this many blocked seconds, assault squads dismount when a complete walking route exists. Story chase targets use their authored abandonment policy. Zero disables the blocked-vehicle handoff. | `ClampMin` |
| `ArrivalSearchDistance` | — | `float` | `2500.f` | Search backwards along the existing route for a free drop-off with a complete walking path. Zero uses only the authored endpoint. | `ClampMin` `ClampMax` |
| `ArrivalSpacing` | — | `float` | `1000.f` | *no description in source* | `ClampMin` `ClampMax` |
| `BrakingDistance` | — | `float` | `1000.f` | Vehicle progressively slows inside this distance. It must be greater than Emergency Stop Distance. | `ClampMin` |
| `EmergencyStopDistance` | — | `float` | `325.f` | Vehicle brakes fully when a blocking vehicle, pawn, world object, or destructible is this close. | `ClampMin` |
| `ForwardProbeDistance` | — | `float` | `1400.f` | How far the centre and left/right vehicle probes look ahead. Narrative Mass traffic has its own obstacle grid; this setting protects the possessed mission vehicle. | `ClampMin` |
| `MaximumAvoidanceSteering` | — | `float` | `0.22f` | Maximum temporary steering correction selected by the left/right probes. | `ClampMin` `ClampMax` |
| `ProbeHalfHeight` | — | `float` | `100.f` | Half height of the awareness boxes. Multi-level roads are ignored when they are outside this height. | `ClampMin` |
| `ProbeHalfWidth` | — | `float` | `125.f` | Half width of each forward awareness box. Keep it close to half the vehicle width so adjacent traffic does not cause false emergency stops. | `ClampMin` |
| `SideProbeOffset` | — | `float` | `275.f` | Side-probe offset used to understand the left and right space around a blocked lane. | `ClampMin` |
| `bAllowSideAvoidance` | — | `bool` | `true` | When the centre is blocked, apply a small steering correction toward a clear side. Disable for narrow roads where braking is safer than changing position. | — |
| `bObeyNarrativeTrafficLights` | — | `bool` | `true` | Obey Narrative's closed-lane annotations. Waiting at a traffic light does not count as a blocked arrival. | — |

### `FTerritoryVehicleRetirementSettings`

*struct* · `Source/TerritoryFramework/Public/Navigation/TerritoryRoadTypes.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `EarliestRetirementDelay` | — | `float` | `20.f` | Minimum time an empty mission vehicle remains after its assault resolves. This avoids cars disappearing during the final animation. | `ClampMin` |
| `HardRetirementTimeout` | — | `float` | `120.f` | Maximum cleanup wait for an empty mission vehicle. A vehicle currently possessed by a player is released from Territory management and is never destroyed by this timeout. | `ClampMin` |
| `PlayerKeepAliveDistance` | — | `float` | `5000.f` | An empty vehicle is kept while any player is within this distance. It can still retire after Hard Retirement Timeout. | `ClampMin` |

### `UTerritoryMapMarker`

*class* · `Source/TerritoryFramework/Public/Navigation/TerritoryMapMarker.h` · 5 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ContestedColor` | — | `FLinearColor` | `FLinearColor(1.f, 1.f, 0.f, 1.f)` | *no description in source* | — |
| `EnemyOwnedColor` | — | `FLinearColor` | `FLinearColor(1.f, 0.f, 0.f, 1.f)` | *no description in source* | — |
| `LockedColor` | — | `FLinearColor` | `FLinearColor(0.5f, 0.f, 0.5f, 1.f)` | *no description in source* | — |
| `TrackedCapturedColor` | — | `FLinearColor` | `FLinearColor(0.08f, 0.95f, 0.46f, 1.f)` | *no description in source* | — |
| `UnclaimedColor` | — | `FLinearColor` | `FLinearColor(1.f, 0.f, 0.f, 1.f)` | *no description in source* | — |

## Tales — narrative tasks, quest cascades, story events

### `ATerritoryNarrativeQuestStarter`

*actor* · `Source/TerritoryFramework/Public/Tales/TerritoryNarrativeQuestStarter.h` · 6 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `InitialDelay` | — | `float` | `0.25f` | Delay before the first readiness check. This is not relied on for correctness; the actor keeps retrying until Narrative is actually ready. | `ClampMin` `UIMin` |
| `QuestClass` | — | `TSubclassOf<UQuest>` | `—` | Narrative Quest class to start. Easy example: NQ_CaptureBlacksmith. | — |
| `RetryInterval` | — | `float` | `0.25f` | How often the server checks for a ready Tales component, completed save loading, and late joiners. | `ClampMin` `UIMin` |
| `StartFromID` | — | `FName` | `NAME_None` | Usually leave None. Set a valid Narrative Quest State ID only when this story should resume from a specific authored state. | — |
| `bKeepPollingForLateJoiningPlayers` | — | `bool` | `true` | Keeps a light server timer active so late-joining players also receive the Quest. Disable only for a one-time session event that must ignore late joiners. | — |
| `bStartForEveryPlayer` | — | `bool` | `true` | Enable for personal story progress so every player receives the Quest. Disable when only the first ready player should own this Quest. | — |

### `FTerritoryDialogueRecipeNode`

*struct* · `Source/TerritoryFramework/Public/Tales/TerritoryDialogueRecipe.h` · 8 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Conditions` | — | `TArray<TObjectPtr<UNarrativeCondition>>` | `—` | *no description in source* | — |
| `Events` | — | `TArray<TObjectPtr<UNarrativeEvent>>` | `—` | *no description in source* | — |
| `ID` | — | `FName` | `—` | *no description in source* | — |
| `Position` | — | `FVector2D` | `FVector2D::ZeroVector` | *no description in source* | — |
| `Replies` | — | `TArray<FName>` | `—` | *no description in source* | — |
| `SpeakerID` | — | `FName` | `—` | *no description in source* | `EditCondition` `EditConditionHides` |
| `Text` | — | `FText` | `—` | *no description in source* | — |
| `bPlayer` | — | `bool` | `false` | *no description in source* | — |

### `FTerritoryQuestCascadeBranch`

*struct* · `Source/TerritoryFramework/Public/Tales/TerritoryQuestCascadeRecipe.h` · 7 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `BranchID` | — | `FName` | `NAME_None` | A unique stable ID for this route. Do not rename it after shipping saves. Easy example: ClearBlacksmithDefenders. | — |
| `Conditions` | — | `TArray<TObjectPtr<UNarrativeCondition>>` | `—` | All conditions must pass before this route can finish. Easy example: Diplomacy is War AND the player is inside Blacksmith. Territory adds a hidden runtime gate so these work in Narrative Quests. | — |
| `Description` | — | `FText` | `—` | What the player must accomplish on this route. Easy example: Defeat the guards and claim the Blacksmith for the Regime. | — |
| `DestinationStateID` | — | `FName` | `NAME_None` | The State ID reached when every task below completes. Easy example: HandoverProperty. | — |
| `Events` | — | `TArray<TObjectPtr<UNarrativeEvent>>` | `—` | Events run by Narrative when this route starts, ends, or both, according to each event's Event Runtime. Easy example: declare war when the assault route starts. | — |
| `Tasks` | — | `TArray<TObjectPtr<UNarrativeTask>>` | `—` | All tasks in this list are AND requirements. Easy example: Kill 3 Guards plus Capture Blacksmith must both complete. For Kill OR Sneak, create two branches instead. | — |
| `bHidden` | — | `bool` | `false` | Hide the route from the player while keeping it active. Easy example: a secret fail route triggered by a hidden timer task. | — |

### `FTerritoryQuestCascadeState`

*struct* · `Source/TerritoryFramework/Public/Tales/TerritoryQuestCascadeRecipe.h` · 6 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Branches` | — | `TArray<FTerritoryQuestCascadeBranch>` | `—` | Alternative routes from this state. Each route owns one or more AND tasks. Easy example: Assault and Stealth are two alternative branches leading to the same Handover state. | — |
| `Conditions` | — | `TArray<TObjectPtr<UNarrativeCondition>>` | `—` | All conditions must pass before any route may leave this state. Easy example: after the guards are defeated, wait until the owner handover is accepted. Put route-specific requirements on that Branch instead. | — |
| `Description` | — | `FText` | `—` | The current story situation shown by Narrative. Easy example: The defenders are down. Find the owner and negotiate a handover. | — |
| `Events` | — | `TArray<TObjectPtr<UNarrativeEvent>>` | `—` | Events run by Narrative when this state starts, ends, or both. Easy example: unlock the Farm when the Blacksmith success state begins. | — |
| `StateID` | — | `FName` | `NAME_None` | A unique stable ID for this story step. The Start State ID must name one of these rows. Easy example: AssaultBlacksmith. | — |
| `Type` | — | `ETerritoryQuestCascadeStateType` | `ETerritoryQuestCascadeStateType::Objective` | Choose Objective for normal play, Success to complete the quest, or Failure to fail it. Easy example: PropertySecured is Success. | — |

### `FTerritoryQuestRuntimeOverrideRule`

*struct* · `Source/TerritoryFramework/Public/Tales/TerritoryQuestRules.h` · 7 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ActiveQuestState` | — | `ETerritoryQuestStateRequirement` | `ETerritoryQuestStateRequirement::InProgress` | Quest state that activates the override. In Progress is recommended so primary rules resume after success or failure. | — |
| `QuestClass` | — | `TSubclassOf<UQuest>` | `—` | Narrative Quest that temporarily owns this Territory's runtime flow. Easy example: Rescue the Prisoner controls the Place until the Quest succeeds or fails. | — |
| `bIncludeChildTerritories` | — | `bool` | `false` | Apply this rule to loaded child Territories too. Example: a City-wide lockdown Quest pauses automatic rules in every District and Place below the City. | — |
| `bPauseAutomaticCapture` | — | `bool` | `true` | Pause automatic capture points and story-bounds contesting. Use an explicit Narrative Territory capture event when the Quest decides the outcome. | — |
| `bPauseAutomaticCounterattacks` | — | `bool` | `true` | Pause automatic and recurring counterattacks. A Wave of Enemies Narrative Event is explicit Quest work and may still launch. | — |
| `bPauseDefenderCombat` | — | `bool` | `false` | *no description in source* | — |
| `bPauseStateRules` | — | `bool` | `true` | Pause primary State Config conditions and entry/exit events. Explicit Quest capture or unlock events still change the live Territory, and skipped rewards are not replayed later. | — |

### `UTerritoryAIObservationTask`

*AI / narrative task* · `Source/TerritoryFramework/Public/Tales/TerritoryAIObservationTask.h` · 8 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ActivityClass` | — | `TSubclassOf<UNPCActivity>` | `—` | *no description in source* | `EditCondition` `EditConditionHides` |
| `DestinationLocation` | — | `FVector` | `FVector::ZeroVector` | *no description in source* | `EditCondition` `EditConditionHides` |
| `DestinationProvider` | — | `TObjectPtr<UNarrativeActorProvider>` | `—` | Actor the observed AI must approach. The task follows provider changes and waits when the destination is not loaded. | `EditCondition` `EditConditionHides` |
| `DistanceTolerance` | — | `float` | `150.f` | Maximum three-dimensional distance that counts as reached. | `ClampMin` `EditCondition` `EditConditionHides` |
| `GoalClass` | — | `TSubclassOf<UNPCGoalItem>` | `—` | *no description in source* | `EditCondition` `EditConditionHides` |
| `Objective` | — | `ETerritoryAIObservationObjective` | `ETerritoryAIObservationObjective::ActorAvailable` | *no description in source* | — |
| `TargetProvider` | — | `TObjectPtr<UNarrativeActorProvider>` | `—` | Narrative Actor Provider for the NPC, its controller, or another observed actor. Find NPC is recommended for World Partition safe quests. | — |
| `bCompleteIfAlreadySatisfied` | — | `bool` | `true` | For positive state objectives, allow the task to complete when the state is already true at start. Loss, vehicle-exit, and token-release objectives always require this task to observe the earlier positive state first. | — |

### `UTerritoryActivateDisguiseEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStealthEvents.h` · 1 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `DisguiseProfile` | — | `TObjectPtr<UTerritoryDisguiseProfile>` | `—` | Useful for a cutscene or dialogue reward. For normal inventory play, use Territory Disguise Clothing so equip and unequip happen automatically. | — |

### `UTerritoryAssaultCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryConditions.h` · 8 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `AttackingFaction` | — | `FGameplayTag` | `—` | Optional exact faction that sends the force. Empty checks forces from every faction. This is not the speaking NPC or requesting player. | `Categories` |
| `Comparison` | — | `ETerritoryIntegerComparison` | `ETerritoryIntegerComparison::AtLeast` | *no description in source* | `EditCondition` `EditConditionHides` |
| `Query` | — | `ETerritoryAssaultConditionQuery` | `ETerritoryAssaultConditionQuery::AnyPendingOrActive` | *no description in source* | — |
| `RequiredResolution` | — | `ETerritoryAssaultResolution` | `ETerritoryAssaultResolution::AllAttackersRemoved` | *no description in source* | `EditCondition` `EditConditionHides` |
| `RequiredState` | — | `ETerritoryAssaultState` | `ETerritoryAssaultState::Active` | *no description in source* | `EditCondition` `EditConditionHides` |
| `ScenarioID` | — | `FName` | `—` | Optional exact story encounter ID. Use the same ID on the enemy wave event so an unrelated victory cannot unlock this handover. | — |
| `TerritoryToCheck` | — | `FGameplayTag` | `—` | Territory whose finite counterattack record is inspected. | `Categories` |
| `Value` | — | `int32` | `1` | Example: Killed Attackers At Least 3 can unlock a reinforcement objective. | `ClampMin` `EditCondition` `EditConditionHides` |

### `UTerritoryAssaultTask`

*AI / narrative task* · `Source/TerritoryFramework/Public/Tales/TerritoryAssaultTask.h` · 4 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `AttackingFaction` | — | `FGameplayTag` | `—` | Optional attacking faction filter. Leave empty when any hostile faction may satisfy the objective. | `Categories` |
| `Objective` | — | `ETerritoryAssaultTaskObjective` | `ETerritoryAssaultTaskObjective::RepelAttack` | Story outcome or progress watched by this task. Easy example: Repel the Counterattack completes only after every finite attacker is removed. | — |
| `ScenarioID` | — | `FName` | `—` | Optional stable Story Pursuit Scenario ID. Leave empty for an ordinary strategic counterattack. | — |
| `TargetTerritory` | — | `FGameplayTag` | `—` | Territory whose assault records drive this task. Easy example: Territory.HavenReach.MarketSquare.Blacksmith. | `Categories` |

### `UTerritoryCancelEnemyWavesEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryEvents.h` · 4 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `AttackingFaction` | — | `FGameplayTag` | `—` | Optional attacker filter. Leave empty to cancel waves from every faction. | `Categories` |
| `ScenarioID` | — | `FName` | `—` | Optional exact story encounter ID from the Wave event. Example: BlacksmithRetake cancels only that encounter. Empty preserves the old behavior and allows all story IDs. | — |
| `TargetTerritory` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `bIncludePhysicallyActiveAssaults` | — | `bool` | `false` | False cancels only grace, warning, and waiting records. True also retires living attackers from an active assault. | — |

### `UTerritoryCaptureEligibilityCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryCaptureEligibilityCondition.h` · 7 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `CapturingFactionSource` | — | `ETerritoryCaptureFactionSource` | `ETerritoryCaptureFactionSource::NarrativeTargetFaction` | *no description in source* | — |
| `ExplicitCapturingFaction` | — | `FGameplayTag` | `—` | *no description in source* | `EditCondition` `EditConditionHides` `Categories` |
| `SituationProfile` | — | `TObjectPtr<class UTerritorySituationProfile>` | `—` | *no description in source* | — |
| `TerritoryToCheck` | — | `FGameplayTag` | `—` | Independent Place that the owner NPC may hand over. | `Categories` |
| `bRequireContestedState` | — | `bool` | `false` | *no description in source* | — |
| `bRequireNoLivingDefenders` | — | `bool` | `true` | *no description in source* | — |
| `bRequireStoryCaptureFlow` | — | `bool` | `false` | *no description in source* | — |

### `UTerritoryCaptureEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryCaptureEvent.h` · 5 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `CapturingFaction` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `CapturingFactionSource` | — | `ETerritoryCaptureFactionSource` | `ETerritoryCaptureFactionSource::ExplicitFaction` | *no description in source* | — |
| `SituationProfile` | — | `TObjectPtr<class UTerritorySituationProfile>` | `—` | *no description in source* | — |
| `TargetTerritoryTag` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `bForceCapture` | — | `bool` | `false` | *no description in source* | — |

### `UTerritoryCaptureTask`

*AI / narrative task* · `Source/TerritoryFramework/Public/Tales/TerritoryCaptureTask.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `RequiredCapturingFaction` | — | `FGameplayTag` | `—` | Faction that must own the Territory. Leave empty to accept any valid owner. Easy example: Narrative.Factions.Heroes means only a Heroes capture completes the task. | `Categories` |
| `TargetTerritoryTag` | — | `FGameplayTag` | `—` | Place, District, or City watched by this Narrative quest task. Easy example: Territory.HavenReach.MarketSquare.Blacksmith. | `Categories` |
| `bCompleteOnLoss` | — | `bool` | `false` | Complete when the owner present at task start loses control. Easy example: start this while Bandits own Blacksmith, then complete when Bandits lose it. | — |

### `UTerritoryCharacterActionTask`

*AI / narrative task* · `Source/TerritoryFramework/Public/Tales/TerritoryCharacterActionTask.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Objective` | — | `ETerritoryCharacterActionObjective` | `ETerritoryCharacterActionObjective::Jump` | Movement action to count. Use Narrative's built-in Move task when the objective is distance travelled. | — |
| `SubjectProvider` | — | `TObjectPtr<UNarrativeActorProvider>` | `—` | Optional Narrative Actor Provider for another Character. Example: Find NPC watches an escort climb. Empty follows the quest player's current Narrative character, including after respawn and while driving. | — |
| `bCountInitialState` | — | `bool` | `false` | For positive state objectives such as Crouch, Sprint, Swim, Climb, or Cover, count an already-active state when the task starts. Stop/exit objectives always require a real transition. | — |

### `UTerritoryClearExposureEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStealthEvents.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `TerritoryToModify` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `bResetSuspicion` | — | `bool` | `true` | *no description in source* | — |

### `UTerritoryCombatProgressTask`

*AI / narrative task* · `Source/TerritoryFramework/Public/Tales/TerritoryCombatProgressTask.h` · 5 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `CounterpartyProvider` | — | `TObjectPtr<UNarrativeActorProvider>` | `—` | Optional actor filter. For Deal Damage this is the damaged target; for Take Damage it is the attacker; for Healing it is the healer. | — |
| `Objective` | — | `ETerritoryCombatProgressObjective` | `ETerritoryCombatProgressObjective::DealDamageAmount` | *no description in source* | — |
| `RequiredEffectTags` | — | `FGameplayTagContainer` | `—` | Optional effect asset tags. Every configured tag must match the Gameplay Effect that produced the combat event. Leave empty to accept any effect. | — |
| `SubjectProvider` | — | `TObjectPtr<UNarrativeActorProvider>` | `—` | Actor whose Narrative combat events are counted. Empty follows the quest player's current character, including after respawn and while driving. Example: Find NPC watches a story boss die. | — |
| `bCompleteIfAlreadyDead` | — | `bool` | `true` | When enabled, an already-dead subject completes a Die task immediately. Disable it when the quest needs a new death after this task starts. | `EditCondition` `EditConditionHides` |

### `UTerritoryConditionGroup`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryConditionGroup.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Conditions` | — | `TArray<TObjectPtr<UNarrativeCondition>>` | `—` | Add existing Territory or Narrative conditions here. A row's Not checkbox reverses that row. Add a Known State check before an inverted state check when missing data must block the dialogue. | — |
| `Match` | — | `ETerritoryConditionGroupMatch` | `ETerritoryConditionGroupMatch::All` | All means every row must pass. Any means one passing row is enough. Use nested groups to mix AND and OR. | — |

### `UTerritoryControlProgressCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryConditions.h` · 5 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Comparison` | — | `ETerritoryFloatComparison` | `ETerritoryFloatComparison::AtLeast` | *no description in source* | — |
| `ContestingFaction` | — | `FGameplayTag` | `—` | Optional exact faction applying capture pressure. Empty checks the Place's progress without a faction filter. A different or missing contesting faction fails this condition. | `Categories` |
| `EqualityTolerancePercent` | — | `float` | `0.5f` | *no description in source* | `ClampMin` `ClampMax` `EditCondition` `EditConditionHides` |
| `ProgressPercent` | — | `float` | `75.f` | Progress percentage used by the comparison. Example: 75 means capture pressure reached seventy-five percent. | `ClampMin` `ClampMax` |
| `TerritoryToCheck` | — | `FGameplayTag` | `—` | Territory whose real capture progress is inspected. | `Categories` |

### `UTerritoryDialogueRecipe`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryDialogueRecipe.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Nodes` | — | `TArray<FTerritoryDialogueRecipeNode>` | `—` | *no description in source* | — |
| `RootID` | — | `FName` | `—` | *no description in source* | — |
| `Speakers` | — | `TArray<TObjectPtr<UNPCDefinition>>` | `—` | *no description in source* | — |

### `UTerritoryDiplomacyCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryDiplomacyCondition.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `FactionA` | — | `FGameplayTag` | `—` | First Narrative faction. Example: Narrative.Factions.Heroes. | `Categories` |
| `FactionB` | — | `FGameplayTag` | `—` | Second Narrative faction. Example: Narrative.Factions.Bandits. | `Categories` |
| `RequiredState` | — | `EDiplomacyState` | `EDiplomacyState::War` | Relationship that must currently be true. Example: War allows hostile capture and physical counterattacks. | — |

### `UTerritoryDisguiseCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStealthConditions.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Faction` | — | `FGameplayTag` | `—` | Faction used by Perceived, True, or Compromised checks. | `Categories` |
| `Requirement` | — | `ETerritoryDisguiseRequirement` | `ETerritoryDisguiseRequirement::Active` | *no description in source* | — |
| `TerritoryToCheck` | — | `FGameplayTag` | `—` | Optional exact Territory for security acceptance. Empty uses the Place containing the player. | `Categories` |

### `UTerritoryDisguiseIdentityCheckEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStealthEvents.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ObserverFaction` | — | `FGameplayTag` | `—` | Optional checking faction. Empty uses the Territory owner. | `Categories` |
| `TerritoryToCheck` | — | `FGameplayTag` | `—` | Optional exact checkpoint Territory. Empty uses the Place containing the player. | `Categories` |

### `UTerritoryDisguiseTask`

*AI / narrative task* · `Source/TerritoryFramework/Public/Tales/TerritoryDisguiseTask.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Faction` | — | `FGameplayTag` | `—` | Optional perceived or observing faction filter. Easy example: Bandits for a Bandit uniform mission. | `Categories` |
| `Objective` | — | `ETerritoryDisguiseTaskObjective` | `ETerritoryDisguiseTaskObjective::EquipDisguise` | Disguise outcome followed by the quest. Easy example: Pass an Identity Check advances after guards accept the player's cover. | — |
| `TargetTerritory` | — | `FGameplayTag` | `—` | Required by enter/exit objectives and useful as an identity-check filter. | `Categories` |

### `UTerritoryEventContextCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryConditions.h` · 5 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `bRequireAbilitySystemComponent` | — | `bool` | `true` | Require Target to expose a valid Gameplay Ability System Component. Enable this before Narrative events such as Give XP or Apply Gameplay Effect. | — |
| `bRequirePlayerControlledTarget` | — | `bool` | `true` | Require Target to be controlled by a real player. Example: an AI recapturing a Place will not receive player XP. | — |
| `bRequirePlayerController` | — | `bool` | `false` | Require the explicit Player Controller passed to the event. Enable this for player-only UI or controller actions. | — |
| `bRequireTalesComponent` | — | `bool` | `false` | Require the explicit Tales Component passed to the event. Enable this for quest or dialogue actions that need Narrative story state. | — |
| `bRequireTargetPawn` | — | `bool` | `true` | Require the event's explicit Target pawn to be valid. Example: a world recovery with no player will not run a player reward. | — |

### `UTerritoryExecuteResourceRecipeEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryEvents.h` · 5 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `BatchCount` | — | `int32` | `1` | Finite number of recipe batches executed in one validated transaction. | `ClampMin` |
| `Faction` | — | `FGameplayTag` | `—` | Faction that owns the explicit Narrative inventory account. | `Categories` |
| `Recipe` | — | `FTerritoryProductionRule` | `—` | Atomic Narrative inventory inputs and outputs. Example: consume medicine supplies and produce one relief package. | — |
| `SourceTerritory` | — | `FGameplayTag` | `—` | Semantic source recorded in the production result. It does not bypass ownership or capture. | `Categories` |
| `UpgradeLevel` | — | `int32` | `0` | *no description in source* | `ClampMin` |

### `UTerritoryExposureCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStealthConditions.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `RequiredExposure` | — | `ETerritoryExposureRequirement` | `ETerritoryExposureRequirement::ExposedOrStealthDisabled` | Easy example: add Exposed Or Stealth Disabled to the Contested Set Diplomacy event. Walking inside stays peaceful; confirmed sight starts the existing War event. | — |
| `TerritoryToCheck` | — | `FGameplayTag` | `—` | Optional exact Territory. Empty uses the containing state-config Territory or target location. | `Categories` |

### `UTerritoryFactionDistrictHoldingCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryConditions.h` · 4 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Comparison` | — | `ETerritoryIntegerComparison` | `ETerritoryIntegerComparison::AtLeast` | *no description in source* | — |
| `DistrictCount` | Claimed District Count | `int32` | `1` | Number of unlocked Districts fully Claimed through their authored Places. Example: At Least 1 means this faction controls one complete District. At Least 2 can trigger a diplomacy reaction. World Partition Districts count through the replicated strategic directory. Locked, Contested, partial, and Unclaimed Districts do not count. | `ClampMin` |
| `Faction` | — | `FGameplayTag` | `—` | Fixed faction whose Claimed Districts are counted. Example: Narrative.Factions.Bandits. | `EditCondition` `EditConditionHides` `Categories` |
| `FactionSource` | — | `ETerritoryCaptureFactionSource` | `ETerritoryCaptureFactionSource::ExplicitFaction` | Where the faction comes from. Explicit keeps a fixed faction. Narrative Target follows the character who caused the quest/event. Controller Pawn follows the current possessed player, so a story faction change is respected. | — |

### `UTerritoryGameplayStateTask`

*AI / narrative task* · `Source/TerritoryFramework/Public/Tales/TerritoryGameplayStateTask.h` · 7 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Attribute` | — | `FGameplayAttribute` | `—` | Replicated GAS attribute to observe. Easy example: Narrative Health. | `EditCondition` `EditConditionHides` |
| `Objective` | — | `ETerritoryGameplayStateObjective` | `ETerritoryGameplayStateObjective::AllTagsPresent` | *no description in source* | — |
| `RequiredTags` | — | `FGameplayTagContainer` | `—` | Tags read from the subject's real ASC. Easy example: Narrative.State.Weapon.IsAiming and Narrative.State.Weapon.IsFiring. | `EditCondition` `EditConditionHides` |
| `SubjectProvider` | — | `TObjectPtr<UNarrativeActorProvider>` | `—` | Actor whose ability system is observed. Example: Find NPC watches a boss. Empty follows the quest player's current Narrative character and keeps the player's ability system while driving. | — |
| `Threshold` | — | `float` | `1.f` | Value compared with the selected attribute. | `EditCondition` `EditConditionHides` |
| `bCompleteIfAlreadySatisfied` | — | `bool` | `true` | Complete immediately when the state is already true. Disable it when the story requires a new tag or attribute transition after this task begins. | — |
| `bExactTagMatch` | — | `bool` | `false` | When enabled, parent tags do not satisfy child tags and child tags do not satisfy parent tags. | `EditCondition` `EditConditionHides` |

### `UTerritoryGarrisonCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryGarrisonCondition.h` · 4 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Comparison` | — | `ETerritoryIntegerComparison` | `ETerritoryIntegerComparison::AtLeast` | How the current value is compared with the number below. | — |
| `Metric` | — | `ETerritoryGarrisonMetric` | `ETerritoryGarrisonMetric::ActiveGuards` | Which garrison value should be compared. | — |
| `TerritoryToCheck` | — | `FGameplayTag` | `—` | Territory to inspect. Example: Territory.HavenReach.MarketSquare.Blacksmith. | `Categories` |
| `Value` | — | `int32` | `1` | Number used by the comparison. Example: 1 with Active Guards At Least means one living guard is required. | `ClampMin` |

### `UTerritoryHierarchyStoryOverrideEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryEvents.h` · 5 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ClaimingFaction` | — | `FGameplayTag` | `—` | Exact Narrative faction that receives every independent Place. District and City ownership is then derived from their children. | `EditCondition` `EditConditionHides` `Categories` |
| `LockReason` | — | `FText` | `—` | Reason shown by locked Territory UI. Example: Complete The Governor's Trial. | `EditCondition` `EditConditionHides` |
| `Operation` | — | `ETerritoryHierarchyStoryOperation` | `ETerritoryHierarchyStoryOperation::ClaimForFaction` | *no description in source* | — |
| `RootTerritory` | — | `FGameplayTag` | `—` | Root City, District, or Place. Easy example: choose Haven Reach to change every currently loaded District and Place below it after a betrayal quest. | `Categories` |
| `bForceStoryOverride` | — | `bool` | `true` | Recommended for a deliberate story override. Bypasses Place lock, diplomacy, and state conditions, but never bypasses server authority or the hierarchy reducer. | — |

### `UTerritoryLockEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryLockEvent.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `LockReason` | — | `FText` | `—` | *no description in source* | — |
| `TargetTerritoryTag` | — | `FGameplayTag` | `—` | Exact City, District, or Place to lock. Easy example: to lock a Farm Place, choose its complete Place tag, not only its parent District tag. | `Categories` |

### `UTerritoryModifyReputationEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryDiplomacyEvent.h` · 4 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Faction` | — | `FGameplayTag` | `—` | Narrative faction whose saved campaign reputation changes. | `Categories` |
| `FactionSource` | — | `ETerritoryCaptureFactionSource` | `ETerritoryCaptureFactionSource::ExplicitFaction` | Explicit changes the fixed faction below. Narrative Target or Controller Pawn follows the current real Narrative faction. A missing dynamic faction cancels the event; it never silently changes the old faction. | — |
| `Operation` | — | `ETerritoryReputationOperation` | `ETerritoryReputationOperation::Add` | Add changes the current value. Set replaces it. | — |
| `Value` | — | `int32` | `0` | Value to add or set. Example: -20 reduces reputation by twenty. | — |

### `UTerritoryNarrativeCheckpointEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryNarrativeCheckpointEvent.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `FallbackCampaignIndex` | — | `int32` | `0` | Fallback campaign number used only before Narrative has an active save. Easy example: 0 creates NarrativeSave0. | `ClampMin` `UIMin` |
| `PlatformUserSlot` | — | `int32` | `0` | Unreal platform user slot, not the visible Narrative campaign number. Keep 0 for Narrative Pro's standard save menu. | `ClampMin` `UIMin` |
| `SaveNameOverride` | — | `FString` | `—` | Usually leave empty. Territory reuses the active Narrative save. Easy example: an empty value reuses NarrativeSave2 when the player loaded campaign slot 2. | — |

### `UTerritoryNarrativeConditionTask`

*AI / narrative task* · `Source/TerritoryFramework/Public/Tales/TerritoryNarrativeConditionTask.h` · 4 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Conditions` | — | `TArray<TObjectPtr<UNarrativeCondition>>` | `—` | Every condition must pass. Easy example: Diplomacy is War AND the player is inside the target Place. Use the condition's inherited Not checkbox for a negative requirement. | — |
| `EvaluationInterval` | — | `float` | `0.25f` | Server check interval. 0.25 seconds is responsive and inexpensive for story conditions. Use a slower value for conditions that do not need instant reaction. | `ClampMin` `ClampMax` |
| `RequirementDescription` | — | `FText` | `—` | Optional player-friendly requirement name. Easy example: Wait until the owner is ready to negotiate. Generated gates are hidden, so this mainly helps manually authored quests. | — |
| `bLatchOnceSatisfied` | — | `bool` | `false` | Disabled means the requirements must stay true until every task on the route completes. Enable for a one-time checkpoint such as 'the player was seen once'. | — |

### `UTerritoryNarrativeDataTask`

*AI / narrative task* · `Source/TerritoryFramework/Public/Tales/TerritoryNarrativeDataTask.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Argument` | — | `FString` | `—` | *no description in source* | — |
| `DataTask` | — | `TObjectPtr<UNarrativeDataTask>` | `—` | *no description in source* | — |
| `bCountPreviousCompletions` | — | `bool` | `false` | *no description in source* | — |

### `UTerritoryOwnerHandoverEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryOwnerHandoverEvent.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `OwnerTerritoryTag` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `bBeginDialogueImmediately` | — | `bool` | `true` | *no description in source* | — |

### `UTerritoryOwnershipCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryOwnershipCondition.h` · 5 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `RequiredOwner` | — | `FGameplayTag` | `—` | Optional exact owner. Leave empty to use the Narrative target pawn/controller faction, including a participant supplied by Tales. A participant whose faction is not ready fails this check. Only a world-level call with no participant accepts any Claimed owner. Easy example: a locked Farm can require the Blacksmith to belong to whichever faction the player currently represents, without hardcoding Heroes. | `Categories` |
| `TerritoryToCheck` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `bPassWhenContested` | — | `bool` | `false` | *no description in source* | — |
| `bPassWhenLocked` | — | `bool` | `false` | *no description in source* | — |
| `bPassWhenUnclaimed` | — | `bool` | `false` | *no description in source* | — |

### `UTerritoryPresenceCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryConditions.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `TerritoryToCheck` | — | `FGameplayTag` | `—` | Place the target pawn must be inside. | `Categories` |
| `bIncludeChildTerritories` | — | `bool` | `true` | When enabled, a pawn inside a child Property also counts as being inside its parent District or City. | — |

### `UTerritoryProductionStatusCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryConditions.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `RequiredStatus` | — | `ETerritoryProductionStatus` | `ETerritoryProductionStatus::Produced` | Example: Missing Input can start a supply quest. | — |
| `RuleTag` | — | `FGameplayTag` | `—` | Optional exact production rule. Leave empty to use the Property's overall last status. | — |
| `TerritoryToCheck` | — | `FGameplayTag` | `—` | Producing Property to inspect. | `Categories` |

### `UTerritoryQuestCascadeRecipe`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryQuestCascadeRecipe.h` · 12 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `CheckpointFallbackCampaignIndex` | — | `int32` | `0` | Used only before a campaign is active. Easy example: 0 falls back to NarrativeSave0. | `ClampMin` `UIMin` `EditCondition` `EditConditionHides` |
| `CheckpointMode` | — | `ETerritoryQuestCheckpointMode` | `ETerritoryQuestCheckpointMode::Disabled` | Choose when generated states automatically save Narrative Pro progress. Easy example: Every Objective State makes a three-state quest resume at the latest reached objective after death. | — |
| `CheckpointSaveNameOverride` | — | `FString` | `—` | Usually leave empty so the active Narrative campaign is reused. Easy example: an empty value keeps saving NarrativeSave2 if slot 2 is active. | `EditCondition` `EditConditionHides` |
| `NarrativeQuestGraph` | Narrative Quest Graph | `TSoftClassPtr<UQuest>` | `—` | Select an existing Narrative Quest file to inspect its compiled runtime graph. The read-only panel shows the real states, routes, tasks, conditions, events, settings, and setup findings. Press Refresh Runtime Quest Summary after editing the Quest graph. | — |
| `QuestDescription` | — | `FText` | `—` | Player-facing quest summary copied into Narrative. Easy example: Remove the occupation and convince the owner to support your faction. | — |
| `QuestDialogue` | — | `TSubclassOf<UDialogue>` | `—` | Optional Narrative Dialogue linked to the generated quest. Easy example: one dialogue contains the briefing, owner handover, and betrayal conversation. | — |
| `QuestDialoguePlayParams` | — | `FDialoguePlayParams` | `—` | Narrative's normal dialogue start node, priority, movement, skipping, and exit overrides. Easy example: start from OwnerHandover and stop player movement for the negotiation. | `EditCondition` `EditConditionHides` |
| `QuestName` | — | `FText` | `—` | Player-facing quest name copied into Narrative. Easy example: Liberate the Blacksmith. | — |
| `StartStateID` | — | `FName` | `NAME_None` | The State ID where the quest begins. It must be an Objective state. Easy example: ApproachBlacksmith. | — |
| `States` | — | `TArray<FTerritoryQuestCascadeState>` | `—` | All objective and ending states in the reusable story. Easy example: Approach, ClearDefenders, Handover, Success, and OwnerDiedFailure. | — |
| `bResumeDialogueAfterLoad` | — | `bool` | `false` | Resume the linked dialogue after loading. Enable for an important multi-step conversation; disable for a short ambient exchange that may safely restart later. | `EditCondition` `EditConditionHides` |
| `bTracked` | — | `bool` | `true` | When enabled, Narrative tracks this quest and displays navigation markers from its active tasks. Easy example: enable it for the main liberation mission; disable it for a hidden background mission. | — |

### `UTerritoryQuestStateCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryConditions.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `QuestClass` | — | `TSubclassOf<UQuest>` | `—` | Narrative Quest to inspect. Easy example: select Stealth Investigation, then choose In Progress to allow an event only during that quest. | — |
| `RequiredState` | — | `ETerritoryQuestStateRequirement` | `ETerritoryQuestStateRequirement::InProgress` | Required Narrative quest state. Use the inherited Not checkbox to invert it; for example Not + In Progress means do not run during this quest. | — |

### `UTerritoryReportDistractionEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStealthEvents.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `LocationOffset` | — | `FVector` | `FVector::ZeroVector` | World offset from the explicit Narrative target. Easy example: use 0,0,0 to make the nearest guards investigate the player's current position. | — |
| `TerritoryToModify` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |

### `UTerritoryReputationCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryConditions.h` · 4 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Comparison` | — | `ETerritoryIntegerComparison` | `ETerritoryIntegerComparison::AtLeast` | *no description in source* | — |
| `Faction` | — | `FGameplayTag` | `—` | Faction whose saved reputation is inspected. | `Categories` |
| `FactionSource` | — | `ETerritoryCaptureFactionSource` | `ETerritoryCaptureFactionSource::ExplicitFaction` | Explicit uses the fixed Faction below. Narrative Target or Controller Pawn follows the current real faction, including a story faction change. Missing character context fails. | — |
| `Value` | — | `int32` | `0` | Reputation used by the comparison. Example: At Least 50 unlocks trusted-faction dialogue. | — |

### `UTerritoryResourceCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryConditions.h` · 4 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Comparison` | — | `ETerritoryIntegerComparison` | `ETerritoryIntegerComparison::AtLeast` | *no description in source* | — |
| `Faction` | — | `FGameplayTag` | `—` | Faction whose registered Narrative resource inventory is inspected. | `Categories` |
| `Quantity` | — | `int32` | `1` | Example: At Least 10 medicine allows a hospital relief event. | `ClampMin` |
| `ResourceItem` | — | `TSubclassOf<UNarrativeItem>` | `—` | Exact Narrative item class used as the strategic resource. | — |

### `UTerritoryRevealInfiltratorEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStealthEvents.h` · 1 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `TerritoryToModify` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |

### `UTerritoryScheduleEnemyWaveEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryEvents.h` · 7 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `AttackingFaction` | — | `FGameplayTag` | `—` | Exact attacker, or preferred tie-break when Best Eligible Attacker is enabled. Example: Narrative.Factions.Bandits. | `Categories` |
| `LaunchMode` | — | `ETerritoryAssaultLaunchMode` | `ETerritoryAssaultLaunchMode::StrategicCounterattack` | Strategic Counterattack starts one finite battle and may later follow the force profile's One Assault, Finite Series, or Unlimited Schedule policy. Story Pursuit / Boss Chase is a deliberate Tales exception, requires force-profile permission, and never repeats automatically. | — |
| `OpposingFaction` | — | `FGameplayTag` | `—` | Optional exact opponent. Empty uses the explicit Narrative target pawn's faction, falling back to the Tales owner. The sender must still own the Place and be at war with this faction. | `EditCondition` `EditConditionHides` `Categories` |
| `ScenarioID` | — | `FName` | `—` | Stable story encounter ID, for example Blacksmith_PostCapture. Supported in every launch mode and required for owner reinforcements. Use the same ID in an assault condition or quest task so another battle cannot count as this one. Named strategic and owner-reinforcement victories survive history trimming. Owner reinforcements also reject a completed encounter; guard other repeatable Wave events with conditions. | — |
| `TargetTerritory` | — | `FGameplayTag` | `—` | Claimed Territory the enemy will physically attack. | `Categories` |
| `bChooseBestEligibleAttacker` | — | `bool` | `false` | When enabled, diplomacy, force power, supply, budgets, and deterministic priority choose the best configured attacker. | — |
| `bStartImmediately` | Start Counterattack Immediately | `bool` | `false` | Launch the physical force immediately after validation. Use for an authored ambush or Quest climax. Leave off for the normal strategic schedule. | — |

### `UTerritorySetDiplomacyEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryDiplomacyEvent.h` · 10 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `FactionA` | — | `FGameplayTag` | `—` | Explicit first Narrative faction and migration fallback. Example: Narrative.Factions.Heroes. | `Categories` |
| `FactionASource` | — | `ETerritoryDiplomacyFactionSource` | `ETerritoryDiplomacyFactionSource::ExplicitTag` | Where Faction A comes from. For a Contested state row, Current Owning Faction means the defending faction. | — |
| `FactionB` | — | `FGameplayTag` | `—` | Explicit second Narrative faction and migration fallback. Example: Narrative.Factions.Bandits. | `Categories` |
| `FactionBSource` | — | `ETerritoryDiplomacyFactionSource` | `ETerritoryDiplomacyFactionSource::ExplicitTag` | Where Faction B comes from. For a Contested state row, Contesting Faction means the attacker. | — |
| `NewState` | — | `EDiplomacyState` | `EDiplomacyState::War` | New rich relationship. Territory also writes the matching Friendly, Neutral, or Hostile attitude to Narrative GameState. | — |
| `TradeDurationGameTime` | — | `float` | `-1.f` | Narrative game-time duration for a Trade Agreement. Zero or a negative value means permanent. | `EditCondition` `EditConditionHides` |
| `bApplyWhenStateStartsActive` | — | `bool` | `true` | Recommended for state policy. On a fresh world only, apply this diplomacy event when the Territory starts already in the configured state. Other entry events are not fired. | — |
| `bFallbackToExplicitFactionWhenContextMissing` | — | `bool` | `true` | If a selected dynamic source is empty, use its Explicit Faction Tag. Recommended during migration and for initial-state policy. | — |
| `bPreserveOtherActiveTerritoryWars` | — | `bool` | `true` | Before applying a peace-like relationship, check loaded and World Partition territory summaries. Skip the change if another Place is still contested by this pair. | `EditCondition` `EditConditionHides` |
| `bRequireContainingTerritoryOwner` | — | `bool` | `true` | When this event is inside a Territory state config, require the post-transition owning faction to be one side of the resolved pair. This prevents an old hardcoded Heroes/Bandits row from changing diplomacy after a third faction owns the Place. | — |

### `UTerritorySetDisguiseCoverEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStealthEvents.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Action` | — | `ETerritoryDisguiseEventAction` | `ETerritoryDisguiseEventAction::Compromise` | *no description in source* | — |
| `ObserverFaction` | — | `FGameplayTag` | `—` | Faction that recognizes the player. Empty means every faction. | `Categories` |
| `TerritoryContext` | — | `FGameplayTag` | `—` | Optional context for Gameplay Event payload and debugging. | `Categories` |

### `UTerritorySetGarrisonTargetEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryEvents.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `DesiredGuards` | — | `int32` | `1` | Exact desired staffing target. Increasing it charges the explicit Narrative target's inventory. | `ClampMin` |
| `TargetTerritory` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |

### `UTerritorySetNarrativePlayerFactionsEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryEvents.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `NewFactions` | — | `FGameplayTagContainer` | `—` | Faction memberships applied to the exact Narrative target player. Easy example: replace Police with Heroes after the Regime betrays the player. | `Categories` |
| `PrimaryFaction` | — | `FGameplayTag` | `—` | Choose the faction the player represents when owning, capturing and managing places. Example: add Rebels and choose Rebels as Primary Faction while keeping a second membership. Leave empty to keep the current order. | `Categories` |
| `bReplaceExistingFactions` | — | `bool` | `true` | True replaces every existing player faction. False adds memberships. Both modes commit once through Narrative Player State. Use Primary Faction to deliberately choose which political membership comes first. | — |

### `UTerritorySetStealthOverrideEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStealthEvents.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `TerritoryToModify` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |
| `bClearOverride` | — | `bool` | `false` | Return control to the active Territory Stealth Profile instead of storing an override. | — |
| `bEnableInfiltration` | — | `bool` | `true` | *no description in source* | — |

### `UTerritorySituationCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritorySituationCondition.h` · 7 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Comparison` | — | `ETerritoryFloatComparison` | `ETerritoryFloatComparison::AtLeast` | *no description in source* | `EditCondition` `EditConditionHides` |
| `FactionOverride` | — | `FGameplayTag` | `—` | Optional exact faction for this condition only. Empty uses the profile's Faction Source. Example: Narrative.Factions.Bandits checks Bandit holdings without changing the player's capture faction. | `Categories` |
| `Profile` | — | `TObjectPtr<UTerritorySituationProfile>` | `—` | *no description in source* | — |
| `Query` | — | `ETerritorySituationQuery` | `ETerritorySituationQuery::RetakeNeeded` | *no description in source* | — |
| `Relationship` | — | `EDiplomacyState` | `EDiplomacyState::War` | *no description in source* | `EditCondition` `EditConditionHides` |
| `Scope` | — | `ETerritorySituationScope` | `ETerritorySituationScope::District` | Used for holdings, share and dominant-faction queries. Defence power always uses the target owner's District front, as the assault scheduler does. | — |
| `Value` | — | `float` | `1.f` | *no description in source* | `ClampMin` `EditCondition` `EditConditionHides` |

### `UTerritorySituationProfile`

*profile DataAsset* · `Source/TerritoryFramework/Public/Tales/TerritorySituationCondition.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ExplicitFaction` | — | `FGameplayTag` | `—` | *no description in source* | `EditCondition` `EditConditionHides` `Categories` |
| `FactionSource` | — | `ETerritoryCaptureFactionSource` | `ETerritoryCaptureFactionSource::NarrativeTargetFaction` | *no description in source* | — |
| `Territory` | — | `FGameplayTag` | `—` | One independent Place. Its authored parents supply District and City context. Duplicate this profile to reuse a dialogue pattern for another Place. | `Categories` |

### `UTerritoryStartBossChaseEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryEvents.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `PursuingFaction` | — | `FGameplayTag` | `—` | Exact pursuing faction. Configure that faction's Attacker Definition as the boss or boss-force NPC Definition in the target's counterattack profile. | `Categories` |
| `PursuitOptions` | — | `FTerritoryStoryPursuitOptions` | `—` | Reusable story options. Enemy Chases Player drives into the Place and hands off to combat. Player Chases Enemy reverses the authored vehicle route and records Target Escaped if the capo reaches the exit. | — |
| `TargetTerritory` | — | `FGameplayTag` | `—` | Claimed Place where the boss force will pursue the player. Its Territory Definition supplies the vehicle and foot approaches. | `Categories` |

### `UTerritoryStateCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryConditions.h` · 5 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Query` | — | `ETerritoryStateConditionQuery` | `ETerritoryStateConditionQuery::PoliticalState` | Choose the fact to check. A place can be Claimed by Bandits and Locked at the same time. Use a Situation condition for gameplay availability through the whole parent hierarchy. | — |
| `RequiredAvailability` | — | `ETerritoryAvailability` | `ETerritoryAvailability::Unlocked` | Locked blocks this place while keeping its owner. Unlocked checks only this place, not its parent District or City. | `EditCondition` `EditConditionHides` |
| `RequiredState` | — | `ETerritoryState` | `ETerritoryState::Claimed` | Political state to match. Claimed does not mean unlocked. Locked (Legacy) is supported and reads the local lock field; prefer Local Lock State for new conditions. | `EditCondition` `EditConditionHides` |
| `TerritoryToCheck` | — | `FGameplayTag` | `—` | Territory to inspect. Example: Territory.HavenReach.MarketSquare. | `Categories` |
| `bAllowUnloadedTerritory` | — | `bool` | `false` | If the actor is streamed out, read its saved or replicated campaign directory entry. This does not load the actor. Missing entries fail. Leave off when the scene requires the actor to be present. | `EditCondition` `EditConditionHides` |

### `UTerritoryStateTask`

*AI / narrative task* · `Source/TerritoryFramework/Public/Tales/TerritoryStateTask.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `Objective` | — | `ETerritoryStateTaskObjective` | `ETerritoryStateTaskObjective::BecomeAvailable` | What must happen. Easy example: choose Unlock Territory for a quest that reveals Castle Hill Farm. | — |
| `TargetTerritory` | — | `FGameplayTag` | `—` | Place, District, or City followed by this task. Easy example: Territory.HavenReach.CastleHill.Farm. | `Categories` |
| `bCompleteIfAlreadySatisfied` | — | `bool` | `true` | If the objective is already true when the quest reaches this task, complete immediately. Leave Territory always requires an observed inside-to-outside transition. | — |

### `UTerritoryStealthEvidenceCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStealthConditions.h` · 3 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `MaximumEvidenceAge` | — | `float` | `5.f` | Zero accepts evidence of any age. Example: 5 accepts only a gunshot heard during the last five seconds. | `ClampMin` |
| `RequiredEvidence` | — | `ETerritoryStealthEvidence` | `ETerritoryStealthEvidence::Sight` | *no description in source* | — |
| `TerritoryToCheck` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |

### `UTerritoryStealthPolicyCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStealthConditions.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `TerritoryToCheck` | — | `FGameplayTag` | `—` | Optional exact Territory. Empty uses the containing state-config Territory or the Territory containing the explicit target. | `Categories` |
| `bRequireEnabled` | — | `bool` | `true` | *no description in source* | — |

### `UTerritorySuspicionCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStealthConditions.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `MinimumSuspicionPercent` | — | `float` | `50.f` | *no description in source* | `ClampMin` `ClampMax` |
| `TerritoryToCheck` | — | `FGameplayTag` | `—` | *no description in source* | `Categories` |

### `UTerritoryUnlockEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryLockEvent.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `TargetTerritoryTag` | — | `FGameplayTag` | `—` | Exact locked City, District, or Place to unlock at runtime. Easy example: choose Territory.MyCity.OldTown.Farm to unlock the Farm Place; choosing Territory.MyCity.OldTown only targets its parent District. | `Categories` |
| `UnlockScope` | — | `ETerritoryUnlockScope` | `ETerritoryUnlockScope::AutomaticHierarchy` | Automatic Hierarchy opens the target's required parent path and respects each Locked Exit Condition. Exact Target changes only this runtime Territory. Force options are trusted story overrides. | — |

### `UTerritoryUpgradePropertyEvent`

*narrative event* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryEvents.h` · 1 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `TargetProperty` | — | `FGameplayTag` | `—` | Loaded Property to upgrade by exactly one level. | `Categories` |

### `UTerritoryWaitTimeCondition`

*class* · `Source/TerritoryFramework/Public/Tales/TerritoryStoryConditions.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `TimeSource` | — | `ETerritoryWaitTimeSource` | `ETerritoryWaitTimeSource::NarrativeCampaignElapsed` | Narrative Campaign time is saved and recommended for story rules. Current World time restarts when the level/world starts. | — |
| `WaitTimeSeconds` | — | `float` | `0.f` | The condition passes when the selected elapsed clock reaches this value. Easy example: 600 means wait until ten minutes of campaign time have elapsed. | `ClampMin` |

## UI — Command Center, HUD, journal, theme hooks

### `UTerritoryDistrictRowWidget`

*class* · `Source/TerritoryFramework/Public/UI/TerritoryDistrictRowWidget.h` · 1 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `bShowInlineGuardActions` | — | `bool` | `false` | *no description in source* | — |

### `UTerritoryEconomyWidget`

*class* · `Source/TerritoryFramework/Public/UI/TerritoryEconomyWidget.h` · 2 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ProductionSiteRowClass` | — | `TSubclassOf<UTerritoryProductionSiteRowWidget>` | `—` | *no description in source* | — |
| `ResourceRowClass` | — | `TSubclassOf<UTerritoryResourceRowWidget>` | `—` | *no description in source* | — |

### `UTerritoryProductionSiteRowWidget`

*class* · `Source/TerritoryFramework/Public/UI/TerritoryProductionWidgets.h` · 1 option(s)

| Option | Shown as | Type | Default | What it does | Rules |
|---|---|---|---|---|---|
| `ResourceRowClass` | — | `TSubclassOf<UTerritoryResourceRowWidget>` | `—` | *no description in source* | — |
