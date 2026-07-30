# TerritoryFramework Deep Reaudit — 2026-07-30

## Scope and boundary

This audit compared TerritoryFramework source, editor validation, tests, Blueprint-facing APIs, project assets through Unreal/Monolith MCP, and the installed Narrative Pro 2.3.3 public source/API surface.

Narrative Pro is a read-only foundation and monthly vendor dependency. No Narrative Pro source or asset is part of this change. Production access to Narrative's enumerable faction relationship representation is isolated in `FTerritoryNarrativeProAdapter`; all other integration uses public Narrative classes/functions/delegates.

## Authority map

| State or behavior | Authority used after audit |
|---|---|
| Territory owner/state/progress/desired guard count | `ATerritoryVolume` |
| Capture validation, participants, progress | `UTerritoryControlSubsystem` |
| Registration, stable-GUID/tag/spatial lookup | `UTerritoryRegistrySubsystem` |
| Player/NPC currency | Narrative inventory component |
| Income/cost rates and transaction projection | `UTerritoryEconomySubsystem` |
| Rich treaties/reputation | `UTerritoryDiplomacySubsystem` |
| Combat attitude | Narrative GameState through Territory adapter |
| Persistent and late-join projection | `ATerritoryWorldState` |
| Strategic assault schedule and finite force | `UTerritoryCounterAttackSubsystem` |
| Strategic per-territory attack slots | `UTerritoryCombatDirector` |
| Tactical per-target attack tokens | Narrative Pro |

No second capture, faction, wallet, save, navigation, map, HUD stack, AI controller, or Behavior Tree authority was added.

## Confirmed defects resolved

### Capture, guards, and Tales

- Dead Narrative ASC attackers could remain in capture sets and continue pressure. Capture registrations now own one reference-counted death binding per actor and remove that actor from every capture immediately.
- Capture completion chained setters and could announce success after a partial/rejected transition. It now uses one `FTerritoryMutationRequest`, verifies the final owner/state, clears participants after commit, and broadcasts one result.
- Capture/lock conditions selected `GetFirstPlayerController`, producing the wrong pawn/Tales context in multiplayer. `FTerritoryTransitionContext` now flows from the actual participant; context-aware lock/unlock entry points were added.
- Killing the last guard directly cleared ownership. Zero defence now leaves the incumbent owner intact and vulnerable; ownership changes only through capture/mutation authority.
- Manual guard removal could report success after removing fewer pawns and did not distinguish reserve policy. It now validates active count, tracks actual removals, updates desired count by the committed amount, and returns exact success.
- Ownership transitions reset spawn-point state after new guards were registered. Spawn-point transition policy now runs before spawning the new owner's guards.
- Guard save/load recreated a full roster, losing finite casualties. Guard posts save active counts and reserve state; restore reconstructs only saved survivors. Fresh territories still deploy the authored desired count, and referenced posts are hydrated before territory reconciliation to remove BeginPlay-order dependence.
- PIE duplication regenerated persistent guard-post GUIDs. Only normal editor duplication now creates a new GUID.

### Replication and player RPC lifecycle

- Client economy/diplomacy subsystem queries could remain empty because replicated WorldState arrays had no hydration path. RepNotify handlers now restore client read models.
- A static management-point PostLogin hook crossed world/PIE lifetime boundaries and depended on at least one placed management point. `UTerritoryPlayerManagementSubsystem` now installs one replicated RPC component on every authoritative player controller.
- Clients could create a competing local RPC component. Dynamic component creation is server-only and forces owner replication.
- First requests could be throttled at time zero and cooldown rejection was silent. Time starts at negative infinity and every rejection produces a result.

### Economy and diplomacy

- Loaded Narrative NPC inventories were excluded from faction payouts/upkeep. Loaded player and NPC members now participate in deterministic order.
- `FactionLeader` and `SharedNarrativeAccount` lacked real routing. Server-only registration maps now route to explicit live Narrative inventory accounts and fall back to deterministic member distribution; pointers are runtime-only and never saved.
- Client/fake faction data could label a debit/credit for the wrong faction. Currency mutations and account registration now require the Narrative team identity to contain the exact faction and require the same world.
- Currency/rate arithmetic could overflow `int32`. Rate totals and affordability totals use `int64`; credits reject balance overflow and member distribution respects each account's remaining capacity.
- Direct access to Narrative `FactionAllianceMap` was spread through diplomacy code. It is isolated behind `FTerritoryNarrativeProAdapter` for monthly upgrade review.
- External Narrative attitudes were imported as expiring treaties despite carrying no expiry. They now import as permanent until another explicit change.
- Friendly round-trip could lose semantics. Friendly maps to Alliance when no richer compatible Territory treaty exists; compatible Alliance/Trade/NonAggression metadata remains intact.

### Counterattack lifecycle (previously missing)

