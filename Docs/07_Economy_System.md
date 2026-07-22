# Economy System — Treasury, Income, Transactions, Upgrades

## Overview

Each faction has a treasury with:
- **Gold**: accumulated currency
- **IncomePerTick**: income from owned territories
- **CostsPerTick**: guard upkeep from owned territories
- **TerritoryCount**: number of owned territories

Economy ticks fire every `EconomyTickIntervalSeconds` (default 300s = 5 min) on the server.

## Treasury API

### Adding Gold

```cpp
// C++
Economy->AddToTreasury(Faction, 1000, TEXT("Quest reward"), ETerritoryTransactionType::Reward);

// Blueprint
GetTerritoryEconomy → AddToTreasury(Faction, 1000, "Quest reward", Reward)
```

### Spending Gold

```cpp
// C++
if (Economy->TryDebitTreasury(Faction, 500, TEXT("Upgrade blacksmith"), ETerritoryTransactionType::UpgradeCost))
{
    // Success — gold deducted
}

// Blueprint
GetTerritoryEconomy → TryDebitTreasury(Faction, 500, "Upgrade", UpgradeCost) → Branch
```

### Checking Balance

```cpp
int32 Gold = Economy->GetTreasury(Faction);
int32 Income = Economy->GetIncome(Faction);
int32 Costs = Economy->GetCosts(Faction);
bool bCanAfford = Economy->CanAfford(Faction, Cost);
```

## Income Calculation

Income is automatically recalculated when:
- Territory is registered
- Territory ownership changes
- Property is upgraded

### Income Formula

```
FactionIncome = Sum of:
  For each owned TerritoryVolume:
    If ATerritoryProperty: GetEffectiveIncome() = PeriodicIncome + (UpgradeLevel × IncomeBonusPerLevel)
    Else: GetPeriodicIncome()

FactionCosts = Sum of:
  For each owned TerritoryVolume: GetGuardCost()

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
  For each faction with territories:
    NetIncome = IncomePerTick - CostsPerTick
    Treasury += NetIncome
    If Treasury < 0: Treasury = 0

    Record Income transaction (+IncomePerTick)
    Record GuardUpkeep transaction (-CostsPerTick)

    Broadcast OnEconomyTickFired(Faction, Snapshot)
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
- `ATerritorySavableData` (single-player — SaveGame properties)

On load, treasuries are **directly assigned** (no artificial transactions created).
