# Narrative Pro pattern audit — 14 September 2026

Status: in progress. This is a source-based audit, not a release certificate.
The reference is TDA's installed Narrative Pro 2.4.2 on UE 5.8. No Narrative Pro
source or content is being modified. The UE 5.7 compatibility project is a
separate build and test target.

## Where each feature belongs

| Need | Use the existing Narrative feature | Territory's role |
|---|---|---|
| An action a character performs: melee combo, block, dodge, jump, throw, mount or animated interaction | `UNarrativeGameplayAbility`, or `UNarrativeCombatAbility` for the combat targeting/damage workflow; Native ability tasks for asynchronous animation/projectile work | Add an ability subclass when an action is missing. Use Native input tags, activation blockers, costs, cooldowns, cancellation and the character's ASC. |
| Health, stamina, damage, a timed buff or persistent character benefit | Native ASC, attributes, Gameplay Effects and Gameplay Cues | Grant/remove only Territory-owned handles. Keep shared inventory and externally granted abilities intact. |
| Who owns a place, capture progress, faction income, assault scheduling | Existing Territory volume and domain subsystems | These remain world-state authorities. An ability or quest event may request a validated transition; it must not become another owner of that state. |
| A quest objective waiting for an action or result | `UNarrativeTask` in a Tales quest branch | Observe Native/ Territory delegates; let Native task progress, quest branching and saves own completion. A task should not manufacture the action it is waiting for. |
| Check whether a dialogue choice or quest branch is available | Instanced `UNarrativeCondition` | Read explicit player, faction, district and place context. Respect Native Not and party policies. Conditions must not pay rewards or change ownership. |
| Give an item, change faction, start an assault or checkpoint after a story decision | Instanced `UNarrativeEvent` and the relevant existing authority | Validate on the server and report actual success. Keep game-specific story choices in quest/dialogue assets. |
| Conversation, owner handover, optional planning, overheard guard speech | Native Dialogue Blueprint, speaker definitions/providers, line conditions/events and tagged dialogue | Add situation queries and coordinated activity adapters where needed. Free movement/overheard dialogue is a valid Native workflow and should not force a camera takeover. |
| Background soundtrack selected by gameplay state | `UNarrativeMusicSubsystem` + `UTaggedMusicSet` and theme tags | Select a theme from the replicated local Territory state. Native owns playback, fades, loaded sets and sound overrides. |
| An arrival cue or a state-change sting | A configured one-shot sound | Play locally once for the observed transition. Keep it separate from the background music theme. |
| A shot during a spoken line or reply selection | Instanced `UNarrativeDialogueSequence` with Level Sequence assets | `UTerritoryDialogueShot` adds camera-local lens, focus and grading options after Native creates/binds the shot. |
| A full staged cutscene with character binding, branching or skipping | `ANarrativeLevelSequenceActor`, `UNarrativeLevelSequencePlayer`, Native binding/playback settings and sequence tracks | Use Native character binding, sequence-controlled tags and cleanup. Do not implement another cinematic player or save live camera pointers. |
| A guard patrol, drive, investigate or combat interruption | NPC Definition → spawn info → activity configuration/TriggerSets → Native goals and activities | Supply Territory goals and finite force bookkeeping. Native runs movement, combat and tactical attack tokens. |
| Items, ammunition, equipment, currency and weapon input | Native inventory, equippable items, item source objects, weapon abilities and player identity | Territory production/reward profiles request Native inventory changes. They do not maintain a second inventory. |

An ability is useful far beyond firing a weapon: the installed content includes
`GA_Jump`, `GA_Crouch`, `GA_Cover`, `GA_Dodge`, `GA_Sprint`, `GA_LockOn`,
`GA_Interact_Mount`, `GA_Mount_Vehicle`, `GA_Interact_Sit`,
`GA_Interact_AnimatedInteractable`, melee combos, reload and wield abilities.
These live under `/NarrativePro/Pro/Core/Abilities`.

Use an ability when an action needs activation rules, interruption, montage
timing, resource cost or network execution. Use a quest task to observe that
action, a condition to offer a choice, and an event to request its story result.
For example, a distraction throw is an ability; “distract the guard” is a quest
task; “the guard is investigating” is a condition; opening the next quest branch
is Tales progression. Capture remains a Territory transition.

For dialogue shots, author speaker/default/line/reply-selection shots in the
Dialogue Blueprint. Native anchors and tracks speakers and listeners. Use a full
Native sequence actor for a staged arrival, escorted scene or branching cutscene.
Native's installed playback settings deprecate the old cinematic-bar, hide-all-HUD
and skip booleans in favour of GameplayTag and skip tracks; new content should
follow those tracks rather than copy the deprecated controls.

## Source references and extension points inspected

Paths below are relative to each plugin's `Source` directory.

| Domain | Narrative reference | Territory integration inspected |
|---|---|---|
| Abilities | `NarrativeArsenal/Public/GAS/NarrativeGameplayAbility.h`, `NarrativeCombatAbility.h`, `AbilityConfiguration.h`; `Private/GAS/AbilityTasks/AbilityTask_SpawnProjectile.cpp`; `ANarrativeCharacter::AddAbility` | Distraction ability/projectile; property benefit ability/effect handle reconciliation; `FTerritoryNarrativeProAdapter` |
| Identity while driving | `ANarrativePlayerController::OnPossess`, `SetOwnedCharacter`, `GetOwnedCharacter` | Existing adapter keeps the Native owned character/PlayerState ASC while the controller possesses a vehicle |
| Music | `Music/NarrativeMusicSubsystem.h/.cpp`, `TaggedMusicSet.h` | `UTerritoryMusicSubsystem`, state audio config, Definition application and music tests |
| Tales | `Tales/QuestTask.cpp`, `TalesComponent.cpp`, `Dialogue.cpp`; Native instanced condition/event APIs | Character-action, gameplay-state and combat-progress tasks; Tales utilities; quest starter and checkpoint event |
| Dialogue camera | `Tales/NarrativeDialogueSequence.h`, `UDialogue::PlayDialogueSequence` / `StopDialogueSequence` | `UTerritoryDialogueShot`, cinematic presentation subsystem and validator |
| Full cinematics | `Cinematics/NarrativeLevelSequenceActor.h`, Native binding and playback settings | Ownership boundary and authoring guidance; complete staged story playback remains an acceptance gate |

## Confirmed findings

