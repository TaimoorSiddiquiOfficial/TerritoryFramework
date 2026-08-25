# Territory Operations UI

## Purpose

The operations UI is the player-facing projection of the Territory authorities. It lists districts that are unlocked, available, owned, manageable, contested, threatened, or operating at a loss, and exposes guarded absolute staffing commands for District and child Property garrisons.

It does not own territory, capture, guard, economy, diplomacy, assault, or save state. Every displayed value is derived from the existing authority and every mutation still passes through the authoritative gameplay API.

## Narrative CommonUI integration

`UTerritoryActivatableWidget` derives from Narrative Pro's supported `UNarrativeActivatableWidget`. It uses Narrative menu input mode, back deactivation, activation focus, Narrative Common buttons, and the gameplay HUD's registered CommonUI layer containers.

Open a Territory screen with:

```text
UTerritoryUIBlueprintLibrary::OpenTerritoryMenu(
    PlayerController,
    TerritoryWidgetClass,
    UI.Layer.Menu)
```

Do not call `AddToViewport`, manually change input mode, or create a second activatable stack. `OpenTerritoryMenu` resolves the player's `UNarrativeGameplayHUD` and pushes the Territory widget into the requested registered Narrative layer.

The project controller selects `WBP_TerritoryGameplayHUD_Modular`. This asset is a project-owned duplicate of Narrative Pro's current `WBP_DefaultGameplayHUD`, with `WBP_TerritoryCaptureHUD` composed as a passive overlay. Its inherited template graph must retain the initialized registrations for `UI.Layer.Game -> GameStack`, `UI.Layer.Menu -> MenuStack`, and `UI.Layer.Modal -> ModalStack`. Rebuilding only the widget tree is invalid: `OpenMenu` then returns null, radial-menu close paths can dereference an unset menu, and CommonUI input routing no longer has a destination.

Narrative Pro's current `UNarrativeMenu` constructor is private, so it cannot be used as a native C++ parent without modifying vendor source. Territory native screens therefore use `UNarrativeActivatableWidget`. Project Blueprint menus reuse `WBP_TerritoryMenuBase`, the project-owned Narrative menu behavior template; both `W_TerritoryPlayerMenu` and `WBP_MainHopTerritoryJornal` inherit it. This keeps the monthly vendor plugin read-only.

## Read models

### `FTerritoryDistrictOperationsView`

`BuildDistrictOperationsView` returns a viewer-relative snapshot containing:

- identity, owner, viewer faction, contesting faction, and territory state;
- registration, lock, unlock, availability, capture eligibility, management eligibility, and exact failure reasons;
- capture progress and known active attacker count;
- aggregate active, desired, maximum, reserve, and pending guards;
- guard quality, fortification, allied support, strategic value, unguarded state, and
  counts for total/aligned Properties and manageable/empty garrison targets;
- Narrative account funds, district income, guard upkeep, net income, guard purchase cost, and financial-risk state;
- guard add/remove eligibility and exact failure reasons;
- a `GarrisonTargets` array with each District/Property's exact staffing, recruitment, upkeep, income, and net projection;
- `ProductionSites` and aggregated `ResourceFlows`, including active/blocked counts, exact item classes, stored quantities, per-cycle input/output/net, and per-rule failure reasons;
- District and child-Property assault state, exact leaf target, attacker faction,
  finite-force counts, selected approaches, launch probability, estimated success,
  attack priority, defence power, power ratio, and threat summary;
- a separately labelled strongest-eligible projected threat when no assault is scheduled.
  Projection is planning data and never claims physical attackers exist.

The struct is a read-only projection. Pointer fields are transient UI references and are never campaign save data.

### `FTerritoryEconomyOperationsView`

`BuildEconomyOperationsView` returns the viewer's available Narrative inventory funds plus the Territory economy projection: income, costs, net, deficit state, territory count, recent credits/debits, bounded transaction history, resource-storage availability, stockpile quantities, and modular production-site rows.

“Available funds” is not a Territory treasury. The Narrative inventory/account remains currency authority.

## Filters

`ETerritoryOperationsFilter` supports:

