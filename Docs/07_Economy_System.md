# Economy System — Income, Transactions, Upgrades

## Overview

Each faction's wealth is the **aggregate of all online faction members' `UInventoryComponent::Currency`** (NarrativePro). There is no separate gold/treasury storage — faction economy flows directly to/from player inventories via the economy tick.

The subsystem tracks per faction:
- **IncomePerTick**: income from owned territories
- **CostsPerTick**: guard upkeep from owned territories
- **TerritoryCount**: number of owned territories

Economy ticks fire every `EconomyTickIntervalSeconds` (default 300s = 5 min) on the server. Each tick credits income across online faction members, then debits affordable guard upkeep via `UInventoryComponent::AddCurrency()`.

## Wealth API

### Adding to Faction Wealth

```cpp
// C++ — distributes evenly across online faction members' Currency
Economy->AddToTreasury(Faction, 1000, TEXT("Quest reward"), ETerritoryTransactionType::ManualCredit);
```

### Spending Faction Wealth

```cpp
// C++ — debits proportionally from faction members' inventories
if (Economy->TryDebitTreasury(Faction, 500, TEXT("Upgrade"), ETerritoryTransactionType::ManualDebit))
{
    // Success — deducted
}
```

### Checking Balance

```cpp
// Faction wealth = aggregate of all online members' Currency
int32 Wealth = Economy->GetTreasury(Faction);
// ^ reads live from player inventories, no separate storage

int32 Income = Economy->GetIncome(Faction);
int32 Costs = Economy->GetCosts(Faction);
bool bCanAfford = Economy->CanAfford(Faction, Cost);
```

Faction member enumeration and aggregate-currency helpers are private implementation details. Use `GetTreasury()` for the public aggregate query.

## Income Calculation

Income recalculation is **deferred** — ownership changes mark factions dirty, actual recalculation runs once per economy tick. This avoids redundant O(N) scans during capture cascades (property → district → city = 3× recalc reduced to 1×).

Triggers for deferred recalculation:
- Territory registered/unregistered
- Territory ownership changes (via `MarkFactionDirty`)
- Property upgraded (via `SetUpgradeLevel` → `MarkFactionDirty`)
- Property captured (via `OnPropertyCaptured` → `SetUpgradeLevel(0)` → `MarkFactionDirty`)

### Income Formula (Leaf-Only)

Only `ATerritoryProperty` contributes income — cities and districts are containers, not income sources.

```
FactionIncome = Sum of:
  For each owned ATerritoryProperty:
    GetEffectiveIncome() = PeriodicIncome + (UpgradeLevel × IncomeBonusPerLevel)
    Capital district multiplier applied if property's district has bIsCapital

FactionCosts = Sum of:
  For each owned TerritoryVolume with configured guards (GuardSpawnCount > 0):
    GetGuardCost()

NetPerTick = FactionIncome - FactionCosts
```

## Transaction Ledger

Every economy mutation records a transaction:

| Field | Type | Example |
|---|---|---|
| TransactionID | FGuid | Auto-generated |
| Faction | GameplayTag | Narrative.Factions.Heroes |
| Type | ETerritoryTransactionType | Income, GuardUpkeep, UpgradeCost, Reward... |
| Amount | int32 | +100 (credit) or -50 (debit) |
| BalanceAfter | int32 | Treasury balance after transaction |
| GameTime | double | Accumulated game time |
| Reason | FString | "Quest reward", "Property upgrade", "Guard upkeep" |
| SourceTerritory | GameplayTag | Optional territory that generated the transaction |

### Querying History

```cpp
// Get last 50 transactions for Heroes faction
TArray<FTerritoryTransaction> History = Economy->GetTransactionHistory(HeroesFaction, 50);
```

## Property Upgrades

Properties can be upgraded to increase their income.

```cpp
// C++
if (ATerritoryProperty* Property = Cast<ATerritoryProperty>(Territory))
{
    if (Property->CanUpgrade())
    {
        int32 Cost = Property->GetUpgradeCost();  // e.g., 500
        if (Economy->CanAfford(OwnerFaction, Cost))
        {
            Property->TryUpgrade();  // Debits treasury, increments level
        }
    }
}

// Blueprint
Property → CanUpgrade() → Branch
  True → GetUpgradeCost() → Economy → CanAfford(Faction, Cost)
    True → TryUpgrade()
```

### Upgrade Cost Formula

```
UpgradeCost = UpgradeCostPerLevel × (CurrentLevel + 1)
```

Example: UpgradeCostPerLevel=500
- Level 0 → 1: costs 500
- Level 1 → 2: costs 1000
- Level 2 → 3: costs 1500

### Effective Income

```
EffectiveIncome = PeriodicIncome + (UpgradeLevel × IncomeBonusPerLevel)
```

Example: PeriodicIncome=50, IncomeBonusPerLevel=25
- Level 0: 50
- Level 1: 75
- Level 2: 100
- Level 3: 125

## Economy Tick Flow

```
Every EconomyTickIntervalSeconds (server only):
  1. Process dirty factions:
     For each faction in DirtyFactions:
       RecalculateIncome(faction) → updates IncomePerTick, CostsPerTick, TerritoryCount
     Clear DirtyFactions

  2. For each faction with treasury:
     a. Split IncomePerTick across online faction members and record the resulting balance
     b. Clamp upkeep to the post-income aggregate balance
     c. Debit that affordable upkeep across members, redistributing shares when one member is short
     d. Record the actual (possibly partial) upkeep and resulting balance
     e. Broadcast OnEconomyTickFired(Faction, Snapshot)

  3. Trim TransactionLedger to MaxTransactionHistory (once, not per-faction)
```

## Developer Settings

| Setting | Default | Range |
|---|---|---|
| EconomyTickIntervalSeconds | 300 (5 min) | 10-3600 |
| DefaultTerritoryIncome | 100 | 0+ |
| DefaultGuardCost | 50 | 0+ |

## Save/Load

Economy state is saved through:
- `ATerritoryWorldState` (multiplayer — replicated arrays)
- `ATerritorySavableData` (single-player — SaveGame properties, **DEPRECATED** — use WorldState)

On load, `SetFactionTreasury` restores only `IncomePerTick`, `CostsPerTick`, and `TerritoryCount`. Currency belongs to Narrative player inventories and is not restored by TerritoryFramework. Offline and NPC-only factions therefore have no persistent TerritoryFramework balance.

Credit and periodic transaction records are suppressed when no online member receives currency.
