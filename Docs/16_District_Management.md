# District Management System

## Overview

District management provides an in-world Narrative POI and a Narrative CommonUI command screen for an owned district. Players inspect aggregate security/finance, select the District or any loaded child Property garrison, set an absolute staffing target, and see capture/counterattack risk without giving the widget gameplay authority.

The journal's District Command Center exposes previous/next garrison navigation, an integer target, staffing/capacity progress, projected recruitment/upkeep/net, and Apply/Empty/Full commands. Market Square can therefore manage the Blacksmith Property garrison even when the District container itself has zero guard capacity. Directory rows are selection-only; staffing mutations remain in the detailed command surface where their complete cost and failure context is visible.

## Architecture

```text
ATerritoryDistrictManagementPoint (Narrative APOIActor)
  ├─ UTerritoryDistrictNavigationMarkerComponent
  ├─ UTerritoryDistrictInteractableComponent
  └─ USphereComponent

UTerritoryDistrictManagementWidget (UTerritoryActivatableWidget)
  ├─ FTerritoryDistrictOperationsView
  ├─ Narrative Common buttons/text
  └─ UTerritoryPlayerManagementComponent RPC bridge
```

`ATerritoryDistrict` remains the owner/state/guard authority. The control, economy, counterattack, registry, and Narrative inventory systems supply their respective projections.

## Opening the screen

The management point resolves `DistrictTag`, validates the local player, and calls `UTerritoryUIBlueprintLibrary::OpenTerritoryMenu` with `ManagementLayerTag` (default `UI.Layer.Menu`). The widget is pushed into the registered Narrative gameplay-HUD CommonUI container.

Do not add the screen directly to the viewport or manually set controller input/cursor state. CommonUI activation, back handling, focus restore, and input routing own that lifecycle.

## Setup

1. Place `ATerritoryDistrictManagementPoint` near the district command location.
2. Assign a stable editor GUID and a `DistrictTag` matching the district.
3. Set `ManagementWidgetClass` to a Blueprint child of `UTerritoryDistrictManagementWidget`.
4. Keep `ManagementLayerTag` on a layer registered by the Narrative HUD.
5. Configure `ManagementDistance` for the physical interaction policy.

The district must be registered by `UTerritoryRegistrySubsystem`. World Partition load order is handled by tag/stable identity resolution; do not save a widget or actor pointer as district identity.

## Displayed operations

| Field | Authority |
|---|---|
| Name, owner, state | `ATerritoryDistrict` |
| Aggregate active / desired / maximum guards | District plus owned child Property garrisons |
| Selected active / target / maximum / reserve / pending | Replicated `FTerritoryGarrisonSnapshot` and Territory ownership data |
| Recruitment cost and recurring upkeep | Selected Territory configuration/economy |
| Available funds | Owning pawn's Narrative inventory/account |
| Income and net per cycle | `UTerritoryEconomySubsystem` projection |
| Availability and exact failure reason | Registry/control/management validation |
| Threat and finite assault force | `UTerritoryCounterAttackSubsystem` / WorldState projection |

The screen refreshes its operations view and reacts to management results. Active, reserve, and pending counts are exact replicated read models on clients; live pawn pointers remain server-only.

The left `Available / Unlocked` queue contains only registered, hierarchy-visible, actionable,
non-owned Districts. `Captured / Owned` contains only hierarchy-visible viewer-owned claimed
Districts. The center directory groups the same visible Districts by City. Locked Districts
and children of a locked or unloaded parent do not appear in the player journal. Locked Places
are also removed from child garrison, production, finance, and threat rows. Directory search is
case-insensitive and tokenized across City, District, visible Place, Territory tag, owner, state,
availability, diplomacy, threat, attacker, and visible child-garrison names/tags.

This is presentation privacy, not gameplay authority. Developer/debug tools may still call the
complete operations API and use the `Locked` filter. Capture and management calls always repeat
their full server validation even when a row was visible to the player.

## Access control

All of the following must pass on the server:

1. the request comes from the owning player controller and a valid pawn;
2. the management point and district exist in the same world;
3. the district is registered, claimed, and owned by the pawn's Narrative faction;
4. the pawn is within `ManagementDistance` when using a physical point;
5. the selected target is the District or a registered child Property;
6. absolute target, request ID, and cooldown are valid;
7. increases fit capacity and the exact Narrative account can pay;
8. reductions may go down to zero even when assigned guards are already dead.

The client cannot supply a trusted faction, price, balance, owner, power, or final state.

## Guard command flow

```text
Narrative Common button / absolute staffing target
  -> player-owned UTerritoryPlayerManagementComponent
  -> validated server RPC
  -> selected ATerritoryVolume authoritative atomic mutation
  -> Narrative inventory recruitment debit when raising
  -> exact deployment or full rollback/refund
  -> pending reserves cancelled and surplus active guards withdrawn when lowering
  -> owning-client result delegate
  -> operations-view refresh
```

