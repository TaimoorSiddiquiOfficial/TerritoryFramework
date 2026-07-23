# Territory Framework — Blueprint Extension Guide

> How to create BP subclasses, which events to call Parent on, and the full save/load lifecycle.

---

## Creating BP Subclasses

When you create a Blueprint child of `ATerritoryVolume`, `ATerritoryCity`, `ATerritoryDistrict`, or `ATerritoryProperty`, certain BlueprintNativeEvents **must call `Parent:`** for correct behavior.

### Events Where Calling Parent is REQUIRED

| Class | Event | What Parent Does | If You Don't Call Parent |
|---|---|---|---|
| **TerritoryVolume** | `OnAllGuardsDefeated` | Clears ownership → sets Unclaimed → enables capture | Territory stays Claimed, capture impossible |
| **TerritoryCity** | `OnDistrictCapturedInCity` | Cascade capture to city level | City never detects district changes |
| **TerritoryCity** | `OnCityFullyCaptured` | Economy bonus + delegate broadcast | No capital bonus gold |
| **TerritoryDistrict** | `OnDistrictFullyCaptured` | District capture events | Events don't fire |
| **TerritoryProperty** | `OnPropertyCaptured` | Resets upgrade level to 0 | Captured property keeps enemy upgrades |

### Events Where Calling Parent is OPTIONAL

| Class | Event | What Parent Does | Notes |
|---|---|---|---|
| **TerritoryVolume** | `OnOwnershipChanged` | Empty — invariants are in the non-virtual setter | BP-only hook, no harm skipping Super |
| **TerritoryVolume** | `OnStateChanged` | Empty — invariants are in the non-virtual setter | BP-only hook |
| **TerritoryVolume** | `OnTerritoryInitialized` | Empty — exists for BP init | No-op |
| **TerritoryCity** | `OnCityLost` | Sets Unclaimed (if not Locked) + economy recalc | Skip if custom loss logic |
| **TerritoryDistrict** | `OnDistrictCapturedInCity` | BP hook only | Cosmetic |

### Guard Lifecycle Invariants (Non-Overridable)

These run inside the non-virtual `SetOwningFaction()` and `SetTerritoryState()` setters. BP subclasses **cannot break these**:

- `DespawnGuards()` — always runs on ownership change
- `SpawnGuards()` — always runs when new owner has a guard definition
- Guards despawn on `Contested` entry from `Claimed`
- Guards respawn on `Claimed` from `Contested` (if definition exists)

---

## Capture Flow — Complete Lifecycle

### Capture Trigger Methods

| Method | Function | Speed |
|---|---|---|
| **Progressive** | `RegisterAttacker(Territory, Actor, Faction)` | ~10s at default rate (0.1/s) |
| **Instant** | `ForceCapture(Territory, Faction)` | Immediate |
| **Progress boost** | `AddCaptureProgress(Territory, Faction, Delta)` | Custom speed |
| **Quest/Dialogue** | `TerritoryCaptureEvent` on quest node | Immediate or progressive |
| **Guard death** | Bind to `OnAllGuardsDefeatedDelegate` | Designer chooses |

### Capture State Machine

```
                    ┌──────────┐
         ┌─────────►│  Locked  │◄──── LockTerritory()
         │          └────┬─────┘
         │               │ TryUnlock() / TerritoryUnlockEvent
         │               ▼
    SetLocked()    ┌──────────┐
         │         │ Unclaimed │◄──── All guards defeated
         │          └────┬─────┘
         │               │ RegisterAttacker()
         │               ▼
         │          ┌──────────┐
         │     ┌───►│Contested │◄──── Attacker enters
         │     │    └────┬─────┘
         │     │         │ Progress reaches 1.0
         │     │         ▼
         │     │    ┌──────────┐
         │     │    │  Claimed  │──── Spawns guards for new owner
         │     │    └────┬─────┘
         │     │         │ Enemy captures a child property
         │     └─────────┘ (back to Contested)
         │
         └── LockTerritory() at any time
```

### Contesting Faction

`ContestingFaction` is maintained by `TerritoryControlSubsystem`:
- Set when `RegisterAttacker` is called
- Updated each tick to the leading faction
- Cleared on `CompleteCapture` or `ResetCapture`
- Replicated as part of `OwnershipData`

---

## Save/Load Lifecycle

### How Territory Data Persists

TerritoryFramework uses Narrative's `INarrativeSavableActor` interface. The save system serializes all `UPROPERTY(SaveGame)` fields.

### What Gets Saved

