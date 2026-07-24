# Economy System — Income, Transactions, Upgrades

## Overview

Each faction's wealth is the **aggregate of all online faction members' `UInventoryComponent::Currency`** (NarrativePro). There is no separate gold/treasury storage — faction economy flows directly to/from player inventories via the economy tick.

The subsystem tracks per faction:
- **IncomePerTick**: income from owned territories
- **CostsPerTick**: guard upkeep from owned territories
- **TerritoryCount**: number of owned territories

Economy ticks fire every `EconomyTickIntervalSeconds` (default 300s = 5 min) on the server. Each tick distributes net income (income - upkeep) evenly to all online faction members via `UInventoryComponent::AddCurrency()`.

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

// Get all online faction members
TArray<ANarrativeCharacter*> Members = Economy->GetFactionMembers(Faction);
int32 Aggregate = Economy->GetFactionAggregateCurrency(Faction);
```

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
     a. Apply income: Treasury.Gold += IncomePerTick
        Record Income transaction (BalanceAfter = current gold)

     b. Apply upkeep: clamp to available gold
        If CostsPerTick > Treasury.Gold: ActualUpkeep = Treasury.Gold
        Treasury.Gold -= ActualUpkeep
        Record GuardUpkeep transaction (negative amount, partial noted in Reason)

     c. Broadcast OnEconomyTickFired(Faction, Snapshot)

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

On load, treasuries are **directly assigned** via `SetFactionTreasury` (exact restore, no additive double-counting).
