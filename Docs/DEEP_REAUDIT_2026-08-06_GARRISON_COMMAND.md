# Deep Reaudit — Garrison Command and District P&L

Date: 2026-08-06

Scope: TerritoryFramework capture transitions, hierarchy cascades, guard lifecycle, Narrative NPC/faction/inventory/Tales integration, economy/read models, save/replication, the District Command Center and all related Territory widgets, plus documentation and MCP validation. Narrative Pro source was inspected as the foundation and was not modified.

## Executive result

The reported “capture always creates three friendlies and removes 150 net” behavior had multiple causes, not one widget bug. The Blacksmith Property authored `GuardSpawnCount=3` and `GuardCost=50`; hierarchy callbacks and the project Blueprint's direct `ForceCapture` call could each lose player context, while the UI managed the zero-capacity District container. The economy then correctly projected `3 × 50 = 150` recurring upkeep, but recruitment and upkeep also shared one price concept, making the result look like an unavoidable purchase.

The implemented flow now starts a physical player capture at zero, lets the player select each District/Property garrison, previews and applies an absolute target, separates one-time recruitment from recurring upkeep, aggregates Property profit/loss, and replicates an exact garrison/economy read model.

## Confirmed defects and disposition

| Severity | Confirmed defect | Root cause | Disposition |
|---|---|---|---|
| P0 | Player District capture still produced three child-Property guards | City/District/Property ownership callbacks lost `FTerritoryTransitionContext` and used a context-free force setter | Fixed: explicit context survives the synchronous hierarchy cascade; `PlayerChooses` resolves target 0 |
| P0 | Forced Tales player capture could still produce three guards | `UTerritoryCaptureEvent` switched to context-free `ForceCapture` | Fixed: normal and force modes submit the same contextual atomic request |
| P0 | Blacksmith Blueprint capture still produced three guards after the Tales/hierarchy fix | `BP_Property_Blacksmith` calls the legacy control-subsystem `ForceCapture` node directly, which constructed an empty transition context | Fixed: the compatibility node resolves a matching live Narrative controller by faction; exact callers can use `ForceCaptureWithContext` |
| P1 | A mismatched explicit transition context could suppress an AI faction's authored garrison | `ForceCaptureWithContext` normalized `RequestingFaction`, while `PlayerChooses` previously checked only for a non-null controller | Fixed: controller/pawn/player-state Narrative membership must exactly match the new owner faction |
| P1 | Territory data validation appeared registered but silently skipped Territory assets | The validator overrode UE's deprecated object-only methods without the deprecated use-case gate, so UE 5.7 never entered its validation path | Fixed: migrated to `FAssetData`/`FDataValidationContext`; a native regression proves valid and invalid counterattack profiles execute the validator |
| P1 | Long campaigns could reuse an old counterattack decision seed after bounded history trimming | The next evaluation cycle was derived only from assault records still retained in memory/save data | Fixed: WorldState saves a server-only per-territory/faction high-water ledger; old saves rebuild from retained records |
| P0 | Documented force capture did not bypass locks | `ApplyTerritoryMutation` had no explicit lock-bypass field | Fixed: safe-default `bBypassLock=false`; only force paths opt in |
| P0 | Journal commands targeted the District instead of the Blacksmith garrison | The command model exposed only `ATerritoryDistrict`; Market Square has zero guard capacity while its Property owns the spawn configuration | Fixed: operations view exposes District plus registered child Properties, with a selected exact target |
| P0 | Guard price conflated acquisition and recurring loss | `GuardCost` was used for both staffing and economy upkeep | Fixed: persisted `GuardRecruitmentCost` is one-time; `GuardCost` is recurring upkeep |
| P1 | Client/late-join UI could not know exact active/reserve/pending guards | Live NPC pointers and server maps were not a valid client read model | Fixed: `FTerritoryGarrisonSnapshot` replicates active/desired/max/reserve/pending counts |
| P1 | Desired-only changes could miss the garrison event | The snapshot omitted desired/max, so 3→2 with zero live guards compared equal | Fixed: desired/max participate in snapshot equality and `OnGarrisonChanged` |
| P1 | A player could not lower a target after every assigned guard died | validation required live removable guards and later required a definition for any nonzero result | Fixed: target reduction is independent of live pawns and requires a definition only for recruitment |
| P1 | Multi-guard purchase could charge/commit partially | delta commands spawned iteratively without an all-or-nothing final invariant | Fixed: absolute mutation plans once, debits once, deploys completely, or removes request-created guards and refunds |
| P1 | Reserve replacement could fight a lowered target | death queues and pending timers did not consistently compare active against desired | Fixed: replacement queues only below target; lowering cancels pending deployments |
| P1 | Client finance rates could remain stale between payout ticks | economy rate recalculation was server-only and WorldState publication was delayed | Fixed: recalculation immediately republishes the existing replicated WorldState snapshot and forces a net update |
| P2 | Live refresh could close/reset an open garrison selector | both command widgets cleared/rebuilt combo options every poll | Fixed: option lists rebuild only when the target set changes |
| P2 | Documentation still promised automatic player guard respawn and exposed a removed mutation field | older setup/AI/extension/API text described the legacy path | Fixed across Quick Start, Core Actors, Guard, Economy, Save, API, AI, District, Operations UI, Blueprint guides, changelog, and the complete flow document |
| P0 | Configured counterattacks could never physically activate | `DA_CounterAttack` referenced `NPC_TerritoryBandit`, whose class derives from `ATerritoryGuardCharacter`, while runtime requires `ATerritoryAssaultCharacter` | Fixed with a separate assault NPC definition; shared Narrative configuration remains reusable |
| P0 | The only Blacksmith approach always failed route admission | its ID was blank and its relative transform resolved outside the navmesh | Fixed: stable `Blacksmith_WestRoad` ID and a verified full 1,814 cm nav path |
| P1 | Seven durable Blacksmith posts had zero GUIDs | runtime generated new GUIDs, making saved reserve identity unstable | Fixed: seven editor-authored GUIDs baked into the map; runtime now logs/skips persistence instead of inventing identity |
| P1 | Counterattack records could remain forever in waiting/active states | profile removal, target locking/unclaiming, and repeated collision-blocked spawns had no complete terminal policy | Fixed: all phases revalidate target/configuration; zero-spawn retries are saved, reset on success, and bounded |
| P1 | `IsAssaultActive` reported grace/warning records as active | query returned every non-terminal state | Fixed: physical Active only; new `IsAssaultPendingOrActive` preserves the strategic query |
| P1 | Defending multi-faction players could miss warnings/activation | relevance compared only `GetActorPrimaryFaction` | Fixed: Narrative faction membership is checked on pawn/controller, not list order |

