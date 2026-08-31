# Narrative Pro Integration

## Supported baseline

TerritoryFramework is verified against Narrative Pro 2.4.2 on Unreal Engine 5.7.
Narrative Pro is a read-only vendor dependency. Project code, assets, adapters, tests, and
migrations belong to TerritoryFramework or `/Game/TerritoryFramework`; never edit an asset
under `/NarrativePro` to add Territory behavior.

Runtime module dependencies are `NarrativeArsenal`, `NarrativeCommonUI`, and
`NarrativeSaveSystem`. The editor validation module depends on `NarrativeArsenal`.

## Authority boundaries

| Domain | Authority | Territory integration |
|---|---|---|
| Faction identity | `INarrativeTeamAgentInterface` and `FGameplayTag` | Reads exact Narrative faction membership from pawn, controller, or player state |
| Combat attitude | `ANarrativeGameState` | `UTerritoryNarrativeProAdapter` reads/writes the supported attitude projection |
| NPC creation | `UNarrativeCharacterSubsystem` | Guards and attackers call `SpawnNPC`; Territory never owns another NPC registry |
| NPC definition | `UNPCDefinition`, `UCharacterDefinition` | Territory supplies spawn context, stable IDs, factions, activities, and TriggerSets |
| AI intent | Narrative goals, activities, configurations, and TriggerSets | Territory contributes patrol and assault goals/activities |
| Tactical combat | Narrative GAS and attack tokens | Territory limits strategic per-Territory slots through `UTerritoryCombatDirector` |
| Currency/items | Narrative inventory | Economy and production settle atomically against one Narrative inventory account |
| Tales | Narrative tasks, conditions, events, and `UTalesComponent` | Territory capture/lock/query adapters preserve the real Tales instigator |
| Persistence | Narrative save interfaces | Territory actors expose stable records; live UObject and pawn pointers are not saved |
| Navigation/UI | Narrative POI, map marker, HUD, and CommonUI | Territory adds state data and project-owned presentation widgets |
| Music | `UNarrativeMusicSubsystem` and `UTaggedMusicSet` | A local cosmetic adapter selects Definition-authored state themes and restores earlier Narrative music |

Territory owner, contest state, and progress remain authoritative on
`ATerritoryVolume` and `UTerritoryControlSubsystem`. Narrative faction membership is not
territory ownership.

## Music and state audio

`UTerritoryMusicSubsystem` is the local bridge to Narrative Music. It reads replicated Territory
state but never changes ownership or capture. The State Config's `Audio` row can select a Narrative
Tagged Music Set and `Music.*` theme, plus optional state-entered/state-exited sounds. Place rules
win over District rules, District rules win over City rules, and an unconfigured hierarchy leaves
the current world music alone. Dedicated servers never create this subsystem. Full authoring and
troubleshooting are in
[27_Narrative_Music_and_State_Audio.md](27_Narrative_Music_and_State_Audio.md).

## Player integration

Use the project-owned `/Game/TerritoryFramework/Framework/BP_TerritoryPlayerCharacter`.
It derives from Narrative Pro's `BP_NarrativePlayer_GASP` and preserves the Narrative team,
GAS, inventory, interaction, camera, and save behavior. `BP_TerritoryGameMode` selects this
project pawn and the project Narrative player controller.

The controller owns `UTerritoryPlayerManagementComponent` and
`UTerritoryFactionResourceAccountComponent`. These are Territory adapters; the possessed
Narrative pawn inventory remains the real account.

Do not add `ITerritoryOwnershipInterface` to the player to report player allegiance. That
interface answers the owner/progress/contester of the object implementing it. Implementing it
on a player caused the old integration to return the player's faction as a Territory owner and
to resolve the contesting faction through player controller index zero. Query player faction
through `INarrativeTeamAgentInterface`; query the current Territory through
`UTerritoryRegistrySubsystem`, then read ownership from that `ATerritoryVolume`.

## NPC, activity, and combat integration

`ATerritoryGuardCharacter` and `ATerritoryAssaultCharacter` derive from
`ANarrativeNPCCharacter`. Their definitions must provide:

- a class derived from the appropriate Territory Narrative NPC class;
- a stable, non-empty `CharacterID` and `NPCID`;
- `bAllowMultipleInstances` when a force may contain more than one pawn;
- a Narrative NPC controller with spawned auto-possession;
- the intended `UNPCActivityConfiguration` and `UTriggerSet` assets.

Narrative Pro 2.4.2 maps characters by `CharacterID`, while NPC lookup and duplicate admission
still use the deprecated `NPCID`. Both identities therefore remain required and project-owned
definitions must not retain an ID copied from a marketplace definition. Current identities are:

| Definition | CharacterID | NPCID |
|---|---|---|
| `NPC_TerritoryBandit` | `NPC_TerritoryBandit` | `TerritoryBandit` |
| `NPC_TerritoryBanditAssault` | `NPC_TerritoryBanditAssault` | `TerritoryBanditAssault` |
| `NPC_TerritoryHero` | `NPC_TerritoryHero` | `TerritoryHero` |

`FNPCSpawnInfo` and faction/activity/TriggerSet overrides are populated before
`SetNPCDefinition`. Narrative then initializes the definition, controller, activity component,
appearance, equipment, and ability system. Territory registers physical capture participation
only after this admission completes.

Narrative Pro 2.4.2 replaced `UNarrativeAbilitySystemComponent::OnDied` with
`OnDeathStateChanged(AActor*, UNarrativeAbilitySystemComponent*, bool bIsDead)`. Territory
death handlers ignore revival notifications (`bIsDead == false`) and remove a dead participant
exactly once. A guard death never calls `ForceCapture`; surviving physical attackers must finish
the existing capture flow.

Narrative Pro 2.4.2 also added the Boolean death-state parameter to the character
`HandleDeath` BlueprintNativeEvent. Blueprint-generated NPC classes migrated from an older
release can dispatch a stale `false` event value even though the replicated Narrative ASC is
already dead. Territory guard and assault character adapters therefore reconcile the event
against `UNarrativeAbilitySystemComponent::IsDead()`, stop the authoritative AI path, clear
the last character-movement sample on every network role, and enter Narrative's replicated
ragdoll state. Migrated Territory guard Blueprints must also forward `HandleDeath.bIsDead` to
the parent event and guard optional activity cleanup with `Is Valid`; the migration contract
tests both graph requirements. Territory NPCs are server-owned, so their `SetRagdoll` override
only initiates the Narrative ragdoll RPC on authority. Simulated proxies consume Narrative's
replicated `bIsRagdoll` notification and never send an unowned `ServerStartRagdoll` request.
The ASC remains the death authority; Territory does not replicate a second death flag. PIE
correlation must use replicated Territory context or stable identity because placed actor object
names can differ between the dedicated server and client worlds.

Before a Territory guard or attacker is destroyed, withdrawn, or handed to Narrative's death
cleanup, Territory deactivates Narrative's activity component and removes its transient goals.
This prevents a delayed goal or target-death callback from scoring an activity after the cached
NPC controller has entered pending-kill. Narrative still owns the controller, activity component,
goals, and final NPC cleanup; Territory only ends its own NPC's use of those public APIs.

## Content integration inventory

The current `/Game/TerritoryFramework` content contains 50 assets. The direct Narrative asset
dependencies are concentrated in these groups:

| Territory content | Narrative dependency |
|---|---|
| `AI/Combat/BPA_TerritoryAttack_*` | Narrative attack goals and melee/ranged attack activities |
| `AI/BPA_TerritoryPatrol`, `AI/BPA_ReturnToTerritory` | Narrative activity base, patrol/return behavior, level-sequence player |
| `AI/AC_TerritoryGuard`, `AI/Triggers_Bandit` | Narrative activity configuration and TriggerSets |
| `AI/NPC_Territory*`, `AI/BP_TerritoryGuard`, `AI/BP_TerritoryAssualtGuard1` | Narrative NPC definitions, controller, character, GAS, abilities/effects, appearance, animation, equipment, and save |
| `Economy/Items/BP_Item_Grain`, `BP_Item_Meat` | Narrative item and inventory types |
| `Framework/BP_TerritoryPlayerCharacter` | Narrative player character, team, GAS, inventory, camera, interaction, and save |
| `Framework/Controller_Reworked/BP_HopNarrativePlayerController_DemoMap` | Narrative player controller plus Territory-owned management/account components |
| `Framework/BP_TerritoryGameMode` | Narrative GameMode/GameState with project pawn/controller overrides |
| `Interaction/BP_TerritoryDistrictManagementPoint`, map/property assets | Narrative POI, navigation marker, map marker, and item references |
| `UI/WBP_*`, `W_TerritoryPlayerMenu`, `WBP_TerritoryInfoWidget` | Narrative CommonUI activatable widgets, buttons, text, styles, HUD layers, and menu assets |

The remaining Territory assets are plugin-native classes, data assets, behavior trees,
blackboards, maps, or widgets with no direct vendor asset reference. They can still participate
indirectly through the C++ adapters above.

## Tales, save, economy, navigation, and UI

- `UTerritoryCaptureTask`, `UTerritoryStateTask`, `UTerritoryAssaultTask`,
  `UTerritoryDisguiseTask`, `UTerritoryCharacterActionTask`,
  `UTerritoryGameplayStateTask`, `UTerritoryCombatProgressTask`,
  `UTerritoryAIObservationTask`, `UTerritoryCaptureEvent`, `UTerritoryLockEvent`,
  `UTerritoryHierarchyStoryOverrideEvent`, `UTerritoryOwnershipCondition`,
  `UTerritoryQuestStateCondition`, `UTerritoryDiplomacyCondition`,
  `UTerritoryGarrisonCondition`, `UTerritorySetDiplomacyEvent`, and
  `UTerritoryModifyReputationEvent` extend Narrative Tales classes. Player-dependent
  transitions use the explicit pawn/controller/`UTalesComponent` context.
- `ATerritoryVolume`, `ATerritoryGuardSpawnPoint`, `ATerritoryWorldState`, and
  `ATerritorySavableData` implement Narrative save contracts with stable records.
- `UTerritoryEconomySubsystem` and production profiles use `UInventoryComponent` and
  `UNarrativeItem`. Narrative Pro 2.4.2 replicates inventory entries with Fast Array
  serialization; Territory uses public inventory mutations and does not inspect the replicated
  container directly.
- `UTerritoryMapMarker`, `UTerritoryNavigationMarkerComponent`, and
  `ATerritoryDistrictManagementPoint` extend Narrative map, marker, POI, and HUD APIs.
- Territory widgets derive from Narrative/CommonUI types. C++ exposes read models and events;
  project widgets own layout and styling.

`BPA_ReturnToTerritory` uses the Narrative Pro 2.4.2 level-sequence API. Its explicit empty
`IdleSequenceViewers` array means all relevant players may receive the sequence; it does not use
`GetPlayerController(0)`.

### Counterattack and chase quest tasks

All Territory quest tasks are real `UNarrativeTask` children. The Territory editor module adds
the plugin-owned `/TerritoryFramework/Tales/Tasks/` folder to Narrative's in-memory search list;
it does not edit Narrative or require a project config change. Narrative discovers the small
Blueprint wrappers there and lists them under **Tasks: Territory**, **Territory Story**, and the
four **Community** categories using the Blueprint Display Name, Category, and Description metadata
expected by `UQuestGraphSchema`. See
[Territory Narrative Quest Tasks](29_Narrative_Quest_Tasks.md) for the unlock, state, defenders,
garrison, presence, capture, assault, and disguise objective matrix, and
[Community Narrative Quest Tasks](30_Community_Narrative_Tasks.md) for movement, GAS, combat, AI,
boss, and chase examples.

Use **Territory Counterattack / Chase Task** inside a normal Narrative Quest. It reads the real
durable assault record, so a quest does not need to count temporary pawns itself.

