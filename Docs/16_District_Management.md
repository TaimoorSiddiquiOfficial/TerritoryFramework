# District Management System

## Overview

District management provides an in-world Narrative POI and a Narrative CommonUI command screen for an owned district. Players can inspect current security and finance, add guards, remove active guards, and see capture or counterattack risk without giving the widget gameplay authority.

The same guarded management flow is available from the journal's District Command Center. Selecting a captured/owned row opens the district detail surface and exposes atomic `+1`, `-1`, `+5`, and `-5` controls. Those controls are presentation adapters over `UTerritoryPlayerManagementComponent`; they do not create another guard, economy, or ownership authority.

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
| Active / desired / maximum guards | `ATerritoryDistrict` and guard posts |
| Reserve count | Server guard-post snapshot when available |
| Guard purchase cost and upkeep | Territory configuration/economy |
| Available funds | Owning pawn's Narrative inventory/account |
| Income and net per cycle | `UTerritoryEconomySubsystem` projection |
| Availability and exact failure reason | Registry/control/management validation |
| Threat and finite assault force | `UTerritoryCounterAttackSubsystem` / WorldState projection |

The screen refreshes its operations view and also reacts to management results. Unknown server-only values are labelled unknown rather than displayed as zero.

## Access control

All of the following must pass on the server:

1. the request comes from the owning player controller and a valid pawn;
2. the management point and district exist in the same world;
3. the district is registered, claimed, and owned by the pawn's Narrative faction;
4. the pawn is within `ManagementDistance` when using a physical point;
5. count, request ID, and cooldown are valid;
6. add requests fit capacity and the Narrative account can pay;
7. remove requests do not exceed removable active guards.

The client cannot supply a trusted faction, price, balance, owner, power, or final state.

## Guard command flow

```text
Narrative Common button / Blueprint RequestAddGuards or RequestRemoveGuards
  -> player-owned UTerritoryPlayerManagementComponent
  -> validated server RPC
  -> ATerritoryDistrict authoritative guard mutation
  -> Narrative inventory debit when adding
  -> owning-client result delegate
  -> operations-view refresh
```

The implementation selects and validates the complete removal set before commit. Success is not reported after a partial multi-guard removal.

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
| `OnGuardPurchaseResult` | Owning-client result for both add/remove operations |
| `OnAssaultNotification` | Owning-client strategic assault notification |

## POI behavior

The management marker reuses Narrative POI/navigation presentation. It refreshes with ownership/state and is visible according to district policy. No duplicate map, minimap, compass, or marker stack is created.

## Known limits

- A live reserve count is intentionally hidden on clients when no replicated reserve snapshot is available.
- The screen does not directly schedule/cancel assaults or alter diplomacy; those actions require their authoritative systems and project policy.
- Dedicated-server/two-client input, focus, and interaction remain required runtime release tests.
