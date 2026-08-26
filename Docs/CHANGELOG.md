# Changelog

## Unreleased — 2026-08-27 (Place waypoint and silent-lock navigation contract)

- District and City waypoint requests now resolve to a loaded, hierarchy-visible child Place;
  aggregate Territory centers are no longer promoted to compass/screen-space guidance.
- Waypoint selection prioritizes a contested Place, then a Place not held by the viewer, then
  the nearest friendly Place. The stable Territory tag breaks equal-distance ties.
- Locked Territories and descendants of locked or unloaded ancestors now leave Narrative
  navigation registration and every marker domain instead of remaining transparent/interactable.
- Untracked Territory markers remain world-map/minimap intel. Only the resolved Place enters
  Narrative compass and screen-space domains, and the journal/live-report buttons now toggle it.
- District Management Point markers remain map/minimap-only and register only for a visible,
  claimed District. Registry callbacks repair World Partition load-order changes without saving
  actor pointers or creating a second POI/map system.
- Territory marker adapters now suppress Narrative registration, removal, and widget refresh
  callbacks while a world is tearing down, after the owning player and HUD can already be gone.
- Fixed tracked Place icons failing to appear when Waypoint added compass/screen-space domains.
  Territory markers now perform one bounded Narrative remove/add when domain membership changes,
  allowing each navigator widget to create or remove its matching icon without duplicate markers.
- Loaded, hierarchy-visible Places now contribute Narrative POI data through their Territory tag
  without creating a second visual marker. Entering a Place records its first discovery once;
  locked Places and Places below locked or unloaded ancestors remain completely silent.

## Unreleased — 2026-08-26 (Quest Journal template contract)

- Fixed Active and Captured Territory queues remaining visually empty when UISpec-authored
  `ScrollBox` and entry children are private Blueprint widgets. Stable-name runtime resolution,
  native row fallback, explicit failure text, and BlueprintPure row-count diagnostics now make
  list population both reliable and testable.
- Matched the Command Center wireframe more closely: Live Events is the full-width top strip,
  Active/Captured Territory lists form the left rail, and selected-Territory control tabs stay
  immediately visible while long ownership/capture text scrolls inside Overview.
- Replaced the congested three-page Command Center with Narrative Pro's actual Quest Journal
  interaction pattern: `ActiveTerritoriesBox`, `CapturedTerritoriesBox`, one reusable entry
  class, and one synchronized selected-Territory information pane.
- Rebuilt `WBP_TerritoryCommandRow` as a compact selected entry with Place completion, a
  known-Place accordion, hidden-Place privacy, and waypoint control.
- Kept the full intelligence databank visible below selected Territory details and retained the
  seven compact command tabs for Places, garrison, economy, production, threats, and diplomacy.
- Added migration fallbacks for old Quest-named list bindings and `DistrictRowWidgetClass`;
  new project widgets should use `TerritoryEntryWidgetClass`.
- Added CommonUI focus navigation and Blueprint contract tests for the two Territory
  `ScrollBox` bindings, selection entry point, selected state, and information pane.

## Unreleased — 2026-08-25 (Narrative journal hierarchy and detail tabs)

- Aligned the Territory journal with Narrative Pro's quest-journal pattern: Available/Unlocked
  and Captured/Owned queues on the left, a City-grouped visible District directory, and selected
  Territory details on the right.
- Added Overview, Places, Garrison, Economy, Production, Threats, and Diplomacy detail tabs so
  the command screen exposes richer information without rendering every control at once.
- Added player-safe `City -> District -> Place` read models. Locked Territories and descendants
  of a locked or unloaded parent are absent from player lists; developer/debug filters retain
  access to the complete registered projection.
- Added viewer/owner diplomacy, reputation, war, alliance, and trade information to District
  operations views and search/revision tracking.
- Added configurable Narrative CommonUI text styles for generated Territory controls and applied
  the project's Narrative RPG master button/text styles to the journal and compact capture HUD.
- Reduced the journal's opaque background and changed the capture HUD into a 328x108 translucent
  combat card; detailed capture explanations remain available in the Territory menu.

## Unreleased — 2026-08-25 (District staging and shared assault objectives)

- Added modular secure-District staging and recurring cooldown policy. Normal forces require
  at least one loaded District held in `Claimed` or story-`Locked` state by default. `Contested`
  and `Unclaimed` Districts do not qualify. Losing the final District prevents new or undeployed
  strategic counters, while already-active finite forces may finish.
- Made a saved assault's valid Territory GUID authoritative across streaming and tag renames.
  Same-tag actors with a different GUID can no longer receive or resolve the old assault; tag
  lookup remains only as a bounded migration for legacy records that have no GUID.
- Extended that stable identity through physical participant replication, Narrative assault
  goals, CombatDirector slots, recurring duplicate admission, loaded command/debug views, and
  Tales reads. Added `GetAssaultsForTerritoryActor` for exact loaded-actor/GUID queries.
- Routed the public Blueprint `Set Owning Faction` compatibility node through the atomic
  Territory Control transaction so counter scheduling and WorldState callbacks cannot be
  skipped. The structured mutation API remains preferred for explicit story context/results.
- Republished normalized assault counts immediately after authoritative save restore so a late
  join cannot observe serialized living NPC counts that were converted to finite reserve.
- Added explicit `StoryPursuit` launch mode with dual event/profile opt-in for a betrayal boss
  chase that intentionally continues after a faction loses its domination holdings.
- Added `UTerritoryFactionDistrictHoldingCondition` so Tales quests, dialogue, event conditions,
  and Territory state configs can branch on the same strategic holding rule.
- Unified strategic defence calculation, route objectives, and physical attacker targeting.
  District attackers now engage registered guards on same-owner child Properties and may use
  overlapping patrol/post points as physical objectives instead of ignoring the garrison.
- Untagged guard posts now resolve from actor placement plus world-space patrol-node overlap;
  editor orphan/capacity validation uses the same rule.
- Preserved the mandatory first-player activation gate, then allowed explicitly configured
  finite reserve waves to continue after activation without requiring the player to stay nearby.
- Persisted assault launch mode and terminal time for deterministic recurring cooldowns and
  added behavioral, save/load, Blueprint-contract, World Partition fail-closed, and validation tests.
- Added `UTerritoryEventContextCondition` for player-only/GAS Tales events. The Blacksmith XP
  reward now skips owner-preserving `Contested -> Claimed` recovery when there is no valid player
  Ability System context, instead of passing `None` into Narrative's `NE_GiveXP` graph.
- Territory NPC removal now deactivates Narrative activities and removes goals before controller
  cleanup. Stale target-death callbacks can no longer score/end Territory activities through a
  Narrative controller that is pending-kill.
- Guarded `BPA_ReturnToTerritory`'s activity-start gameplay event with an `OwnerController`
  validity branch, closing its remaining pending-kill dereference without modifying Narrative Pro.
- Named `DA_PostGuardDefinition` as “Standard Territory Guard Post”; the Territory content and
  integration map now pass editor data validation with no errors or warnings.
- Marked guard-post registration/removal Blueprint nodes authority-only, matching their existing
  server guards and preventing client graphs from presenting them as valid state mutations.
- Made TDA-only editor integration fixtures optional for community installations: an entirely
  absent fixture is reported as skipped, while any partially installed or broken fixture still
  fails its contract test.

## Unreleased — 2026-08-24 (counterattack combat and audit closure)

- Migrated the supported Narrative Pro baseline from 2.3.3 to 2.4.2 without changing vendor
  source. Territory death listeners now use `OnDeathStateChanged` and ignore revival events.
- Added the project-owned `BP_TerritoryPlayerCharacter`, kept Territory ownership semantics off
  both vendor and project players, and made `BP_TerritoryGameMode` select the project pawn.