| Filter | Meaning |
|---|---|
| `All` | Every registered district |
| `Unlocked` | District is not locked |
| `Available` | Viewer can currently participate in capture |
| `Owned` | Viewer faction owns the claimed district |
| `Manageable` | Viewer owns it and satisfies management policy |
| `UnderAttack` | Physical contest, non-terminal scheduled assault, or eligible projected threat |
| `Contested` | Existing capture subsystem reports an active contest |
| `Locked` | Territory is locked |
| `FinancialRisk` | Guard upkeep exceeds district income |
| `Producing` | At least one production site is active or settled |
| `ProductionBlocked` | At least one site has missing input, unavailable/full storage, or an invalid profile |
| `MissingInputs` | At least one exact production rule lacks required Narrative items |
| `StorageFull` | At least one output cannot fit by slots or weight |

`GetDistrictOperationsRevision` hashes every displayed authority used by the supplied list. The journal rebuilds when guards, capture, finance, lock state, or assault state changes, fixing the former stale-row bug where only item count and filter text invalidated the list.

`DoesDistrictMatchSearch` applies case-insensitive AND-token matching across the complete player-facing projection: display name, stable Territory tag, owner, state, availability and lock reasons, threat/attacker data, and child garrison names/tags. The search field therefore filters the same rows that the directory actually renders rather than a separate count-only model.

## District Command Center

`WBP_HopTerritoryJournalWidget` is supplied as a responsive single-column command surface:

1. **Operations queues** show currently actionable Available/Unlocked Districts and,
   separately, captured Districts controlled by the viewer.
2. **District directory** exposes name, owner, state, and operations filters over every registered district projection.
3. **District command** shows owner/state, availability and lock reason, Property
   alignment, every local garrison, child-Property capture pressure, income/upkeep/net,
   strongest diplomacy-eligible attacker, exact target, defence/power ratio,
   grace/cooldown, finite force, probabilities, and approaches.

The command surface uses a fill-width shell with a 760-pixel desktop maximum. Below 800 pixels it fills the available width; at larger widths it remains centered. Operations queues, directory, and District command are stacked in the page scroll, while bounded list regions keep their own minimum heights. Long operational readouts use automatic wrapping, so the layout does not depend on a fixed 1920x1080 canvas or viewport scaling.

The action and ownership predicates remain strict, while visibility is broader:

```text
Available / Unlocked = registered AND unlocked AND currently available AND not owned by viewer
Captured / Owned     = registered AND owned by viewer AND state is not Unclaimed
Complete Directory   = every registered District (locked entries remain selectable)
```

An unlocked District that is diplomatically blocked, defended, or otherwise unavailable
is not counted as actionable. A registered locked District remains visible in the complete
directory, where selecting it is read-only: server management
remains disabled until exact ownership/claimed/capacity rules pass. An owned District
cannot duplicate into the Available queue. Counts derive from the same row predicates.

Threat and capture details cascade from loaded same-owner child Properties. A Blacksmith
assault therefore appears in Market Square even though the durable assault correctly
targets the capturable Property rather than the aggregate-only District.

Clicking a responsive selection-only row selects that district and opens its command details. The garrison planner selects the first manageable target with capacity (normally a child Property when the District is a zero-capacity container), navigates District/Property posts with Previous/Next controls, and exposes an integer target, capacity progress, projected recruitment/upkeep/net, and Apply/Empty/Full actions. List rows do not mutate guards; all staffing changes remain in this contextual command surface.

## Supplied widgets

| Widget | Role |
|---|---|
| `W_TerritoryPlayerMenu` | Existing Narrative player menu with the Territory journal tab and a valid activation focus target |
| `WBP_MainHopTerritoryJornal` | Narrative menu wrapper around the Territory journal; forwards activation focus to the inner widget |
| `WBP_HopTerritoryJournalWidget` | Responsive stacked District Command Center with actionable/owned queues, searchable directory, selected-district controls, finance ledger, and exposure report |
| `WBP_TerritoryCommandRow` | Responsive project-styled Narrative CommonUI selection row used by all journal lists |
| `WBP_TerritoryDistrictManagement` | In-world district command panel for guards, funds, income, production summary, availability, and threat status |
| `WBP_TerritoryEconomyWidget` | Faction economy health plus bounded scrolling stockpile and production-site modules |
| `WBP_TerritoryInfoWidget` | Passive current-territory status card with availability, threat, net income, and production status |
| `WBP_TerritoryCaptureHUD` | Compact capture progress and contesting-faction projection; it never creates capture progress |
| `WBP_TerritoryGameplayHUD_Modular` | Project-owned copy of Narrative's complete GameplayHUD template graph and tree, with the Territory capture HUD composed as a passive overlay |
| `WBP_TerritoryResourceRow` | Reusable stockpile/input/output/net resource row |
| `WBP_TerritoryProductionSiteRow` | Reusable production-site module that composes resource rows |
| `BP_TerritoryDebugWidget` | Scrollable live territory/counterattack diagnostic output |