| ID | Finding | Change / state |
|---|---|---|
| NP-01 | Arrival sounds were inside a “previous Territory state exists” guard. Entering from outside every volume skipped an explicitly enabled arrival cue. | Implemented separate arrival selection. Transition sound requests are selected before playback so the actual observer transitions can be tested without an audio device. Regression passes on both engines. |
| NP-02 | Tales broadcasts `Finished(old, true)` before trying its replacement. If replacement creation fails, no new Began event arrives. Territory retained the old presentation and LOD overrides. | Implemented a one-shot next-tick reconciliation using `UTalesComponent::GetCurrentDialogue`. Successful chains cancel it. Old-speaker overrides are restored on replacement; stale finishes cannot close the current conversation. Native failure-path regression passes on both engines. |
| NP-03 | Character-action, gameplay-state and combat-progress tasks can retain an old live subject, or never bind after starting without a pawn. Provider polling often retries only when the cached actor becomes invalid. | Implemented live context resolution through the existing adapter, immediate possession callbacks, 0.25-second readiness/identity retries, removal of old listeners and rejection of late callbacks after EndTask. Actual action/state progress still comes from Native delegates. Regression passes on both engines. |
| NP-04 | Gameplay State Task uses `GetTagCount` for Exact Tag Match. GAS counts include implied parent tags, so a child can satisfy a supposedly exact parent check. NewOrRemoved listeners also miss an explicit parent change while a child keeps the aggregate count positive. | Implemented explicit owned-tag queries, AnyCountChange listeners and a bounded check for missed removal transitions. A missing provider subject now releases the old ASC. Exact matching, addition/removal, client authority, listener cleanup and Native load-progress replay regressions pass on both engines. |
| NP-05 | Both Farm handover assets have no shot and zero blend-out time. | Current asset validation reports warnings. Authored handover camera/flow acceptance is still open; valid data is not the same as warning-free validation. |
| NP-06 | Blacksmith and Farm owner definitions, in both the project and plugin, use the prototype Narrative Manny appearance. | Four assets report cinematic-appearance warnings. Keep these visible for story authoring; do not replace community sample characters merely to silence validation. |
| NP-07 | Native `UNarrativeTask::IsComplete()` also returns true for optional tasks. Territory's action, combat, GAS, disguise, AI and assault observers used it as a progress guard, preventing optional objectives from earning progress. | Use Native's `CurrentProgress >= RequiredQuantity` pattern for observer completion. Native still decides whether an optional objective blocks its quest branch. The action/GAS/combat regression now runs with optional objectives; a new regression observes real disguise and assault delegates. |
| NP-08 | Presence, disguise, AI and condition-gate tasks still used the initially cached player. AI also kept live old providers and their death listeners. Old inside/perception/token history could satisfy a different subject's objective. | Reuse live Narrative player resolution; re-resolve Native providers at the existing bounded interval; release obsolete listeners. Reset transition evidence when player, target, destination or registered Territory changes. |
| NP-09 | Native BeginTask ticks immediately. Enter/availability polling could therefore ignore Complete If Already Satisfied being disabled. Disguise exit history also survived lost cover and could credit a later restoration outside as an undetected exit. | Observe real state transitions and reset cover-exit evidence when cover is lost. Tests cover an already-inside start, real entry, respawn, registry unload/reload, exposure and restoration outside. |
| NP-10 | Distraction activation trusted an item's inventory pointer and quantity. Native inventory load replaces the item array while old live source objects can retain that pointer. Equipped-source mode also did not check equipment or the throwing avatar. | Check exact membership, Native equipment state and the inventory's current owning pawn before the throw. A stale or foreign source cannot consume an item or create a projectile. |
| NP-11 | GAS commit and deferred-spawn callbacks could cancel the ability, remove its source or destroy the projectile while activation continued. | Recheck the active invocation, avatar, source and pending ability removal after callbacks and before item payment. A transient invocation number prevents an older call from completing a newer activation. A paid projectile still survives the ability ending, including Native last-item revocation. |
| NP-12 | The distraction component marked its impact as reported after sending the Gameplay Event. An event listener could synchronously report the same impact again. | Commit the one-shot flag before hearing/event publication. Reentrant and later impact requests return false. |
| NP-13 | A baseline theme accepted into Native's fade queue was submitted again on later Territory polls. A newer queued quest theme could be replaced before it became active. | Submit the restore once, then observe it. A visible external theme/set or a rejected request ends restoration. Real Native fade-queue regression passes on both engines. |
| NP-14 | Territory could submit a pending state or restore request while a story wanted exclusive music control. Native exposes no public queued-request owner. | Add local `SetAutomaticMusicEnabled` / `IsAutomaticMusicEnabled` Blueprint hooks. Disable before the story requests Native music; re-enable after restoring the intended world music. This does not cancel requests Native has already accepted. |
| NP-15 | GameInstance deinitialization could start another asynchronous baseline music load. | Clear local observation on teardown. Native retains responsibility for audio components, loads and world teardown. |
| NP-16 | Native `SetCurrentDialogue` ends the old dialogue before constructing its replacement. When construction fails, `BeginDialogue` sends neither a new dialogue nor an exit to the remote owner. The client can remain in the old conversation. | A transient server component observes Native's Finished/Began delegates and checks once on the next tick. If no replacement exists, it calls Native `ExitDialogue`, which already sends the reliable client cleanup even when the server session is empty. Successful replacements cancel the check. Solo dialogue only; party routing remains open. |
| NP-17 | Two local presentation subsystems sharing a speaker recorded different original LOD values. The first exit could lower detail during the other dialogue, and the last exit could leave cinematic detail permanently enabled. | Share the original value across existing presentation instances. Only the last holder restores it, and a visible later external LOD change is preserved. No new global registry or saved presentation state. |
| NP-18 | TDA redirects Native pause/player-info widgets into the RPG UI theme, but its asset-reference policy allowed only TerritoryFramework to follow those redirects. Native's standard controller therefore failed validation. | Extend the existing project-only reference exception to NarrativePro's plugin directory and the same theme domain. Standard restrictions remain active. No vendor asset, source or plugin dependency is changed. |
| NP-19 | Native party replacement has the same rejected-construction gap as solo dialogue. In addition to clients retaining the old session, server members retain aliases to the deinitialized party dialogue. | Extend the existing lifecycle component to observe one exact Tales authority. Native join delegates install one observer per party, even with multiple members. The existing virtual party exit clears member aliases and sends Native group exits once. No new RPC or membership authority. |
| NP-20 | Territory presentation subscribed only to personal Tales delegates. Native party Began/Finished/line events are published by the party component, so the Territory HUD/LOD bridge missed them. | Subscribe to the current Native party and follow join/leave notifications. Recover current dialogue on local binding; unbind obsolete parties and reject deinitialized or former-party aliases during deferred reconciliation. |
| NP-21 | Native's party reply-control policy is documented as UI-only. `UNarrativePartyComponent::SelectDialogueOption` accepts a valid option without enforcing leader identity on the server. | `UTerritoryNarrativePartyComponent` validates current Native membership, controller/PlayerState identity and the existing policy before forwarding selection. `ATerritoryNarrativeParty` installs it in Native's existing Tales slot. This is an authored opt-in; plain Native parties remain unchanged. The inherited direct-party RPC is rejected because it lacks an authenticated member identity. |
| NP-22 | Native replies exist before NPC playback ends. An early personal-Tales request can reach `PlayPlayerDialogueNode` with a nonempty NPC chain and hit its assertion. | The party adapter waits for Native Replies Available for manual choices and consumes readiness before node events. Native's authored/global automatic choices retain their separate server callback path. Pending NPC chains, foreign options, stale sessions and repeated choices are rejected. |
| NP-23 | Native component-level party transfer removes a member from the old Tales list without updating `ANarrativeParty`'s separate actor relevance cache. | Still open. Use old actor Remove Party Member before new actor Add Party Member. Atomic transfer, departure-session cleanup, actor destruction and connection timing require a separate membership adapter and live acceptance. |

## Compatible adaptations to retain

- Native Music is `MinimalAPI`; its Blueprint setters are exposed but their C++
  symbols are not exported. The focused, signature-checked `ProcessEvent` adapter
  is intentional. Replacing it with direct calls would break linking. Native's
  `OverrideMusicSet` also returns false after requesting a load, so that return
  value is not a reliable “request rejected” signal.
- Native's projectile ability task deliberately leaves projectiles alive when
  an ability ends. Territory's distraction uses a Native projectile subclass and
  the same deferred actor-spawn pattern. Its server-only, fire-and-forget lifetime
  is compatible; changing the base class or building another projectile system
  is unnecessary.
- Native keeps the owned player character and its ASC when driving. Territory's
  existing adapter already follows this for accounts, UI and property benefits.
  Task subjects must be checked against that same rule.
- Tales condition adapters exist for source-proven party-policy and event
  dispatch gaps. Their behavioural tests matter; removing them merely to make
  code look like the vendor implementation would restore those failures.