- Removed `BP_Property_Blacksmith`'s guard-death `ForceCapture` shortcut. District recapture now
  completes only through physical capture participants and the authoritative control subsystem.
- Added `ATerritoryCapturePoint` and the project `BP_TerritoryCapturePoint` adapter. The Haven
  Reach test map now captures Blacksmith and Farm Places physically, reduces Market Square and
  Castle Hill Districts from those children, and reduces the City when both Districts are owned.
- Migrated `BPA_ReturnToTerritory` to the 2.4.2 multiplayer cinematic API with an explicit empty
  viewer array and no player-controller-index lookup.
- Added Narrative NPC `CharacterID`/`NPCID` admission validation and migrated
  `NPC_TerritoryBandit` from the copied `CubistSoldier` NPCID to `TerritoryBandit`.

- Added server-authoritative, campaign-day Property production using exact Narrative item
  classes. Profiles support input debit, multiple outputs, upgrade scaling, capture-only
  Properties, and Properties that independently pay currency and produce resources.
- Added atomic Narrative inventory recipe settlement reusable by crafting, deterministic
  per-day catch-up in Priority/RuleTag order, per-rule outcomes, ownership checkpoints,
  storage/missing-input failure states, and World Partition-safe site records.
- Added `UTerritoryFactionResourceAccountComponent` for explicit faction inventory routing;
  resource balances remain owned and saved by Narrative inventory. PlayerController accounts
  use bounded startup retries until possession makes the Narrative pawn inventory available.
- Replicated/saved production scheduling and stockpile read models through WorldState, and
  added modular resource/site widgets plus producing/blocked/missing/full operations filters.
- Added profile editor validation and native tests for scaling, scheduling gates, authority,
  save/replication contracts, UI invalidation, and real atomic Narrative inventory debit/output.
- Added a validated Farm example with Grain input, Meat output, independent money income,
  modular scrolling Economy panels, and compact production status in management/info views.
- Rebuilt the Territory journal as a responsive stacked CommonUI command surface and added
  capture HUD, Territory GameplayHUD, reusable resource/site rows, and a project-owned copy of
  Narrative's text-button template with shared Territory button/text styles.
- Replaced the incomplete Territory GameplayHUD reconstruction with
  `WBP_TerritoryGameplayHUD_Modular`, a project-owned copy of Narrative's complete current HUD
  template plus the Territory capture overlay. Game, menu, and modal stacks register during
  initialization again, restoring Quick Use, weapon wheel, inventory/tab, interaction, and
  CommonUI routing without modifying Narrative Pro.
- Fixed new placements inheriting a serialized Blueprint CDO Territory GUID. Every newly
  placed Territory actor now receives a fresh editor identity, while load and PIE retain it.
- Fixed guard post-spawn verification rejecting Narrative characters after normal capsule
  floor settling. The authored X/Y position and facing remain strict, while a bounded 10 cm
  vertical floor snap is accepted instead of destroying valid guards and leaving garrisons at zero.
- Fixed `Claimed -> Contested` deleting the incumbent garrison and `Contested -> Claimed`
  granting free replacements. Surviving guard actors now persist across both transitions;
  owner changes and explicit lock transitions retain their existing retirement policy.
- Added deterministic counterattack formation slots, bounded placement attempts, and idle
  movement recovery. Narrative-spawned attackers no longer share one exact approach transform,
  and a permanently failed movement handoff consumes one finite withdrawal instead of waiting
  forever.
- Counterattack data validation now enforces deployment spacing, bounded placement attempts,
  movement-retry timing/counts, approach limits, and spawn-failure limits through one shared
  profile-bounds path for direct assets and referencing Territories.
- Added a package redirect from the legacy `/Game/TerritoryFramework/Blueprints/Triggers_Bandit`
  path to the existing authoritative `/Game/TerritoryFramework/AI/Triggers_Bandit` TriggerSet.
  This repairs the stale Narrative `NPC_Felix` reference without duplicating the asset.
- Added project-owned `AC_TerritoryAssault`, retaining Narrative combat/interaction support
  without guard patrol or Return-to-Territory activities. The guard return activity now blocks
  Narrative's dead state and no longer starts during death teardown.

- Lowered the durable Territory assault movement goal below Narrative Pro 2.4.2's
  attack goal so detected defenders interrupt movement and the same goal resumes afterward.
- Restricted strategic slots to configured physical assault participants. Defenders use
  Narrative tactical attack tokens without consuming the counterattack force budget.
- Added streamed-Territory slot/delegate cleanup and bounded defender ASC binding retries.
- Required War or Narrative Hostile attitude for physical counterattacks and added
  monotonic profile validation plus behavioral activation/casualty regression tests.
- Added typed Blueprint guard-spawn migration, Blueprint reputation enumeration, and a
  functional Territory Gameplay Debugger category.
- `NoCurrencyPayout` now disables income and upkeep. Capturing-player rewards receive the
  transition instigator; registered Narrative accounts support factions without players.
- Repaired `compile_plugin.bat` with absolute paths and supported UnrealBuildTool syntax.
- Fixed guard auto-wield initialization so unrelated Narrative cosmetic loads cannot cause a
  false timeout, and ensured the retry timer stops when a weapon is already wielded or play ends.
- Fixed migrated Narrative Pro 2.4.2 death events leaving dead Territory guards and attackers
  in locomotion. Territory reconciles the Blueprint event value with the authoritative Narrative
  ASC, stops server AI movement, clears locomotion on every role, and reuses Narrative's
  replicated ragdoll state without adding a competing death flag. The guard Blueprint now
  forwards `HandleDeath.bIsDead`, null-checks its optional activity component, and prevents
  simulated proxies from sending unowned Narrative ragdoll RPCs.
- Completed live MCP validation of all 58 native classes/interfaces/adapters, all 46 plugin
  Blueprint classes, all 14 CommonUI widgets, and all 62 plugin content assets. Two-client
  PIE created one dedicated server world plus two client worlds; both clients joined, all
  worlds used the project pawn/controller integration, and both Territory controller
  components were present.
- Win64 Development Editor and Game targets build, and all 103 TerritoryFramework automation
  tests pass, including the Narrative Pro 2.4.2 player/content migration contract, the new
  placement-GUID regression, the guard/assault death-ragdoll regression, deterministic assault
  deployment, garrison contest preservation, and five production behavior/contract/UI tests.
- A UE 5.7 Windows cook of `/Game/HopDistrictTest` ran to completion but failed the project-wide
  release gate with 318 errors and 116 warnings. No Territory asset produced a cook error. The
  blockers are stale project MetaHuman `Face_AnimBP` nodes/imports and Narrative Pro demo assets
  that reference missing or NeverCook packages, plus Narrative demo vehicle Vulkan shaders.

## Unreleased — 2026-07-30 (TerritoryFramework lifecycle and integration re-audit)

### Counterattack state events, death, and economy account routing — 2026-08-07

- Repaired the District Command Center lists and presentation: the left queue now uses the
  exact Available/Unlocked predicate, Captured/Owned rows and directory rows use a responsive
  selection-only card instead of losing their width to hidden guard buttons, and the search
  bar now token-matches names, tags, owners, states, availability, threats, attackers, and
  child garrisons. The cramped garrison dropdown/raw text block was replaced by post navigation,
  staffing progress, structured finance readouts, and Apply/Empty/Full planning actions.

- Added state-wise `OnCounterHappened` delivery after every committed counterattack transition.
  The server subsystem exposes the global event; relevant owning clients receive the complete
  value payload reliably through `UTerritoryPlayerManagementComponent`; and
  `UTerritoryHUDWidget` forwards it as a Blueprint event for Narrative HUD notifications.
  Same-state record updates and save/load hydration do not duplicate or replay the event.
