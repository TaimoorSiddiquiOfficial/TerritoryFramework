# Blueprint Extension Guide — TerritoryFramework v0.2.0

> Complete reference for every Blueprint override point, delegate, and interface.
> **Critical:** some `BlueprintNativeEvent` overrides MUST call Parent (Super) — skipping Super silently breaks guard lifecycle, economy, or hierarchy propagation.

---

## Table of Contents

1. [Super-Call Quick Reference](#super-call-quick-reference)
2. [BlueprintNativeEvent Overrides — Super Required](#blueprintnativeevent-overrides--super-required)
3. [BlueprintNativeEvent Overrides — Super Optional](#blueprintnativeevent-overrides--super-optional)
4. [BlueprintImplementableEvent — BP Only, No Super](#blueprintimplementableevent--bp-only-no-super)
5. [BlueprintAssignable Delegates](#blueprintassignable-delegates)
6. [Interfaces](#interfaces)
7. [State Model Reference](#state-model-reference)
8. [Common Patterns](#common-patterns)

---

## Super-Call Quick Reference

| Event | Class | Super Required? | What Breaks Without Super |
|---|---|---|---|
| `OnOwnershipChanged` | `ATerritoryVolume` | **No** | Nothing — invariants run before this event |
| `OnStateChanged` | `ATerritoryVolume` | **No** | Nothing — invariants run before this event |
| `OnAllGuardsDefeated` | `ATerritoryVolume` | **YES — CRITICAL** | Territory stays Claimed with dead guards; capture stuck |
| `OnTerritoryInitialized` | `ATerritoryVolume` | **No** | Nothing — extension hook only |
| `OnPropertyCaptured` | `ATerritoryProperty` | **YES — HIGH** | Upgrade level retained by new owner; income not recalculated |
| `OnCityFullyCaptured` | `ATerritoryCity` | **YES — HIGH** | Income not recalculated; capital bonus (1000g) lost |
| `OnCityLost` | `ATerritoryCity` | **YES — HIGH** | Ownership cascade skipped; income not recalculated |
| `OnDistrictCapturedInCity` | `ATerritoryCity` | **No** | Nothing — notification only |
| `OnDistrictFullyCaptured` | `ATerritoryDistrict` | **YES — MEDIUM** | Capital district bonus (500g) lost |

---

## BlueprintNativeEvent Overrides — Super Required

These events have C++ `_Implementation` that performs **critical invariant work**. If your Blueprint override does NOT call Parent/Super, the invariants are silently skipped.

### `ATerritoryVolume::OnAllGuardsDefeated()`

| | |
|---|---|
| **Class** | `ATerritoryVolume` (and all subclasses: City, District, Property) |
| **Category** | `Territory\|Guards` |
| **When it fires** | After the last `RegisteredDefender` dies (guards AND any non-guard defenders registered via `RegisterDefender`) |
| **Parameters** | None |

**What the C++ `_Implementation` does:**
1. Calls `SetOwningFaction(FGameplayTag())` — clears the territory owner
2. Calls `SetControlProgress(0.f)` — resets capture progress
3. Calls `SetTerritoryState(Unclaimed)` — unless territory is Locked

**If you skip Super:**
- **CRITICAL:** Territory stays Claimed by the old faction even though all defenders are dead. Capture system cannot start because the territory appears owned. Economy continues paying income to the old owner. Guards do not respawn because the ownership never changed.

**Correct BP override pattern:**
```
Event: OnAllGuardsDefeated
  → Call Parent (Super::OnAllGuardsDefeated)
  → [Your custom logic: play defeat cinematic, notify quest system, etc.]
```

---

### `ATerritoryProperty::OnPropertyCaptured(FGameplayTag NewOwner)`

| | |
|---|---|
| **Class** | `ATerritoryProperty` |
| **Category** | `Territory\|Property` |
| **When it fires** | Every time ownership changes on a property (from `OnOwnershipChanged_Implementation`) |
| **Parameters** | `NewOwner` — the faction that now owns this property |

**What the C++ `_Implementation` does:**
1. Calls `SetUpgradeLevel(0)` — resets property upgrades, which also:
   - Triggers `MarkFactionDirty` for deferred income recalculation
   - Logs the level change

**If you skip Super:**
- **HIGH:** Property retains its upgrade level after capture. The new owner gets the old owner's upgrade income bonus. Economy recalculation is skipped for this property.

**Correct BP override pattern:**
```
Event: OnPropertyCaptured
  → Call Parent (Super::OnPropertyCaptured)
  → [Your custom logic: play capture VFX, drop loot, etc.]
```

---

### `ATerritoryCity::OnCityFullyCaptured(FGameplayTag CapturingFaction)`

| | |
|---|---|
| **Class** | `ATerritoryCity` |
| **Category** | `Territory\|Hierarchy` |
| **When it fires** | When ALL districts in the city are owned by the same faction |
| **Parameters** | `CapturingFaction` — the faction that now owns the entire city |

**What the C++ `_Implementation` does:**
1. Calls `Economy->MarkFactionDirty(CapturingFaction)` — queues income recalculation for next economy tick
2. If city has a capital district: awards **1000 gold** bonus to capturing faction via `AddToTreasury`

**If you skip Super:**
- **HIGH:** Income is never recalculated for the capturing faction. Capital district bonus (1000 gold) is never awarded.

**Correct BP override pattern:**
```
Event: OnCityFullyCaptured
  → Call Parent (Super::OnCityFullyCaptured)
  → [Your custom logic: city-wide celebration, unlock city quests, etc.]
```

---

### `ATerritoryCity::OnCityLost(FGameplayTag PreviousFaction)`

| | |
|---|---|
| **Class** | `ATerritoryCity` |
| **Category** | `Territory\|Hierarchy` |
| **When it fires** | When a faction that owned all districts loses at least one district |
| **Parameters** | `PreviousFaction` — the faction that lost the city |

**What the C++ `_Implementation` does:**
1. Checks if any other faction now owns ALL districts → if so, calls `SetOwningFaction` for that faction (cascade capture)
2. If no faction owns all districts → sets city state to `Contested`
3. Calls `Economy->MarkFactionDirty(PreviousFaction)` — queues income recalculation for the losing faction

**If you skip Super:**
- **HIGH:** City ownership cascade is skipped — new faction doesn't gain the city. Income recalculation for the losing faction is skipped. City may stay Claimed by old faction even after losing districts.

**Correct BP override pattern:**
```
Event: OnCityLost
  → Call Parent (Super::OnCityLost)
  → [Your custom logic: city loss notification, penalty, etc.]
```

---

### `ATerritoryDistrict::OnDistrictFullyCaptured(FGameplayTag CapturingFaction)`

| | |
|---|---|
| **Class** | `ATerritoryDistrict` |
| **Category** | `Territory\|Hierarchy` |
| **When it fires** | When ALL properties in the district are owned by the same faction |
| **Parameters** | `CapturingFaction` — the faction that now owns the entire district |

**What the C++ `_Implementation` does:**
1. If district is a capital (`bIsCapital`): awards **500 gold** bonus via `AddToTreasury`

**If you skip Super:**
- **MEDIUM:** Capital district bonus (500 gold) is not awarded.

**Correct BP override pattern:**
```
Event: OnDistrictFullyCaptured
  → Call Parent (Super::OnDistrictFullyCaptured)
  → [Your custom logic: district celebration, strategic buff, etc.]
```

---

## BlueprintNativeEvent Overrides — Super Optional

These events have C++ `_Implementation` that is either **empty** or does only cosmetic work. Safe to override without calling Super.

### `ATerritoryVolume::OnOwnershipChanged(FGameplayTag OldOwner, FGameplayTag NewOwner)`

| | |
|---|---|
| **Class** | `ATerritoryVolume` (and all subclasses) |
| **Category** | `Territory` |
| **When it fires** | After `SetOwningFaction` completes ALL invariant work (guards despawned/spawned, state set, replication data updated) |
| **Parameters** | `OldOwner`, `NewOwner` — faction tags before and after |

**C++ `_Implementation`:** Empty. All guard lifecycle, state transitions, and replication are handled in the non-virtual `SetOwningFaction` BEFORE this event fires.

**Super required:** **No.** Safe to override freely.

**Guaranteed state when this fires:**
- `GetOwningFaction()` returns `NewOwner`
- `GetTerritoryState()` returns `Claimed` (if NewOwner is valid) or `Unclaimed` (if invalid)
- Old guards are destroyed, new guards are spawned
- `OnTerritoryOwnershipChanged` delegate has NOT fired yet (fires after this event)

**Note for `ATerritoryProperty`:** This class overrides `OnOwnershipChanged_Implementation` to call `OnPropertyCaptured(NewOwner)` and broadcast `OnPropertyCapturedDelegate`. If you create a BP child of `ATerritoryProperty`, override `OnPropertyCaptured` instead — it has the Super-call requirements documented above.

---

### `ATerritoryVolume::OnStateChanged(ETerritoryState OldState, ETerritoryState NewState)`

| | |
|---|---|
| **Class** | `ATerritoryVolume` (and all subclasses) |
| **Category** | `Territory` |
| **When it fires** | After `SetTerritoryState` completes all invariant work (guard despawn/spawn for state transitions) |
| **Parameters** | `OldState`, `NewState` |

**C++ `_Implementation`:** Empty. Guard lifecycle for state transitions (Contested→Locked despawns guards, Contested→Claimed respawns guards) runs in the non-virtual `SetTerritoryState` BEFORE this event fires.

**Super required:** **No.**

**Important state change (Claimed → Contested):** When transitioning to Contested, the C++ code **clears OwningFaction**. `GetOwningFaction()` returns invalid during Contested state. The previous owner is cached in `PreviousOwningFaction` for RepNotify diff.

---

### `ATerritoryVolume::OnTerritoryInitialized()`

| | |
|---|---|
| **Class** | `ATerritoryVolume` (and all subclasses) |
| **Category** | `Territory` |
| **When it fires** | At the end of `BeginPlay`, after registration, save/load, and guard reconciliation |
| **Parameters** | None |

**C++ `_Implementation`:** Empty. Extension hook only.

**Super required:** **No.**

**Guaranteed state when this fires:**
- Territory is registered in the RegistrySubsystem
- Save data has been loaded (if available)
- Guards have been reconciled (despawned stale, spawned for current owner)
- Spatial index includes this territory

---

### `ATerritoryCity::OnDistrictCapturedInCity(ATerritoryVolume* District, FGameplayTag OldOwner, FGameplayTag NewOwner)`

| | |
|---|---|
| **Class** | `ATerritoryCity` |
| **Category** | `Territory\|Hierarchy` |
| **When it fires** | Every time any district within this city changes ownership |
| **Parameters** | `District` — the district actor, `OldOwner`, `NewOwner` |

**C++ `_Implementation`:** Log message only. Notification hook.

**Super required:** **No.**

**Note:** This fires BEFORE city-level capture checks. After this event, the city checks `AllDistrictsOwnedBy` and may fire `OnCityFullyCaptured` or `OnCityLost`.

---

## BlueprintImplementableEvent — BP Only, No Super

These events are implemented entirely in Blueprint. There is no C++ `_Implementation`. No Super call is possible or needed.

### `UTerritoryInfoWidget`

| Event | Parameters | When |
|---|---|---|
| `OnTerritoryBound(Territory)` | `ATerritoryVolume* Territory` | First time the widget binds to a territory. Populate initial UI data here. |
| `OnTerritoryOwnershipChanged(OldOwner, NewOwner)` | `FGameplayTag OldOwner, NewOwner` | Bound territory's ownership changed. Update owner display. |
| `OnTerritoryStateChanged(NewState)` | `ETerritoryState NewState` | Bound territory's state changed. Update state display (color, text). |

### `UTerritoryEconomyWidget`

| Event | Parameters | When |
|---|---|---|
| `OnEconomyUpdated(Faction, Snapshot)` | `FGameplayTag Faction`, `FTerritoryEconomySnapshot Snapshot` | Every economy tick (default 300s). Snapshot contains Gold, TotalIncome, TotalCosts, TerritoryCount. |
| `OnTransactionRecorded(Transaction)` | `FTerritoryTransaction Transaction` | Every treasury mutation. Transaction contains TransactionID, Faction, Type, Amount, BalanceAfter, GameTime, Reason. |

### `UTerritoryDebugWidget`

| Event | Parameters | When |
|---|---|---|
| `OnUpdateDebugText(DebugText)` | `FText DebugText` | Every 0.5 seconds when debug is enabled. Pre-formatted text with territory, economy, diplomacy, and capture summaries. |

### `ATerritoryProperty`

| Event | Parameters | When |
|---|---|---|
| `OnUpgradeLevelChanged(NewLevel)` | `int32 NewLevel` | RepNotify — fires on clients when UpgradeLevel replicates. Update visual model. |

---

## BlueprintAssignable Delegates

Bind to these in Blueprint Event Graph with custom event nodes. They fire at specific points in the mutation pipeline.

### `ATerritoryVolume` Delegates

| Delegate | Signature | Fires When | Guaranteed State |
|---|---|---|---|
| `OnTerritoryOwnershipChanged` | `(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)` | AFTER `SetOwningFaction` completes and AFTER `OnOwnershipChanged` BP event | All invariants done. Guards spawned. State is Claimed/Unclaimed. Replication data updated. |
| `OnTerritoryStateChangedDelegate` | `(ATerritoryVolume* Territory, ETerritoryState NewState)` | AFTER `SetTerritoryState` completes and AFTER `OnStateChanged` BP event | Guard lifecycle done. State is finalized. |
| `OnAllGuardsDefeatedDelegate` | `(ATerritoryVolume* Territory)` | AFTER `OnAllGuardsDefeated` BP event completes | Territory is Unclaimed (if Super was called). Progress is 0. |
| `OnGuardKilled` | `(ATerritoryVolume* Territory, AActor* Guard, AActor* Killer, int32 RemainingDefenders)` | Immediately after a defender dies, before all-guards-defeated check | Killer is best-effort (ASC avatar). RemainingDefenders is count AFTER removal. |

### `ATerritoryCity` Delegates

| Delegate | Signature | Fires When |
|---|---|---|
| `OnCityCapturedDelegate` | `(ATerritoryCity* City, FGameplayTag CapturingFaction)` | AFTER `OnCityFullyCaptured` BP event. Income recalculated. Capital bonus awarded. |
| `OnCityLostDelegate` | `(ATerritoryCity* City, FGameplayTag PreviousFaction)` | AFTER `OnCityLost` BP event. Cascade logic completed. |

### `ATerritoryDistrict` Delegates

| Delegate | Signature | Fires When |
|---|---|---|
| `OnDistrictCapturedDelegate` | `(ATerritoryDistrict* District, FGameplayTag OldOwner, FGameplayTag NewOwner)` | AFTER `OnDistrictFullyCaptured` BP event. Capital bonus awarded if applicable. |

### `ATerritoryProperty` Delegates

| Delegate | Signature | Fires When |
|---|---|---|
| `OnPropertyCapturedDelegate` | `(ATerritoryProperty* Property, FGameplayTag NewOwner)` | From `OnOwnershipChanged_Implementation`. Upgrade level already reset by Super. |

### `UTerritoryControlSubsystem` Delegates

| Delegate | Signature | Fires When |
|---|---|---|
| `OnTerritoryControlChanged` | `(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)` | AFTER `CompleteCapture` or `ForceCapture` completes. Ownership changed, state set to Claimed. |
| `OnCaptureAttempted` | `(FCaptureAttempt Attempt)` | After every `AttemptCapture` call. Attempt struct contains Result (Success/Locked/DefendersRemain/DiplomaticallyBlocked/AlreadyOwned/InvalidTerritory). |

### `UTerritoryEconomySubsystem` Delegates

| Delegate | Signature | Fires When |
|---|---|---|
| `OnEconomyTickFired` | `(FGameplayTag Faction, FTerritoryEconomySnapshot Snapshot)` | Every economy tick per faction. |
| `OnTransactionRecorded` | `(FTerritoryTransaction Transaction)` | Every treasury mutation (income, upkeep, upgrade cost, purchase, reward, manual). |

### `UTerritoryDiplomacySubsystem` Delegates

| Delegate | Signature | Fires When |
|---|---|---|
| `OnDiplomacyStateChanged` | `(FGameplayTag FactionA, FGameplayTag FactionB, EDiplomacyState NewState)` | After any diplomacy state change (war, peace, alliance, trade, ceasefire, expiration). |
| `OnDiplomacyEvent` | `(FDiplomacyEvent Event)` | After diplomacy events. Event contains EventType, FactionA, FactionB, GameTime. |
| `OnReputationChanged` | `(FGameplayTag Faction, int32 NewReputation)` | After reputation add/set. |

### `UTerritoryRegistrySubsystem` Delegates

| Delegate | Signature | Fires When |
|---|---|---|
| `OnTerritoryRegistered` | `(ATerritoryVolume* Territory, bool bWasUnregistered)` | On registration (`bWasUnregistered=false`) and unregistration (`bWasUnregistered=true`). |

---

## Interfaces

### `ITerritoryOwnershipInterface`

Query ownership state. Default implementations read from `ATerritoryVolume::OwnershipData`.

| Function | Returns | Notes |
|---|---|---|
| `GetTerritoryOwner()` | `FGameplayTag` | Returns `OwningFaction`. Invalid when Contested or Unclaimed. |
| `GetTerritoryControlProgress()` | `float` | 0.0–1.0 progress of current capture. |
| `IsTerritoryContested()` | `bool` | True when State == Contested. |
| `GetContestingFaction()` | `FGameplayTag` | Leading faction by capture progress. Updated each capture tick. |

### `ITerritoryEconomyInterface`

Query economy state. Default implementations read from `UTerritoryEconomySubsystem`.

| Function | Returns | Notes |
|---|---|---|
| `GetTreasury(Faction)` | `int32` | Current gold balance. |
| `GetPeriodicIncome(Faction)` | `int32` | Income per economy tick. |
| `CanAfford(Faction, Cost)` | `bool` | True if Treasury >= Cost. |

### `ITerritoryEventReceiverInterface`

Receive territory events. Implement on any actor that needs to react to territory changes.

| Function | When |
|---|---|
| `OnTerritoryControlChanged(TerritoryTag, OldOwner, NewOwner)` | Territory ownership changed. |
| `OnTerritoryContested(TerritoryTag, ContestingFaction)` | Territory became contested. |
| `OnTerritoryUncontested(TerritoryTag)` | Contesting ended (captured or decayed). |
| `OnTerritoryStateChanged(TerritoryTag, NewState)` | Territory state changed. |

---

## State Model Reference

```
Unclaimed ──(attacker enters)──→ Contested ──(progress >= 1.0)──→ Claimed
    ↑                                │                                │
    │                                │                                │
    └────(all guards die)────────────┘                                │
    ↑                                                                 │
    │                                                                 │
    └────(all guards die / force unclaim)─────────────────────────────┘

Claimed ──(LockTerritory)──→ Locked ──(TryUnlock)──→ Claimed or Unclaimed
    ↑                         │
    │                         │
    └─────────────────────────┘

Any State ──(ForceCapture)──→ Claimed (by NewOwner)
```

**Contested state:** `OwningFaction` is **cleared** (invalid). The territory has no owner while contested. `IsOwnedByFaction()` returns false for all factions. `GetContestingFaction()` returns the leading faction by progress.

---

## Common Patterns

### Pattern: Custom capture reward

```
ATerritoryVolume BP → Event Graph:
  OnOwnershipChanged:
    → [No Super needed]
    → If NewOwner == PlayerFaction:
        → AddToTreasury(PlayerFaction, 200, "Capture reward")
```

### Pattern: Quest-gated territory

```
ATerritoryVolume BP:
  LockConditions: [QuestComplete_Q001]
  → Territory stays Locked until quest Q001 completes
  → TryUnlock checks all conditions automatically
```

### Pattern: Custom guard behavior on defeat

```
ATerritoryVolume BP → Event Graph:
  OnAllGuardsDefeated:
    → MUST Call Parent (Super::OnAllGuardsDefeated)
    → PlayDefeatCinematic
    → NotifyQuestSystem("TerritoryLost")
```

### Pattern: Hierarchy cascade reaction

```
ATerritoryCity BP → Event Graph:
  OnCityFullyCaptured:
    → MUST Call Parent (Super::OnCityFullyCaptured) [income + capital bonus]
    → SpawnCelebrationVFX
    → UnlockCityContent

  OnCityLost:
    → MUST Call Parent (Super::OnCityLost) [cascade + income recalc]
    → NotifyStrategicMap
```

### Pattern: Economy HUD widget

```
UTerritoryEconomyWidget BP:
  Event Graph:
    OnEconomyUpdated:
      → SetGoldText(Snapshot.Treasury)
      → SetIncomeText(Snapshot.TotalIncome)
      → SetCostsText(Snapshot.TotalCosts)

    OnTransactionRecorded:
      → AnimateGoldChange(Transaction.Amount)
      → ShowTransactionReason(Transaction.Reason)
```

### Pattern: Property upgrade visual

```
ATerritoryProperty BP:
  OnPropertyCaptured (override):
    → Call Parent (resets UpgradeLevel to 0)
    → SwapMesh(DefaultMesh)
    → ResetUpgradeVFX

  OnUpgradeLevelChanged (BP event):
    → SwapMesh(UpgradeMeshes[NewLevel])
    → PlayUpgradeVFX(NewLevel)
```

### Pattern: Map marker customization

```
UTerritoryMapMarker BP:
  GetMarkerColor (override):
    → If Locked → return transparent (invisible marker)
    → If Contested → return pulsing orange
    → If owned by player → return green
    → If owned by enemy → return red

  GetMarkerDisplayText (override):
    → Return territory display name
    → Set OutSubtitleText to owner faction or "Contested"

  MarkerOnPaint (override):
    → Call Parent for default rendering
    → Draw territory outline if bDrawTerritoryOutline is true
```

### Pattern: Guard spawn point with patrol

```
ATerritoryGuardSpawnPoint placed in level:
  PatrolRoute: [Node0(Location, WaitTime=2), Node1(Location, WaitTime=5), Node2(Location, WaitTime=2)]
  bLoopPatrol: true
  ReserveSlots: 2
  Priority: 75

  → Guards spawn at this point and follow the patrol route
  → When a guard dies, a reserve guard spawns (up to ReserveSlots times)
  → Patrol route data available via GetPatrolRouteAsTransforms() for Narrative activities
```