## Validation and remaining coverage

Evidence folder: `Saved/Verification/20260914_NarrativePatterns` in TDA.
`AssetInventory.json` records 984 registry entries from the framework/project
roots and selected Native reference folders. Inventory is not a usage proof.
`FinalAssetValidation.json` checked 76 assets: 76 valid, zero invalid, six assets
with warnings (NP-05 and NP-06). The strict wrapper therefore reports
`passed=false`. Validation ran with PIE stopped and zero dirty packages.
The three affected task Blueprints compile UpToDate, with zero errors or
warnings (`BlueprintCompilation.json`). No content changes were saved.

First-batch automation: **319/319 pass on each engine**, zero failures and zero unrun.
UE 5.8 has 23 tests with warnings; UE 5.7 has 25. This does not mean the logs are
warning-free. All six targets pass: Editor, Development and Shipping for both
engines, including runtime/editor modules and UHT. Editor builds use the tooling
exclusions described below (`Builds.json`, `FinalBuilds.json`). UE 5.8 cooking,
staging and a 60-second packaged Development game server-mode smoke pass
(`Package58.json`). This is not a compiled dedicated-server test. Previous
315-test results from 12 September are not reused.

All 741 installed Narrative Pro source files match the UE 5.8 Marketplace
baseline (`NarrativeSourceComparison.json`).

Not yet closed: all authored ability graphs and item source/cancellation paths;
dialogue replacement in multiplayer;
shared split-screen speaker LOD ownership; music override/load races; full Native
cutscene interruption/skip/cleanup; asset dependency/unused-system coverage.
The existing [roadmap](ROADMAP_AND_REMAINING.md) also retains Hashir arrival/save
recovery, finite reserves, guard conversations, AlMalik streaming and release gates.

### Task observation follow-up

`TaskObservation` beneath the evidence folder contains the follow-up builds and
tests for NP-07 through NP-09. All nine shipped task Blueprints passed preflight
validation with PIE stopped. The focused Tales suite passes 29/29 on each engine.
Final follow-up result: **320/320 tests pass on each engine** (UE 5.8: 23 tests
with warnings; UE 5.7: 25). All six Editor/Development/Shipping builds pass.
All nine shipped task Blueprints compile UpToDate with zero errors/warnings and
validate with zero invalid assets or warnings. UE 5.8 cooking, packaging and the
60-second Development game server-mode startup smoke pass. These results
supersede the first batch's 319-test count, with the same tooling exclusions.

The Native reference is `UNarrativeTask::BeginTask`, `IsComplete` and
`SetProgressInternal` in `NarrativeArsenal/Private/Tales/QuestTask.cpp`. Native
explicitly avoids IsComplete when checking whether an optional task has earned
its quantity. Territory uses that same distinction and leaves Native branch
eligibility, journal presentation, authority and progress persistence unchanged.

State presence and disguise exit history are transient observations of one player
and one registered Territory. AI loss/token history is transient evidence for one
AI/player pair. Changing an identity, ending a task or losing the registered
actor resets that evidence; none of those changes invents a gameplay event.
Save/load restores Native progress and reconstructs observations from loaded
actors. No saved or replicated field, public class or asset path was removed.

The new regression uses registered Native ASCs, real disguise Gameplay Effects,
Native condition evaluation, provider/death delegates and the existing assault
record publisher. It also replays Native loading/progress and rejects client-side
progress. Its registry unload/reload checks are a load-order fixture, not a full
World Partition streaming session. Its Native token-array fixture checks task
observation, not combat token allocation or an actual two-client fight.

The assault task remains a read-only observer of `OnAssaultChanged` and durable
records. Scheduling/activation/casualties remain in
`UTerritoryCounterAttackSubsystem`; attacker participation remains in the combat
participant adapter/control subsystem; capture remains the existing volume/control
flow. `ATerritoryWorldState` persists and replicates assault snapshots and the
existing notification path presents them. Optional quest progress changes none of
those transitions, force budgets, proximity policies or deterministic decisions.

### Music request ownership

The reference authoring patterns are Native `BP_MusicTrigger`, which requests
and restores its prior set/theme once, and `BP_MusicTrackInst`, which uses Native
sound override/clear calls for Sequencer sections. Native owns its two fading
tracks, queue, asynchronous set loading, MetaSound and override component.
Territory retains only local observation and the requests it submitted.

NP-13 removes the repeated baseline request. An accepted request may still be
queued, so Territory observes completion without submitting it again. A newer
visible theme or set ends that observation. A rejected request also yields;
Native's false result cannot distinguish an already queued theme from a manual
sound override or missing content.

Native exposes no public pending-theme/set owner or request-generation delegate.
A newer request queued **before** Territory exits can still be hidden behind the
active Territory theme. A set/theme already accepted by Native cannot be cancelled
by merely disabling Territory. Use the explicit local story handoff before a
quest or scene requests music. This limitation is documented rather than masked
by reading private queue fields or replacing the Native player.

The new switch and restore bookkeeping are transient, local cosmetic state.
There are no owner/capture mutations, new RPCs or campaign save fields. Loading
a new world clears pending Territory observation and restores automatic selection
from the local listener and replicated Territory state. Existing Blueprint assets
require no migration; scenes needing exclusive control can opt into the new hooks.
See [setup and example](27_Narrative_Music_and_State_Audio.md).

### Available assets versus authored story usage

`TaskObservation/FeatureAssetReferences.json` checks 30 task, combat, audio and
camera assets. `BlacksmithTaskUsage.json` reads the Native QuestTemplate's actual
branches. Both project and plugin Blacksmith quests contain three task instances:
one Territory Capture and two Territory State tasks. Those instances are not
optional. They reference the shipped Capture/State Blueprint classes.

Seven other shipped task Blueprints currently have no saved-asset referencers:
AI Observation, Character Movement Action, Combat Progress, Gameplay State,
Boss Fight, Counterattack and Disguise. These are reusable authoring choices;
their native classes are also available as instanced tasks. Passing their tests
does not mean they have been wired into Act 1. Choose them when the story has a
matching objective; do not add an artificial objective merely to use a feature.

Three camera sequences with no saved-asset referencers (Close Up, Insert and Over
Shoulder) also appear in the shot editor's `GetStudioShotSpecs` authoring library.
That is a concrete example of why registry referencer counts cannot prove that
a reusable asset or code system does nothing. Grenade attack and the plugin's
Blacksmith music preset also need an intentional authoring-use review; the
project keeps its own editable copies. No unused-asset deletion was performed.

### Source-proven GAS callback limitation

UE 5.7 and 5.8 `UAbilitySystemComponent::UpdateTagMapSingle_Internal` only
executes deferred removal callbacks when `UpdateTagCount_DeferredParentRemoval`
reports a significant aggregate-count change. Removing an explicit parent while
a child keeps its aggregate count positive can therefore skip even an
AnyCountChange callback. The initial regression reproduced this with a real
Native ASC and a separate callback probe.

The focused adaptation keeps GAS delegates and compares the exact predicate on
the existing 0.25-second task tick. It only acts when that predicate changes;
an initially satisfied task that requires a new transition does not complete
merely because a timer ran. No engine or vendor patch is needed.

### Limits of the new tests

The arrival test checks actual observer state transitions and selected sounds;
it does not measure loudness or output-device playback. The dialogue test uses
Native's real rejected-replacement path with transient initialized dialogue
objects. It checks local presentation and LOD cleanup, not remote replication
of a server-side dialogue rejection. Native sends a replacement RPC only after
server creation succeeds; that separate multiplayer failure path still needs
acceptance coverage.

