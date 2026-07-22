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
| OnTerritoryRegistered | (ATerritoryVolume*) | After RegisterTerritory |
| OnTerritoryUnregistered | (ATerritoryVolume*) | After UnregisterTerritory |

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
   └── Checks attack budget via CombatDirector
   └── Returns false if budget exhausted

2. Capture tick (every CaptureTickInterval, server-only)
   └── For each contested territory:
       ├── Count attackers present in bounds
       ├── Progress += Attackers × TickRate
       ├── If Progress >= 1.0 → ForceCapture(Territory, ContestingFaction)
       └── Broadcast OnCaptureProgressUpdated

3. UnregisterAttacker(Territory, Actor, Faction)
   └── If no attackers remain, progress decays back to 0

4. On capture complete:
   └── SetOwningFaction(NewFaction)
   └── SetTerritoryState(Controlled)
   └── Broadcast OnTerritoryControlChanged
   └── SpawnGuards() for new owner
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
| Success | Capture initiated |
| AlreadyOwned | Attacker owns this territory |
| Locked | bStartsLocked = true |
| DiplomaticallyBlocked | Factions are Friendly |
| DefendersRemain | Guards still alive |
| NoAttackBudget | CombatDirector denied — too many attackers |

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

| Delegate | Signature |
|---|---|
| OnCaptureStarted | (Territory*, Faction) |
| OnCaptureProgressUpdated | (Territory*, Faction, Progress) |
| OnCaptureCompleted | (Territory*, OldFaction, NewFaction) |

---

## UTerritoryEconomySubsystem

Manages faction treasuries, income calculation, and transaction ledger.

### Treasury Structure

```cpp
struct FTerritoryTreasury
{
    int32 Gold;
    int32 IncomePerTick;
    int32 CostsPerTick;
    int32 TerritoryCount;
};
```

### API

See [07_Economy_System.md](07_Economy_System.md) for full economy documentation.

### Key Behaviors

1. **Income recalculation** — triggered on territory register, ownership change, property upgrade
2. **Transaction ledger** — every mutation (add/debit) records a `FTerritoryTransaction`
3. **Server-only timer** — economy tick fires every `EconomyTickIntervalSeconds` (default 300s)
4. **OnTransactionRecorded** — broadcast after every transaction (UI binds to this)
5. **MaxTransactionHistory** — caps stored transactions (default 500 per faction)

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

1. **Attitude bridge** — `SetNarrativeAttitude()` directly sets Narrative attitude (no early-return bug)
2. **Treaty expiration** — timer checks every `TreatyExpirationCheckInterval` (default 10s)
3. **Dead treaty cleanup** — `OnFactionAttitudeChanged` removes treaty records when Narrative attitude shifts away from the treaty type
4. **Reset to Neutral** — `SetDiplomacyState(None)` resets Narrative attitude to Neutral

---

## UTerritoryCombatDirector

Manages attack permissions — limits how many attackers a faction can field against a single territory.

### API

#### Actions (BlueprintAuthorityOnly)

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| RequestAttackPermission | Territory, Faction, Actor | bool | Returns false if budget exhausted |
| ReleaseAttackPermission | Territory, Faction, Actor | void | Frees a slot |
| ReleaseAllPermissions | Territory, Faction | void | Frees all slots for faction |

#### Queries (BlueprintPure)

| Function | Returns |
|---|---|
| GetActiveAttackers(Territory, Faction) | int32 |
| GetAttackBudget(Territory, Faction) | int32 |
| HasAttackBudget(Territory, Faction) | bool |

### Budget Source

Each `ATerritoryVolume` has `MaxConcurrentAttackers` (default 4). The CombatDirector enforces this:

```
If ActiveAttackers(Faction) >= MaxConcurrentAttackers → deny new attacker
```

### Integration with ControlSubsystem

`RegisterAttacker` in ControlSubsystem calls `CombatDirector->RequestAttackPermission` internally:

```
ControlSubsystem::RegisterAttacker
  └── CombatDirector->RequestAttackPermission
      └── Returns false → RegisterAttacker returns false
      └── Returns true → attacker added to capture flow
```

### Delegates

| Delegate | Signature |
|---|---|
| OnAttackPermissionGranted | (Territory*, Faction, Actor) |
| OnAttackPermissionDenied | (Territory*, Faction, Actor) |

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