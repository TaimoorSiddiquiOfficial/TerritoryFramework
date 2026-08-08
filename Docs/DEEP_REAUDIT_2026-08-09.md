# TerritoryFramework Deep Re-Audit — 2026-08-09

## Scope

Full audit of TerritoryFramework plugin: 51 headers, 50 implementations, 3 test files (89 automation tests), all 25 docs. Blueprint override points, interface docs, and doc-vs-code accuracy verified. Compared against AGENTS.md authority map and NarrativePro foundation.

Method: 5 parallel scouts (Core/Capture, Economy, Diplomacy/Counterattack, Guards/NPC, Save/BP-API) plus direct header/doc reading. Adversarial refutation applied to all P0/P1 claims.

---

## Summary

| Severity | Count | Description |
|---|---|---|
| P0 | 0 | No release-blocking code bugs found |
| P1 | 6 | Logic/replication concerns needing investigation |
| P2 | 14 | Doc-code mismatches, missing BP docs, dead fields, minor gaps |
| P3 | 5 | Intentional limits / known incomplete features |
| **Total** | **25** | |

**Test count:** 89 automation tests confirmed via grep.
**Authority map:** All AGENTS.md §4.1 authorities verified intact — no duplicate capture, faction, wallet, save, navigation, map, HUD stack, AI controller, or BT authority was introduced.

---

## P1 — High Priority Findings

### P1-01: Three WorldState live handlers omit ForceNetUpdate — client treaty/reputation/capture staleness

**File:** `TerritoryWorldState.cpp:704, 728, 749`

`OnDiplomacyChangedLive`, `OnReputationChangedLive`, and `OnTerritoryControlChangedLive` mutate `ReplicatedTreaties`/`ReplicatedReputation`/`ReplicatedCaptureSummaries` but never call `ForceNetUpdate()`. By contrast, `OnEconomyTickLive` (line 663), `OnTransactionRecordedLive` (line 701), and `OnAssaultChangedLive` (line 646) all do.

**Impact:** Diplomacy, reputation, and capture-summary changes are held until UE's periodic dirty-property sweep triggers. Clients observe stale treaty/reputation state until some other replicated property on the actor dirties. Not a permanent desync — latency only.

**Fix:** Add `ForceNetUpdate()` at the end of each of these three handlers.

---

### P1-02: Capture rewards hardcode EqualSplitOnlineMembers, ignoring configured IncomePayoutPolicy

**File:** `TerritoryHierarchy.cpp:213-215` (city bonus 1000g), `TerritoryHierarchy.cpp:553-555` (capital district bonus 500g)

Capital-city and capital-district capture bonuses hardcode `ETerritoryIncomePayoutPolicy::EqualSplitOnlineMembers`, regardless of the subsystem's configured `IncomePayoutPolicy`. A project configured for `SharedNarrativeAccount` gets periodic income routed to the shared account but one-shot capture bonuses split among online players — inconsistent payout semantics.

**Fix:** Use the territory/economy subsystem's configured payout policy, or at minimum pass the faction's configured policy.

---

### P1-03: SetControlProgress is BlueprintCallable but bypasses atomic commit path

**File:** `TerritoryVolume.h:169` (UFUNCTION), `TerritoryVolume.cpp:958-963` (implementation)

`SetControlProgress(float)` writes directly to `OwnershipData.ControlProgress` — no `CommitOwnershipData`, no state reconciliation, no events, no `ForceNetUpdate()`. The actual capture flow never calls it (uses `CommitCaptureReadModel` instead). A BP project calling this creates inconsistent state: progress changes without contested-state commitment or event firing.

**Fix:** Either route through `CommitOwnershipData`, or mark the BP node as deprecated, or document the hazard. The sibling `SetContestingFaction` (not UFUNCTION, correctly non-BP-accessible) sets the precedent — these inline mutators should not be BlueprintCallable.

---

### P1-04: CommitOwnershipData lacks reentrancy guard

**File:** `TerritoryVolume.cpp:786`

`CommitOwnershipData` checks `HasAuthority()` but does NOT check `bTransitionInProgress` before proceeding. `SetOwningFactionWithContext` (line 727) and `SetTerritoryState` (line 966) both guard on `bTransitionInProgress`, but `CommitOwnershipData` itself sets `bTransitionInProgress = true` only at line 811 without first checking it's false.