- Clean Build22 completed with UHT, Runtime, Editor, and linking under warnings-as-errors;
  all 88 TerritoryFramework automation tests and all 45 plugin-content asset validations
  passed with zero errored Blueprints. Live PIE/MCP verified one committed `Grace -> Cancelled`
  transition delivered one matching server event and one reliable owning-player event, while
  initial Grace record creation delivered neither.

- Deferred Territory casualty-delegate binding until the Narrative NPC definition,
  controller, activity, appearance, and native death handlers are ready. Narrative's
  replicated ragdoll now precedes finite-force retirement, with an idempotent participant
  safeguard and `HasValidDeathRagdollSetup` live diagnostic.
- Corrected `EqualSplitOnlineMembers` to mean online Narrative player characters only.
  Same-faction guards, counterattackers, companions, vendors, and other NPC inventories
  can no longer receive automatic Territory income or subsidize guard upkeep.
- Periodic income and upkeep now resolve against the same policy-specific account cohort.
  Shared/leader policies require their explicit registered Narrative account and fail
  closed instead of silently falling back to an arbitrary loaded faction actor.
- Added a behavioural regression proving player accounts remain eligible while Territory
  guard and counterattack NPC wallets are excluded, and updated the economy policy docs
  to the actual `SharedNarrativeAccount` symbol.
- Closed two explicit-account bypasses: a `PreferredBeneficiary` can no longer replace the
  registered Shared/Leader actor, and streamed or faction-changed registrations are
  revalidated before every settlement.
- Clean Build21 completed with UHT, Runtime, Editor, and linking under warnings-as-errors;
  all 87 TerritoryFramework automation tests passed. Live PIE verified six naturally killed
  Bandits ragdolled with mesh physics, left capture pressure, and resolved the finite assault;
  periodic `+600` income and `-150` upkeep both used the player account while all guard and
  attacker balances remained zero.

### District garrison command center — 2026-08-06

- Physical counterattack creation now goes through Narrative Pro's
  `UNarrativeCharacterSubsystem::SpawnNPC`, keeping `CharacterMap`, `NPCMap`, duplicate
  policy, controller creation, and teardown authoritative. The complete Territory spawn
  context is applied before `SetNPCDefinition`, and multi-pawn forces now reject Narrative
  definitions that do not allow multiple instances.
- Physical guard creation now uses that same Narrative character-subsystem authority.
  Complete guard SpawnInfo and Territory/spawn-point context are present before definition
  assignment; invalid class/controller/auto-possession/multiple-instance contracts fail
  closed, and any unexpected spawn adjustment is rejected to preserve authored staging.
- Guard and assault pawns initialize collision from valid `Pawn`/`CharacterMesh` profiles
  and reapply Narrative trace-channel responses after registration, eliminating invalid
  `Custom` profile lookups. Goal startup waits for Narrative definition/appearance readiness;
  optional weapon visuals never block movement, and valid unarmed guards stop wield polling
  without producing a false runtime warning.
- Counterattack warnings now use the Territory HUD's inline description alert and a non-modal
  Blueprint animation hook. Both Narrative notification presentations in this project's HUD
  pause the world, which previously froze the newly activated physical force until dismissed.
- Capture contest read models now commit state, contesting faction, and progress together
  through `ATerritoryVolume::CommitOwnershipData`; typed gameplay reads use that authority
  directly, so Blueprint interface overrides cannot expose a stale contesting faction during
  physical assault evaluation.
- Fixed the native physical assault pawn contract: dynamically spawned attackers now
  use `ANarrativeNPCController` with spawned auto-possession. Scheduling, preview,
  lifecycle revalidation, physical spawn, and editor validation reject custom classes
  that cannot supply Narrative activities. This closes the live six-attacker withdrawal
  regression where every finite participant failed goal initialization.
- Fixed the first successful participant registration cancelling its own assault:
  `Active` now accepts the matching attacker's legitimate `Contested` Territory state,
  while pre-activation pressure, third-faction contests, and locked/unclaimed targets
  still cancel fail-closed.
- Counterattack defence now bounds raw post reserves by the player's authorized desired
  staffing target. A `PlayerChooses` target of zero keeps its saved replacement stock
  visible but contributes zero hidden reserve defence; target one contributes at most one.
- Tagged streamed guard posts now log their pre-registration wait as expected verbose
  state, and guard weapon loading reports a warning only if no supported equipped weapon
  exists after the bounded Narrative-definition retry window.
- Guard placement now uses one active combat slot per unique spawn-point actor. Normal
  recruitment preserves exact authored X/Y/facing, aligns only capsule height, and fails
  blocked slots instead of using broad NavMesh projection, random fallback, or collision
  relocation. Legacy per-post `MaxGuards` values are bounded during save migration.
- Hardened `PlayerChooses` again for context-loss paths: exact membership is recovered
  from every live Narrative player controller/pawn/player-state for the committed owner,
  so a Property → District → City reduction cannot fall back to paid automatic staffing.
- Automatic retaliation now uses diplomacy-first `ScheduleBestCounterAttack`, evaluates
  every configured finite force, and selects the strongest eligible faction; the former
  owner is only a stable tie-break.
- Added `UnguardedLaunchProbability` (default 100%) plus same-owner local District
  defence cascading. Zero active guards guarantees the post-grace launch decision, never
  ownership or offscreen capture.
- Cascaded child Property contests/assaults and strongest-attacker planning into the
  District Journal, including exact leaf target, Property alignment, defence power,
  power ratio, priority, garrison counts, and projected risk.
- Registered locked Districts now remain visible and selectable in the primary intel
  queue with exact lock/management reasons; server mutations remain owner/claimed gated.
- Closed the Editor, archived the prior plugin build products, and completed a clean
  unsuffixed UE 5.7 project-directory Runtime/Editor build with UHT and linking; all 84
  TerritoryFramework automation tests pass with zero failures or skipped tests.
- Player-driven captures now start with zero assigned friendlies by default; explicit transition context is preserved through City/District/Property cascades so child Properties cannot silently respawn the authored three guards.
- Fixed the project Blueprint's direct `ForceCapture` path: the control subsystem now resolves a deterministic matching Narrative player controller by faction, and exposes `ForceCaptureWithContext` for exact quest/script instigators.
- Hardened `PlayerChooses` against spoofed transition context: zero guards now requires the supplied controller, pawn, or player state to have exact Narrative membership in the new owner faction.
- Tag/proximity-bound guard posts now register into the Territory's unique post union, contribute capacity/defence, reconcile after World Partition load-order changes, and never generate unstable save GUIDs at runtime.
- Counterattack Approach IDs have clear editor metadata and blank entries receive editor-only stable defaults; the Blacksmith uses `Blacksmith_WestRoad` on a verified full navmesh route.
- Migrated the Territory data validator to UE 5.7's current `FAssetData`/`FDataValidationContext` API. The previous deprecated overrides registered but never executed; errors and warnings now retain their proper severities.
- Added a saved per-territory/faction counterattack evaluation-cycle high-water ledger so bounded terminal-history trimming cannot reuse a prior deterministic seed. Older saves migrate from their retained assault records.
- Added the dedicated `/Game/TerritoryFramework/AI/NPC_TerritoryBanditAssault` definition backed by native `ATerritoryAssaultCharacter`; the counterattack profile no longer points at the rejected guard-only class.
- Counterattacks now distinguish physical `IsAssaultActive` from queued `IsAssaultPendingOrActive`, reject diplomacy at scheduling, cancel non-claimed or invalidated configurations in every phase, match any defending Narrative faction membership, deduplicate approach IDs defensively, and bound zero-spawn retries with a saved failure counter.
- Forced Tales capture now preserves its player/Tales transition context, and the explicit force request genuinely bypasses locks as well as conditions and diplomacy.
- Separated one-time `GuardRecruitmentCost` from recurring `GuardCost` upkeep and added an atomic absolute staffing mutation with full deployment rollback/refund.
- Replicated exact active/reserve/pending garrison snapshots and prevented delayed reserves from exceeding a lowered desired target.
- Expanded the journal and in-world District management screens with child-Property selection, integer target, Apply/0/Max controls, aggregate District profit/loss, local garrison previews, authoritative replicated economy rates, and transaction reasons.
- Added Blueprint/save/replication contracts and regressions for player capture target zero, unrelated AI target preservation, tag-bound capacity, dead-guard target reduction, pending-reserve UI invalidation, explicit force context, and absolute management APIs.

