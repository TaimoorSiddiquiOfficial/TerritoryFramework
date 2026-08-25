# TerritoryFramework Audit Summary — 2026-08-09

> Historical snapshot. The implementation and release status changed in the 2026-08-24 remediation pass; use `REMEDIATION_2026-08-24.md`, `CHANGELOG.md`, and current source/tests for present behavior.

## Overview

| Metric | Value |
|---|---|
| **Date** | 2026-08-09 |
| **Method** | 5 parallel scouts + direct header/doc analysis + adversarial refutation |
| **Scope** | 51 headers, 50 implementations, 3 test files (89 automation tests), 25 docs |
| **Findings** | 6 P1, 14 P2, 5 P3 |
| **Resolved** | 6 P1 + 14 P2 + 1 P3 = **21 of 25** |
| **Accepted** | 4 P3 (intentional limits) |
| **Build** | ✅ Verified — 857/857 compiled, zero errors, zero warnings |

---

## Commits

| Commit | Type | Content |
|---|---|---|
| `00d6952` | P1 code | 6 high-priority fixes |
| `b962bd7` | Parent ref | Submodule update |
| `fa08d89` | P2 code | 8 doc/code mismatches and dead fields |
| `f7ed8f4` | Parent ref | Submodule update |
| `fd45b77` | P2 docs | 6 documentation gap fixes |
| `4a76d3f` | Parent ref | Submodule update |
| `ba0c45a` | P3-05 | BPA_ReturnToTerritory verified resolved; doc 15 updated |
| `2e64506` | Parent ref | Submodule update |

---

## P1 Fixes (6) — All Resolved

### P1-01: WorldState live handlers missing ForceNetUpdate
**File:** `TerritoryWorldState.cpp` — `OnDiplomacyChangedLive`, `OnReputationChangedLive`, `OnTerritoryControlChangedLive`

**Problem:** Three handlers mutated replicated arrays without calling `ForceNetUpdate()`, causing client-side staleness for diplomacy, reputation, and capture summaries until the next periodic dirty-property sweep.

**Fix:** Added `ForceNetUpdate()` to all 6 return paths across the 3 handlers.

### P1-02: Capture rewards hardcoded EqualSplitOnlineMembers
**File:** `TerritoryHierarchy.cpp` — `OnCityFullyCaptured_Implementation`, `OnDistrictFullyCaptured_Implementation`

**Problem:** Capital city (1000g) and capital district (500g) bonuses hardcoded `EqualSplitOnlineMembers`, ignoring the project-configured `IncomePayoutPolicy`. A `SharedNarrativeAccount` project would see capture bonuses split among online players instead of routing to the shared account.

**Fix:** Changed to `Economy->IncomePayoutPolicy`.

### P1-03: SetControlProgress bypassed atomic commit
**File:** `TerritoryVolume.cpp`

**Problem:** `SetControlProgress(float)` wrote directly to `OwnershipData.ControlProgress` — no `CommitOwnershipData`, no events, no `ForceNetUpdate`. Blueprint callers created inconsistent state: progress changed without contested-state commitment or replication push.

**Fix:** Routed through `CommitOwnershipData` with a copy of the struct.

### P1-04: CommitOwnershipData lacked reentrancy guard
**File:** `TerritoryVolume.cpp`

**Problem:** `CommitOwnershipData` checked `HasAuthority()` but not `bTransitionInProgress` before proceeding. A delegate listener that called back into `CommitOwnershipData` during the synchronous event bundle (lines 862-869) would overwrite `OwnershipData` mid-transition.

**Fix:** Added `if (bTransitionInProgress) return false;` guard at entry.

### P1-05: Inconsistent Server RPC _Validate null checks
**File:** `TerritoryPlayerManagementComponent.cpp`

**Problem:** 4 of 6 `_Validate` functions checked only count + RequestId, never null-checking the target actor. Clients could spam validated RPCs with null actor targets that passed `_Validate` but failed in `_Implementation`.

**Fix:** Added target-actor null checks to all 4 functions (`ServerRequestPurchaseGuards_Validate`, `ServerRequestPurchaseGuardsForDistrict_Validate`, `ServerRequestRemoveGuards_Validate`, `ServerRequestRemoveGuardsForDistrict_Validate`).

### P1-06: Synthetic late-join events from OnRep_OwnershipData
**File:** `TerritoryVolume.h/.cpp`

**Problem:** `PreviousOwningFaction` defaults to empty, `PreviousState` defaults to `Unclaimed`. On initial RepNotify (late join), every owned territory fired full ownership/state change events — causing spurious VFX, analytics, and quest triggers.

**Fix:** Added `bReplicationInitialized` flag. First `OnRep_OwnershipData` call syncs cached values and returns without firing events. Authority sets the flag in `BeginPlay`.

---

## P2 Fixes (14) — All Resolved

### Code Fixes (8)

