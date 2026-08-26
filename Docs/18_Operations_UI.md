# Territory Operations UI

## Purpose

The operations UI is the player-facing projection of the Territory authorities. It lists visible Districts that are unlocked, available, owned, manageable, contested, threatened, or operating at a loss, and exposes guarded absolute staffing commands for District and child Place garrisons. In C++, a player-facing Place is represented by `ATerritoryProperty`.

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

- City identity plus a visible `City -> District -> Place` hierarchy;
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
- the viewer/owner diplomacy state, reputation, war, alliance, and trade flags.

The struct is a read-only projection. Pointer fields are transient UI references and are never campaign save data.

### `FTerritoryEconomyOperationsView`

`BuildEconomyOperationsView` returns the viewer's available Narrative inventory funds plus the Territory economy projection: income, costs, net, deficit state, territory count, recent credits/debits, bounded transaction history, resource-storage availability, stockpile quantities, and modular production-site rows.

“Available funds” is not a Territory treasury. The Narrative inventory/account remains currency authority.

## Filters

`ETerritoryOperationsFilter` supports:

| Filter | Meaning |
|---|---|
| `All` | Every registered District in the developer/read-model API; the player journal adds hierarchy visibility rules |
| `Unlocked` | District is not locked |
| `Available` | Viewer can currently participate in capture |
| `Owned` | Viewer faction owns the claimed district |
| `Manageable` | Viewer owns it and satisfies management policy |
| `UnderAttack` | Physical contest, non-terminal scheduled assault, or eligible projected threat |
| `Contested` | Existing capture subsystem reports an active contest |
| `Locked` | Territory is locked; intended for tools/debug screens, not the player journal |
| `FinancialRisk` | Guard upkeep exceeds district income |
| `Producing` | At least one production site is active or settled |
| `ProductionBlocked` | At least one site has missing input, unavailable/full storage, or an invalid profile |
| `MissingInputs` | At least one exact production rule lacks required Narrative items |
| `StorageFull` | At least one output cannot fit by slots or weight |

`GetDistrictOperationsRevision` hashes every displayed authority used by the supplied list. The journal rebuilds when guards, capture, finance, lock state, or assault state changes, fixing the former stale-row bug where only item count and filter text invalidated the list.

`DoesDistrictMatchSearch` applies case-insensitive AND-token matching across the complete player-facing projection: City, District, visible Place, stable Territory tags, owner, state, availability, diplomacy, threat/attacker data, and visible child-garrison names/tags. The search field therefore filters the same rows that the directory actually renders rather than a separate count-only model.

## District Command Center

`WBP_HopTerritoryJournalWidget` now uses Narrative Pro's real Quest Journal template pattern,
not a dashboard that only borrows Quest names:

1. **Active Territories** is a bounded `ScrollBox`, in the same role as
   `ActiveQuestsBox`. It contains unlocked, non-owned District entry widgets.
2. **Captured Territories** is the second bounded `ScrollBox`, in the same role as
   `FinishedQuestsBox`. It contains Districts currently owned by the viewer's faction.
3. **Territory Intelligence** is the full-width top strip. Current reports use the live accent;
   expired reports stay readable in a disabled colour, and actionable reports expose waypoint
   control without opening a second screen.
4. **Selected Territory** is the persistent right-side information pane. Selecting one entry
   unselects the others, updates this pane, and immediately exposes Overview, Places, Garrison,
   Economy, Production, Threats, and Diplomacy tabs. Longer ownership and capture explanations
   live inside the scrollable Overview page so they cannot push the controls off-screen.

The Territory classes do not inherit from Quest data classes. They reuse the Quest Journal's
presentation contract: two lists, one reusable entry template, one selected-item controller,
and one detail pane. Territory ownership still comes from `ATerritoryVolume`; the UI only reads
the viewer-relative projection.

The supplied UISpec keeps most visual children private (`bIsVariable = false`). The native
widgets therefore use `BindWidgetOptional` when a project exposes a child and also resolve the
documented stable child names at runtime. This prevents a valid Territory projection from
producing an empty list merely because an authored `ScrollBox`, label, or row child is private.
If a custom `TerritoryEntryWidgetClass` cannot be created, the journal uses the native row and
shows a clear data-versus-widget failure message instead of silently showing an empty queue.

The action and ownership predicates remain strict, while visibility is broader:

```text
Available / Unlocked = registered AND unlocked AND currently available AND not owned by viewer
Captured / Owned     = registered AND owned by viewer AND state is not Unclaimed
Player Directory     = registered AND this District and every parent are loaded and unlocked
```

An unlocked District that is diplomatically blocked, defended, or otherwise unavailable
is not counted as actionable. A locked District does not appear in the player journal. A
locked City hides its Districts and Places. A locked Place is omitted from the selected
District's Places, garrison, production, finance, and threat projections. If a required parent
is streamed out, the hierarchy fails closed and stays hidden until the parent is loaded and
can be verified. An owned District cannot duplicate into the Available queue. Counts derive
from the same row predicates.

