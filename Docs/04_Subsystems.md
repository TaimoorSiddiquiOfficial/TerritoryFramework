# Subsystems — Registry, Control, Economy, Diplomacy, CombatDirector

## Architecture Overview

```
GameInstance / World
├── UTerritoryRegistrySubsystem   (lookup — territories by tag, GUID, location)
├── UTerritoryControlSubsystem     (capture flow — attackers, progress, tick)
├── UTerritoryEconomySubsystem     (treasury — income, costs, transactions)
├── UTerritoryDiplomacySubsystem    (treaties — war, peace, reputation)
└── UTerritoryCombatDirector        (attack permissions — budget per territory)
```

All subsystems are `UWorldSubsystem` — one instance per world, auto-created by engine. Access via `GetWorld()->GetSubsystem<U...>()` or the Blueprint Library helpers.

## UTerritoryRegistrySubsystem

The central lookup index. Every territory registers itself on BeginPlay and unregisters on EndPlay.

### Registration

```cpp
// Automatic — ATerritoryVolume calls these in BeginPlay/EndPlay
Registry->RegisterTerritory(Volume);
Registry->UnregisterTerritory(Volume);
```

### Lookup API

| Function | Input | Output | Notes |
|---|---|---|---|
| GetTerritoryByTag | GameplayTag | ATerritoryVolume* | O(1) hash map |
| GetTerritoryByGUID | FGuid | ATerritoryVolume* | O(1) hash map |
| GetTerritoryAtLocation | FVector | ATerritoryVolume* | O(1) spatial grid |
| GetAllTerritories | — | TArray<ATerritoryVolume*> | All registered |
| GetChildTerritories | ParentTag | TArray<ATerritoryVolume*> | Districts under a city |
| GetTerritoriesByFaction | FactionTag | TArray<ATerritoryVolume*> | All owned by faction |

### Spatial Index

Uses a grid-based hash (`FTerritorySpatialIndex`) for O(1) point-in-territory queries.

- Cell size configurable via `TerritoryDeveloperSettings::SpatialCellSize` (default 2000uu)
- Each territory inserted into all cells its bounds overlap
- `GetTerritoryAtLocation` queries the cell containing the point, tests each candidate with `ContainsPoint`

### Identity-Safe Unregistration

When an actor is destroyed, `UnregisterTerritory` checks that the mapping still points to THIS actor before removing. This prevents a duplicate-destroy scenario from wiping a valid mapping:

```cpp
void UnregisterTerritory(ATerritoryVolume* Volume)
{
    const FGameplayTag Tag = Volume->GetTerritoryTag();
    if (TagToTerritoryMap.Contains(Tag) && TagToTerritoryMap[Tag] == Volume)
    {
        TagToTerritoryMap.Remove(Tag);  // Only remove if it's OURS
    }
    // Same for GUID map and spatial index
}
```

### Delegates

| Delegate | Signature | When |
|---|---|---|
| OnTerritoryRegistered | (ATerritoryVolume*, bool bWasUnregistered) | After Register (`false`) or Unregister (`true`) |

Cities subscribe to `OnTerritoryRegistered` to catch districts that spawn after the city.

### Blueprint Access

```
GetTerritoryRegistry(WorldContext) → RegistrySubsystem
  → GetTerritoryByTag(Tag)
  → GetTerritoryAtLocation(Location)
  → GetAllTerritories()
  → GetChildTerritories(ParentTag)
  → GetTerritoriesByFaction(Faction)
```

---

## UTerritoryControlSubsystem

Manages the capture flow — attacker registration, progress accumulation, capture completion.

### Capture Lifecycle

```
1. RegisterAttacker(Territory, Actor, Faction)
   └── Identity-based: TSet<TWeakObjectPtr<AActor>> per faction (no count inflation)
   └── Seeds capture progress entry

2. Capture tick (every CaptureTickInterval, server-only)
   └── Phase 1: Evaluate all contested territories (no map mutation)
       ├── Attacker present → progress += DeltaTime × ProgressRate
       ├── No attackers → progress decays by DeltaTime × DecayRate
       ├── Defenders present → progress decays instead of advancing
       ├── ContestingFaction updated to leading faction (highest progress → most attackers → tag name)
       └── If progress >= 1.0 → defer Complete command
   └── Phase 2: Apply deferred commands
       ├── Complete → CompleteCapture(Territory, Faction)
       └── Reset → clear capture state, restore Claimed/Unclaimed

3. CompleteCapture:
   └── SetOwningFaction(NewFaction) → state = Claimed, guards respawn
   └── Broadcast OnTerritoryControlChanged

4. ForceCapture(Territory, Faction):
   └── Bypasses all rules, sets state to Claimed explicitly
```