### Correctness and architecture

- Added the complete deterministic counterattack lifecycle: grace, evaluation, warning, relevant-player proximity activation, finite physical Narrative NPC force, capture participation, casualty accounting, resolution, persistence, replication, and bounded history.
- Added Territory-owned Narrative goal/activity/participant adapters without modifying Narrative Pro or creating another AI controller, faction system, currency wallet, or capture subsystem.
- Capture registration now reports admission, tracks exact participant identity, binds Narrative ASC death, and removes dead/withdrawn attackers from capture pressure exactly once.
- Guard defeat no longer directly unclaims a territory. Fresh and saved garrison restoration are distinguished, referenced streamed spawn points are loaded during restore, and manual multi-guard removal selects the full mutation set before committing.
- Added explicit transition context APIs for lock/unlock and removed implicit first-player-controller selection from gameplay transitions.
- Added server-owned per-player management components and targeted assault notification RPCs; clients no longer construct an unauthorized local management bridge.
- Economy validates world/faction ownership of Narrative accounts, settles automatically
  against online Narrative players only, supports fail-closed explicit shared/leader account
  registration, uses `int64` intermediates, and refuses currency overflow.
- Centralized Narrative attitude access in `UTerritoryNarrativeProAdapter`; external Friendly attitudes import as Alliance without the previous Friendly→Neutral round-trip loss.
- WorldState now hydrates replicated economy, diplomacy, capture, and assault read models and retains only the configured number of terminal assault records.
- Editor validation now covers stable GUIDs, hierarchy cycles, streamed World Partition actors, guard Narrative definitions/configuration, and counterattack profiles/approaches.

### Documentation and tests

- Added `17_Counterattack_System.md`, `18_Operations_UI.md`, and `DEEP_REAUDIT_2026-07-30.md`; corrected API, economy, diplomacy, guard, multiplayer, save/load, AI, Blueprint, UI, and setup documentation against current symbols.
- Added deterministic/monotonic counterattack tests, notification-without-force regression coverage, active-force reconstruction and trimmed-history cycle-ledger coverage, authority/replication/Narrative contract checks, and fresh-vs-saved garrison-count regression coverage.
- Narrative Pro vendor source remains unchanged.

### Narrative CommonUI operations

- Added `FTerritoryDistrictOperationsView` and `FTerritoryEconomyOperationsView`, with Blueprint filters for unlocked, available, owned, manageable, under-attack, contested, locked, and financial-risk districts.
- Redesigned `WBP_HopTerritoryJournalWidget` as a three-column District Command Center with live KPI cards, actionable and captured queues, a searchable directory, selected-district security/finance/threat/assault detail, atomic `+1/-1/+5/-5` guard controls, and separate finance/exposure views.
- Made the Command Center resolution-safe with a 1920×1080 `ScaleToFit` design surface, automatic text wrapping, and a focus-aware scrolling command column so the journal and lower controls remain inside constrained viewports.
- Added project-styled `WBP_TerritoryCommandRow` rows. Selecting a district from any queue or ledger opens the same command detail surface and reuses the existing guarded management delegates.
- Corrected the actionable queue contract to require registered, unlocked, currently available, non-owned districts. Captured/owned uses its own mutually exclusive predicate, and counts now derive from the exact population predicates.
- Fixed journal invalidation so guard, capture, finance, lock, and assault changes refresh existing rows even when list count and filter text are unchanged.
- Added captured/unlocked side lists, finance/loss lists, guard add/remove controls, exact disabled reasons, threat/assault summaries, and targeted Narrative HUD assault notifications.
- Replaced direct viewport/input manipulation with pushes into the Narrative gameplay HUD's registered CommonUI layer.
- Fixed stale current-territory display, pawn-versus-controller faction/account lookups, and misleading reserve/treasury values.
- Made Territory activatable widgets focusable by default, fixed nested journal activation focus, and added explicit bidirectional keyboard/gamepad navigation, accessible names, control tooltips, and Narrative CommonText accessibility styling to the supplied Territory widgets.
- Added native UI contract, operations-filter, and live-revision regression tests. All Territory widget assets compile cleanly through scoped MCP.

## v0.2.5 — 2026-07-28 (Deep Re-Audit: 24 Fixes Across P0/P1/P2)

Comprehensive deep re-audit with 3 parallel verification agents scanning 34 findings against current source. 24 confirmed findings fixed, 5 disputed, 5 partially confirmed.

### P0 — Release Blockers (3 fixed)

- **P0-01 — Friendly diplomacy round-trip corruption**: `AttitudeToDiplomacyState(Friendly)` now returns `Alliance` instead of `Ceasefire`, preventing lossy downgrade chain: `Friendly → Ceasefire → Neutral`. Round-trip is now lossless: `Friendly ↔ Alliance` (`TerritoryDiplomacySubsystem.cpp`)
- **P0-02 — City/District permanently stuck Contested**: Added Contested→Claimed recovery in both `ATerritoryCity::OnDistrictControlChanged` and `ATerritoryDistrict::OnPropertyControlChanged`. When incumbent retakes all children, state transitions from `Contested` back to `Claimed` (`TerritoryHierarchy.cpp`)
- **P0-03 — GetFirstPlayerController in conditions/events**: Introduced `FTerritoryTransitionContext` struct with `Instigator`, `TargetPawn`, `PlayerController`, `TalesComponent`, `RequestingFaction`. `CheckStateConditions` and `FireStateEvents` now accept explicit context with graceful fallback. TalesComponent is now properly passed to `ExecuteEvent` (`TerritoryVolume.h/.cpp`)

### P1 — High Priority (11 fixed)