During the event broadcast inside `CommitOwnershipData` (lines 862-869, while `bTransitionInProgress=true`), any delegate listener that calls back into `CommitOwnershipData` would overwrite `OwnershipData` mid-transition.

**Likelihood:** Low — requires specific delegate wiring to trigger reentrant call. Default code does not exhibit this.

**Fix:** Add `if (bTransitionInProgress) return false;` guard at the top.

---

### P1-05: Server RPC _Validate functions inconsistently validate target actors

**File:** `TerritoryPlayerManagementComponent.cpp:241-410`

`ServerRequestSetGuardTarget_Validate` (line 241) and `ServerRequestSetGuardTargetForTerritory_Validate` (line 267) null-check the target actor. But `ServerRequestPurchaseGuards_Validate` (line 290), `ServerRequestPurchaseGuardsForDistrict_Validate` (line 323), `ServerRequestRemoveGuards_Validate` (line 354), `ServerRequestRemoveGuardsForDistrict_Validate` (line 385) check ONLY count + RequestId, never the target actor.

**Impact:** Not exploitable — `_Implementation` bodies null-check safely. But a client can spam validated RPCs with null/invalid actor targets that pass `_Validate` and only fail in `_Implementation`.

**Fix:** Add target-actor null checks to all six `_Validate` functions for consistency.

---

### P1-06: OnRep_OwnershipData fires synthetic transitions on late join

**File:** `TerritoryVolume.cpp:228-268`

`PreviousOwningFaction` defaults to empty tag, `PreviousState` defaults to `Unclaimed`. On initial replication (late join), a client whose territory is already `Claimed` fires `OnOwnershipChanged(empty, actual_owner)` and `OnStateChanged(Unclaimed, Claimed)`.

**Impact:** Every late-join client sees a full ownership/state change event for every owned territory. Downstream listeners that treat these as real transitions (play capture VFX, log analytics) fire spuriously on join.

**Fix:** Add an `bInitialReplication` flag to suppress events on first RepNotify. Standard UE pattern.

---

## P2 — Documentation and Minor Code Issues

### P2-01: Doc 03 states Contested→Unclaimed cleared by OnAllGuardsDefeated — contradicts actual behavior

**File:** `Docs/03_Core_Actors.md` — State Transition Logic section

**Doc says:** "Contested → Unclaimed (all defenders dead): Cleared by OnAllGuardsDefeated Super call"

**Actual behavior:** Guard defeat leaves the owner intact and vulnerable. The territory must still be captured through the normal capture flow. The `OnAllGuardsDefeated_Implementation` only sets `DefenderCount=0` and calls `ForceNetUpdate()` — it does NOT change ownership or state.

**Fix:** Update doc 03 to match actual behavior and the Blueprint_Extension_Guide.

---

### P2-02: Doc 03 still documents removed CRC32 GUID fallback

**File:** `Docs/03_Core_Actors.md` — Startup GUID Fallback note

**Doc says:** "If TerritoryGUID is not baked, a deterministic GUID is derived from TerritoryTag via FCrc::StrCrc32."

**Actual (v0.2.5 P1-05 fix):** CRC fallback was removed. Missing GUIDs log an error and fail closed.

**Fix:** Remove the fallback note from doc 03.

---

### P2-03: Doc 03 lists OnUpgradeLevelChanged as BlueprintNativeEvent — actual is BlueprintImplementableEvent

**File:** `Docs/03_Core_Actors.md` vs `TerritoryHierarchy.h:168`

**Fix:** Correct the specifier to BlueprintImplementableEvent (no C++ implementation, no Super call needed).

---

### P2-04: OnAllGuardsDefeated Super requirement overstated as "CRITICAL"

**File:** `Docs/03_Core_Actors.md` — Events table

Doc says Super is "YES — CRITICAL" and skipping it causes "Territory stays Claimed with dead guards; capture stuck."

**Actual code** (`TerritoryVolume.cpp:1128-1137`): `DefenderCount` is already 0 (set by `UnregisterDefender` before `OnAllGuardsDefeated` fires). The `_Implementation` redundantly writes 0 and calls `ForceNetUpdate()`. Skipping Super misses only the `ForceNetUpdate()` call — it does NOT cause capture to be stuck.

**Fix:** Change Super requirement to "Recommended" and fix the impact description.

---