### API

#### Actions (BlueprintAuthorityOnly)

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| AttemptCapture | Territory, Faction | ECaptureResult | Checks diplomacy, locks, defenders |
| ForceCapture | Territory, Faction | void | Bypasses all checks — admin/quest |
| ResetCapture | Territory | void | Resets progress to 0 |
| AddCaptureProgress | Territory, Faction, Delta | void | Manual progress injection |
| RegisterAttacker | Territory, Actor, Faction | bool | Returns false if budget full |
| UnregisterAttacker | Territory, Actor, Faction | void | Releases attack slot |

#### Queries (BlueprintPure)

| Function | Returns |
|---|---|
| IsCaptureInProgress(Territory) | bool |
| GetCaptureProgress(Territory) | float (0.0–1.0) |
| GetContestingFaction(Territory) | GameplayTag |
| HasAttackBudget(Territory, Faction) | bool |
| GetActiveAttackers(Territory, Faction) | int32 |

### ECaptureResult

| Value | Meaning |
|---|---|
| Success | Capture initiated (territory now Contested) |
| AlreadyOwned | Attacker owns this territory |
| Locked | Territory is locked |
| DiplomaticallyBlocked | Factions are Friendly (via Narrative attitude) |
| DefendersRemain | Guards/defenders still alive |
| InvalidTerritory | Null territory or invalid faction |

### Faction Attitude Check

Before allowing capture, the subsystem queries Narrative GameState for the relationship between attacker and owner:

```cpp
// Internal — uses NarrativePro's attitude system
bool bFriendly = NarrativeGameState->GetAttitude(AttackerFaction, OwnerFaction) == EAttitude::Friendly;
if (bFriendly) return ECaptureResult::DiplomaticallyBlocked;
```

### Timer

- Runs **only on server** (`NM_Client` check)
- Interval: `CaptureTickInterval` from DeveloperSettings (default 0.1s)
- Each tick: process all territories with registered attackers

### Delegates

| Delegate | Signature | When |
|---|---|---|
| OnTerritoryControlChanged | (Territory*, OldOwner, NewOwner) | After CompleteCapture or ForceCapture |
| OnCaptureAttempted | (FCaptureAttempt Attempt) | After every AttemptCapture call |

---

## UTerritoryEconomySubsystem

Manages faction treasuries, income calculation, and transaction ledger.

### Treasury Structure

Faction wealth is the **aggregate of all online faction members' `UInventoryComponent::Currency`** (NarrativePro). There is no separate `Gold` field — `GetTreasury()` reads live from player inventories each call. The tracked parameters are:

```cpp
struct FTerritoryTreasury
{
    int32 IncomePerTick;
    int32 CostsPerTick;
    int32 TerritoryCount;
};
```

### API

See [07_Economy_System.md](07_Economy_System.md) for full economy documentation.

### Key Behaviors

1. **Deferred income recalculation** — ownership changes mark factions dirty via `MarkFactionDirty()`. Actual `RecalculateIncome` runs once per economy tick (not immediately). This avoids O(3N) redundant scans during capture cascades.
2. **Transaction ledger** — every mutation (add/debit) records a `FTerritoryTransaction`. Ledger is trimmed once per tick (not per faction).
3. **Server-only timer** — economy tick fires every `EconomyTickIntervalSeconds` (default 300s). Processes dirty factions first, then applies income/upkeep per faction.
4. **OnTransactionRecorded** — broadcast after every transaction (UI binds to this)
5. **MaxTransactionHistory** — caps stored transactions globally (default 500)
6. **Delegate cleanup** — per-territory `OnTerritoryOwnershipChanged` delegates are unbound on `Deinitialize`

### Runtime Backstop

All mutations use `GetAuthGameMode()` check in C++ even though BlueprintAuthorityOnly covers BP callers:

```cpp
if (!GetWorld()->GetAuthGameMode()) return;  // C++ backstop
```

---

## UTerritoryDiplomacySubsystem

Manages treaties, reputation, and the bridge to Narrative GameState attitudes.

### Architecture Principle

**Narrative GameState is the SOLE authority for AI attitudes.** TerritoryFramework stores treaty metadata and pushes attitude changes to Narrative.

```
Treaty Signed → SetNarrativeAttitude() → Narrative GameState updates → AI behavior changes
```

### API

See [08_Diplomacy_System.md](08_Diplomacy_System.md) for full diplomacy documentation.

### Key Behaviors