- **P1-02 — Hierarchy stale after BeginPlay**: City and District now reconcile derived ownership from already-registered children after binding delegates, handling World Partition streaming and save/load ordering (`TerritoryHierarchy.cpp`)
- **P1-03 — OnCityLost fires repeatedly**: Added `bCityLostFired` guard to `ATerritoryCity`. Fires once per loss episode, reset on recapture by any faction (`TerritoryHierarchy.h/.cpp`)
- **P1-05 — CRC32 fallback GUID**: Removed collision-prone 32-bit CRC fallback. Missing GUIDs now log an error and fail closed instead of generating unsafe identity (`TerritoryVolume.cpp`)
- **P1-07 — WorldState replicated snapshots stale**: Added `OnFactionUpkeepDeficit` delegate so territories can bind to upkeep shortfall events and respond (`TerritoryEconomySubsystem.h/.cpp`)
- **P1-09 — Guard reserve state transient**: Marked `CurrentReserveCount` and `PendingReserveSpawns` as `SaveGame`. Added `SavedActiveGuardCount` to persist active guard count. `InitializeReserves` respects saved values (`TerritoryGuardSpawnPoint.h/.cpp`)
- **P1-10 — Guard narrative overrides not authorable**: Added `NPCDefinitionOverride`, `ActivityConfigurationOverride`, `TriggerSetOverrides` to `ATerritoryGuardSpawnPoint`. Wired through `SpawnGuards()` and `TrySpawnSingleGuard()` (`TerritoryGuardSpawnPoint.h`, `TerritoryVolume.cpp`)
- **P1-13 — Damage causer not reliable kill attribution**: `TakeDamage` now resolves instigator via `EventInstigator->GetPawn()` first, then `DamageCauser->GetInstigator()`, then `DamageCauser` fallback. Projectiles no longer mask the shooter (`TerritoryGuardCharacter.cpp`)
- **P1-14 — Weapon retry timer keeps running**: `TryWieldDefaultWeapon` now clears `DefaultWeaponWieldTimer` on successful wield instead of letting it fire until attempt cap (`TerritoryGuardCharacter.cpp`)
- **P1-15 — Generic defenders become immortal**: `RegisterDefender` now logs warning when actor lacks `IAbilitySystemInterface` — non-ASC defenders cannot fire `OnDied` (`TerritoryVolume.cpp`)
- **P1-16 — ForceCapture has no result**: Now returns `bool` and verifies final state matches requested owner/state before broadcasting. Returns `false` with error log on failure (`TerritoryControlSubsystem.h/.cpp`)
- **P1-19 — FactionLeader payout placeholder**: Added runtime warning log documenting that `FactionLeader` resolved to the first iterated member, not an actual leader (`TerritoryEconomySubsystem.cpp`). Superseded by the 2026-08-24 explicit registered-account routing.
- **P1-20 — Unpaid upkeep no consequence**: Economy tick now broadcasts `OnFactionUpkeepDeficit(Faction, Deficit)` when a faction can't pay full guard upkeep, enabling territories to suspend reserves (`TerritoryEconomySubsystem.h/.cpp`)

### P2 — Minor (8 fixed)

- **P2-01/P2-02 — Unused settings deprecated**: `EconomyStartingGold` and `MaxCaptureHistory` marked `DeprecatedProperty` — no runtime effect, scheduled for removal in v0.3.0 (`TerritoryDeveloperSettings.h`)
- **P2-03 — Diplomacy records events for no-op mutations**: `DeclareWar`, `DeclarePeace`, `FormAlliance`, `SignNonAggression` now check old state before recording events (`TerritoryDiplomacySubsystem.cpp`)
- **P2-04 — Contested capture resume undocumented**: Added policy documentation in `RestoreCaptureState` — progress restores but attackers are transient, next tick decays. Intentional design (`TerritoryControlSubsystem.cpp`)
- **P2-05 — OnTerritoryUncontested ignores tag**: Now validates `InTerritoryTag == TerritoryTag` before clearing `ContestingFaction` (`TerritoryVolume.cpp`)
- **P2-06 — Guard class fallback silent**: Both `SpawnGuards()` and `TrySpawnSingleGuard()` now log warning with definition name and class name when falling back to base `ATerritoryGuardCharacter` (`TerritoryVolume.cpp`)
- **P2-07 — Registry specificity from AABB only**: `GetTerritoryAtLocation` now uses class priority (`Property=3 > District=2 > City=1 > Volume=0`) before bounds volume as tiebreaker (`TerritoryRegistrySubsystem.cpp`)
- **P2-10 — Transaction trimming duplicated**: Removed per-insert `while` loop trim from `RecordCurrencyTransaction`. Batch trim in `OnEconomyTick` and `RestoreTransactions` is now the single trim point (`TerritoryEconomySubsystem.cpp`)

### Other Changes
- **TerritoryPlayerController removed**: Deleted duplicate header (copy of `ANarrativePlayerController`) and input binding bug that caused T-key weapon wheel freeze. Parent class handles all input correctly
- **13 stale documentation fixes** across 5 doc files (02_Interfaces, 03_Core_Actors, 06_Narrative_Integration, 14_API_Reference, 16_District_Management, CHANGELOG)

### Commits
- `182de58` — P0 fixes (diplomacy, hierarchy, transition context)
- `5124269` — P1 batch 1 (OnCityLost guard, weapon timer, hierarchy reconcile, GUID)
- `36ed107` — P1 batch 2 (reserve SaveGame, narrative overrides, kill attribution, defender validation, ForceCapture result, economy warnings)
- `7da7795` — P2 fixes (deprecated settings, diplomacy events, tag validation, class priority, trimming)

---

## v0.2.4 — 2026-07-25 (Deep Audit Fixes)

Bug fixes from the complete codebase audit:

### Bugs Fixed
- **A1 — Deprecated CRC API**: Replaced `FCrc::StrCrc_DEPRECATED` with `FCrc::StrCrc32` in territory GUID fallback (`TerritoryVolume.cpp`)
- **A2 — Stale weak-pointer guard spawn guard**: Changed `SpawnedGuards.Num() == 0` to `GetSpawnedGuardCount() == 0` in state-transition guard check to avoid stale weak-pointer false negatives (`TerritoryVolume.cpp`)
- **A3 — OnGuardKilled null killer**: Added `LastDamagingInstigator` tracking via `TakeDamage` override on `ATerritoryGuardCharacter`; the broadcast now passes the actual killer instead of `nullptr` (`TerritoryGuardCharacter`, `TerritoryVolume.cpp`)
- **A4 — Reserve spawn chain loss**: Removed `IsTimerActive` guard in `ScheduleAutomaticReserveSpawn` so multiple pending reserve spawns chain correctly when guards die between timer ticks (`TerritoryGuardSpawnPoint.cpp`)
- **A5 — Validation order**: Swapped `DefendersRemain` check before `DiplomaticallyBlocked` in `ValidateAndBeginCapture` so allied defenders return the correct result code (`TerritoryControlSubsystem.cpp`)
- **A6 — Capture summary restore gap**: Added capture summary sync (ownership, state, contesting faction, control progress) in `SyncSubsystemsFromReplicatedState` via `RestoreCaptureState` — summaries were exported/imported but never restored to the ControlSubsystem (`TerritoryWorldState.cpp`)
- **A7 — Transaction ledger fragmentation**: Replaced O(N²) `while` + `RemoveAt(0)` loop with a single `RemoveAt(0, Excess)` bulk call (`TerritoryEconomySubsystem.cpp`)
- **A8 — SpatialIndex stale keys**: Added `RemoveInvalidTerritories()` to clean up GC'd territory entries from the forward and reverse maps; called periodically from `PollBoundsChanges` (`TerritorySpatialIndex`, `TerritoryRegistrySubsystem.cpp`)

### Documentation Updated
- `03_Core_Actors.md`: Added notes about GUID fallback hash limitation and volume tick being disabled
- `04_Subsystems.md`: Updated `SyncSubsystemsFromReplicatedState` description to include capture state restore
- `10_Save_Load.md`: Updated load flow and state-reconstructed section; added deprecation note for `ATerritorySavableData`
- `13_Multiplayer.md`: Updated capture persistence limitation to reference the fix

## v0.2.3 — 2026-07-24 (API Refactor: Pure Function Markers + Tooltips)

Comprehensive API refactor for improved Blueprint usability. Read-only getters are marked BlueprintPure (no exec pin needed), with rich tooltips and contract coverage for Pure/Callable invariants.