### P2-05: Five phantom BlueprintLibrary functions documented in API reference

**File:** `Docs/14_API_Reference.md` lines 58-64

Lists: `GetDistrictsForCity`, `GetPropertiesForDistrict`, `GetOwningDistrict`, `GetOwningCity`, `GetDistrictByTag`. Grep against `TerritoryBlueprintLibrary.h` returns **zero matches** for all five. These are phantom API entries.

**Fix:** Remove the five phantom entries and replace with the actual hierarchy helpers that exist.

---

### P2-06: ~25 actual BlueprintLibrary functions undocumented in API reference

**File:** `Docs/14_API_Reference.md` TerritoryBlueprintLibrary table

The table lists ~15 functions. The header declares ~40. Undocumented: `GetTerritoryDiplomacy`, `GetFactionIncome`, `GetTerritoryCount`, `GetFactionTerritoryCount`, `IsTerritoryAtLocation`, `GetTerritoryState`, `GetCaptureProgress`, `GetTreatyState`, `IsAllied`, `IsAtWar`, `GetActorFactions`, `IsActorInFaction`, `GetActorPrimaryFaction`, `AreActorsAllied`, `GetChildTerritories`, `GetAllCities`, `GetAllDistricts`, `GetCityForDistrict`, `DoesFactionControlCity`, `GetFactionCityCount`, `GetFactionDistrictCount`, `GetCapitalDistricts`, `PrintTerritoryDebug`, `PrintAllTerritoryDebug`, `GetFriendlyTagDisplayName`.

**Fix:** Complete the table to cover the full BlueprintLibrary surface.

---

### P2-07: FTerritoryTransaction.SourceTerritory is a dead field — never populated

**File:** `TerritoryTypes.h:199-200` (declared), `TerritoryEconomySubsystem.cpp:230-250` (never set)

Every recorded transaction has an empty `SourceTerritory`. The field exists but is never populated by `RecordCurrencyTransaction`.

**Fix:** Either populate it in the transaction recorder (pass the territory tag from callers), or document it as reserved/unused.

---

### P2-08: bClearCaptureState is a dead field — declared, never read

**File:** `TerritoryMutationTypes.h:55`, `TerritoryControlSubsystem.cpp:776`

`FTerritoryMutationRequest::bClearCaptureState` is BlueprintReadWrite but never checked in `ApplyTerritoryMutation`. Comment at line 776: "regardless of bClearCaptureState." Capture state is always cleared on success. A BP setting `bClearCaptureState=false` expecting state to persist will be silently ignored.

**Fix:** Either implement the flag, or mark deprecated with a Blueprint warning.

---

### P2-09: EqualSplit front-loads remainder — name overstates equality

**File:** `TerritoryEconomySubsystem.cpp:510-516`

`DivideAndRoundUp(Remaining, AccountsRemaining)` gives the first sorted player more than others. E.g., 10 gold over 3 players → 4/3/3, not 3/3/3.

**Fix:** Either rename to `ProportionalSplitOnlineMembers`, document the front-load behavior, or use even distribution with remainder stored for next tick.

---

### P2-10: ITerritoryEconomyInterface has no framework implementer

**File:** `TerritoryInterfaces.h` (interface), `TerritoryEconomySubsystem.h` (subsystem)

`UTerritoryEconomySubsystem` provides identical methods (`GetTreasury`, `GetActorCurrency`, `CanActorAfford`, `GetIncome`) but does NOT implement `ITerritoryEconomyInterface`. An actor querying via the interface cannot resolve to the subsystem. The interface is documented as an extension point for user actors, which is by-design, but the framework not implementing its own interface is a consistency miss.

**Fix:** Either have `UTerritoryEconomySubsystem` implement `ITerritoryEconomyInterface`, or document that the interface is project-only (not used internally).

---

### P2-11: Guard has no EndPlay cleanup — DespawnGuards skips spawn-point unregister

**Files:** `TerritoryGuardCharacter.cpp` (no EndPlay override), `TerritoryVolume.cpp` DespawnGuards

When `DespawnGuards()` is called, it destroys guard actors but does not call `SpawnPoint->UnregisterGuard()` for each. The spawn point's `ActiveGuards` array retains stale weak pointers until its own cleanup. Guard character has no `EndPlay` override to self-unregister. Save/load recomputation compensates, but between despawn and save there is drift in the spawn-point bookkeeping.