## Implemented command surface

### Follow-up runtime acceptance correction

The first in-Editor retest was performed in an Editor process started before the fixed
module DLL existed, so its observed three-guard spawn came from stale loaded code. The
follow-up nevertheless removed the remaining context dependency: `PlayerChooses` now
recognizes the committed owner as player-controlled through exact membership on every
live Narrative controller/pawn/player-state. AI owners retain configured staffing.

Counterattack admission now checks diplomacy before scoring every configured finite
force and schedules the strongest eligible faction. Local defence cascades across the
same-owner District/Property front; its zero-active-guard policy defaults to a 100%
launch decision after grace. Ownership still requires the existing physical capture
lifecycle.

The Journal now treats locked registered Districts as selectable intel rather than
hiding them from its primary queue. Child Property contests, assaults, garrisons,
profits/losses, Property alignment, exact attack target, strongest eligible attacker,
launch/success probabilities, defence, power ratio, and priority cascade into the
selected District view. Locked/unowned selection stays read-only.

`UTerritoryJournalWidget` and `UTerritoryDistrictManagementWidget` now supply:

- District/Property garrison selection;
- active, assigned, maximum, reserve, and pending counts;
- exact integer staffing target;
- Apply, Set 0, Set Max, `+1/-1/+5/-5` actions;
- one-time recruitment preview;
- local and District aggregate income, upkeep, and net yield;
- owning-client structured success/failure status.

The journal uses a remote owned-target RPC. The in-world screen uses the same authority but also verifies the selected target belongs to the managed District and that the pawn remains in range.

## Authority, save, replication, and migration