| Property | Saved | Notes |
|---|---|---|
| `OwnershipData` (full struct) | ✅ | Owner, State, Progress, ContestingFaction, DefenderCount, LockReason |
| `TerritoryGUID` | ✅ | Must be stable — generated from tag hash if not editor-baked |
| `TerritoryTag` | ✅ | Level-config |
| `InitialOwningFaction` | ✅ | Level-config |
| `GuardNPCDefinition` | ✅ | Level-config |
| `FactionGuardDefinitions` | ✅ | Level-config |
| `SpawnedGuards` | ❌ | Transient — guards respawn from saved ownership |
| `RegisteredDefenders` | ❌ | Transient — rebuilt on spawn |

### What Gets Saved on Guards

Guards (`ATerritoryGuardCharacter`) return `ShouldRespawn = false` — Narrative's save system does **NOT** restore guard actors on load. TerritoryVolume spawns fresh guards from saved ownership data.

### BeginPlay Load Sequence

```
1. Generate deterministic GUID from TerritoryTag (if not editor-baked)
2. LoadSingleActor(this)
   └─ Narrative deserializes saved SaveGame UPROPERTYs into this actor
   └─ Calls Load_Implementation()
3. Sync level-config settings (income, attacker cap, guard cost)
4. If no owner after load AND InitialOwningFaction set → apply initial owner
5. If bStartsLocked AND no owner → set Locked
6. Spawn guards from loaded OwnershipData.OwningFaction
7. Register with TerritoryRegistrySubsystem + bind hierarchy delegates
8. Fire OnTerritoryInitialized()
```

### Why GUID Must Be Stable

`LoadSingleActor` searches for saved data by the actor's GUID. If the GUID changes between sessions, the lookup fails and saved data is lost.

| Source | When Applied |
|---|---|
| Editor hook (`PostEditChangeProperty`) | When designer edits any property in the Details panel |
| Editor hook (`PostDuplicate`) | When actor is duplicated in the editor |
| Runtime fallback (`FCrc::StrCrc_DEPRECATED(tag)`) | If no editor-baked GUID exists — deterministic from tag |

**Always set a TerritoryTag on placed actors** — it's the fallback for GUID generation.

---

## Per-Faction Guard Definitions

TerritoryVolume supports per-faction NPC definitions so each faction spawns its own guard type.

### Setup

On the TerritoryVolume Details panel under **Territory|Guards**:

1. **GuardNPCDefinition** — default fallback (used when no faction-specific entry matches)
2. **FactionGuardDefinitions** — array of per-faction overrides:
   - `Faction` = `Narrative.Factions.Bandits` → `NPC_BanditGuard`
   - `Faction` = `Narrative.Factions.Heroes` → `NPC_HeroGuard`

### Resolution Order

```
ResolveGuardDefinition(Faction):
  1. Check FactionGuardDefinitions for matching faction tag
  2. If found → use that definition
  3. If not found → fall back to GuardNPCDefinition
  4. If neither → no guards spawn
```

### Faction Assignment During Spawn

Faction is determined by precedence:

1. `SpawnPoint->FactionOverride` (if valid)
2. `TerritoryVolume->OwnershipData.OwningFaction`

Faction is set via `FNPCSpawnParams.bOverride_DefaultFactions` before `SetNPCDefinition` — Narrative's initialization reads ONLY the territory owner faction.

---

## Hierarchy Capture Rules

### Unanimity Policy

A district is captured **ONLY** when ALL its properties are owned by the same faction. A city is captured **ONLY** when ALL its districts are owned by the same faction.

```
City: HavenReach
  └── District: MarketSquare
       ├── Property: Blacksmith     ← must be same faction
       ├── Property: Tavern         ← must be same faction
       └── Property: General Store  ← must be same faction
```

### State Transitions

| Event | State Change |
|---|---|
| Any child property captured by enemy | District → **Contested** |
| All properties owned by one faction | District → **Claimed** by that faction |
| Any district captured by enemy | City → **Contested** |
| All districts owned by one faction | City → **Claimed** by that faction |

### Scripted Override (Cascade)

`CascadeCaptureToProperties` force-reassigns all child properties when a district is explicitly captured via `ForceCapture` or quest event. This is a **scripted override**, not the default path.

---

## Lock System

### Lock API

| Function | Access | Effect |
|---|---|---|
| `IsLocked()` | BlueprintPure | Returns true if state is Locked |
| `CanUnlock()` | BlueprintPure | Checks all LockConditions pass |
| `LockTerritory(Reason)` | BlueprintAuthorityOnly | Sets state to Locked |
| `TryUnlock(bForce)` | BlueprintAuthorityOnly | Unlocks if conditions pass or forced |
| `GetLockReason()` | BlueprintPure | Returns lock reason text |