**Fix:** Add `EndPlay` to `ATerritoryGuardCharacter` that notifies `OwningTerritorySpawnPoint` if the guard is being destroyed outside the normal despawn path.

---

### P2-12: Doc 02 C++ subscribe example uses wrong delegate name

**File:** `Docs/02_Interfaces.md` — "How to Subscribe to Events"

Doc: `Control->OnTerritoryOwnershipChanged` — Actual subsystem delegate: `OnTerritoryControlChanged`. The name `OnTerritoryOwnershipChanged` exists on `ATerritoryVolume` (per-actor), not on `UTerritoryControlSubsystem`.

---

### P2-13: Undocumented economy functions in API reference

**File:** `Docs/14_API_Reference.md` economy table

Missing: `RegisterFactionCurrencyAccount`, `UnregisterFactionCurrencyAccount`, `GetOnlineFactionPlayers`. `CreditCurrencyToFaction` omits its 6th parameter `PreferredBeneficiary`. `TryUpgrade` doc says "Debits treasury" — stale, it debits Narrative inventory.

---

### P2-14: Guard spawn point effective config getters undocumented in BP reference

**File:** `Docs/12_Blueprint_Reference.md`

10 `GetEffective*` functions on `ATerritoryGuardSpawnPoint` resolve inline vs GuardPostDefinition precedence. Present in `14_API_Reference.md` but not in `12_Blueprint_Reference.md`.

---

## P3 — Intentional Limits / Known Incomplete

### P3-01: TerritoryDebugger is an empty stub
Header and .cpp acknowledge removal. `IGameplayDebuggerCategoryExtender` not implemented.

### P3-02: Offscreen assault simulation disabled
Intentionally. Future implementation must use multiple attrition rounds.

### P3-03: Dedicated-server/two-client PIE not yet verified
Listed as remaining release gate in previous audits.

### P3-04: Capture participant identities not saved
Intentional: saved contests restore leading faction/progress for decay only.

### P3-05: BPA_ReturnToTerritory ScoreActivity pending Blueprint fix
Doc 15 documents two pending editor fixes (missing territory check for non-territory NPCs, unconnected CastFailed pin). Status needs verification in current project assets.

---

## Blueprint Override Audit — Complete Reference

### BlueprintNativeEvent — Super Required (Critical Invariants)

| Event | Class | What Breaks Without Super |
|---|---|---|
| `OnPropertyCaptured(NewOwner)` | `ATerritoryProperty` | Upgrade level retained by wrong owner; income not recalculated |
| `OnCityFullyCaptured(CapturingFaction)` | `ATerritoryCity` | Income recalc skipped; 1000g capital bonus lost |
| `OnCityLost(PreviousFaction)` | `ATerritoryCity` | Cascade skipped; income not recalculated |
| `OnDistrictFullyCaptured(CapturingFaction)` | `ATerritoryDistrict` | Capital district bonus (500g) lost |

### BlueprintNativeEvent — Super Recommended (ForceNetUpdate Only)

| Event | Class | What Super Does |
|---|---|---|
| `OnAllGuardsDefeated()` | `ATerritoryVolume` | Redundantly sets DefenderCount=0 + ForceNetUpdate. Skipping misses only the net update. |

### BlueprintNativeEvent — Super Optional (Empty or Notification-Only)

| Event | Class | C++ Implementation |
|---|---|---|
| `OnOwnershipChanged(OldOwner, NewOwner)` | `ATerritoryVolume` | Empty — all invariants run before this event |
| `OnStateChanged(OldState, NewState)` | `ATerritoryVolume` | Empty — guard lifecycle runs before this event |
| `OnTerritoryInitialized()` | `ATerritoryVolume` | Empty — extension hook only |
| `OnDistrictCapturedInCity(District, Old, New)` | `ATerritoryCity` | Log message only |

### BlueprintImplementableEvent — BP Only, No C++ Implementation, No Super