### API Purity Fixes
- **TerritoryVolume**: All query getters now `BlueprintPure` (GetOwningFaction, GetTerritoryState, GetControlProgress, IsContested, GetTerritoryTag, GetTerritoryDisplayName, IsLocked, GetLockReason, GetSpawnedGuardCount, GetConfiguredGuardCount, HasGuardsAlive, IsOwnedByFaction, ContainsPoint, etc.)
- **TerritoryGuardSpawnPoint**: All getters now `BlueprintPure` (HasAvailableSlot, HasReserveAvailable, GetActiveGuardCount, GetReserveCount, GetSpawnTransform, GetPatrolRoute, HasPatrolRoute, IsLoopPatrol, GetOwningTerritory, GetPatrolRouteAsTransforms, GetPatrolWaitTimes)
- **TerritoryGuardCharacter**: Added 3 new BlueprintPure functions (GetTerritoryPatrolRoute, HasTerritoryPatrolRoute, GetPatrolNodeCount) and 4 new helper functions (GetSafePatrolNode, GetSpawnTransform, GetOwningTerritory, GetGuardFaction, IsSpawnPointGuard)
- **TerritoryBlueprintLibrary**: GetTerritoryByTag, GetTerritoryAtLocation, IsTerritoryAtLocation now `BlueprintPure`
- **TerritoryRegistrySubsystem**: GetTerritoryByTag, GetTerritoryByGUID, GetTerritoryAtLocation, GetAllTerritories, GetTerritoryCount, GetTerritoryCountForFaction now `BlueprintPure`

### New Guard Patrolling API
Added 7 new BlueprintPure functions on ATerritoryGuardCharacter to simplify patrol route access:

```cpp
/** Returns this guard's patrol route via spawn point */
UFUNCTION(BlueprintPure, Category="Territory|Guard|Patrol")
TArray<FTerritoryPatrolNode> GetTerritoryPatrolRoute() const;

/** Returns true if this guard has a configured patrol route */
UFUNCTION(BlueprintPure, Category="Territory|Guard|Patrol")
bool HasTerritoryPatrolRoute() const;

/** Returns the number of patrol nodes in this route */
UFUNCTION(BlueprintPure, Category="Territory|Guard|Patrol")
int32 GetPatrolNodeCount() const;

/** Safely fetches a single patrol node by index */
UFUNCTION(BlueprintPure, Category="Territory|Guard|Patrol")
bool GetSafePatrolNode(int32 Index, FTerritoryPatrolNode& OutNode) const;

/** Returns the spawn transform for this guard */
UFUNCTION(BlueprintPure, Category="Territory|Guard")
FTransform GetSpawnTransform() const;

/** Returns the owning territory for this guard */
UFUNCTION(BlueprintPure, Category="Territory|Guard")
ATerritoryVolume* GetOwningTerritory() const;

/** Returns the faction tag this guard belongs to */
UFUNCTION(BlueprintPure, Category="Territory|Guard")
FGameplayTag GetGuardFaction() const;
```

### Blueprint Tooltips Added
All public getters across TerritoryVolume, TerritoryGuardSpawnPoint, TerritoryGuardCharacter, TerritoryBlueprintLibrary, and TerritoryRegistrySubsystem now have rich `ToolTip` meta with usage examples that appear on hover in Blueprint editor.

Examples:
- `TerritoryVolume::GetOwningFaction()`: returns the stable owner, or the incumbent defender while contested; it is empty only when unclaimed.
- `TerritoryGuardCharacter::GetTerritoryPatrolRoute()`: "Returns this guard's patrol route via spawn point. Use HasTerritoryPatrolRoute() to check if a route is configured."
- `TerritoryGuardSpawnPoint::HasPatrolRoute()`: "Returns true if this spawn point has a configured patrol route. Use before accessing GetPatrolRoute() to avoid empty array issues."

### Signature Changes (Breaking)
- `ATerritoryGuardSpawnPoint::GetPatrolRoute()` signature changed from `const TArray<FTerritoryPatrolNode>&` to `TArray<FTerritoryPatrolNode>` (returns by value to match BlueprintPure requirement)

### Current Automation Coverage (53/53 Passing)
- **FTFContract_TerritoryGuardCharacter**: Verifies the replicated guard context and 8 BlueprintPure helpers (GetTerritoryPatrolRoute, HasTerritoryPatrolRoute, GetPatrolNodeCount, GetSafePatrolNode, GetSpawnTransform, GetOwningTerritory, GetGuardFaction, IsSpawnPointGuard)
- **FTFContract_GuardSpawnPointPure**: Verifies 11 BlueprintPure functions
- **FTFContract_VolumePureGetters**: Verifies the BlueprintPure volume query API
- **FTFContract_BlueprintLibraryPure**: Verifies pure subsystem/query helpers and `ForceCaptureTerritory` authority metadata
- **FTFContract_RegistrySubsystemPure**: Verifies 6 BlueprintPure functions
- **FTFFunctional_RuntimeInvariants**: Verifies contested ownership and guard spawn context behavior

### Audit Stabilization
- **Contested ownership invariant**: entering `Contested` preserves the incumbent defending faction; `IsOwnedByFaction()` remains false unless state is `Claimed`
- **Capture authority and validation**: capture mutations reject native client calls; external progress and attacker registration revalidate gameplay rules; registration enforces the attack budget
- **Capture cleanup**: invalid weak attackers are pruned, locked captures reset, and reset/decay clears the contesting faction while restoring the incumbent to `Claimed`
- **ForceCapture contract**: valid server calls set progress to 1.0, clear runtime capture state, set `Claimed`, and broadcast old/new owner tags
- **Guard context**: home transform, owning territory, and owning spawn point now replicate; helpers return the stored home transform and Narrative faction
- **API authority**: `ForceCaptureTerritory` is now `BlueprintAuthorityOnly`
- **Economy integrity**: ticks credit income before debiting affordable upkeep, redistribute member debits without partial mutation, and suppress direct ghost credits when no member can receive currency
- **Persistence completeness**: transaction ledgers and rich treaty metadata restore through WorldState/SavableData; WorldState exports capture summaries; saved contests resume decay in ControlSubsystem
- **Diplomacy bridge**: treaty-derived attitudes synchronize in both directions, internal writes suppress their delegate echo, external Neutral ends ceasefires, and `OnFinishedLoad` reapplies rich metadata after every Narrative load

---

## v0.2.2 — 2026-07-24 (Session 4: Security & Integration Audit)

Session 4 audit findings resolved. 4 HIGH-severity issues fixed in currency bridge and combat systems.

### Economy Subsystem (HIGH)
- **Periodic ghost transactions prevented**: Economy-tick transaction recording is gated on `Members.Num() > 0`; the direct credit path was completed in v0.2.3
- **TryDebitTreasury atomicity**: Implemented two-phase commit — first validate all members can afford their share, then apply all debits. No partial debits recorded as full transactions
- **Delegate lifecycle fixed**: Added `UnbindControllerDeath()` to properly unbind `OnDied` delegate when releasing assault slots

### Combat Director (HIGH)
- **Territory-specific abort**: `BTTask_RequestTerritoryPermission::AbortTask` now releases only the specific territory slot from BB, not all slots across all territories
- **Delegate unbinding**: `ReleaseAssaultSlot`, `ReleaseAllSlots`, and `OnAssaultControllerDied` all call `UnbindControllerDeath()` to prevent delegate accumulation

### Documentation (CRITICAL/HIGH)
- **04_Subsystems.md**: Removed deprecated `Gold` field from `FTerritoryTreasury` documentation (field removed in v0.2.1)
- **14_API_Reference.md**: Fixed `FTerritoryEconomySnapshot` (correct field: `Treasury`), `FTerritoryTreasury` (removed `Gold`), `FReplicatedFactionEconomy` (removed `Gold`/`Income`), added `GetAllReputation` method
- **06_Narrative_Integration.md**: Updated WorldState description to clarify currency bridge architecture

### Testing
- Added 6 new contract tests for v0.2.1 APIs (47 total)

---