| Objective | Easy story example |
|---|---|
| Repel the Counterattack | Defend Blacksmith until the finite Bandit force is defeated |
| Allow the Faction to Take the Territory | An undercover quest succeeds when Bandits complete physical takeover |
| Defeat Counterattack Enemies | Set Narrative Required Quantity to 3 |
| Counterattack Reaches the Territory | Advance dialogue when the warning becomes a live battle |
| Final Fight Started | Advance when the underboss leaves a damaged car |
| Target Escaped | Branch the quest after a chase escape or lost-distance outcome |
| Defeat Story Boss | Complete only after an explicit finite Story Pursuit boss force is removed |
| Chase Target Reaches Exit | Branch only when the target reaches the authored Road Guide exit |
| Player Loses Chase Distance | Branch only after every player exceeds chase distance for the grace period |
| Force Attackers to Withdraw | Count the durable withdrawn force with Narrative Required Quantity |
| Assault Is Cancelled | React to diplomacy, quest rule, ownership, route, capability, or story cancellation |

Filter by Territory, optional attacking faction, and optional Story Scenario ID. The task uses
Narrative's inherited marker settings and attaches its marker to the live Territory actor. The
Capture/Lose task also rebinds after World Partition streams its actor out and back in.

Faction force presentation has three optional Narrative tagged-dialogue moments: reserve wave
arrival, first physical takeover pressure, and damaged-car final fight. Dialogue content remains
in Narrative; Territory only chooses the story moment and speaker.

## Diplomacy-aware guard dialogue

Territory guards and assault characters keep Narrative's interaction and dialogue authority.
Their native `UTerritoryDiplomacyInteractable` asks the attached
`UTerritoryDiplomacyDialogueComponent` for the current relationship before Narrative opens a
dialogue. Author one `UTerritoryDiplomacyDialogueProfile` with optional Same Faction, Neutral,
Ceasefire, Non-Aggression, Trade, Alliance, and War dialogue classes, then assign that profile to
the character Blueprint's **Territory Diplomacy Dialogue** component.

`Faction Dialogue Profiles` can select a different profile from the guard's exact current
Narrative faction. This matters when one shared guard class may represent Heroes, Bandits, or a
story faction after ownership changes. An empty relationship slot falls back to Default Dialogue;
it never invents text or modifies Narrative's `NPCDefinition` asset.

The reference `/Game/TerritoryFramework/AI/BP_TerritoryGuard` is wired to
`DA_TerritoryBanditDiplomacyDialogue`: neutral relationships resolve the normal greeting and War
resolves the Bandit hostile dialogue. Dialogue choice and attack permission are related but
separate checks. Claimed Territory guards still do not attack merely because they see a player;
normal territorial combat requires the Territory to be Contested and diplomacy to be War.

## State Config conditions and events

`UTerritoryDefinition::StateConfigs` is the one editor-facing place for state entry/exit rules.
These are Narrative templates stored in the City, District, or Place Data Asset; they are not
another quest, faction, guard, or ownership database. Each loaded Territory receives private
runtime copies of the templates. The copies are owned by that live actor, so diplomacy, handover,
waves, inventory, and other events always have valid World and transition context.