- Added deterministic scheduling with saved decision seed/result/roll and separate launch versus estimated-success values.
- Added grace, evaluation, warning, proximity wait, one-time physical activation, finite waves/casualties, capture success, cancellation, and defeat/recovery states.
- Added typed attack approaches, navigation validation, faction/territory/global budgets, and multi-approach selection for stronger attacks.
- Added physical `ATerritoryAssaultCharacter` Narrative NPCs with `UNPCDefinition`, spawn info, optional activity configuration/TriggerSets, interruptible `UNPCActivity`, and `UNPCGoalItem`.
- A warning/waiting record owns zero pawns and creates zero capture pressure. Relevant player proximity is required before activation and state commits before spawning, preventing duplicate forces from multiple players.
- Each participant owns one `AssaultID`; death/withdrawal removes pressure exactly once and permanently consumes finite force. Resolution commits all counts before one final broadcast.
- Peace/friendly policy, invalid route/config, locked target, third-party ownership, exhausted force, and budget failure paths are explicit.
- WorldState saves/replicates records without live pointers. Active survivors become pending finite reconstruction on load; World Partition resolution waits for stable GUID/tag registration.

### Validation and monthly-update safety

- Validator iteration was level-only and missed unloaded World Partition actors. Relevant descriptors are pinned for validation and all loaded levels are inspected.
- Stable GUID collision checks now cover territories, guard spawn points, WorldState, and deprecated SavableData.
- Hierarchy validation now detects cycles of any length, not only self-parenting.
- Guard and counterattack configuration validates Narrative NPC subclass contracts, duplicate factions/approaches, finite force/wave values, profile presence, and enabled ingress.
- Native CommonUI fallback no longer hard-fails on one embedded Narrative widget path. The class is a soft developer setting; if unavailable, the row remains read-only and project widgets can provide styling.

### Narrative CommonUI operations

- The journal cached only district count and filter text, leaving existing rows stale when guards, finance, capture, locks, or assaults changed. A full operations revision now invalidates every displayed authority.
- Guard eligibility and Narrative funds were queried with the player controller in several UI paths. Viewer identity and balance now resolve through the owning pawn's Narrative faction/inventory.
- The management point manually used `AddToViewport`, input mode, cursor state, and `RemoveFromParent`, bypassing the Narrative CommonUI stack. Territory activatable screens now push into the Narrative gameplay HUD's registered layer.
- `BindToTerritoryAtPlayer` retained stale data when no territory was found. It now unbinds and clears the display.
- Added viewer-relative district/economy read models, operational filters, exact disabled reasons, captured/unlocked lists, finance/loss views, guard add/remove controls, assault force/threat summaries, and targeted HUD warnings.
- Reserve and attacker counts are labelled unknown when their server-side detail is unavailable; the client UI no longer treats a missing server map as an authoritative zero.
- Narrative Pro's `UNarrativeMenu` has a private native constructor. Territory C++ screens therefore derive from the supported `UNarrativeActivatableWidget`; existing Blueprint wrappers may still subclass Narrative's menu Blueprint. No vendor source change is required.
- Scoped MCP repaired activation focus in the existing player/journal wrappers, added deterministic keyboard/gamepad navigation and filter tooltips, applied Narrative CommonText accessibility styles, and compiled all eight Territory widget assets without warnings.
- Native UI tests cover CommonUI inheritance/API contracts, viewer filters, and live revision invalidation.

## Counterattack transition trace

| Transition | Authored by | Persisted/replicated by | Test/presentation |
|---|---|---|---|
| Capture -> grace | Control delegate -> CounterAttack subsystem | Assault record -> WorldState | query/delegate |
| Grace -> evaluation | Server timer using Narrative accumulated time | seed, cycle, inputs/results | determinism and monotonicity tests |
| Evaluation -> warning | probability plus hard rules | state/roll/approaches/finite force | targeted management-component RPC |
| Warning -> proximity wait | Server scheduler | state + notification marker | zero-live-force regression |
| Proximity -> active | Relevant-player radius + route revalidation | activation time/state | state commits before spawn |
| Active -> capture | Physical Narrative NPC registration -> existing Control subsystem | Territory ownership + assault success | no direct owner setter |
| Active -> defeated | Exact-once deaths/withdrawals and zero remaining force | casualty counts/resolution | finite-force/save tests |
| Cancellation | Diplomacy, lock, invalid config/route, ownership | terminal resolution | one atomic final record |
| Load/late join | WorldState -> CounterAttack subsystem | SaveGame + RepNotify | active survivors reconstructed as pending |

## Blueprint migration