The subject test uses Native characters, ASCs, providers and delegates. Its
transient controller emits the public possession notification after setting
the character/pawn, avoiding the project HUD and stable actor spawn contract.
It is not a two-client possession playthrough. Load coverage replays Native's
task progress restoration and listener setup; a full campaign save/restore and
World Partition playthrough remain required.

### Build environment findings

The first full TDA editor build failed in newly installed Automation Forge
editor modules with unresolved implementation symbols. Verification builds use
command-line exclusions for AutomationForge, AutomationForgePipelines and
AutomationForgeToolset. No project setting or external plugin source was changed.
The full-project failure is retained in `Build58_FullProjectFailure.log`.

Headless startup also failed in SurfaceForgePatina and SurfaceForgeLocal before
tests could start. Test commands exclude the installed editor-only Forge tools;
the exact list is in `IsolatedDisabledEditorPlugins.txt`. Runtime modules remain
enabled. A zero process exit code from the first startup failure did not count as
a passing test run. Reports must contain the expected completed tests.

GUI editor verification additionally excludes PythonBlueprintFixer and OmniScape:
their installed descriptors declare UE 5.7 and show incompatible-plugin startup
prompts on UE 5.8. These exclusions are command-line only. HopDistrictTest was
reopened for the user with PIE stopped; project plugin settings were preserved.

### Migration and ownership

- No reflected field, Blueprint class or asset path was removed. Existing task
  assets retain their objective, provider, tag, attribute and quantity settings.
- Exact Tag Match now means what its tooltip says. Uncheck it when a child tag
  should satisfy a parent requirement.
- Default subjects follow the Native owned player character after a character
  change and through a vehicle trip. Use an explicit Native Actor Provider when
  a task must remain tied to one named NPC instead of the player.
- Native owns quest progress and its save/network updates. Rebinding changes
  transient listeners only. Cinematic and audio observation are local transient
  presentation, rebuilt from Native dialogue and replicated Territory state.
- No counterattack rules, ownership transitions, production balances or authored
  story branches were changed by this batch.

### Distraction ability and item follow-up

The closest authored reference is Native `GA_Attack_ThrowGrenade`: the attack
starts through Native's combo/animation workflow, checks activity before its
animation event, spawns through `UAbilityTask_SpawnProjectile`, and consumes
Native inventory. Territory's small distraction action remains instantaneous;
its supplied Blueprint does not have an authored throw montage to wait for.
Do not advertise this as an animated grenade/weapon combo replacement.

Native `UEquippableItem::HandleEquip` grants abilities with the item as their
source; `HandleUnequip` removes those handles. `ANarrativeCharacter::AddAbility`
and `RemoveAbilities`, Native inventory `ConsumeItem`, and the existing
replicated projectile remain the public extension points. Property benefit
grants already use those Native handle APIs and retain the correct owned
character while driving; this batch does not introduce another grant manager.

NP-10 adds a necessary boundary check because Native `Load_Implementation`
rebuilds the inventory array, while old item objects and ability source references
can remain alive. Checking `OwningInventory` or a matching item class is not
enough. The throw checks exact membership in `GetItems()` and the current
inventory owner before spawning and before consuming. No Native source patch
or separate saved item registry is needed.

The existing **Require Equipped Narrative Item Source** setting now also checks
Native `IsEquipped()`. Keep it enabled for the supplied rock. For a custom
unequipped consumable, disable that setting and keep **Consume Source Item On
Successful Throw** enabled. Turn both off only for an intentional ability that
does not require an inventory item. No Blueprint field or path was renamed.

GAS still owns costs and cooldowns. If a callback cancels after GAS commit,
already committed GAS costs/cooldowns follow the normal GAS contract; the
unspawned throw does not consume its inventory item. The ability never refunds
unrelated effects. After one item has paid for a successful throw, the projectile
owns its lifetime, even when consuming the last unit revokes the ability.

`ActivationSerial` is transient invocation bookkeeping. It is not campaign
state, a replicated field or an alternate ability authority. Impact reporting
similarly uses the component's existing transient one-shot flag. Inventory
quantity/equipment saving and replication remain Native responsibilities.
Streaming the source actor away cancels an unfinished invocation through its
existing lifetime; this change does not add offscreen projectiles or a new
World Partition registration path.

Evidence: `Saved/Verification/20260914_AbilityCancellation` in TDA.

| Check | Result |
|---|---|
| UE 5.8.2 and 5.7.4 Editor, Development and Shipping builds | All six pass; same documented editor-tool exclusions as the earlier batch. |
| Full automation suites | 322/322 per engine; zero failed/not run. UE 5.8: 299 success + 23 warning results. UE 5.7: 297 success + 25 warning results. |
| New cancellation/restore regression | Real Native inventory save/load, stale and foreign source rejection, client authority, commit/spawn cancellation, destroyed spawn, cancel-and-reactivate, one paid throw and reentrant impact. |
| Shipped rock regression | Native equip grants the authored Blueprint ability; last-unit consumption unequips and revokes it; the successful projectile survives. |
| Blueprint compilation | All six plugin/project distraction ability, rock and projectile Blueprints UpToDate; zero errors/warnings. |
| Focused validation with PIE stopped | Eight plugin/project/Native reference assets valid; zero invalid/warnings. Earlier Farm/owner authoring warnings remain open. |
| HopDistrictTest listen server + two clients | 41 observed samples: each world sees one projectile, server and owning client consume the single rock, and the projectile expires everywhere. A later activation returns false. |
| UE 5.8 cook/stage/package and 60-second packaged smoke | Both exit 0. Smoke uses a Development game in server mode, not a compiled dedicated-server target. |
| Narrative source comparison | All 741 files match the installed UE 5.8 Marketplace source. |

The network recorder is `Scripts/Territory/verify_distraction_network_pie.py`.
It observes a server-initiated Native activation. It does not prove remote input
transport, hardware throw timing, an animated throw montage, or full campaign
and World Partition recovery. The native save/load regression exercises actual
inventory reconstruction, not a complete campaign save. Those wider acceptance
gates remain on the roadmap. Initial synthetic-fixture failures are retained
under `InitialFixtureFailure`; the final reports above use corrected fixtures
that permit Native callbacks and explicitly configure native ability instances.

HopDistrictTest is reopened with PIE stopped, the previous player-count settings
restored, and zero dirty editor packages. The user's controller Blueprint disk
edit is excluded from this source batch.

### Music fade queue and local story handoff follow-up

Evidence: `Saved/Verification/20260914_MusicOwnership` in TDA.

| Check | Result |
|---|---|
| UE 5.8.2 and 5.7.4 Editor, Development and Shipping | All six builds pass with the documented editor-tool exclusions. |
| Full automation | 323/323 per engine, zero failed/not run. UE 5.8: 300 success + 23 warning results. UE 5.7: 298 success + 25 warning results. |
| New regression | Native's real SetTheme queue and fade timers preserve the newer quest request through repeated Territory polls. A visible external set and rejected sound-override restore yield. Story disable, re-enable, world reset and cosmetic Blueprint flags pass. |
| Audio-enabled HopDistrictTest | 54 samples, all 11 checks pass: Territory entry, exit during fade, newer story selection, handoff, continued place observation, Native sound override, clear and automatic reacquisition. |
| Listen host and two clients | Only the selected remote client disables automatic music and selects Music.Combat. Host and other client remain automatic with Music.Ambient. |
| Blueprint compilation | Native BP_MusicTrigger and BP_MusicTrackInst are UpToDate, zero errors/warnings. |
| Final focused validation with PIE stopped | Eight plugin/project/Native assets valid, zero invalid/warnings; zero dirty packages after verification. |
| UE 5.8 cook/stage/package and 60-second smoke | Both exit 0. Startup smoke runs a Development game in server mode, not a compiled dedicated-server target. The previously tracked optional intro-cutscene warnings remain. |
| Narrative source comparison | All 741 source files match the installed UE 5.8 Marketplace package. |