| Definition-owned Narrative template | Plain-English use | Easy example |
|---|---|---|
| `UTerritoryCaptureEligibilityCondition` | Check whether an independent Place may be handed to the resolved faction now | The neutral Blacksmith owner offers surrender only after every defender is defeated |
| `UTerritoryQuestStateCondition` | Read one Narrative Quest from the explicit player's Tales component | Run a boss pursuit only while Betrayal is In Progress; use inherited Not to block it during that quest |
| `UTerritoryOwnershipCondition` | Check who owns a loaded place | Farm unlocks after Blacksmith is Claimed by Heroes |
| `UTerritoryOwnershipTransitionCondition` | Distinguish a real owner handover from same-owner recovery | Give capture XP only when Claimed was entered by a new faction |
| `UTerritoryDiplomacyCondition` | Check the exact rich treaty between two Narrative factions | Locked exit requires Heroes and Bandits to be at War |
| `UTerritoryGarrisonCondition` | Compare guards, every registered defender, reserve, pending replacements, or staffing shortfall | Living Defenders equals 0 means the Place is genuinely undefended |
| `UTerritoryStateCondition` | Check Unclaimed, Claimed, Contested, or Locked | Use emergency dialogue only while a District is Contested |
| `UTerritoryControlProgressCondition` | Compare real capture progress as a percentage | Start the final warning after control pressure reaches 75% |
| `UTerritoryReputationCondition` | Compare one faction's saved reputation | Regime reputation below -50 opens betrayal dialogue |
| `UTerritoryFactionDistrictHoldingCondition` | Compare current unlocked Claimed Districts for an explicit, Narrative-target, or controller-pawn faction; World Partition rows are included | Player Faction At Least 2 can trigger a Regime diplomacy reaction; Bandits At Least 1 allows a normal counter |
| `UTerritoryAssaultCondition` | Check warning/active/result state or finite force counts | Complete “Hold the Line” after Remaining Attackers equals 0 |
| `UTerritoryPresenceCondition` | Check whether the explicit Narrative target is inside a Place | The commander speaks only while the player is inside Castle Hill |
| `UTerritoryEventContextCondition` | Require the exact pawn/controller/Tales/GAS context an event needs | Give XP only when a player pawn with a valid Ability System caused the transition |
| `UTerritoryProductionStatusCondition` | Check a Property's durable overall or per-rule production result | Missing Input begins a supply quest, even after World Partition streaming |
| `UTerritoryResourceCondition` | Compare an exact Narrative inventory resource for a faction | At least 10 medicine permits a relief operation |
| `UTerritoryStealthPolicyCondition` | Check whether the Place currently permits infiltration | Rescue dialogue is available only while stealth policy is enabled |
| `UTerritoryExposureCondition` | Check hidden, suspicious, exposed, or burned-cover state | Continue the disguise branch only while the player is not exposed |
| `UTerritoryStealthEvidenceCondition` | Check the latest authored evidence type | A bullet impact starts the armed-investigation branch |
| `UTerritorySuspicionCondition` | Compare current suspicion with a threshold | Extraction remains quiet while suspicion is below 50% |
| `UTerritorySetDiplomacyEvent` | Change Territory treaty metadata and the matching Narrative AI attitude | A betrayal changes Heroes and Regime to War |
| `UTerritoryModifyReputationEvent` | Add to or set the saved reputation integer for one faction | Attacking a Bandit convoy adds -20 Bandit reputation |
| `UTerritoryScheduleEnemyWaveEvent` | Schedule a normal strategic counter or an explicit finite story pursuit | A regime boss chases the betrayed player only after the event's quest conditions pass |
| `UTerritoryStartBossChaseEvent` | Start one finite, non-recurring pursuit through the target Place profile | The regime boss arrives in the configured Narrative Sedan after Betrayal becomes In Progress |
| `UTerritoryCancelEnemyWavesEvent` | Cancel matching durable assault records | A peace treaty cancels warnings that have not physically activated |
| `UTerritorySetGarrisonTargetEvent` | Use the atomic guard target and Narrative currency transaction | A player dialogue assigns two guards and pays their recruitment cost |
| `UTerritoryUpgradePropertyEvent` | Buy exactly one normal Property upgrade | A reconstruction quest buys one Farm level if the player can pay |
| `UTerritoryExecuteResourceRecipeEvent` | Atomically consume/produce resources in the explicit Narrative inventory | Consume medicine and food to make one relief shipment |
| `UTerritoryLockEvent` / `UTerritoryUnlockEvent` | Change one Territory's Locked state through its existing authority | A gate quest unlocks a District |
| `UTerritoryCaptureEvent` | Request the existing atomic capture mutation | A trusted quest awards an undefended outpost |
| `UTerritoryOwnerHandoverEvent` | Reveal or invoke the configured neutral owner handover flow | The Blacksmith owner appears after the final defender dies |
| `UTerritoryHierarchyStoryOverrideEvent` | Claim, clear, lock, or unlock one loaded Place/District/City tree while preserving the normal hierarchy authority | After a regime betrayal, give every loaded independent Place under Haven Reach to the Regime; District and City ownership derive from the leaves |
| `UTerritorySetStealthOverrideEvent` | Temporarily enable, disable, or clear a Place's infiltration override | A rescue quest permits stealth inside an otherwise open battle zone |
| `UTerritoryRevealInfiltratorEvent` | Force confirmed exposure for the explicit player | A scripted alarm burns the player's cover |
| `UTerritoryClearExposureEvent` | Clear exposure and optionally suspicion | Changing disguise after escape resets the stealth state |
| `UTerritoryReportDistractionEvent` | Submit an authored investigation stimulus | A thrown bottle makes the closest guard investigate its landing point |