Easy example: `Old City -> Market District -> Blacksmith`. If Old City or Market District is
locked by a quest condition, the player sees none of that branch. When the quest unlocks the
District, Market District appears. If only Blacksmith remains locked, the District appears but
Blacksmith does not appear in its Places or production list.

Threat and capture details cascade from loaded same-owner child Properties. A Blacksmith
assault therefore appears in Market Square even though the durable assault correctly
targets the capturable Property rather than the aggregate-only District.

Clicking a compact entry selects that District and opens its known-Place accordion. The header
always shows the complete Place count, such as `1 / 5 PLACES`, but only unlocked Place names are
created. Easy example: selecting `Market Square` may reveal `Blacksmith`; three story-locked
Places still contribute to the `1 / 5` count without leaking their names. Clicking the selected
entry again can close the accordion without clearing the right-side details.

For a simple runtime check, call `GetActiveTerritoryEntryCount` and
`GetCapturedTerritoryEntryCount` on the journal. These functions count real District entry
widgets and ignore empty-state text. Example: before taking Market Square the expected result
can be `Active = 1, Captured = 0`; after the player's faction owns it, the expected result is
`Active = 0, Captured = 1`.

The garrison planner selects the first manageable target with capacity (normally a child
Property when the District is a zero-capacity container), navigates District/Property posts
with Previous/Next controls, and exposes an integer target, capacity progress, projected
recruitment/upkeep/net, and Apply/Empty/Full actions. Entry rows do not mutate guards; all
staffing changes remain in the selected information pane.

## Supplied widgets

| Widget | Role |
|---|---|
| `W_TerritoryPlayerMenu` | Existing Narrative player menu with the Territory journal tab and a valid activation focus target |
| `WBP_MainHopTerritoryJornal` | Narrative menu wrapper around the Territory journal; forwards activation focus to the inner widget |
| `WBP_HopTerritoryJournalWidget` | Narrative Quest Journal-style Command Center with Active/Captured Territory ScrollBoxes, a selected-item detail pane, seven control tabs, and visible intelligence |
| `WBP_TerritoryCommandRow` | Compact reusable Narrative CommonUI entry with selected styling, Place count, known-Place accordion, and waypoint action |
| `WBP_TerritoryDistrictManagement` | In-world district command panel for guards, funds, income, production summary, availability, and threat status |
| `WBP_TerritoryEconomyWidget` | Faction economy health plus bounded scrolling stockpile and production-site modules |
| `WBP_TerritoryInfoWidget` | Passive current-territory status card with availability, threat, net income, and production status |
| `WBP_TerritoryCaptureHUD` | Compact translucent capture card with name, state, owner, pressure, and progress; detailed descriptions stay in the Territory menu |
| `WBP_TerritoryGameplayHUD_Modular` | Project-owned copy of Narrative's complete GameplayHUD template graph and tree, with the Territory capture HUD composed as a passive overlay |
| `WBP_TerritoryResourceRow` | Reusable stockpile/input/output/net resource row |
| `WBP_TerritoryProductionSiteRow` | Reusable production-site module that composes resource rows |
| `BP_TerritoryDebugWidget` | Scrollable live territory/counterattack diagnostic output |

Project styling should replace `TerritoryEntryWidgetClass`; `DistrictRowWidgetClass` remains a
migration fallback for widgets authored before this refactor. The native fallback uses
Narrative CommonUI controls and the same delegates.

`UTerritoryResourceRowWidget` and `UTerritoryProductionSiteRowWidget` are reusable, read-only modules. The economy base can populate optional `ResourceStockpileRows` and `ProductionSiteRows` containers, while any project Blueprint can consume the same structs through `OnEconomyOperationsUpdated`. Production rows compose resource rows; they never call settlement functions or own resource quantities.

The supplied Economy widget constrains both dynamic row collections in scrolling viewports. The journal operational selector presents all four production filters shown above.

## Project templates and styles

`WBP_TerritoryButton_Text` is a project-owned duplicate of Narrative Pro's `WBP_NarrativeButton_Text`. It keeps `UNarrativeCommonButtonBase` behavior while removing the unused input-action block and vendor click animation. `ButtonStyle_TerritoryPrimary` and its Territory text styles provide the shared normal, hovered, pressed, selected, and disabled presentation.

`UTerritoryDeveloperSettings::DefaultNarrativeButtonClass`, `DefaultTerritoryButtonStyle`,
`DefaultTerritoryTextStyle`, `TerritoryTitleTextStyle`, `TerritoryHeadingTextStyle`, and
`TerritoryMutedTextStyle` are soft runtime defaults for C++-generated controls. Community
projects work with Narrative Pro's base styles; a game can override them in Project Settings
without changing Territory Framework source. Static buttons in the journal, management panel,
and command rows use the same project template. This keeps styling modular without duplicating
CommonUI navigation or button behavior.

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
