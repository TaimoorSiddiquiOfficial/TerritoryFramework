# District Management System

## Overview

The District Management system provides a physical in-world interaction point for players to view and manage a captured district. Each district has a `ATerritoryDistrictManagementPoint` POI actor that appears on the map and can be interacted with directly.

## Architecture

```
ATerritoryDistrictManagementPoint (APOIActor)
  ├── UTerritoryDistrictNavigationMarkerComponent   — Map marker + POI marker
  │     └── UTerritoryDistrictPOIMarker              — Custom marker for district info
  ├── UTerritoryDistrictInteractableComponent        — In-world interaction
  └── USphereComponent                               — Interaction sphere trigger
  
UTerritoryDistrictManagementWidget (UTerritoryInfoWidget)
  └── Reads from ATerritoryDistrict + UTerritoryEconomySubsystem
```

Two independent entry points to open the management widget:

1. **Map marker**: Player clicks the district marker on map/compass → `OnSelect()` → `OpenManagementWidget()`
2. **Physical interaction**: Player walks within range and presses interact → `OnInteract()` → `HandleInteraction()` → local `OpenManagementWidget()`

## Setup

### Placing a Management Point

1. Place an `ATerritoryDistrictManagementPoint` actor in the level near its district
2. Set `DistrictTag` to match the district's `TerritoryTag`
3. Assign a `ManagementWidgetClass` (a Widget Blueprint subclass of `UTerritoryDistrictManagementWidget`)
4. Adjust `ManagementDistance` (default 600uu) for interaction range

### Required Dependencies

The management point resolves the district by tag through `UTerritoryRegistrySubsystem`. The district must be:
- Registered in the registry (auto-registered in `ATerritoryVolume::BeginPlay()`)
- Have its `TerritoryTag` set in the editor

## Access Control (`CanManage`)

Three conditions must all pass:

1. **District exists** — `DistrictTag` resolves to a registered `ATerritoryDistrict`
2. **District is Claimed** — state != Contested/Locked/Unclaimed
3. **Interactor is owner** — interactor's primary faction matches district's `GetOwningFaction()`

Faction is resolved via `UTerritoryBlueprintLibrary::GetActorPrimaryFaction()` which reads `INarrativeTeamAgentInterface::GetFactions()`.

## Management Widget

The widget (`UTerritoryDistrictManagementWidget`) extends `UTerritoryInfoWidget` and displays:

| Field | Source | Update |
|---|---|---|
| District Name | `District->GetTerritoryDisplayName()` | 0.5s poll + delegate |
| Owner Faction | `District->GetOwningFaction()` | 0.5s poll + delegate |
| State | `District->GetTerritoryState()` | 0.5s poll + delegate |
| Guard Count | `District->GetSpawnedGuardCount() / GetMaxGuardCount()` | 0.5s poll |
| Guard Cost | `District->GetGuardPurchaseCost(1)` | 0.5s poll |
| District Income | `District->GetEffectiveIncome()` | 0.5s poll |
| Player Currency | `Economy->GetActorCurrency(GetOwningPlayer())` | 0.5s poll |
| Add Guard Button | `District->CanPurchaseGuards()` | 0.5s poll |

The base class `UTerritoryInfoWidget` subscribes to ownership/state change delegates for immediate refresh on capture/contest events. Economy and guard data is polled because those subsystems don't push per-change delegate updates.

### Guard Purchase Flow

```
Widget "Add Guard" button
  → HandleAddGuardClicked()
    → ManagementComponent->RequestPurchaseGuards(Point, Count)
      → (Client) ServerRequestPurchaseGuards (RPC)
        → PerformPurchase()
          → Validate: district, pawn, faction, range
           → District->TryPurchaseGuards(RequestingPawn, Count)
             → Validate owner, range, budget, capacity
             → Economy->TryDebitCurrency(RequestingPawn)
            → SpawnGuards()
          → ClientReceiveGuardPurchaseResult (RPC)
            → HandleGuardPurchaseResult()
```

Anti-spam: server-enforced cooldown and monotonic request IDs on the player-owned management component. The management point is never used as a client RPC owner.

## POI Marker Refresh

The district POI marker (`UTerritoryDistrictPOIMarker`) auto-refreshes when the district's ownership or state changes. The marker is only visible for **Claimed** districts — unclaimed, contested, and locked districts are hidden (alpha 0). Color semantics:

| State/Faction | Color |
|---|---|
| Claimed by player faction | Green (0.1, 0.75, 0.35) |
| Claimed by enemy faction | Red |
| Unclaimed / Contested / Locked | Hidden (alpha 0) |

## Blueprint API

### UTerritoryDistrictManagementWidget (native-backed)

| Function | Type | Purpose |
|---|---|---|
| `InitializeManagement(ManagementPoint)` | BlueprintCallable | Bind widget to a management point |
| `GetManagedDistrict()` | BlueprintPure | Returns resolved ATerritoryDistrict |
| `GetManagedFaction()` | BlueprintPure | Returns player's faction tag |
| `GetDistrictIncome()` | BlueprintPure | Sum of property incomes |
| `CanPurchaseGuard(OutFailureReason)` | BlueprintPure | Guard purchase validation |
| `RefreshManagementDisplay()` | BlueprintCallable | Force display refresh |
| `OnManagementRefreshed()` | BlueprintImplementableEvent | BP hook after each refresh |

### ATerritoryDistrictManagementPoint

| Function | Type | Purpose |
|---|---|---|
| `ResolveDistrict()` | BlueprintPure | Get district from DistrictTag |
| `CanManage(Interactor, OutFailureReason)` | BlueprintPure | Access control check |
| `IsInteractorInRange(Interactor)` | BlueprintPure | Distance check |
| `OpenManagementWidget(PlayerController)` | BlueprintCallable | Open the management UI |

### UTerritoryPlayerManagementComponent

| Function | Type | Purpose |
|---|---|---|
| `RequestPurchaseGuards(Point, Count)` | BlueprintCallable | Initiate guard purchase |
| `OnGuardPurchaseResult` | BlueprintAssignable | Result delegate: (Territory, Success, Message, RequestId) |

## Lifecycle Notes

- The management widget polls display data every 0.5s via the base class timer
- Guard purchase results arrive asynchronously via RPC and update the status text
- The widget auto-unsubscribes from territory delegates in `NativeDestruct()`
- The navigation marker auto-unsubscribes from territory delegates in `EndPlay()`
- Multiple management points can exist in a level (one per district)