All State Config conditions must pass. Every Narrative Event also has its own inherited
`Conditions` array; all conditions inside that event must pass before the event mutates anything.
Narrative's inherited **Not** option is honored in both places. Territory mutation events set
`Refire On Load` false so quest restoration cannot purchase, spawn, or reward twice.

### Claimed District count -> diplomacy

`Territory Faction Claimed District Count Condition` measures current political control, not a
historical lifetime capture total. A District counts only when it is **Unlocked**, its aggregate
state is **Claimed**, and all authored Places have reduced to the same owner. Contested, partial,
locked, and unclaimed Districts do not count. The replicated World State directory keeps the
answer valid while a District actor is streamed out.

Easy betrayal setup on `Set Territory Diplomacy`:

```text
Set Territory Diplomacy
  Faction A = Regime
  Faction B = Heroes (or your current story faction)
  New State = War
  Conditions
    Territory Faction Claimed District Count
      Faction Source = Narrative Target / Player Faction
      Comparison = At Least
      Claimed District Count = 2
```

This means: “declare the betrayal only after the faction currently represented by the player
controls two complete Districts.” If choices later change the player's Narrative faction tag, the
same condition follows the new tag. Use `Explicit Faction` when the rule should always inspect a
fixed group such as Bandits, and use inherited **Not** to invert the answer.

Quest conditions require a real Narrative event context. Capture and player-triggered events
carry the explicit Tales component. Defender-death hooks resolve it through the killer's
Narrative player controller, so `On Defender Died + Quest In Progress` works when a player caused
the death. If AI or world damage caused the death, there is deliberately no guessed first player;
use profile Quest Rules for a world-level counter gate instead.

`Apply Territory Hierarchy Story Override` is a one-shot event over actors currently loaded and
registered under its root. Ownership operations mutate only independent leaf Places; the normal
unanimity reducer derives District and City ownership. Its Force option may bypass Place lock,
diplomacy, and conditions for a deliberate story decision, but cannot bypass server authority or
the hierarchy. Load the intended World Partition cells before firing it; unloaded descendants
are not silently changed in a second database.

Player rewards need `Territory Event Context Condition` in the reward event's own Conditions
list. A Territory can legitimately re-enter Claimed when contested progress decays back to its
current owner; that world-level recovery has no player target. For example, put the condition on
`NE_GiveXP` with **Require Target Pawn**, **Require Player Controlled Target**, and **Require
Ability System Component** enabled. A real player capture passes, while recovery, save restore,
AI capture, and missing-ASC contexts safely skip the reward.

A death is a **trigger**, not a lasting condition. `On Defender Died Events` fires after each
registered defender death. `On All Defenders Defeated Events` fires only after living defenders
and pending replacements both reach zero. Add `Wave of Enemies` to one of those lists, then add a
`Territory Diplomacy Condition` inside the wave event's own Conditions list. This reads naturally
as: “when the final defender dies, schedule the Bandit wave only if Heroes and Bandits are at War.”
Unregistered or duplicate defender-death callbacks are ignored, so the same casualty cannot
refire the story mutation.
The wave still obeys profile, route, grace, warning, first-player proximity, finite force,
diplomacy, Narrative-time window, and budget rules. Strategic mode also requires the force's
normal District staging rule. Its profile chooses One Assault, Finite Series, or Unlimited
Schedule; every individual fight still has a finite force. Story Pursuit / Boss Chase may bypass
missing staging only when both the event and the force profile explicitly opt in. It never rolls
ownership, repeats automatically, or creates an infinite respawn system.