| Event | Class | When |
|---|---|---|
| `OnUpgradeLevelChanged(NewLevel)` | `ATerritoryProperty` | RepNotify on clients |
| `OnCounterAttackAlert(AlertText, Duration)` | `UTerritoryHUDWidget` | Counterattack warning display |
| `OnCounterHappened(Event)` | `UTerritoryHUDWidget` | Counterattack state transition |
| `OnTerritoryBound(Territory)` | `UTerritoryInfoWidget` | First territory bind |
| `OnTerritoryOwnershipChanged(Old, New)` | `UTerritoryInfoWidget` | Ownership change |
| `OnTerritoryStateChanged(NewState)` | `UTerritoryInfoWidget` | State change |
| `OnUpdateDebugText(DebugText)` | `UTerritoryDebugWidget` | Debug poll (0.5s) |
| `OnEconomyUpdated(Faction, Snapshot)` | `UTerritoryEconomyWidget` | Economy tick |
| `OnTransactionRecorded(Transaction)` | `UTerritoryEconomyWidget` | Every treasury mutation |
| `OnManagementRefreshed()` | `UTerritoryDistrictManagementWidget` | Display refresh hook |

---

## Blueprint Interface Audit

### ITerritoryOwnershipInterface — ✅ Complete and Accurate

| Function | Specifier | BP Accessible | Doc Accurate |
|---|---|---|---|
| `GetTerritoryOwner()` | BlueprintNativeEvent | ✅ | ✅ |
| `GetTerritoryControlProgress()` | BlueprintNativeEvent | ✅ | ✅ |
| `IsTerritoryContested()` | BlueprintNativeEvent | ✅ | ✅ |
| `GetContestingFaction()` | BlueprintNativeEvent | ✅ | ✅ |

### ITerritoryEconomyInterface — ✅ Complete (2 deprecated, correctly labeled)

| Function | Status | Doc Accurate |
|---|---|---|
| `GetTreasury(Faction)` | ⚠️ Deprecated (returns 0) | ✅ |
| `GetPeriodicIncome(Faction)` | ✅ | ✅ |
| `CanAfford(Faction, Cost)` | ⚠️ Deprecated (Cost==0 only) | ✅ |
| `GetActorCurrency(Requester)` | ✅ | ✅ |
| `CanActorAfford(Requester, Cost)` | ✅ | ✅ |

### ITerritoryEventReceiverInterface — ✅ Complete and Accurate

All four functions correctly specified, documented, and implemented by ATerritoryVolume.

### Missing Interfaces: None

All declared interfaces are documented in `02_Interfaces.md` with real-world examples.

---

## Blueprint Override Examples

### Example 1: Custom Capture Reward (Super Optional)

```blueprint
// BP_TerritoryProperty: OnOwnershipChanged event
// No Super needed — implementation is empty
If NewOwner == PlayerFaction:
    → Economy → CreditCurrency(RequestingActor, 200, NewOwner, "Capture reward")
→ Play Sound: CaptureFanfare
```

### Example 2: Property Capture with Upgrade Reset (Super REQUIRED)

```blueprint
// BP_TerritoryProperty: OnPropertyCaptured event
Call Parent: OnPropertyCaptured  ← REQUIRED: resets upgrade level + income
→ SwapMesh(DefaultMesh)
→ ResetUpgradeVFX
→ NotifyQuestSystem("PropertyCaptured", NewOwner)
```

### Example 3: City Capture with Capital Bonus (Super REQUIRED)

```blueprint
// BP_TerritoryCity: OnCityFullyCaptured event
Call Parent: OnCityFullyCaptured  ← REQUIRED: income recalc + 1000g bonus
→ SpawnCelebrationVFX
→ UnlockCityQuests

// BP_TerritoryCity: OnCityLost event
Call Parent: OnCityLost  ← REQUIRED: cascade + income recalc
→ NotifyStrategicMap
→ Play Sound: CityLostStinger
```

### Example 4: Guard Defeat Notification (Super Recommended)

```blueprint
// BP_TerritoryVolume: OnAllGuardsDefeated event
Call Parent: OnAllGuardsDefeated  ← Recommended: ForceNetUpdate
// Ownership is NOT cleared — territory is vulnerable but still owned
→ Play Sound: AlarmHorn
→ NotifyQuestSystem("TerritoryVulnerable", GetTerritoryTag())
→ Show HUD Warning: "Territory undefended!"
```

### Example 5: Implementing ITerritoryEventReceiverInterface on a Music Manager

```blueprint
// BP_MusicManager — Add ITerritoryEventReceiver interface
// Override OnTerritoryControlChanged:
If NewOwner == PlayerFaction:
    → PlayMusic(VictoryTheme)
Else If OldOwner == PlayerFaction:
    → PlayMusic(DefeatTheme)
Else:
    → PlayMusic(NeutralTheme)
```

