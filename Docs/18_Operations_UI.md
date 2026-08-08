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

Narrative Pro's current `UNarrativeMenu` constructor is private, so it cannot be used as a native C++ parent without modifying vendor source. Territory native screens therefore use `UNarrativeActivatableWidget`. Existing Blueprint wrapper menus may still inherit `WBP_NarrativeMenu`, as `WBP_MainHopTerritoryJornal` does. This keeps the monthly vendor plugin read-only.

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
- District and child-Property assault state, exact leaf target, attacker faction,
  finite-force counts, selected approaches, launch probability, estimated success,
  attack priority, defence power, power ratio, and threat summary;
- a separately labelled strongest-eligible projected threat when no assault is scheduled.
  Projection is planning data and never claims physical attackers exist.

The struct is a read-only projection. Pointer fields are transient UI references and are never campaign save data.

### `FTerritoryEconomyOperationsView`

`BuildEconomyOperationsView` returns the viewer's available Narrative inventory funds plus the Territory economy projection: income, costs, net, deficit state, territory count, recent credits/debits, and bounded transaction history.

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

`GetDistrictOperationsRevision` hashes every displayed authority used by the supplied list. The journal rebuilds when guards, capture, finance, lock state, or assault state changes, fixing the former stale-row bug where only item count and filter text invalidated the list.

`DoesDistrictMatchSearch` applies case-insensitive AND-token matching across the complete player-facing projection: display name, stable Territory tag, owner, state, availability and lock reasons, threat/attacker data, and child garrison names/tags. The search field therefore filters the same rows that the directory actually renders rather than a separate count-only model.

## District Command Center

`WBP_HopTerritoryJournalWidget` is supplied as a three-column command surface:

1. **Operations queues** show currently actionable Available/Unlocked Districts and,
   separately, captured Districts controlled by the viewer.
2. **District directory** exposes name, owner, state, and operations filters over every registered district projection.
3. **District command** shows owner/state, availability and lock reason, Property
   alignment, every local garrison, child-Property capture pressure, income/upkeep/net,
   strongest diplomacy-eligible attacker, exact target, defence/power ratio,
   grace/cooldown, finite force, probabilities, and approaches.

The command surface is authored on a 1920×1080 design canvas inside a `ScaleBox` using `ScaleToFit`, so the complete journal remains inside smaller or differently shaped viewports. The selected-district column has its own clipped vertical `ScrollBox`; mouse-wheel and focus navigation scroll lower guard controls into view instead of increasing the screen's desired size. Long operational readouts use automatic wrapping.

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
| `WBP_HopTerritoryJournalWidget` | Three-column District Command Center with actionable/owned queues, searchable directory, selected-district controls, finance ledger, and exposure report |
| `WBP_TerritoryCommandRow` | Responsive project-styled Narrative CommonUI selection row used by all journal lists |
| `WBP_TerritoryDistrictManagement` | In-world district command panel for add/remove guards, funds, income, reserve visibility, availability, and threat status |
| `WBP_TerritoryEconomyWidget` | Faction economy health, net, deficit, and recent activity |
| `WBP_TerritoryInfoWidget` | Passive current-territory status card with availability, threat, and net income |
| `BP_TerritoryDebugWidget` | Scrollable live territory/counterattack diagnostic output |

Project styling can replace `DistrictRowWidgetClass`; the native fallback uses Narrative CommonUI controls and the same delegates.

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