Success is reported only after the final desired/live invariant is verified. A partial multi-guard deployment is rolled back rather than committed as a smaller purchase.

## Blueprint API

### `UTerritoryDistrictManagementWidget`

| Function | Type | Purpose |
|---|---|---|
| `InitializeManagement(Point)` | Callable | Bind the screen to a management point |
| `GetManagedDistrict()` | Pure | Resolve the authoritative district |
| `GetManagedFaction()` | Pure | Viewer faction resolved from the pawn |
| `GetDistrictIncome()` | Pure | Current district income |
| `GetOperationsView()` | Pure | Complete viewer-relative operations projection |
| `CanPurchaseGuard(Reason)` | Pure | Exact add eligibility |
| `CanRemoveGuard(Reason)` | Pure | Exact remove eligibility |
| `RequestAddGuards(Count)` | Callable | Submit a validated server request |
| `RequestRemoveGuards(Count)` | Callable | Submit a validated server request |
| `RefreshManagementDisplay()` | Callable | Rebuild current display state |
| `OnManagementRefreshed()` | Implementable event | Project presentation hook |

### `ATerritoryDistrictManagementPoint`

| Function/property | Purpose |
|---|---|
| `ResolveDistrict()` | Resolve `DistrictTag` through the registry |
| `CanManage(Interactor, Reason)` | Read-only access validation |
| `IsInteractorInRange(Interactor)` | Physical range policy |
| `OpenManagementWidget(PlayerController)` | Push screen to the Narrative HUD layer |
| `ManagementLayerTag` | Registered CommonUI target layer |

### `UTerritoryPlayerManagementComponent`

| Function/delegate | Purpose |
|---|---|
| `RequestPurchaseGuards*` | Add guards through a point or direct district policy |
| `RequestRemoveGuards*` | Remove guards through a point or direct district policy |
| `RequestSetGuardTarget(Point, Territory, Target)` | Set a District/Property target through a nearby command point |
| `RequestSetGuardTargetForTerritory(Territory, Target)` | Remote journal command for an owned target |
| `OnGuardPurchaseResult` | Owning-client result for both add/remove operations |
| `OnAssaultNotification` | Owning-client strategic assault notification |
| `OnCounterHappened` | Reliable owning-client event for each committed counterattack state transition |

## POI behavior

The management marker reuses Narrative Navigation's marker registration and domain API. It is
present on the world map/minimap only while its District is loaded, hierarchy-visible, and
claimed. It never appears on the compass merely because the command post exists. Locked,
unclaimed, contested, unloaded, or structurally hidden Districts remove the marker from
Narrative Navigation completely; zero-alpha icons are not used as a privacy policy.

The Command Center's **Waypoint** button represents the selected District operation, but the
physical route always resolves to a child Place POI. For example, selecting Market Square tracks
Blacksmith, and selecting Castle Hill tracks Farm. With several visible Places, a contested Place
wins first, then a Place not held by the viewer, then the nearest friendly Place; the Territory
tag gives deterministic tie-breaking. Only that Place enters Narrative compass and screen-space
domains. Clicking the selected row/report again clears it.

This route is local presentation state. It is neither replicated nor saved and it never changes
Territory ownership. `ATerritoryVolume` remains the owner/state authority, the hierarchy reducer
captures a District when all Places agree, and Narrative Navigation remains the POI/map/compass
authority. World Partition registration events refresh child visibility and late-loaded management
points without retaining durable actor pointers.

### Waypoint Blueprint API

| Function | Purpose |
|---|---|
| `ResolveTerritoryWaypointTarget(Controller, Territory)` | Return the visible Place that owns the physical route, or null when the hierarchy is silent |
| `SetTerritoryWaypoint(Controller, Territory)` | Promote the resolved Place into Narrative compass/screen-space domains |
| `GetTrackedTerritory(Controller)` | Return the actual tracked Place, not its aggregate District |
| `IsTerritoryWaypointTracked(Controller, Territory)` | Return true for the tracked Place and its City/District ancestors |
| `ClearTerritoryWaypoint(Controller)` | Remove all Territory route promotion for the local player |

No duplicate map, minimap, compass, POI discovery, or waypoint stack is created.

## Known limits

- The screen does not directly schedule/cancel assaults or alter diplomacy; those actions require their authoritative systems and project policy.
- A tracked Place still appears on Narrative's compass and screen-space layer without navmesh data.
  Narrative's optional ground breadcrumb line additionally requires valid RecastNavMesh coverage
  between the player and that Place; otherwise Narrative reports that no breadcrumb path exists.
- Dedicated-server/two-client input, focus, and interaction remain required runtime release tests.