## v0.2.1 — 2026-07-24 (Sessions 2 + 3)

Deep re-audit sessions 2 and 3. All P0, P1, P2 findings resolved plus NarrativePro currency bridge and 47 automation tests.

### Core Bugs (P0)
- **OnCityLost fires correctly**: Added fallback for contested transition — `SetTerritoryState(Contested)` clears `OwningFaction` before hierarchy check, so loss detection now handles the empty-tag case
- **TryUnlock respawns guards**: Extended guard respawn guard to cover `Locked→Claimed` transition (was only `Contested→Claimed`)
- **BT abort releases slots**: Added `AbortTask` override to `BTTask_RequestTerritoryPermission` — prevents assault slot leak when behavior tree aborts between request and release

### BoundControllers (P1)
- **Memory leak fixed**: `BoundControllers` set now pruned in both `ReleaseAssaultSlot` and `ReleaseAllSlots` — prevents slot exhaustion when NPC re-enters combat after ASC recreation

### Reputation Persistence (P1/P2)
- **Added `GetAllReputation`**: New method on `UTerritoryDiplomacySubsystem` to export full reputation map — enables save/load of reputation via `ATerritoryWorldState`
- **WorldState serialization**: `ExportPersistentState` now writes reputation to `ReplicatedReputation` array; `SaveGame` preserves reputation across sessions

### Economy Subsystem (P1/P2)
- **Init order sensitivity**: Added `InitializeDependency<UTerritoryRegistrySubsystem>()` to economy init — ensures dependency order regardless of plugin load sequence
- **Guard cost heuristic**: Changed `GetConfiguredGuardCost` to use `GuardSpawnCount` (configured) instead of `SpawnedGuards.Num()` (runtime) — eliminates one-tick undercount after guard wipe
- **Deferred RecalculateIncome**: Ownership changes mark factions dirty, actual recalculation runs once per economy tick (was O(3N) per capture cascade)

### Debug Widget (P1)
- **Cache invalidation**: Added `NativeDestruct` override to reset cached subsystem pointers and `bSubsystemsCached` flag — prevents use-after-free when widget is destroyed

### Diplomacy (P1/P2)
- **SignNonAggression API**: New `SignNonAggression(FactionA, FactionB)` method creates non-aggression pacts
- **BreakCeasefire API**: New `BreakCeasefire(FactionA, FactionB)` method ends ceasefires cleanly
- **Inbound bridge fixed**: `OnFactionAttitudeChanged` no longer collapses TradeAgreement/NonAggression/Ceasefire to Alliance when external Friendly attitude arrives. Rich treaties are preserved. Hostile always overrides.
- **Reentrancy guard**: `bSuppressSync` RAII guard prevents recursive mutation during diplomacy broadcasts
- **const_cast removed**: Uses non-const `FindTreaty` overload instead of `const_cast`

### Guards (P1)
- **Null-guard KilledActor**: `OnDefenderDied` adds null check before accessing `KilledActor` (defense against ASC fire-on-destroy edge case)
- **RegisteredDefenders check**: `OnAllGuardsDefeated` checks ALL `RegisteredDefenders` (includes non-guard defenders), not just `SpawnedGuards`
- **CombatDirector death hook**: Binds ASC `OnDied` when granting assault slots, auto-releases on NPC death
- **Stale slot counts**: `GetGrantedSlots` filters dead weak pointers

### Validation (P1)
- **Faction prefix**: Validator now accepts `Narrative.Factions.` OR `Narrative.Faction` (trailing dot optional)
- **Coexistence error**: `CheckSingletonActors` now errors if both `ATerritoryWorldState` and `ATerritorySavableData` exist (prevents save corruption)

### Save/Load (P0/P1)
- **SavableData gold fixed**: `LoadFromSelf` uses `SetFactionTreasury` (exact restore) instead of `AddToTreasury` (additive, caused double gold with WorldState)
- **PIE duplicate guard**: `PostDuplicate` only regenerates GUID for editor duplication, not PIE world creation
- **Null-guard GetWorld**: EconomySubsystem `Initialize`/`Deinitialize` check `GetWorld()` before dereferencing

### Capture (P1)
- **ForceCapture state**: Explicitly sets state to Claimed after `SetOwningFaction` (was stuck Contested)
- **ContestingFaction tracks leader**: Updated by `EvaluateCaptureState` based on highest progress, not last-registered attacker
- **EvaluateCaptureState safety**: Re-fetches State pointer after cleanup to handle potential reentrancy

### Delegate Cleanup (P1)
- **Ledger trim moved**: `TransactionLedger` trimming runs once after all factions processed, not per-faction
- **Registry unbind**: EconomySubsystem unbinds per-territory `OnTerritoryOwnershipChanged` delegates on `Deinitialize`

### Performance (P2)
- **Spatial index Remove()**: Now uses reverse map `TerritoryToCells` to remove only from occupied cells (O(cells occupied) instead of O(all cells))
- **Tag matching direction**: `GetParentCity()` and `GetOwningDistrict()` now use `MatchesTag` with correct child→parent direction (was backwards)

### Testing
- Added 6 new contract tests for v0.2.1 API surface: `DiplomacySubsystemExtended`, `DiplomacyEventTypeExtended`, `EconomySubsystemExtended`, `VolumeExtended`, `BTAbortTask`, `DebugWidgetExtended`
- Total: **47 automation test suites** (up from 41)

### Documentation
- Updated 8 stale docs: `14_API_Reference` (18 fixes), `09_Map_Navigation`, `12_Blueprint_Reference`, `03_Core_Actors`, `04_Subsystems`, `01_Quick_Start`, `Blueprint_Setup_Tutorial`, `README`
- All docs now reflect v0.2.1 API surface
- **Stale map keys**: `CleanupStaleTerritoryKeys` removes dead territory entries from SlotMap

### Hierarchy (P1/P2)
- **Empty district protected**: `AllPropertiesOwnedBy` returns false for empty property list (was trivially true)
- **Contested clears owner**: `SetTerritoryState(Contested)` clears `OwningFaction` so `IsOwnedByFaction` and `GetOwningFaction` agree. *Correction (v0.2.3): SetTerritoryState(Contested) now preserves the incumbent owner until capture completes.*
- **Cascade single event**: `CascadeCaptureToProperties` no longer double-fires `OnPropertyCaptured`
- **Property upgrade reset**: Uses `SetUpgradeLevel(0)` instead of direct assignment (triggers income recalc)

### Combat (P1/P2)
- **BTTask_Release targeted**: Reads `TerritoryKey` from blackboard for per-territory slot release, fallback to `ReleaseAllSlots`
- **BTTask_Request validates**: Fails on unconfigured `bPermissionGrantedKey` instead of silent success

### Tales (P1)
- **Server authority**: `TerritoryCaptureEvent`, `TerritoryLockEvent`, `TerritoryUnlockEvent` skip on `NM_Client`
- **Capture fallback**: Logs warning when ControlSubsystem is unavailable instead of silent direct `SetOwningFaction`

### Registry (P1)
- **Client timer removed**: `PollBoundsChanges` timer only starts on server (`NM_Client` guard added)

### UI (P2)
- **DebugWidget throttled**: Rebuilds every 0.5s instead of every frame; caches subsystem pointers
- **Switch default**: `ETerritoryState` switch adds `default: "Unknown"` case

### Navigation (P2)
- **MapMarker outline**: Uses `BoxCenter` as reference (handles offset BoxComponents correctly)
- **Double bind removed**: `TerritoryNavigationMarkerComponent` no longer binds delegates that the marker already binds

### Validator (P2)
- **WorldState/SavableData**: Individual asset validation now checks GUID validity

