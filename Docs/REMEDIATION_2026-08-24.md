# TerritoryFramework Remediation Report - 2026-08-24

This is the current status report for the August 2026 deep re-audit. The older dated
audit documents remain historical evidence and must not be read as the current API or
release status.

## Resource production and live-editor integration

- Added arbitrary Narrative-item production profiles with ordered input/output rules,
  upgrade scaling, claimed/contested gates, bounded deterministic daily catch-up, and
  atomic rollback-safe inventory settlement.
- Added WorldState save and late-join projections for checkpoints, sites, rule outcomes,
  and resource snapshots without saving live actor or inventory pointers.
- Added modular resource/site rows, economy stockpile/site panels, district and territory
  summaries, and selectable production filters.
- Added a Farm example using Grain to produce Meat while retaining independent currency
  income. Each placed Farm retains instance-authored Territory identity.
- Added bounded resource-account registration retries for PlayerControllers whose pawn is
  not possessed during BeginPlay.
- New Territory actors no longer inherit a Blueprint CDO GUID. Editor placement and normal
  duplication author fresh identities; load and PIE preserve saved identities.

## Physical capture flow and UI integration

- Added `ATerritoryCapturePoint`, a server-only overlap adapter that resolves the player pawn's
  Narrative faction and registers it with the existing capture subsystem. It adds no owner,
  progress, save, or replication authority and rejects aggregate-only targets.
- `/Game/HopDistrictTest` now has physical capture points for the Blacksmith and Farm Places.
  Blacksmith capture reduces Market Square, Farm capture reduces Castle Hill, and ownership of
  both Districts reduces Haven Reach City through the hierarchy authority.
- Removed the Blacksmith Blueprint's guard-death `ForceCaptureTerritory` shortcut. Defeating a
  guard exposes the Place but never transfers ownership without a physical participant.
- Rebuilt the Territory journal as a responsive stacked command center, added project capture
  and GameplayHUD widgets, and composed modular resource and production-site row widgets.
- Reused Narrative's menu, GameplayHUD, and text-button templates through project-owned
  `WBP_TerritoryMenuBase`, `WBP_TerritoryGameplayHUD_Modular`, and `WBP_TerritoryButton_Text` assets.
  Runtime-generated controls load the same configurable Territory button class and style.
- Removed `ITerritoryOwnershipInterface` from the project player. The interface describes a
  Territory object's owner/progress/contester; player allegiance remains Narrative team state.

## Resolved Findings

- Counterattack NPCs now yield their durable Territory movement goal to Narrative Pro's
  attack goal, so an assigned guard can be attacked and movement resumes after combat.
- Only configured physical assault participants consume strategic Territory attacker
  slots. Ordinary defenders continue to use Narrative tactical attack tokens.
- Streamed-out targets, invalid controllers, participant death, and withdrawal release
  strategic slots and capture participation exactly once.
- Defender Narrative ASC death binding is retried for bounded asynchronous definition
  initialization and is unbound from the exact ASC on removal or EndPlay.
- Physical counterattacks require the capture gate plus War or Narrative Hostile state.
  Neutral or protected treaty states cannot create non-hostile assault NPCs.
- Proximity activation is one-time and casualty accounting consumes a finite force budget.
  Scheduled warnings alone do not create capture pressure.
- `NoCurrencyPayout` performs no income, upkeep, debt, or deficit settlement. Shared and
  leader policies use explicitly registered Narrative inventory accounts rather than NPC
  iteration order.
- Guard spawning has a typed, validated context-first Blueprint API. The legacy function is
  deprecated and fails closed when it cannot resolve a complete Territory context.
- The Territory Gameplay Debugger category is functional and reports ownership, capture,
  garrison, and finite-assault state.
- Guard auto-wield waits for Narrative character initialization and only times the relevant
  weapon visual. Unrelated cosmetic load handles no longer produce a false failure.
- Migrated Narrative Blueprint death events can no longer leave a dead guard walking without
  ragdoll. Guard and assault adapters reconcile the event value against the authoritative
  Narrative ASC, stop the AI path, clear locomotion on every role, and use Narrative's existing
  replicated ragdoll state. The migrated guard graph now forwards `bIsDead` to its parent and
  checks the optional activity component before `RemoveAllGoals`. Server-owned Territory NPCs
  suppress client-originated ragdoll RPCs; simulated proxies use Narrative replication.