- Prefer `TryRegisterAttacker` when the caller needs admission success; legacy `RegisterAttacker` delegates to it.
- For condition-sensitive lock operations, migrate to `LockTerritoryWithContext`, `TryUnlockWithContext`, and `CanUnlockWithContext`. Context-free functions remain deliberate world-level operations.
- Register shared/leader Narrative accounts with `RegisterFactionCurrencyAccount` after spawn/stream and unregister on removal. Do not save those pointers.
- Configure `DefaultNarrativeButtonClass` in Territory Framework settings if the installed Narrative package moves its default widget.
- Existing territories do not counterattack until a profile and valid approaches are assigned. This is opt-in and does not migrate ownership.
- Old guard saves without per-post active counts use the saved aggregate defender count, bounded by desired guards. New saves preserve per-post active counts and reserves.
- Open interactive Territory screens with `UTerritoryUIBlueprintLibrary::OpenTerritoryMenu`; remove project Blueprint calls that add those screens directly to the viewport or manually set UI input mode.
- Use `DesiredFocusTargetName` for new Territory activatable widgets. `InitialFocusWidgetName` remains a compatibility fallback for existing assets.
- Existing project rows may bind directly to `FTerritoryDistrictOperationsView`; guard commands must continue through the player-management component.

## False positives and intentional limits

- Territory CombatDirector slots do not duplicate Narrative attack tokens: they budget per-territory strategic participation; Narrative tokens budget attackers per tactical target.
- Runtime-added assault activity/goal objects are adapters into Narrative activity ownership, not a parallel AI framework.
- `ReplicatedCaptureSummaries` is a client projection only; `ATerritoryVolume::OwnershipData` remains the durable authority.
- Live attacker and guard pawn pointers are intentionally not saved.
- Offscreen assault simulation remains disabled until physical multiplayer behavior is proven.
- Final CommonUI styling remains project-widget responsibility.
- Static focus audits report bidirectional navigation cycles because reverse links return to the previous control. Those cycles are intentional; all concrete interactive Territory controls are reachable with zero dead ends or dangling targets.

## Verification and unresolved release gates

Verified on UE 5.7 on 2026-07-30:

- UHT plus `TDAEditor Win64 Development` runtime/editor compilation succeeded.
- All 77 `TerritoryFramework.*` automation tests passed, including the three operations-UI tests.
- Monolith MCP found and compiled all 29 Blueprint/WidgetBlueprint assets under `/Game/TerritoryFramework`; every asset returned zero errors and zero warnings.
- Unreal Data Validation, invoked through MCP against the exact `/Game/TerritoryFramework` asset set, checked 42/42 assets: 42 valid, zero warnings, zero invalid, zero unable-to-validate.
- Scoped MCP compiled the nine Territory widget assets with zero errors/warnings. CommonUI and accessibility audits reported zero issues; the interactive screens had zero unreachable controls, dead ends, or dangling explicit focus targets.
- MCP AI validation checked the four scoped BT/EQS-classified AI assets with zero issues.
- Project UHT reflection was rebuilt through MCP and confirmed the counterattack subsystem/profile, Narrative goal/activity, assault character/participant, player-management subsystem, WorldState RepNotify functions, and replicated properties.
- MCP source verification confirmed the referenced public Narrative classes and `USaveSystemStatics::LoadSingleActor` are present, includable, and not deprecated.
- MCP network audit reported no TerritoryFramework `ReplicatedUsing` property missing its matching RepNotify.
- A `TDAServer Win64 Development` build was attempted, but Epic's installed UE 5.7 distribution rejected the target before compilation with `Server targets are not currently supported from this engine distribution`. This is an environment/toolchain gate, not a TerritoryFramework compiler error.
- A scoped Windows cook was attempted. It produced no TerritoryFramework errors but remained in UE global Vulkan shader compilation for more than ten minutes without reaching plugin asset cooking, so the process and its remaining shader worker were stopped. The partial output under `Saved/Cooked/Windows` is derived data and can be overwritten by the next cook; the cook gate remains inconclusive.

The attached `ParentAnimClass->GetSparseClassDataStruct()` assertion originates in `MonolithIndex` loading an AnimBlueprint during broad asset indexing. It is not a TerritoryFramework native or scoped Blueprint compile failure. Monolith remains disabled in `TDA.uproject`; this audit enabled it only with a temporary editor command-line override and deliberately used scoped asset validation plus project-UHT reflection refresh instead of broad reindexing.

The MCP module-dependency heuristic misattributes Narrative public classes to the vendor plugin directory name and even classifies `UPROPERTY` as a Monolith symbol. Those findings are false positives; UBT is authoritative and the runtime/editor non-unity build succeeded with explicit `NarrativeArsenal`, `NarrativeCommonUI`, and `NarrativeSaveSystem` dependencies.

The following require an actual game session or content-owner decision and must not be inferred from reflection tests:

- dedicated-server plus two-client PIE assault activation/casualty/capture;
- World Partition stream-out/stream-in during an active physical assault;
- representative Narrative NPC definition/activity/TriggerSet asset configuration;
- cook/package smoke test for the project platform set;
- remediation of vendor/project GameplayTag warnings and unrelated third-party content validation failures.

See `17_Counterattack_System.md` for configuration and operational behavior.