### Build (P2)
- **Private dependencies**: `NarrativeArsenal`, `NarrativeSaveSystem`, `DeveloperSettings` moved from Public to Private

### Documentation
- **Blueprint Extension Guide**: Complete rewrite with Super-call quick reference table, all delegates with fire context, interface reference, state model diagram, and common patterns

---

## v0.2.0 — 2026-07-23

Comprehensive audit-driven stabilization release. 32+ fixes across capture, guards, hierarchy, economy, diplomacy, replication, and Narrative Pro integration.

### Breaking Changes
- `OnGuardDied` delegate renamed to `OnGuardKilled` with new signature `(Territory, Guard, Killer, RemainingDefenders)`
- `OnTerritoryControlChanged` delegate renamed to `OnTerritoryOwnershipChanged`
- `OnTerritoryStateChanged` delegate renamed to `OnTerritoryStateChangedDelegate`
- `RequestAttackPermission` → `RequestAssaultSlot` on TerritoryCombatDirector
- `ReleaseAttackPermission` → `ReleaseAssaultSlot`
- `HasAttackPermission` → `HasAssaultSlot`
- `GetGrantedPermissions` → `GetGrantedSlots`
- Removed `GuardBehaviorTree` and `GuardBlackboardAsset` properties from TerritoryVolume
- Removed custom BT tasks: `BTTask_MoveToPatrolNode`, `BTService_UpdatePatrolRoute`, `BTTask_SetPatrolPoint`
- `SetTerritorySaveGUID` and `SetOwningTerritoryGUID` replaced by `ConfigureTerritorySpawn()`
- District capture policy changed from mixed majority+unanimity to **unanimity only**
- `AutoMapMarkerComponent` replaced by proper `MapMarkerComponent` default subobject

### Capture System
- **Capture flow fixed**: `RegisterAttacker` → progress accumulates → `CompleteCapture` → ownership transfer
- **Deferred command list**: evaluate-then-apply pattern prevents TMap mutation during iteration
- **Identity-based attackers**: `TSet<TWeakObjectPtr<AActor>>` per faction — no count inflation
- **Deterministic tie-breaking**: progress → attacker count → lexicographic tag
- **ContestingFaction maintained**: set on register, cleared on reset/complete
- **AddCaptureProgress validates** via `AttemptCapture` before adding progress
- **Capture decay restores state**: decayed territories return to Claimed/Unclaimed, not stuck Contested
- **No auto-capture**: removed `PollPlayerPresence` — capture is designer-driven only
- **Late binding**: `TerritoryCaptureTask` subscribes to `OnTerritoryRegistered` for streaming recovery

### Guard Spawn Contract
- **`ConfigureTerritorySpawn()`**: single entrypoint sets `FNPCSpawnParams` before `SetNPCDefinition`
  - `bOverride_DefaultFactions = true` → exact territory owner faction only
  - Optional activity configuration override
  - Optional trigger set overrides
- **Faction set before `FinishSpawningActor`** — Narrative's BeginPlay reads correct faction
- **`FactionOverride`** on spawn points now applied (precedence: SpawnPoint > Territory Owner)
- **`HasAvailableSlot()` only** for initial population — reserves not consumed as active capacity
- **`SpawnSingleGuard()`** — one-for-one reserve replacement, not full batch
- **`LoadSynchronous()`** for NPC class — no silent fallback to base C++ class
- **Late binding**: spawn points subscribe to `OnTerritoryRegistered` when initial resolution fails

### Hierarchy
- **Unanimity policy**: district captures ONLY when ALL properties owned by one faction
- **District goes Contested** when owner loses any property
- **City goes Contested** when not all districts owned by one faction
- **`SetOwningFaction` called** on district majority capture (was missing)
- **Property side effects** run on every ownership path via `OnOwnershipChanged_Implementation` override
- **Authority guards** on all hierarchy handlers — no client-side mutations

### Lock System
- **`LockConditions`** array (`UNarrativeCondition` instanced) for quest-gated unlocks
- **`LockTerritory`/`TryUnlock`/`CanUnlock`/`IsLocked`** Blueprint API
- **`LockReason`** in replicated `FTerritoryOwnershipData` — visible on clients and in saves
- **`bStartsLocked`** overrides initial owner — locked territories stay locked regardless
- **`TerritoryLockEvent`/`TerritoryUnlockEvent`** — drop into quest/dialogue nodes

### Replication & Save
- **`OnRep_OwnershipData`** diffs `PreviousOwningFaction` and `PreviousState` — no false events
- **Client BlueprintNativeEvent parity** — `OnOwnershipChanged`/`OnStateChanged` fire on clients
- **`GetDefenderCount`** returns replicated `OwnershipData.DefenderCount` — correct on clients
- **Property `BeginPlay`** preserves saved ownership — only syncs to district on first init
- **`NavigationMarkerComponent`** is proper `CreateDefaultSubobject` — visible in BP Components panel

### Economy
- **Leaf-only income**: only `ATerritoryProperty` contributes — no hierarchy double-counting
- **Sequential ledger**: income applied first, then upkeep — each with own running balance
- **Insufficient gold clamps** upkeep to available — logs "partial — insufficient gold"

### Authority Enforcement
- All diplomacy mutation APIs check `GetAuthGameMode()`: `DeclareWar`, `DeclarePeace`, `FormAlliance`, `BreakAlliance`, `SignTradeAgreement`, `AddReputation`, `SetReputation`
- All territory mutations check `HasAuthority()`: `SetOwningFaction`, `SetTerritoryState`, `LockTerritory`, `TryUnlock`

### Map Markers
- **Red/Yellow/Green** defaults: unclaimed=red, enemy=red, player=green (via FactionColorMap), contested=yellow
- **Locked = invisible** (zero alpha)
- **Map outline** uses Narrative's coordinate system — respects rotation, works at any zoom
- All colors `BlueprintReadWrite` — fully customizable

### Spatial Index
- **`GetTerritoryAtLocation`** returns smallest-volume territory (most specific)
- **Auto-update on move/resize**: 2s poll compares cached bounds, re-indexes on change
- **`UpdateTerritoryBounds()`** BlueprintCallable

### Patrol Data
- **`GetPatrolRouteAsTransforms()`** and **`GetPatrolWaitTimes()`** — bridge territory patrol data into Narrative's `Goal_Patrol.PatrolPoints` format
- `FactionOverride` actively applied during spawn
- Debug visualization unchanged

### Combat Director
- Renamed to clarify **strategic assault budget** vs Narrative's per-target attack tokens
- `RequestAssaultSlot`/`ReleaseAssaultSlot`/`HasAssaultSlot`/`GetGrantedSlots`
- Header documents the two-system approach

### Tales Integration
- **`TerritoryCaptureEvent`**: completes capture via `AddCaptureProgress(1.0)` when non-forced
- **`TerritoryLockEvent`/`TerritoryUnlockEvent`**: new event subclasses
- **`TerritoryCaptureTask`**: late binding via `OnTerritoryRegistered`
- **Validator warnings emitted**: `[WARNING]` prefix in Data Validation output

### NavMesh
- Guard spawn positions projected to NavMesh via `ProjectPointToNavigation`
- `GetRandomSpawnPoint` respects territory actor rotation

### Cleanup
- Removed `OnGuardTakeAnyDamage` — was breaking knockback, ragdoll, hit reactions
- Removed `PlayerFactionFallback` — auto-capture removed
- Removed `ATerritorySavableData` deprecation metadata (UE requires ADEPRECATED_ prefix)
- `GetActorLabel()` → `GetName()` in runtime logs
- Debug draw bounds respect rotation
- Plugin version: `0.2.0`

### Documentation
- New: `Docs/Blueprint_Setup_Tutorial.md` — complete step-by-step setup guide