The headless test supplies a non-playing audio component so it can exercise
Native's actual queue and timers without an output device. It only seeds Native
fixture fields inside the test; production never reads or writes private queue
state. The separate PIE recorder uses public music calls and verifies the real
`MS_MusicMaster` component is playing. MetaSound logs show Unity in the Ashes and
Controlled Advance starting. It does not certify perceived loudness or mix quality.

The current project Blacksmith is locked and its authored music overrides are
disabled. The first live attempt therefore correctly selected no Territory rule;
that report is retained. The final test copies editable settings to a transient
Definition with fading audio enabled for all states. Derived hierarchy fields
use fixture defaults, and the original Definition is restored afterward. This is
an isolated audio test, not a quest/guard acceptance run. No saved asset changes
were made. A second retained fixture report used Python struct identity instead
of tag values; the final recorder compares exact tag names.

The reusable recorder is `Scripts/Territory/verify_music_handoff_pie.py` in TDA.
The original pending-request limitation above remains: Native does not expose a
newer request that is still hidden in its queue. Cold asynchronous loading,
seamless travel, World Partition restoration, split-screen ownership and remote
Sequencer playback remain acceptance gates. Source review also found Native's
global world-init/destroy music callbacks do not filter the owning GameInstance;
this is a travel/late-world lifecycle concern requiring reproduction, not a
verified Territory fix. No vendor callback or private audio state was patched.

The first cook failed because an editor tool could not bind port 8000 while the
GUI editor was open. `CookInitialEditorPortConflict.log` is retained. Closing the
editor and rerunning the same cook/package command passes; no project setting was
changed to hide the error. HopDistrictTest was then reopened with PIE stopped,
the original three-player PIE setting restored, and zero dirty packages.

### Remote solo dialogue and shared speaker detail follow-up

Evidence: `Saved/Verification/20260914_DialogueLifecycle` in TDA.

The Native reference is `UTalesComponent::SetCurrentDialogue`, `BeginDialogue`,
`ExitDialogue` and `ClientExitDialogue_Implementation`, plus
`UDialogue::Deinitialize`. Native's `UAsyncAction_BeginDialogueAndWait` shows
the delegate-before-start pattern. Territory binds both dialogue delegates
and removes both on unregister/end play. `UTerritoryPlayerManagementSubsystem`
uses its existing post-login and initial-controller lifecycle to install
`UTerritoryDialogueLifecycleComponent` on authoritative Narrative controllers.
Designers keep using their existing Dialogue Blueprints and Tales calls; there
is no controller reparenting or manual component setup.

Native publishes Finished before trying a replacement. The observer waits one
tick, cancels on a successful Began, and asks Native to exit only if the current
solo session is empty. A stale finish, lower-priority rejection, lost authority
or removed controller cannot close a newer session. The component adds no RPC,
replicated field, saved campaign record or alternate dialogue authority. Native
continues to route its reliable client exit and release dialogue audio/cameras.
Party dialogues have separate Native ownership/routing and are outside this fix.

Speaker LOD ownership stays in the existing local-player presentation subsystem.
When another local presentation already holds the same component, the new holder
copies its original baseline. Only the last holder restores it. A later visible
external setting is preserved. Exact component identity separates independent
PIE worlds. Lookup runs at acquisition/release, not every tick. This does not
detect an external writer choosing the same forced value, or provide a general
priority system for all cinematic LOD writers.

No Blueprint field/path or save format changed. Controller replacement rebuilds
the observer, and local presentation reconstructs from Native's current dialogue.
Weak component records tolerate a removed visual. These lifecycle regressions
do not replace full campaign or World Partition streaming acceptance.

| Check | Result |
|---|---|
| UE 5.8.2 and 5.7.4 Editor, Development and Shipping | All six builds pass with the previously documented editor-tool exclusions. |
| Full automation | 325/325 per engine, zero failed/not run. UE 5.8: 300 success + 25 warning results; UE 5.7: 298 success + 27 warning results. |
| New server lifecycle regression | Real Native rejected replacement dispatches one exit; successful replacement, stale finish, lost authority, unregister/re-register and delegate cleanup pass. |
| New shared-speaker regression | Both release orders, repeat registration, local-player removal, external LOD changes and removed visuals pass for LODSync, groom and skeletal mesh components. |
| HopDistrictTest listen host + two clients | All 13 recorded assertions pass: authoritative installation, client cleanup/isolation, successful chaining, priority rejection, invalid authored start, normal exit and Native owner release. |
| Focused Blueprint compilation | Project Hashir greeting, plugin Blacksmith handover and Native player controller are UpToDate, zero errors/warnings. |
| Focused asset validation, PIE stopped | Four assets valid, zero invalid/warnings. Original three-player PIE setting retained; zero dirty editor packages. |
| UE 5.8 cook/stage/package and 60-second smoke | Both exit 0. This is a Development game running in server mode, not a compiled dedicated-server target. The previously tracked optional intro-cutscene warnings remain. |
| Narrative source comparison | All 741 files match the installed UE 5.8 Marketplace source. |

The shared-speaker fixture uses transient render data with three LOD records;
it does not load a rendered character or create a split-screen viewport. Its
missing-skeleton/world-context warnings and Native's OnEndDialogue warning are
visible in the successful test reports. The initial empty-mesh fixture failed
because Unreal clamps forced LOD to its available render-data count. The corrected
fixture and its rendering dependencies live only in the editor test module.
Initial failures and missing test-library link attempts are retained under
`InitialFixtureFailure`; runtime rendering dependencies were not expanded.

The live recorder is `Scripts/Territory/verify_dialogue_lifecycle_pie.py`.
Hashir's current greeting has one auto-selected reply, no authored shot and no
voice track. Actions are recorded before it naturally finishes. This verifies
actual Native client messages and Territory presentation flags, not camera
composition or voice playback. Initial tool calls used the C++ parameter name
`DialogueClass`; reflection calls require the header's `Dialogue` name. Those
no-op observations are retained. A tool-deduplicated replacement and a request
to call the unreflected `ExitDialogue` are not counted as passed actions; the
successful chain and normal exit use separate recorded stages and Native's
callable `TryExitDialogue` wrapper.

The first asset preflight failed on Native's player-controller Blueprint because
TDA redirects its pause/player-info widgets into `NP_RPGUITheme` but did not allow
Native to follow that project-specific reference. TDA's `DefaultGame.ini` now
extends the existing narrow plugin-path rule to `/NarrativePro/`, with only the
same project/theme domains. It does not disable reference restrictions, add a
circular plugin dependency or change vendor content. All four assets pass after
restart. The initial failed report remains available as `PreflightAssets.json`.

Party dialogue replacement, rendered split-screen/shared-camera behavior, full
sequence skipping/interruption and complete story save/streaming recovery remain
on the roadmap. The plugin is not ready for a release certificate based on these
focused checks alone.

HopDistrictTest is reopened after the package check, with PIE stopped and the
three-player setting retained. The scoped commit excludes the user's existing
controller Blueprint, map, road and other project edits.

### Native party dialogue follow-up

Evidence: `Saved/Verification/20260914_PartyDialogue` in TDA.

The pre-fix listen-host/two-client run reproduces both failures. After a rejected
replacement, the server party has no dialogue, both clients retain one, and the
server members still return the old dialogue through their personal aliases.
During a successful party start, Territory's local presentation flag stays off
on all three machines. `PreFixPartyReproduction.json` records that state.