- Replaced the former Territory GameplayHUD widget-tree-only reconstruction with a complete
  project-owned copy of Narrative's current default HUD graph and tree. The Game, Menu, and
  Modal layers now register during initialization, so `OpenMenu` returns a real widget and the
  controller's Quick Use close path no longer dereferences an unset menu. The Territory capture
  HUD is composed as a passive overlay and does not own input or a second CommonUI stack.
- Guard spawn verification now accepts Narrative CharacterMovement's bounded vertical capsule
  floor settling while continuing to reject horizontal drift, large vertical relocation, and
  facing changes. Valid authored Blacksmith slots no longer destroy their guards after spawn.
- Capture pressure no longer retires incumbent guards. The same surviving pawn remains registered
  across `Claimed -> Contested -> Claimed`; only owner replacement or locking retires it, and
  contest recovery cannot create a free replacement.
- Counterattack waves deploy into deterministic separated formation slots with bounded route
  attempts. Idle Territory movement is retried without interrupting Narrative combat, and a
  permanently failed mover becomes one finite withdrawal.
- The editor validator rejects invalid deployment spacing, placement-attempt bounds,
  movement-retry timing/counts, approach counts, and consecutive spawn-failure limits through
  the same checks whether the profile is validated directly or through a Territory.
- Assault NPCs use project-owned `AC_TerritoryAssault`, which retains Narrative attack and
  interaction generators without guard patrol/return activities. `BPA_ReturnToTerritory` now
  blocks Narrative's dead state and has no unused duplicate sequence-player variable.

## Authority And Integration

- `ATerritoryVolume` remains the sole owner of Territory ownership, progress, and garrison
  read state.
- `UTerritoryControlSubsystem` remains the capture validator and progress authority.
- `UTerritoryCounterAttackSubsystem` schedules and persists assaults; it never changes
  ownership directly.
- `UTerritoryCombatDirector` owns only strategic per-Territory assault slots. Narrative Pro
  remains the tactical attack-token, NPC, activity, combat, faction, and inventory authority.
- `ATerritoryWorldState` remains the replicated late-join projection. No live UObject or NPC
  pointers were added to campaign save data.
- No Narrative Pro source or asset was modified.

## API And Migration

- New Blueprint guard-spawn work should call `ConfigureTerritorySpawnWithContext` and check
  its Boolean result. Existing `ConfigureTerritorySpawn` nodes remain callable but are
  deprecated and require resolvable typed context.
- `GetAllReputation`, `IsControllerEligibleForTerritory`, assault participant getters, and
  `UTerritoryDebugger` Blueprint access were added without changing durable save formats.
- Counterattack saves keep their existing deterministic decision and finite-force fields;
  no save migration is required for this pass.
- `NoCurrencyPayout` now follows its literal name. Projects that relied on its former hidden
  settlement behavior must select an explicit payout policy.

## Verification

- UnrealBuildTool: UHT, TerritoryFramework runtime, TerritoryFrameworkEditor, and final link
  completed successfully in Win64 Development Editor.
- Automation: 103 of 103 TerritoryFramework tests passed, including the Narrative Pro 2.4.2
  player/content migration contract, the assigned-defender
  combat regression, activation/casualty lifecycle, monotonic probability, save/load,
  authority, Blueprint contracts, data-validation failures, and the migrated death-event
  movement/ragdoll regression for guards and assault characters.
- Live native source: all 58 runtime/editor classes, interfaces, and adapters resolved in
  the refreshed MCP source index; zero missing symbols.
- Live Blueprints: all 46 Blueprint and WidgetBlueprint classes under
  `/Game/TerritoryFramework` compiled; zero errored Blueprints.
- CommonUI: all 14 Territory Widget Blueprints passed the live MCP audit with zero issues.
- Data validation: all 62 `/Game/TerritoryFramework` assets validated, and
  `/Game/HopDistrictTest` validated separately with no errors or warnings.
- Standalone physical-capture PIE: entering the Blacksmith point claimed the Place and reduced
  Market Square; entering the Farm point claimed the Place and reduced Castle Hill; ownership
  of both Districts then reduced Haven Reach City. Warning-only states and guard defeat were not
  used to write ownership, and the run emitted no Blueprint runtime or capture-invariant errors.
- Player integration: the project pawn inherits Narrative's Marketplace player and team,
  GAS, inventory, camera, interaction, and save stack. Its generated class no longer implements
  the Territory ownership interface after a cold editor restart. The Narrative controller owns
  the Territory management/resource adapters and selects `WBP_TerritoryGameplayHUD_Modular`.