A missing or World-Partition-unloaded actor makes live state/garrison/presence conditions return
false rather than guessing from a second actor database. Production and assault conditions read
their existing durable authority records and can therefore inspect supported streamed-out state.

State Config means an enum-state transition. A normal physical owner replacement passes
`Claimed -> Contested -> Claimed`, so Claimed entry events run after the successful capture.
A trusted direct `Claimed -> Claimed` owner mutation does not enter the state again. Bind the
control-changed delegate when a story hook must run for every owner replacement, including a
direct administrative/quest mutation.

The demo map uses these rules without hard-coding them in C++:

- Blacksmith `Claimed -> Entry Events` sets Heroes and Bandits to War. This ensures Narrative
  combat attitude and Territory assault policy agree before the capture-change broadcast.
- Farm `Locked -> Exit Conditions` requires both the existing “Blacksmith owned by Heroes”
  condition and the rich Heroes/Bandits War relationship.
- Blacksmith `On All Defenders Defeated Events` schedules one finite Bandit wave. The event's
  inherited Diplomacy condition requires Heroes and Bandits to still be at War.

The design follows Narrative's documented extension contracts: custom conditions override
`CheckCondition`, conditions already provide inversion, and custom events may own assigned
conditions. Narrative NPC definitions/spawn parameters remain the physical enemy source.

## Disguise and double-agent integration

Territory disguise clothing derives from Narrative's normal Clothing Equippable Item. Narrative
continues to own inventory, equipment slots, character meshes, save/load, abilities, Armor,
Attack Rating, Stealth Rating, and the Equipment Gameplay Effect. Territory adds only a temporary
perceived faction. The player's PlayerState faction container is never edited.

Use `UTerritoryDisguiseTask` for equip, accepted entry, checkpoint, compromised cover, restored
cover, remove, and undetected exit objectives. Use `UTerritoryDisguiseCondition` in dialogue or
event conditions, and use the four disguise Narrative Events to activate, remove, burn, restore,
or explicitly check a cover identity. Complete setup and examples are in
[Disguises and Double-Agent Missions](25_Disguise_and_Double_Agent_Missions.md).

## Marketplace update procedure

Road missions reuse Narrative's Chaos vehicle, ZoneGraph, Mass traffic, Quest Road Controls,
impact mesh, and destructible response rather than editing them. Territory's integration and
mission-authoring boundary is documented in [23_Road_Missions.md](23_Road_Missions.md).

1. Close Unreal Editor and verify no dirty packages.
2. Back up the complete active Narrative Pro plugin directory outside the project.
3. Replace the complete project plugin directory with the matching Marketplace build.
4. Do not merge old and new plugin files and do not copy Territory changes into `/NarrativePro`.
5. Build the full editor target so UHT, runtime, and editor modules are checked.
6. Reopen the editor, compile every Territory Blueprint, run data validation and all
   `TerritoryFramework.*` automation tests, then run dedicated-server/two-client, save/load,
   and cook smoke gates.

The 2.3.3 backup created for the 2.4.2 migration is stored at
`D:/HOP TRENDY UNREAL/Unreal Projects/TDA_PluginBackups/NarrativePro_2.3.3_20260824_030205`.

## Verified limitations

- Dedicated-server plus two-client PIE passes the Territory pawn/controller/component and
  join topology checks. The vendor `BP_NarrativePlayer_GASP` BeginPlay graph still invokes
  `UpdateInputState_Server` for simulated remote pawns, producing one no-owning-connection
  warning per client. A project child cannot suppress that parent call without replacing the
  vendor BeginPlay behavior; Territory does not assign false ownership or modify the vendor.
- The installed Epic binary engine cannot build `TDAServer`. A direct
  `UnrealEditor.exe -server -nullrhi` startup is blocked by a pre-gameplay
  `NarrativeArsenalEditor` Slate assertion. Standalone Server and cook/package validation
  therefore remain release-environment gates.