The reference is `UNarrativePartyComponent::BeginDialogue`, `ExitDialogue`,
`AddPartyMember`, `RemovePartyMember` and `UTalesComponent::OnRep_PartyComponent`,
`BeginPartyDialogue`, `ClientBeginPartyDialogue_Implementation`,
`ClientExitPartyDialogue_Implementation` and `GetCurrentDialogue`.
`ANarrativeParty` owns the replicated membership and member-only actor relevance.
Its component owns shared Tales quests/dialogue and group message routing.

The existing Territory observer is now attached to one exact Tales component.
Each Native controller's join delegate requests the party observer; repeated
members reuse it, and two party components on one actor remain independent.
Failed replacement calls Native's virtual group exit once. Native clears server
member aliases and sends its existing reliable group messages. The observer
does not synthesize Finished events, clear Native private fields or replace the
party component. Its weak source reference and timer are transient. Unregister,
end play and lost authority stop deferred work. Native source/content is unchanged.

Local presentation now follows personal and current-party delegates, including
speaker line events. Joining an already-running local dialogue recovers from
Native state. Leaving or changing party unbinds old callbacks and releases only
that local presentation's LOD requests. A stale Native member alias cannot
reactivate a former party's presentation. An older leave broadcast cannot undo
current membership if an earlier callback already rejoined that same party.

No saved field, replicated property, new RPC, asset path or Blueprint signature
changed. Existing Narrative controllers and party assets keep their authoring
workflow. No manual adapter installation is needed for parties joined through
those controllers. The new `FindOrCreateForTales` C++ helper supports an explicit
integration with other Native Tales owners; it does not create party membership.
Controller/party lifetime and weak actor references remain the integration
boundary. Full campaign saves and actual World Partition party restoration are
separate acceptance gates.

The two new regressions exercise Native membership events, actual failed
replacement and group exits, per-member cleanup, duplicate installation, two
parties on one owner, stale events, authority loss, unregister/re-register,
late local binding, shared LOD release and membership changes. The existing solo
regressions remain part of the full suite. A final review added the rejoin guard
after the first full pass; that earlier evidence is under `BeforeRejoinGuard`.

Remaining Native party constraints are not hidden by these fixes:

- **Reply authority:** leader-only selection is UI-only in Native. A supported
  component override must enforce it on the server before multiplayer story
  rewards and shared decisions can be certified (NP-21).
- **Departure while speaking:** Native removes membership without explicitly
  clearing the departing member's dialogue alias. Territory now releases its
  own presentation; Native input/sequence cleanup and continuation policy still
  need a separate adapter and live acceptance.
- **Connection timing and ownership:** starting immediately after a join,
  joining an already-running remote conversation, remote-only parties on a
  listen server, party destruction and seamless travel still need acceptance.
  Native selects a local member as the listen-server dialogue controller and
  explicitly uses the leader only in dedicated-server mode.
- **Cinematics:** Hashir's greeting has no authored camera shot or voice track.
  Session/presentation verification does not certify camera composition, sound,
  full sequence interruption/skip or rendered split-screen views.

| Check | Result |
|---|---|
| UE 5.8.2 and 5.7.4 Editor, Development and Shipping | All six builds pass, with the previously documented tooling exclusions. |
| Full automation after the rejoin guard | 327/327 per engine; zero failed/not run. UE 5.8: 300 success + 27 warning results. UE 5.7: 298 success + 29 warning results. |
| New Native party regressions | One observer per exact party, one group exit, per-member cleanup, unrelated party isolation, membership changes, authority loss, re-registration, stale events and local presentation cleanup pass. |
| HopDistrictTest listen host + two clients | All 13 recorded checks pass: party replication, one server observer/no client observers, three local presentations, rejected replacement cleanup, successful replacement, priority rejection, invalid authored start and normal group exit. |
| Focused Blueprint compilation | Project Hashir greeting, plugin Blacksmith handover and Native player controller are UpToDate with zero errors/warnings. |
| Focused asset validation, PIE stopped | All three assets valid, zero invalid/warnings. |
| UE 5.8 cook/stage/package and 60-second smoke | Both exit 0. Development game in server mode, not a compiled dedicated-server target. The existing optional intro-cutscene null-player warning remains tracked. |
| Narrative source comparison | All 741 files match the installed UE 5.8 Marketplace source. |

The live fixture uses a transient `ANarrativeParty`, with its public actor-level
`AddPartyMember` so both relevance and Tales membership are populated. Gameplay
calls use native `pie_call_function`, never Python RPC dispatch. One repeated
lower-priority call was suppressed by the tool's duplicate-call guard. Those
snapshots are explicitly excluded; a fresh priority-6 conversation and a real
priority-11 rejection prove that each local instance stays unchanged. Hashir's
short greeting is sampled immediately before it naturally finishes.

The new recorder is `Scripts/Territory/verify_party_dialogue_pie.py` and writes
only this batch's evidence path. An initial reuse of the solo recorder overwrote
its earlier raw report. The prior 13-check result and six printed snapshots were
recovered from `20260914_DialogueLifecycle/LiveEditorVerification.log`; the
unprinted older snapshots were not reconstructed. The recovered file states
this limit. Original build, automation, asset and package evidence is intact.

After packaging, HopDistrictTest is reopened with PIE stopped, three players and
one process retained, and zero dirty packages. The three focused assets validate
again without warnings. Only Territory source/tests/docs and the project recorder
are committed; existing project and controller-Blueprint edits remain untouched.

### Server reply policy and readiness follow-up

Evidence: `Saved/Verification/20260914_PartyAuthority` in TDA. Setup:
[Party dialogue reply rules](PARTY_DIALOGUE_REPLY_RULES.md).

The source references are `UNarrativePartyComponent::SelectDialogueOption`,
`UTalesComponent::ServerSelectDialogueOption_Implementation` and
`TrySelectDialogueOption`, `UDialogue::NPCFinishedTalking`,
`PlayPlayerDialogueNode`, and `ANarrativeParty`'s existing `PartyTalesComponent`.
The new `ATerritoryNarrativeParty` uses the same `SetDefaultSubobjectClass` pattern
as Native's NPC controller and level-sequence actor. Its actor membership wrappers
also reject client-side mutations before calling Native.

`UTerritoryNarrativePartyComponent` owns only the validation adapter and transient
reply readiness derived from Native events. Native remains the single authority
for membership, leader, policy, current dialogue, node events and group messages.
Manual requests arrive through each player's existing personal Tales RPC; the
server derives the selector from that component's controller. The party validates
current membership, PlayerState/controller identity, world, policy, initialized
session, exact offered option and finished NPC chain before forwarding. Readiness
is consumed before Native node callbacks. Unregister/end play release delegates.

Native's automatic replies run before Replies Available. The party's virtual
Try Select hook retains that path and uses the actual Native leader as selector.
The inherited direct-party server RPC is intentionally rejected: it lacks member
identity, including when a custom actor supplies a network owner. This override
is a rejection boundary, not an unfinished implementation. No new RPC, replicated
field, saved field or competing party policy was added.

The authored opt-in is deliberate. Existing plain Native parties and content
are not replaced at runtime. Registry inheritance lookup found no project party
Blueprints; the only derived class is the new Territory native actor. Designers
can create a party Blueprint from that actor or use its component on a custom
replicated party. Native's inherited property tooltip still describes its own
UI-only enforcement; the Territory class and guide explain the added server check.