- HUD/input and guard-spawn regression PIE: the modular GameplayHUD resolved Game, Menu, and
  Modal containers; Quick Use, weapon wheel, player menu, and interaction open/close paths ran;
  and all three authored Blacksmith guards survived Narrative's 3.42 cm capsule floor settling.
  The cold run emitted zero `Accessed None`, Blueprint runtime, guard post-spawn verification,
  or no-free-authored-slot errors.
- Two-client PIE: one dedicated server world and two client worlds started, both clients
  joined, and every world used `BP_TerritoryPlayerCharacter` with the project Narrative
  controller. Both Territory controller components were present. No Blueprint runtime error,
  Accessed None, or Territory capture invariant failure was emitted.
- Build: Win64 Development Editor and Win64 Development Game targets completed successfully.
  The active Narrative Pro 2.4.2 copy matches Marketplace outside generated `Intermediate`
  output; the one timestamp-only `UnrealEditor.modules` difference has identical SHA-256 data.
- Assigned-guard regression PIE: after capture and desired guard count 1, all three finite
  assault NPCs selected Narrative `Goal_Attack`, closed from roughly 1880 cm to melee range,
  and continued attacking instead of being blocked by the strategic movement goal.
- One-guard counterattack regression PIE: the same guard object survived direct
  `Claimed -> Contested -> Claimed` transitions. A physical assault then activated while the
  player remained outside the Property, deployed three separated attackers, and entered combat.
  In the second run the guard killed the first three attackers, `KilledForce` reached 3, the
  second finite wave deployed, dead attackers contributed zero capture pressure, and no new
  Return-to-Territory blackboard failure was logged.
- Death regression PIE: a guard was killed while its server path status was `Moving`. It lost
  its controller and path immediately, entered Narrative ragdoll movement, enabled skeletal
  physics, and retained the same actor location with zero character velocity after two seconds
  in standalone. Dedicated-server plus two-client PIE showed the same ASC death, zero health,
  ragdoll, mesh simulation, and custom ragdoll mode on both simulated proxies. Actors were
  matched by replicated Territory home transform rather than PIE-local object name. The runtime
  log contained zero guard `ServerStartRagdoll` ownership warnings, Blueprint runtime errors,
  or `Accessed None` errors.

## Intentional Constraints And External Gates

- Offscreen capture simulation remains disabled by design. Physical activation requires a
  relevant player and a valid route.
- Individual live capture participant identities remain transient by design; durable saves
  store finite counts and IDs, not pawn pointers.
- Formation slots and movement retry counters are transient deployment state. Existing assault
  records keep the same saved decision, activation, force, and casualty fields; no save migration
  is required.
- A standalone `TDAServer` executable was not produced because the installed Epic binary
  engine reports that Server targets are unsupported. The supported in-editor dedicated
  server plus two-client PIE path passed. A standalone server/package gate requires a source
  engine or an installed distribution with Server target support.
- Starting `UnrealEditor.exe -server -nullrhi` is also blocked before gameplay by a vendor
  `NarrativeArsenalEditor` Slate assertion in `FNarrativeEditorSaveMenus`. This is an editor
  module headless-startup issue, not a Territory runtime failure, and no vendor workaround was
  added to the standalone plugin.
- A UE 5.7 Windows cook of `/Game/HopDistrictTest` was executed to its terminal result in
  7m36s. It failed the project-wide gate with 318 errors and 116 warnings after all Territory
  assets serialized without a Territory cook error. Confirmed external blockers include the
  stale project `/Game/MetaHumans/Common/Face/Face_AnimBP` LiveLink/property-access nodes,
  Narrative Pro demo packages referencing missing or NeverCook GASP traversal, menu, crowd,
  and firearm assets, and Narrative demo vehicle Vulkan shader failures. Full packaging remains
  blocked until those project/vendor content defects are repaired. A real World Partition
  unload/reload cycle also remains a release-environment gate.
- The cook additionally exposed a stale Narrative `NPC_Felix` reference to
  `/Game/TerritoryFramework/Blueprints/Triggers_Bandit`. The real TriggerSet already lives at
  `/Game/TerritoryFramework/AI/Triggers_Bandit`; a project package redirect now provides the
  bounded migration without duplicating or modifying the Narrative asset. After a cold editor
  restart, MCP loaded the legacy path and verified that it resolves to the authoritative asset.
- Narrative Pro's `/NarrativePro/Pro/Core/Character/BP/BP_NarrativePlayer_GASP` emitted two
  `UpdateInputState_Server` no-owning-connection warnings during two-client PIE. They did not
  affect Territory replication or joins and are outside the standalone plugin's ownership;
  Narrative Pro source and assets remain unmodified.