### Locked Territory Behavior

- **No marker** on map (zero alpha)
- **No guards** spawn
- **No capture** possible
- **No display text**

### Quest Integration

| Asset | Effect |
|---|---|
| `TerritoryLockEvent` | Locks territory from quest/dialogue node |
| `TerritoryUnlockEvent` | Unlocks territory from quest/dialogue node |
| `LockConditions` array | `UNarrativeCondition` instances — all must pass for `TryUnlock` |

---

## Map Markers

### Color Resolution

```
GetMarkerColor():
  1. State == Locked → zero alpha (invisible)
  2. State == Contested → ContestedColor (yellow)
  3. Owner has FactionColorMap entry → that color (e.g., green for player)
  4. Owner valid but no entry → EnemyOwnedColor (red)
  5. No owner → UnclaimedColor (red)
```

### Customization

All colors are `BlueprintReadWrite` on `UTerritoryMapMarker`:

| Property | Default |
|---|---|
| `UnclaimedColor` | Red |
| `EnemyOwnedColor` | Red |
| `ContestedColor` | Yellow |
| `LockedColor` | Purple (unused — locked = invisible) |
| `FactionColorMap` | Empty — add entries for per-faction colors |

Set player faction to green:
```
MapMarkerComponent → FactionColorMap → Add Entry:
  Key: Narrative.Factions.Heroes
  Value: (R=0, G=1, B=0, A=1)
```

---

## Delegates Reference

### TerritoryVolume

| Delegate | Parameters | When Fired |
|---|---|---|
| `OnTerritoryOwnershipChanged` | (Territory, OldOwner, NewOwner) | Faction changes |
| `OnTerritoryStateChangedDelegate` | (Territory, NewState) | State changes |
| `OnGuardKilled` | (Territory, Guard, Killer, RemainingDefenders) | A guard dies |
| `OnAllGuardsDefeatedDelegate` | (Territory) | Last guard dies |

### TerritoryCity

| Delegate | Parameters | When Fired |
|---|---|---|
| `OnCityCapturedDelegate` | (City, CapturingFaction) | All districts owned by one faction |
| `OnCityLostDelegate` | (City, PreviousFaction) | City owner loses a district |

### TerritoryDistrict

| Delegate | Parameters | When Fired |
|---|---|---|
| `OnDistrictCapturedDelegate` | (District, OldOwner, NewOwner) | All properties owned by one faction |

### TerritoryProperty

| Delegate | Parameters | When Fired |
|---|---|---|
| `OnPropertyCapturedDelegate` | (Property, NewOwner) | Property ownership changes |

### TerritoryControlSubsystem

| Delegate | Parameters | When Fired |
|---|---|---|
| `OnCaptureAttempted` | (FCaptureAttempt) | AttemptCapture is called |
| `OnTerritoryControlChanged` | (Territory, OldOwner, NewOwner) | CompleteCapture fires |

### TerritoryRegistrySubsystem

| Delegate | Parameters | When Fired |
|---|---|---|
| `OnTerritoryRegistered` | (Territory, bWasUnregistered) | Territory registers with subsystem |

---

## Debug Settings

Enable in **Project Settings → Plugins → Territory Framework**:

| Setting | Effect |
|---|---|
| `bEnableDebug` | Master toggle |
| `bDebugOwnership` | Log ownership changes |
| `bDebugCapture` | Log capture progress ticks |
| `bDebugCaptureAttempts` | Log AttemptCapture results |
| `bDebugGuards` | Log guard spawn/despawn |
| `bDebugGuardDeaths` | Log guard deaths |
| `bDebugStateTransitions` | Log state changes |
| `bDebugDiplomacy` | Log diplomacy changes |
| `bDebugTransactions` | Log economy transactions |
| `bDebugSaveLoad` | Log save/load results |
| `bDebugSpatial` | Log spatial index queries |
| `bDrawTerritoryBounds` | Debug box in PIE |
| `bDrawOwnershipOverlay` | Green overlay for owned |
| `bDrawCaptureProgress` | Progress bar above contested |
| `bDrawGuardSpawnPoints` | Spheres + patrol routes |

### Blueprint Debug Helpers

```
PrintTerritoryDebug(WorldContext, Territory, Duration)
PrintAllTerritoryDebug(WorldContext, Duration)
```