| Check | Result |
|---|---|
| UE 5.8.2 and 5.7.4 Editor, Development and Shipping | All six builds pass with the existing tooling exclusions. |
| Full automation | 329/329 on each engine; zero failed/not run. UE 5.8: 301 success + 28 warning results. UE 5.7: 299 success + 30 warning results. |
| Native authority/readiness regressions | Actual Native selection and dispatch, early request, nonleader/outsider/null selector, departed alias, unknown policy, foreign option, direct-party RPC, pending NPC chain, repeat request, unregistration and fresh session pass. |
| Native automatic selection | Single response, authored auto flag and global setting preserve Native playback and leader identity. |
| Listen host + two clients | Eight checks pass. Plain Native baseline accepts a nonleader's client request; the Territory party rejects it, permits the leader/all-member policy and preserves cleanup. |
| Dedicated PIE server + two clients | Eight checks pass with `IsDedicatedServer` confirmed for the server world, plus two checks for Hashir's actual automatic Salam reply and completion on all members. This is dedicated PIE, not a compiled server executable. |
| Focused assets / Blueprints | Hashir greeting, Blacksmith handover and Native player controller validate without warnings and compile UpToDate with zero errors/warnings, PIE stopped. |
| UE 5.8 cook/stage and 60-second packaged smoke | Both exit 0. Development game in server mode, not a compiled TDAServer executable. Existing Native content/tag and optional intro-cutscene null-player warnings remain; this is not warning-free campaign acceptance. |
| Native source comparison | All 741 source files match the installed Marketplace package. |

The editor-only held dialogue fixture has two NPC lines and two explicit replies;
it executes Native playback and reply routing, without a camera or voice asset.
The native test driver invokes personal Tales from the correct PIE client object,
outside the Python script stack. The recorder reads each world's current Native
node and local presentation. Asynchronous client calls are given time to arrive;
an immediate snapshot is not treated as an authoritative result.

Initial evidence is retained: a missing test-only PlayerState include, then an
assertion that incorrectly assumed Native's available-reply ordering matched its
stored node order. The corrected test captures the offered auto reply before
selection. The recorder also corrected unsupported Python accessors; those failed
reads are not passed checks. Warnings from headless/no-avatar fixtures and Native
begin/end logging remain visible in successful automation reports.

This live run additionally reproduced the existing join/start race: adding the
members and starting immediately left both clients without a dialogue, although
the server returned success. Restarting after membership was present on all worlds
worked. This batch verifies reply authority after membership settles; it does not
mask or certify that race. Departure alias/camera/input cleanup, actor relevance
on atomic transfer, party destruction, late remote joins, remote-only listen-server
controller selection and full campaign/World Partition restoration remain gates.
No claim of rendered cinematic or full multiplayer story acceptance is made.

After packaging, HopDistrictTest is reopened with no PIE worlds or dirty packages.
The saved editor-user settings and live CDO both use listen-server mode, three
players and one process. The live CDO restoration alone did not persist on editor
exit, so the same saved user-settings section was restored before reopening;
project defaults were not changed. `EditorReopenedState.json` and the live CDO
query record the final state. The recorder's Python syntax check also passes.

### Remote-only server context and Character Light Rig research

Evidence: `Saved/Verification/20260914_PartyDepartureLightRig` in TDA.

`UNarrativePartyComponent::GetOwningController` selects a local member on a
listen server, but returns the Native leader on a dedicated server. With only
remote members on a listen server, its local search returns null. Native
`UDialogue::Initialize` caches the resulting controller and pawn, so conditions,
events and initial player-avatar resolution can receive empty context.

The Territory override first preserves Native's result, then uses its actual
leader only on authority when the original result is empty. The leader must
still belong to that party, have authority and have a valid controller in the
same world. Clients cannot use this fallback to adopt a remote viewer. No global
first-player lookup, new membership authority, save field, replicated property
or Blueprint signature was added. Existing Territory party authors need no
migration; plain Native party classes still require the documented opt-in.

The native behavioral test models remote controllers and proves the original
empty result, actual dialogue initialization, client rejection, empty-party
failure, loss of controller authority and fresh lookup after Native changes the
leader. It does not claim to migrate an already-active dialogue's cached context.
All six UE 5.8.2/5.7.4 builds pass. Full automation: 330 per engine, zero failed
or not run (301 success + 29 warning results on 5.8; 299 + 31 on 5.7).

The live listen-server test has a host outside the party and two remote members.
All seven checks pass: server leader selection, server dialogue context, each
client's local viewer, shared NPC playback, host exclusion, a real remote leader
reply RPC and normal group exit. Membership is allowed to settle before Begin;
the known immediate join/start race remains open. Two direct calls to unreflected
Native C++ methods and one unsupported Python PlayerId accessor were rejected by
the tools; the successful run uses the existing native test driver, reflected
TryExitDialogue and the actual PlayerId property. Failed attempts remain in the
log and are not counted as executed gameplay.

Character Light Rig research covers 95 valid asset records, eight clean core
Blueprint compiles, nine clean validation results, runtime graph/dependency
inspection and actual Native player/Hashir visual components. The pack is not
automatically connected by inheritance. The runtime actor, editor-only wrapper,
cached camera, skeleton-name mismatch, delayed visual readiness, element rebuild
cost and unbound exposure control are documented in
[Character Light Rig compatibility](CHARACTER_LIGHT_RIG_COMPATIBILITY.md).

User decision: **continue the shared dialogue for remaining members on leave**.
Member-local session/input/camera and owned-tag cleanup, safe active leader
departure, atomic transfer and immediate join/start delivery remain required.
Deinitializing the shared server dialogue to clean up one member is not compatible
with that decision. Automatic rig playback and rendered lighting acceptance also
remain open; the inspection is not presented as a finished lighting integration.

UE 5.8 cook/stage and the 60-second Development game server-mode smoke both
exit 0. This is not a compiled TDAServer target. The cooker explicitly includes
the runtime light-rig asset; the staged IoStore inventory contains its runtime
actor and element dependencies, and excludes the pack's editor wrapper, control
panel and unbound post-process actor. That is dependency/cook evidence, not
proof of rig playback. Existing project/Native content warnings, including the
optional intro-cutscene null-player warning, remain tracked. All 741 Narrative
source files match the installed Marketplace package. HopDistrictTest is reopened
with PIE stopped, listen-server mode, three players, one process and no dirty
packages. No user content or Native source was changed for this batch.

### Departing member's personal dialogue reference

Evidence: `Saved/Verification/20260914_PartyMemberLeave` in TDA.

Native's `BeginPartyDialogue` aliases each server member's `CurrentDialogue` to
the party's one `UDialogue`. `UNarrativePartyComponent::RemovePartyMember` clears
membership and publishes `OnLeaveParty`, but does not clear that alias.
`UTalesComponent::TryExitDialogue` can still reach the old `OwningComp`, and a
personal `BeginDialogue`/`ExitDialogue` can deinitialize the shared object. Reply
policy validation alone did not prevent these paths.

The Territory party overrides Native's public `RemovePartyMember` hook. It checks
server authority, exact current membership and the controller/world before
clearing only a personal alias owned by this party. This happens before the
Native leave broadcast so a synchronous story callback can safely start a
personal conversation. Native still performs the membership mutation. Failed
removal preserves a still-current alias; an unrelated/new dialogue is untouched.
No group exit or shared deinitialization is sent for this cleanup.

The existing local presentation subsystem drops the exact old personal alias
on Native's replicated leave/switch notification, only for an opted-in Territory
party. Current membership wins over an older leave, and a new personal or other
party dialogue cannot be cleared by the old notification. Authority and shared
local copies remain alive. If no other local Narrative controller is still a
member, the exact old client copy closes through Native's local
`ClientExitDialogue`. This sends no server/group exit. Native runs its existing
finish/deinitialization path immediately instead of waiting for the now-irrelevant
party actor's `EndPlay`. The initial reference-only test reports are retained in
`InitialReferenceOnly`; final validation reruns after this cleanup improvement.