1. **Attitude bridge** — `SetNarrativeAttitude()` pushes treaty-derived attitudes to Narrative. `OnFactionAttitudeChanged` reconciles treaties when Narrative changes: Friendly creates Alliance only if no treaty exists (preserves TradeAgreement/NonAggression), Hostile overrides any peaceful treaty, Neutral removes treaty record.
2. **Reentrancy guard** — `bSuppressSync` RAII guard prevents recursive mutation during diplomacy broadcasts
3. **Treaty expiration** — timer checks every `TreatyExpirationCheckInterval` (default 10s)
4. **History cap** — DiplomacyHistory capped at 500 entries
5. **Reset to Neutral** — `SetDiplomacyState(None)` resets Narrative attitude to Neutral

---

## UTerritoryCombatDirector

Strategic assault budget manager — limits how many AI can simultaneously attack within a territory. This is **separate** from Narrative Pro's per-target attack tokens:

- **Narrative tokens** = tactical: limits how many AI gang up on ONE defender
- **Assault slots** = strategic: limits how many AI participate in a territory assault

### API

#### Actions (BlueprintAuthorityOnly)

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| RequestAssaultSlot | Territory, NPCController | bool | Returns false if budget exhausted or territory locked |
| ReleaseAssaultSlot | Territory, NPCController | void | Frees one slot |
| ReleaseAllSlots | NPCController | void | Frees all slots across all territories |

#### Queries (BlueprintPure)

| Function | Returns | Notes |
|---|---|---|
| HasAssaultSlot(Territory, Controller) | bool | Does controller hold a slot? |
| GetGrantedSlots(Territory) | int32 | Active slots (filters dead controllers) |
| GetAvailableSlots(Territory) | int32 | MaxSlots - GrantedSlots |

### Budget Source

Each `ATerritoryVolume` has `MaxConcurrentAttackers` (default 3). The CombatDirector enforces this:

```
If GrantedSlots(Territory) >= MaxConcurrentAttackers → deny new slot
```

### Death Hook

When an assault slot is granted, the CombatDirector binds to the controller's ASC `OnDied` delegate. If the NPC dies while holding a slot, all slots are automatically released — no BT cleanup needed.

### Stale Entry Cleanup

- `CleanupInvalidControllers` removes dead controller references per territory (runs on each `RequestAssaultSlot`)
- `CleanupStaleTerritoryKeys` removes destroyed territory entries from SlotMap (runs on each `RequestAssaultSlot`)

### Integration with ControlSubsystem

The CombatDirector is a standalone strategic gate. BT tasks call it directly:

```
BTTask_RequestTerritoryPermission
  └── CombatDirector->RequestAssaultSlot(Territory, NPCController)
      └── Returns false → BT Failed (denied)
      └── Returns true → BT Succeeded (slot granted)

BTTask_ReleaseTerritoryPermission
  └── Reads TerritoryKey from blackboard
  └── CombatDirector->ReleaseAssaultSlot(Territory, NPCController)
```

---

## Subsystem Access Patterns

### C++

```cpp
UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
UTerritoryControlSubsystem* Control = GetWorld()->GetSubsystem<UTerritoryControlSubsystem>();
UTerritoryEconomySubsystem* Economy = GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>();
UTerritoryDiplomacySubsystem* Diplomacy = GetWorld()->GetSubsystem<UTerritoryDiplomacySubsystem>();
UTerritoryCombatDirector* Combat = GetWorld()->GetSubsystem<UTerritoryCombatDirector>();
```

### Blueprint

```
GetTerritoryRegistry(WorldContext)   → Registry
GetTerritoryControl(WorldContext)    → Control
GetTerritoryEconomy(WorldContext)    → Economy
GetTerritoryCombatDirector(WorldContext) → CombatDirector
```

Diplomacy has no static helper — access via:
```
GetGameInstanceSubsystem → Cast to UTerritoryDiplomacySubsystem
```
Or use `GetWorld()->GetSubsystem` in C++.

### Actor Shortcut

Any `ATerritoryVolume` can get its registry directly:
```cpp
UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
```

## Subsystem Initialization Order

```
1. World initialized
2. Subsystems created (engine-managed)
3. ATerritoryVolume::BeginPlay → Registry->RegisterTerritory
4. ATerritoryWorldState::BeginPlay → SyncSubsystemsFromReplicatedState
5. Subsystem timers start (server only)
```

Late-registering territories (spawned at runtime) are handled by `OnTerritoryRegistered` delegate — cities and WorldState subscribe to catch them.

## Cross-Subsystem Dependencies

| Subsystem | Depends On | Why |
|---|---|---|
| Control | Registry | Look up territory by tag |
| Control | Diplomacy | Check faction attitudes before capture |
| Control | CombatDirector | Check attack budget |
| Economy | Registry | Enumerate territories for income calc |
| Diplomacy | Narrative GameState | Sole authority for attitudes |
| CombatDirector | — | Standalone budget tracker |