### Example 6: Implementing ITerritoryOwnershipInterface on a Vehicle

```blueprint
// BP_FactionVehicle — Add ITerritoryOwnership interface
GetTerritoryOwner → Return DriverFaction
GetTerritoryControlProgress → Return VehicleHealth / MaxHealth
IsTerritoryContested → Return bUnderAttack
GetContestingFaction → Return LastAttackerFaction
```

### Example 7: Implementing ITerritoryEconomyInterface on a Shop

```blueprint
// BP_TerritoryShop — Add ITerritoryEconomy interface
GetActorCurrency(Requester):
    → Return Requester → GetInventoryComponent → GetCurrency()
CanActorAfford(Requester, Cost):
    → Return GetActorCurrency(Requester) >= Cost
GetPeriodicIncome(Faction):
    → Return Economy → GetIncome(Faction)
// Deprecated (return defaults):
GetTreasury(Faction) → Return 0
CanAfford(Faction, Cost) → Return Cost == 0
```

### Example 8: HUD Counterattack Notification

```blueprint
// BP_TerritoryHUDWidget — child of UTerritoryHUDWidget
Event OnCounterHappened(Event):
    Switch on Event.NewState:
        ScheduledWarning → Show HUD Notification("Counterattack incoming!")
        Active           → Show HUD Notification("The attack has begun!")
        Succeeded        → Show HUD Notification("District lost!")
        Defeated         → Show HUD Notification("Attack repelled!")
        Cancelled        → Optional: show reason

Event OnCounterAttackAlert(AlertText, Duration):
    → Set AlertTextBlock = AlertText
    → Play WarningAnimation
    → Set Timer(Duration): clear alert
```

### Example 9: Implementing ITerritoryOwnershipInterface — Full C++ Pattern

```cpp
// Header
UCLASS()
class AMyCaptureableTower : public AActor, public ITerritoryOwnershipInterface
{
    GENERATED_BODY()
public:
    // ITerritoryOwnershipInterface
    virtual FGameplayTag GetTerritoryOwner_Implementation() const override
    { return GarrisonFaction; }
    virtual float GetTerritoryControlProgress_Implementation() const override
    { return SiegeProgress; }
    virtual bool IsTerritoryContested_Implementation() const override
    { return bUnderSiege; }
    virtual FGameplayTag GetContestingFaction_Implementation() const override
    { return AttackingFaction; }

private:
    FGameplayTag GarrisonFaction;
    FGameplayTag AttackingFaction;
    float SiegeProgress = 0.f;
    bool bUnderSiege = false;
};
```

---

## Currency Safety Verification

**CONFIRMED: No faction wallet exists.** No `FactionGold` TMap. `FTerritoryTreasury` contains only rate metadata (IncomePerTick, CostsPerTick, TerritoryCount). All real currency flows through `UNarrativeInventoryComponent::AddCurrency/GetCurrency`.

- `GetTreasury(Faction)` → returns 0 (deprecated)
- `GetFactionGold` BP node → delegates to `GetTreasury` → 0
- All credits/debits use `int64` intermediates, reject overflow
- `DoesAccountBelongToFaction` gates every transaction by Narrative faction membership
- Registered shared/leader accounts are revalidated on every resolve (world, faction, inventory presence)

---

## Remaining Release Gates (from previous audits, still open)

1. Dedicated-server + two-client PIE for proximity activation, casualty removal, late join
2. World Partition stream-out/stream-in during active assault
3. Cook/package smoke test
4. Real Narrative NPC configuration gameplay validation
5. BPA_ReturnToTerritory Blueprint fixes verification (doc 15 pending items)

---

## Supplementary Findings from Scout Reports

The following additional findings were surfaced by the 5 parallel scouts and verified during adversarial refutation:

### P2-SUP-01: UnguardedLaunchProbability config-dependent monotonicity edge case

**File:** `TerritoryCounterAttackSubsystem.cpp:131-134`

When `Active==0` guards, probability is clamped to `[0,1]` using `UnguardedLaunchProbability`. When `Active>=1`, it clamps to `[MinimumLaunchProbability, MaximumLaunchProbability]`. If a designer sets `UnguardedLaunchProbability < MinimumLaunchProbability` (defaults are 1.0 and 0.01, so safe), going from 0→1 guards **increases** launch probability, violating monotonicity. No validator enforces `UnguardedLaunchProbability >= MinimumLaunchProbability`. The monotonicity test only covers default config.