Project styling can replace `DistrictRowWidgetClass`; the native fallback uses Narrative CommonUI controls and the same delegates.

`UTerritoryResourceRowWidget` and `UTerritoryProductionSiteRowWidget` are reusable, read-only modules. The economy base can populate optional `ResourceStockpileRows` and `ProductionSiteRows` containers, while any project Blueprint can consume the same structs through `OnEconomyOperationsUpdated`. Production rows compose resource rows; they never call settlement functions or own resource quantities.

The supplied Economy widget constrains both dynamic row collections in scrolling viewports. The journal operational selector presents all four production filters shown above.

## Project templates and styles

`WBP_TerritoryButton_Text` is a project-owned duplicate of Narrative Pro's `WBP_NarrativeButton_Text`. It keeps `UNarrativeCommonButtonBase` behavior while removing the unused input-action block and vendor click animation. `ButtonStyle_TerritoryPrimary` and its Territory text styles provide the shared normal, hovered, pressed, selected, and disabled presentation.

`UTerritoryDeveloperSettings::DefaultNarrativeButtonClass` and `DefaultTerritoryButtonStyle` are the runtime defaults for C++-generated controls. Static buttons in the journal, management panel, and command rows use the same project template. This keeps styling modular without duplicating CommonUI navigation or button behavior.

## Guard commands and authority

Rows and management screens call `UTerritoryPlayerManagementComponent`. The owning client sends only the selected Territory and absolute desired target. The server resolves the requesting pawn/faction and validates:

1. count bounds and anti-spam request order;
2. target world and registered district;
3. ownership, territory state, and diplomacy/capture policy;
4. management-point identity and range when a point is required;
5. selected Territory is the District or one of its registered child Properties;
6. desired target is within authored capacity;
7. Narrative inventory affordability for increases.

Only the server mutates desired guards, spawns/removes physical guards, or debits currency. The owning client receives one structured result notification and the UI refreshes from authoritative projections.

## Client visibility rules

- Ownership/state/progress and WorldState projections are safe client read models.
- Narrative funds come from the viewer pawn's Narrative inventory, not the controller.
- Active, reserve, and pending garrison counts come from the replicated `FTerritoryGarrisonSnapshot`.
- Active attacker counts are labelled known only when the local capture projection supports that detail.
- Widgets must never query a server-only map and assume an empty result means zero.
- Production widgets read `ReplicatedProductionSites` and `ReplicatedResourceSnapshots` on clients; `bResourceStorageAvailable` distinguishes a known empty stockpile from unavailable routing.

## Focus and accessibility

`UTerritoryActivatableWidget` is focusable by default. The outer `WBP_MainHopTerritoryJornal` focuses its embedded journal, and the journal resolves `Btn_TerritoryTab` as its concrete activation target. `DesiredFocusTargetName` is authoritative, with `InitialFocusWidgetName` retained as a migration fallback. The player menu, journal tabs/filters/commands, and row controls have deterministic keyboard/gamepad navigation. Interactive controls include tooltips and explicit accessible names, and Territory text uses Narrative CommonText at the audited minimum size.

Bidirectional navigation naturally forms cycles so users can move forward and backward. The MCP focus audit requires zero unreachable controls, zero dead ends, and zero dangling explicit targets; cycles are expected when they are the inverse edges of the same navigation chain.

## Blueprint extension rules

- Derive interactive Territory screens from `UTerritoryActivatableWidget`, `UTerritoryJournalWidget`, or `UTerritoryDistrictManagementWidget`.
- It is valid to use a Blueprint child of Narrative's `WBP_NarrativeMenu` as an outer wrapper.
- Keep widget names used by `BindWidgetOptional`, or bind your own presentation to the public operations-view functions.
- Use Narrative Common buttons/text and the registered Narrative HUD layers.
- Keep styling in project widgets; keep state and validation in the existing Territory/Narrative authorities.
- Never perform a guard, capture, economy, diplomacy, or assault mutation directly from widget state.

## Current limits

- The operations dashboard is a strategic snapshot, not an offscreen combat simulator.
- Live dedicated-server/two-client gamepad and screen-reader verification is still a release gate even when native, Blueprint, and static MCP audits pass.