- `ATerritoryVolume` remains the single owner of owner/state/desired garrison data.
- `UTerritoryControlSubsystem` remains the capture validator and atomic transition path.
- `UTerritoryEconomySubsystem` remains the rate authority; Narrative inventory remains real player currency.
- `ATerritoryWorldState` remains the replicated global economy/late-join authority.
- Server RPC validation checks request ordering, cooldown, bounds, ownership faction, claimed state, capacity, target membership, and (for local management) range.
- Desired count and both guard prices persist in `OwnershipData`; active/reserve/pending restoration stays with stable Territory/spawn-point GUID records.
- Old saves keep their legacy desired count, including 3. Missing recruitment price migrates from the Blueprint default, and an owner can set the legacy target to 0.
- No Narrative Pro source file was modified. The focused project integration updates are `HopDistrictTest.umap`, `DA_CounterAttack`, and the new `NPC_TerritoryBanditAssault`; the project-styled widgets keep ownership of their visual layout while their C++ parents add the command controls to the existing stacks.

## MCP evidence

Monolith 0.22.0 reported Unreal 5.7 and compiled all eight `/Game/TerritoryFramework/UI` assets as `up_to_date`, with zero compile errors and zero warnings:

- `BP_TerritoryDebugWidget`
- `W_TerritoryPlayerMenu`
- `WBP_HopTerritoryJournalWidget`
- `WBP_MainHopTerritoryJornal`
- `WBP_TerritoryCommandRow`
- `WBP_TerritoryDistrictManagement`
- `WBP_TerritoryEconomyWidget`
- `WBP_TerritoryMenuBase`

Accessibility and CommonUI audits returned zero issues for all eight. Tree inspection confirmed the 129-widget journal still contains `CommandPanelScroll -> CommandStack`, the management widget contains `ManagementStack`, the command row keeps its selection/add/remove controls, and the Narrative wrapper embeds the journal. MCP reported zero dirty packages under `/Game/TerritoryFramework/UI` after the read-only audit.

The follow-up integration pass also confirmed seven unique valid Blacksmith post GUIDs,
`Blacksmith_WestRoad`, a dedicated assault NPC class path, zero errored Blueprints,
zero dirty packages after scoped saves, and a full non-partial navmesh path from the
approach to the Property center.

## Verification status

Completed:

- after closing the Editor, the old TerritoryFramework `Binaries` and `Intermediate`
  directories were moved to the recoverable
  `Saved/TerritoryFrameworkOldBuildArtifacts_20260806_2230` backup and a clean,
  unsuffixed project-directory Runtime/Editor build completed with UHT and link success;
- Cargo and Monolith were disabled only for the clean build and automation process, then
  restored to enabled. The clean build had removed Monolith's generated module manifest;
  recreating it for the current project BuildId restored all 20 existing Monolith modules,
  confirmed by a full headless startup through MCP server initialization;
- all 84 final `TerritoryFramework` automation tests passed with zero failures, zero
  not-run tests, and automation exit code 0;
- the focused player-managed garrison regression passed;
- `git diff --check` reports no whitespace errors;
- MCP Blueprint compile/accessibility/CommonUI/tree/dirty-package checks passed.
- scoped Unreal data validation checked seven changed assets with zero invalid, warning, skipped, or unable-to-validate results; the Territory validator itself executed for the map and counterattack profile.

Still required for release sign-off:

- dedicated-server plus two-client PIE for remote journal staffing and late join;
- real Narrative save/reload with World Partition streaming order;
- cook/package smoke test (the project Cargo plugin currently prevents the ordinary game target from being built with a command-line disable override);
- gameplay validation with the actual Blacksmith NPC definition, navigation placement, and Narrative inventory item/currency configuration.

These are environment/integration gates, not unimplemented command-flow stubs. They are not claimed as passed.

## Deliberate limitations

- Remote journal management accepts currently loaded, registered owned targets. An unloaded World Partition Property becomes manageable after registration.
- Increasing a target fails completely if every requested Narrative NPC cannot be placed; it never silently buys fewer guards.
- Lowering a target gives no recruitment refund; it reduces future upkeep immediately.
- Guard death never rolls ownership. The existing capture subsystem remains the only real capture path.
- Offscreen counterattack capture remains disabled.

See [19_Garrison_Command_Flow.md](19_Garrison_Command_Flow.md) for the implemented end-to-end lifecycle and API ownership map.