**Fix:** Add editor validation: `UnguardedLaunchProbability >= MinimumLaunchProbability`.

---

### P2-SUP-02: Guard death-binding has no retry (asymmetric with assault path)

**File:** `TerritoryVolume.cpp:1283` (`BindDefenderDeath`)

Guard death binding attempts ASC bind exactly once during `RegisterDefender`. If ASC is null at that instant, the bind silently no-ops — no log, no retry. The assault participant path (`TerritoryAssaultParticipantComponent.cpp`) retries up to 40 times via `BindNarrativeDeathAfterSpawnReady`. If Narrative hasn't initialized the ASC at guard registration time, the guard's death is never detected → no reserve, no `OnAllGuardsDefeated`.

**Likelihood:** Low — Narrative likely creates ASC early. Latent only.

---

### P2-SUP-03: ConfigureTerritorySpawn is a dead BlueprintCallable with null ownership

**File:** `TerritoryGuardCharacter.cpp:356-407`

`ConfigureTerritorySpawn` is `BlueprintCallable` but has no production caller (grep confirmed). Its body explicitly leaves `OwningTerritory` and `OwningTerritorySpawnPoint` null. Any BP caller gets a guard with no patrol route access and no territory link. All route helpers early-return when `OwningTerritorySpawnPoint` is null. The core garrison path uses `SpawnThroughNarrative` instead.

**Fix:** Mark deprecated or document clearly that this is an advanced compatibility entrypoint requiring manual ownership assignment.

---

### P2-SUP-04: GetAllReputation not Blueprint-exposed

**File:** `TerritoryDiplomacySubsystem.h`

`GetAllReputation()` has no `UFUNCTION` — only the single-faction `GetReputation` is `BlueprintPure`. Blueprint/UI cannot enumerate reputation for all factions. Used internally by WorldState save only.

---

### P2-SUP-05: Evaluating state absent from AdvanceAssault switch

**File:** `TerritoryCounterAttackSubsystem.cpp:~655`

The `Evaluating` state has no `case` in `AdvanceAssault`'s switch. This is intentional: Grace→Evaluating→evaluation happens synchronously in one tick, immediately transitioning to `ScheduledWarning` or `Cancelled`. `Evaluating` never persists to the next `UpdateAssaults`. However, if a future change makes evaluation async, `Evaluating` becomes a stuck-state bug.

---

### Verified Sound (Explicit Refutation Confirmed)

The adversarial refutation pass confirmed these are NOT bugs:

| Concern | Verdict | Evidence |
|---|---|---|
| Capture tick doesn't re-validate diplomacy | ✅ Correctly handled | `EvaluateCaptureState` re-checks `CanFactionCaptureTerritory` every tick; blocked attackers decay |
| TOCTOU in attacker registration | ✅ Guarded | `TryRegisterAttacker` re-prunes and re-checks budget immediately before insertion |
| Casualty double-count | ✅ Prevented | `bRemovalReported` + `Removed==0` guards in `Retire`/`NotifyParticipantRemoved` |
| Ceasefire break sets War not Neutral | ✅ Correct | Fixed in v0.2.5 P0-01 |
| FactionGold TMap persists as wallet | ✅ Eliminated | No FactionGold TMap exists; all currency through Narrative InventoryComponent |
| Probability owns territory directly | ✅ Never | Full lifecycle: probability → schedule → physical NPCs → existing capture flow |
| Save/load rerolls decision | ✅ Stable | DecisionSeed stored in record, evaluation never re-run after Grace |
| Monotonicity (more guards → less likely) | ✅ Confirmed | Verified math: deterrence weight subtractive, shortfall ratio additive, clamping preserves order |

---

## Updated Finding Count

| Severity | Count |
|---|---|
| P0 | 0 |
| P1 | 6 |
| P2 | 19 (14 original + 5 supplementary) |
| P3 | 5 |
| **Total** | **30** |

---

*Audit performed 2026-08-09 via 5-scout parallel methodology (51 headers, 50 implementations, 89 tests, 25 docs) + direct header/doc analysis + adversarial refutation.*