Native remains the membership, dialogue and message authority. No RPC, saved
field, replicated field, persistent identity, or Blueprint signature was added.
The personal alias is transient; this change adds no conversation save/resume
format. Existing Territory party authors need no migration. Plain Native parties
still require the documented opt-in. Actor relevance/streaming and full campaign
save recovery are not certified by the transient-reference regression.

Both new native behavioral tests pass on UE 5.8.2 and 5.7.4. They cover rejected
authority/null/outsider removals, repeat removal, departed Native skip/exit and
queued exit requests, personal begin/finish, synchronous leave callbacks that
start a personal conversation, remaining-member playback, exact local cleanup,
stale/rejoined membership, shared local-viewer retention, immediate last-viewer
client deinitialization and new-dialogue preservation. A fixture also removes
the original leader and proves reference/playback isolation; it does not claim
to migrate that dialogue's cached leader context.

All six Editor/Development/Shipping builds pass. Full automation is 332/332 per
engine with zero failed or not run (5.8: 301 success + 31 warning results;
5.7: 299 + 33). Native headless fixture begin/end and missing-avatar warnings
remain visible. Three focused assets validate and compile without warnings.

The listen host plus two clients and a dedicated PIE server plus two clients
each pass ten live checks. A nonleader leaves, loses its personal reference on
both sides, cannot use that reference to skip/exit the old conversation, starts
and finishes a separate personal dialogue, while the remaining remote member
advances the original shared dialogue through Native's client RPC. Native's final
group exit still clears the remaining aliases. The direct exit attempt is made
on the authoritative personal component; the skip attempt/remaining-player skip
use the actual client Tales object through the native test driver. The C++
regression also calls Native's queued server exit implementation.
Both final live runs additionally retain the departed client's old dialogue
object and confirm that Native has cleared its owning component shortly after
leave, before a new personal conversation is started. This verifies immediate
local deinitialization rather than relying on eventual actor removal.

The read-only recorder is `Scripts/Territory/verify_party_departure_pie.py`.
Gameplay dispatch runs outside Python, and snapshots wait for network delivery.
Initial tool attempts supplied an actor path where Monolith requires its object
name; those were rejected before gameplay and are not counted as executed checks.
The first shared-client cleanup fixture incorrectly treated a bare test controller
as local; the corrected fixture explicitly models locality, like the existing
remote-controller fixture. Those failed reports are retained in
`ClientCleanupFixtureFailure`. An overlapping cross-engine UBT run also hit the
shared Marketplace build-rules file lock; builds were then serialized and rerun.

This is a bounded reference-isolation fix. Per-player Native camera/input,
voice/shot and owned-tag cleanup, cached active leader/pawn/speaker migration,
last-member removal, disconnect, atomic transfer, immediate join/start delivery,
late join, World Partition travel and rendered split-screen remain open. The
held-line fixture has no camera or voice asset. Starting a new personal dialogue
is safe for the remaining party object; complete concurrent cinematic/tag behavior
still needs the following lifecycle batch. Character Light Rig automatic wiring
and rendered acceptance also remain open.

The final UE 5.8 cook/stage and 60-second packaged Development game server-mode
smoke both exit 0. This is not a compiled TDAServer target or a full campaign
playthrough. The existing content/authoring warning backlog remains separate.
The packaged NullRHI smoke retains the known render-target Canvas warnings and
Native controller's optional `CutscenePlayerActor` warning; exit 0 is not a
claim of warning-free cinematic playback.
All 741 Narrative Pro source files still match the installed Marketplace package.

## Follow-up: validated Native party transfers

Evidence: `Saved/Verification/20260914_PartyTransfer` in the TDA project.

The closest Native feature is `UNarrativePartyComponent::AddPartyMember` and
`RemovePartyMember`, together with `ANarrativeParty`'s actor wrapper and
`IsNetRelevantFor`. Native Add ignores the previous party's removal result.
It can therefore return success while the old party still has the member.
Component transfers also bypass the stock actor's member/relevance lists.
The actor Add wrapper fills those lists even if the component rejects the join.

Territory's existing component now validates the authoritative controller,
PlayerState, world and canonical Narrative personal Tales slot. It asks the old
party to remove the player and verifies the result before calling Native Add.
Native remains the sole membership authority and owns its normal callbacks and
replication. The actor fields remain derived read models of the component;
Territory updates them before its Native membership callbacks and reconciles
them afterward. A stock Native source's actor lists are reconciled after its
removal returns; that source still owns its callback order and dialogue cleanup.
The Territory actor wrapper delegates to the component instead of independently
writing the same read models. No polling, additional RPC, replicated field or
saved member database was added.

Native callbacks may intentionally redirect a player during Leave, or remove
them during Joined. The adapter checks the final membership and reports false
for the superseded join; it does not undo the story's new decision. Transfers
retain Native's separate Leave and Joined events. This is membership validation
and cache consistency, not an atomic migration of active dialogue presentation.

Migration: existing Blueprint node signatures are unchanged. Use the Narrative
controller's actual Get Tales Component result. A missing/mismatched PlayerState,
client, foreign world, extra personal component or duplicate identity now fails
without adding a member. Retry an early join after PlayerState is ready. A Native
party actor requires Narrative PlayerStates. A custom non-party actor using just
the component may use another PlayerState type and still owns its relevance.
Campaign task data remains Native's serialized data. Live connections are not
saved; reattach players explicitly after load.

Two new native behavioral tests cover refused source removal, actor and component
entry points, duplicate/canonical identity, missing PlayerState and retry,
authority, foreign world, callback read-model consistency, callback redirects,
immediate departure during join, stock Native source compatibility, unregister /
register, Native task-history serialization into a fresh party and explicit
post-load reconnection. Existing party fixtures now use real Native PlayerStates
and install their test Tales object in the real personal slot. The new tests
pass without warnings; full suites pass 334/334 per engine (5.8: 303 success + 31
warning results; 5.7: 301 + 33), with zero failed or not run. All six
Editor/Development/Shipping builds pass.

The listen host plus two clients and dedicated PIE server plus two clients each
pass ten live checks. A nonleader transfers during a held shared conversation;
the source members retain that exact session and line, both sides receive the
new party reference, the destination starts its separate conversation, and each
remote client's Native skip RPC advances only its own conversation. Native final
group exits clear both. The recorder is
`Scripts/Territory/verify_party_transfer_pie.py`; gameplay dispatch uses the
editor-only native test driver. Membership replication is allowed to settle
before Begin Dialogue. This does not claim that the immediate join/start race,
late connection, host/leader transfer or authored camera/voice playback is fixed.
Three focused assets validate and compile without warnings. All 741 Native Pro
source files still match Marketplace.

The speaker-tag audit also confirms the next defect: Native OnBegin adds a
player-speaker contribution through the initial avatar and through each party
member's ASC. OnEnd removes the member contribution from the then-current member
list, so a departed member can retain a contribution. The initial avatar may
have two contributions, and the active dialogue retains protected speaker-avatar
bindings plus cached OwningController/OwningPawn. Removing every matching tag or
blindly replacing those public pointers would interfere with other tag owners
and Native's later cleanup. This batch deliberately makes no tag/context change.
The next adapter must preserve exact contributions, authored callback data,
remaining viewers and Native's end lifecycle before claiming leader transfer.

The final UE 5.8 iterative cook/stage and 60-second packaged Development game
server-mode smoke both exit 0. It is not a compiled TDAServer build. Known
NullRHI Canvas and optional Native CutscenePlayerActor warnings remain; the
smoke result does not certify rendered cinematic quality or a complete campaign.
