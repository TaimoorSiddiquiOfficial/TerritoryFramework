# Economy System — Income, Transactions, Upgrades

## Overview

Narrative Pro's `UNarrativeInventoryComponent::Currency` is the only currency balance. TerritoryFramework does not maintain `FactionGold`, a faction wallet, or a second saved currency value. The economy subsystem calculates rates and applies transactions to an explicit Narrative inventory account.

The subsystem tracks per faction:
- **IncomePerTick**: income from owned territories
- **CostsPerTick**: guard upkeep from owned territories
- **TerritoryCount**: number of owned territories

Economy ticks fire every `EconomyTickIntervalSeconds` (default 300s = 5 min) on the server. The configured `IncomePayoutPolicy` determines the settlement accounts. `EqualSplitOnlineMembers` discovers only online `ANarrativePlayerCharacter` accounts with exact Narrative faction membership and sorts them deterministically. Guards, counterattackers, vendors, companions, and other NPC inventories are never automatic income recipients or upkeep payers. An NPC-backed faction account is eligible only when server gameplay explicitly registers it as `SharedNarrativeAccount` or `FactionLeader`.

## Wealth API

### Credit and Debit API

```cpp
// C++ — credit one explicit Narrative inventory account
Economy->CreditCurrency(Beneficiary, 1000, Faction, TEXT("Quest reward"),
    ETerritoryTransactionType::ManualCredit);

// For territory-generated income, select an explicit distribution policy.
Economy->CreditCurrencyToFaction(Faction, 1000,
    ETerritoryIncomePayoutPolicy::EqualSplitOnlineMembers,
    TEXT("Territory reward"), ETerritoryTransactionType::Reward);

// SharedNarrativeAccount and FactionLeader require an explicit live account.
Economy->RegisterFactionCurrencyAccount(Faction,
    ETerritoryIncomePayoutPolicy::SharedNarrativeAccount, FactionAccountActor);
```

`GetOnlineFactionPlayers(Faction)` exposes the automatic player settlement cohort for diagnostics and UI. Explicit-account policies fail closed when their registered account is unavailable, changes world/faction, or a supplied preferred beneficiary is not that exact registered actor; they do not silently redirect a faction payout into arbitrary member or guard inventories. `PreferredBeneficiary` is an override only for `CapturingPlayer` routing.

### Spending Currency

```cpp
// C++ — debits only the requesting player's Narrative inventory
if (Economy->TryDebitCurrency(Requester, 500, Faction, TEXT("Upgrade"),
    ETerritoryTransactionType::UpgradeCost))
{
    // Success — deducted
}
```

### Checking Balance

```cpp
int32 Currency = Economy->GetActorCurrency(Requester);
bool bCanAfford = Economy->CanActorAfford(Requester, Cost);

int32 Income = Economy->GetIncome(Faction);
int32 Costs = Economy->GetCosts(Faction);
```

`GetTreasury`, `CanAfford`, `AddToTreasury`, and `TryDebitTreasury` are deprecated compatibility functions and do not mutate a wallet. New gameplay code must provide the exact payer or beneficiary actor.

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
  For each owned TerritoryVolume:
    GetGuardCost() × GetDesiredGuardCount()

NetPerTick = FactionIncome - FactionCosts
```

`GuardCost` is recurring upkeep only. `GuardRecruitmentCost` is the one-time price used when an absolute desired target is raised. Both default to 50 but are independently authored. Lowering a target has no refund; it immediately reduces the calculated upkeep rate.

Staffing mutations call `RecalculateIncome` immediately and publish the new rate snapshot to the existing `ATerritoryWorldState`. Ownership/upgrade cascades remain deferred through the dirty-faction reducer. Clients and late joiners read income/cost/territory-count and transaction history from the replicated WorldState projection; Narrative inventory remains the balance authority.

## Transaction Ledger

Every economy mutation records a transaction:

| Field | Type | Example |
|---|---|---|
| TransactionID | FGuid | Auto-generated |
| Faction | GameplayTag | Narrative.Factions.Heroes |
| Type | ETerritoryTransactionType | Income, GuardUpkeep, UpgradeCost, Reward... |
| Amount | int32 | +100 (credit) or -50 (debit) |
| BalanceAfter | int32 | Narrative account balance after transaction |
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
        if (Economy->CanActorAfford(Requester, Cost))
        {
            Property->TryUpgrade(Requester);  // Debits Requester's Narrative inventory
        }
    }
}

// Blueprint
Property → CanUpgrade() → Branch
  True → GetUpgradeCost() → Economy → CanActorAfford(Requester, Cost)
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

   2. For each faction with tracked rates:
      a. Resolve one policy-specific settlement cohort
      b. Apply IncomePayoutPolicy to distribute IncomePerTick
      c. Debit affordable upkeep from the same settlement cohort
         - EqualSplitOnlineMembers: matching online player characters only
         - SharedNarrativeAccount/FactionLeader: the exact registered account only
         - Guards and other NPCs: never automatic accounts
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

On load, `SetFactionTreasury` restores only `IncomePerTick`, `CostsPerTick`, and `TerritoryCount`. Currency belongs to Narrative inventory accounts and is not restored by TerritoryFramework. Explicit shared/leader registrations are live references only and must be registered again after their account actors stream or respawn.

Credits reject overflow rather than wrapping. Equal distribution pays only what online Narrative player accounts can store and logs any remainder; no transaction is recorded for currency that was not actually delivered. Runtime account registrations are deliberately not saved as UObject pointers and must be restored by the owning game system after load/streaming.

## Closed Account-Routing Loopholes

- Spawning extra same-faction guards cannot dilute or steal a player's territory income.
- A wealthy guard or counterattacker cannot subsidize upkeep or conceal a player deficit.
- A missing explicit shared/leader account cannot fall back to whichever faction NPC loaded first.
- Income and upkeep resolve against the same periodic policy cohort, avoiding a recipient/payer mismatch.
- Multiple online players split deterministically; one player cannot be paid twice through both pawn enumeration and NPC enumeration.