| # | Fix | File |
|---|---|---|
| P2-01 | Doc 03: Contested→Unclaimed transition no longer claims guard defeat unclaims territory | `03_Core_Actors.md` |
| P2-02 | Doc 03: Removed obsolete CRC32 GUID fallback note; documented fail-closed behavior | `03_Core_Actors.md` |
| P2-03 | Doc 03: Corrected `OnUpgradeLevelChanged` to `BlueprintImplementableEvent` | `03_Core_Actors.md` |
| P2-04 | Doc 03 + Extension Guide: Downgraded `OnAllGuardsDefeated` Super from CRITICAL to Recommended | `03_Core_Actors.md`, `Blueprint_Extension_Guide.md` |
| P2-05 | API Ref: Removed 5 phantom BlueprintLibrary functions that don't exist in code | `14_API_Reference.md` |
| P2-06 | API Ref: Added ~25 missing BlueprintLibrary functions to match actual header | `14_API_Reference.md` |
| P2-07 | Code: `RecordCurrencyTransaction` now populates `SourceTerritory` via optional parameter | `TerritoryEconomySubsystem.h/.cpp` |
| P2-08 | Code: Marked `bClearCaptureState` as deprecated (silently ignored by ApplyTerritoryMutation) | `TerritoryMutationTypes.h` |

### Documentation Fixes (6)

| # | Fix | File |
|---|---|---|
| P2-09 | Fixed wrong delegate name in C++ example (`OnTerritoryControlChanged`, not `OnTerritoryOwnershipChanged`) | `02_Interfaces.md` |
| P2-10 | Documented `ITerritoryEconomyInterface` as project-only extension point | `02_Interfaces.md` |
| P2-11 | Added Known Limitations section (guard death bind, EndPlay cleanup, SetControlProgress) | `Blueprint_Extension_Guide.md` |
| P2-12 | Added missing economy functions to API reference | `14_API_Reference.md` |
| P2-13 | Added 10 guard spawn point `GetEffective*` functions to Blueprint reference | `12_Blueprint_Reference.md` |
| P2-14 | Added `ControlMode`, `StateConfigs`, counterattack properties to doc 03 | `03_Core_Actors.md` |

---

## P3 Items (5) — 1 Resolved, 4 Accepted

| # | Status | Description |
|---|---|---|
| P3-01 | Accepted | TerritoryDebugger is an empty stub (dev convenience only) |
| P3-02 | Accepted | Offscreen assault simulation intentionally disabled (AGENTS.md §6) |
| P3-03 | Open | Dedicated-server / two-client PIE testing not yet performed |
| P3-04 | Accepted | Capture participant identities not saved (intentional design) |
| P3-05 | ✅ Resolved | BPA_ReturnToTerritory ScoreActivity verified fixed via binary inspection (`ba0c45a`) |

---

## Verified Sound (Adversarial Refutation Confirmed)

| Concern | Verdict |
|---|---|
| Capture tick diplomacy re-validation | ✅ Re-checked every tick |
| TOCTOU in attacker registration | ✅ Guarded (re-prune + re-check before insert) |
| Casualty double-count | ✅ Prevented (`bRemovalReported`) |
| Ceasefire break sets War not Neutral | ✅ Correct |
| No faction wallet exists | ✅ All currency through Narrative InventoryComponent |
| Probability never directly owns territory | ✅ Full physical lifecycle |
| Save/load rerolls decision | ✅ DecisionSeed stored, never re-evaluated |
| Monotonicity (more guards → less likely) | ✅ Math verified under shipped defaults |
| Authority map (AGENTS.md §4.1) | ✅ All 11 authorities intact — zero duplicate systems |
| Currency safety (overflow, membership) | ✅ int64 intermediates, faction enforcement, fail-closed |

---

## Blueprint Override Reference

### Super Required (Critical Invariants)
- `OnPropertyCaptured(NewOwner)` — ATerritoryProperty
- `OnCityFullyCaptured(CapturingFaction)` — ATerritoryCity
- `OnCityLost(PreviousFaction)` — ATerritoryCity
- `OnDistrictFullyCaptured(CapturingFaction)` — ATerritoryDistrict

### Super Recommended
- `OnAllGuardsDefeated()` — ATerritoryVolume (ForceNetUpdate only)

### Super Optional (Empty/Notification Implementation)
- `OnOwnershipChanged(Old, New)` — ATerritoryVolume
- `OnStateChanged(OldState, NewState)` — ATerritoryVolume
- `OnTerritoryInitialized()` — ATerritoryVolume
- `OnDistrictCapturedInCity(District, Old, New)` — ATerritoryCity

### BlueprintImplementableEvent (No C++, No Super)
- `OnUpgradeLevelChanged(NewLevel)` — ATerritoryProperty
- `OnCounterAttackAlert`, `OnCounterHappened` — UTerritoryHUDWidget
- `OnTerritoryBound`, `OnTerritoryOwnershipChanged`, `OnTerritoryStateChanged` — UTerritoryInfoWidget
- `OnUpdateDebugText` — UTerritoryDebugWidget
- `OnEconomyUpdated`, `OnTransactionRecorded` — UTerritoryEconomyWidget
- `OnManagementRefreshed` — UTerritoryDistrictManagementWidget

### Blueprint Interfaces (All Complete and Accurate)
- `ITerritoryOwnershipInterface` — 4 functions, all BlueprintNativeEvent ✅
- `ITerritoryEconomyInterface` — 5 functions (2 deprecated), all BlueprintNativeEvent ✅
- `ITerritoryEventReceiverInterface` — 4 functions, all BlueprintNativeEvent ✅

---

## Remaining Release Gates

1. Dedicated-server + two-client PIE (P3-03)
2. World Partition stream-out/stream-in during active assault
3. Cook/package smoke test
4. Real Narrative NPC gameplay validation
5. BT task placement in shipped behavior trees (recommendation, not blocking)

---

*Full audit report: `Docs/DEEP_REAUDIT_2026-08-09.md`*
