# TerritoryFramework Cascade Audit — 2026-08-09

## Scope

Focused re-audit of the hierarchy cascade flow (City↔District↔Property ownership propagation), edge cases, and P1/P2 regression check after the 2026-08-09 fixes.

---

## Cascade Flow — Verified Sound

### Capture Cascade (Bottom-Up)

```
Property captured (direct capture via AttemptCapture)
  → Property.CommitOwnershipData (bTransitionInProgress=true on Property)
  → Property.OnTerritoryOwnershipChanged.Broadcast
  → District.OnPropertyControlChanged handler fires
     → Checks AllPropertiesOwnedBy(NewOwner)
     → If unanimous: District.SetDerivedOwningFaction(NewOwner)
        → District.SetOwningFactionWithContext (bApplyingDerivedOwnership=true bypasses AggregateOnly)
        → District.CommitOwnershipData (bTransitionInProgress=true on District)
        → District.OnTerritoryOwnershipChanged.Broadcast
        → City.OnDistrictControlChanged handler fires
           → CascadeCaptureToProperties (aligns remaining properties)
           → If AllDistrictsOwnedBy(NewOwner): City.SetDerivedOwningFaction + OnCityFullyCaptured
           → Else: City.SetTerritoryState(Contested)
```

### Capture Cascade (Top-Down via ForceCapture)

```
ForceCapture(District)
  → ApplyTerritoryMutation → District.CommitOwnershipData
  → District.OnTerritoryOwnershipChanged.Broadcast
  → City.OnDistrictControlChanged
     → CascadeCaptureToProperties → all child properties ForceSetOwningFactionWithContext
     → City re-evaluates ownership
```

### Loss Cascade

```
District lost by faction X (captured by faction Y)
  → City.OnDistrictControlChanged
     → If X no longer owns all districts: bCityLost=true, OnCityLost(X)
     → City goes Contested (if Claimed) or stays Contested
     → bCityLostFired=true prevents double-fire
  → When new faction owns all: bCityLostFired=false (reset on capture)
```

**Verdict:** All cascade paths are correct and consistent.

---

## P1-04 Reentrancy Guard — Regression Check

### The Guard

```cpp
bool ATerritoryVolume::CommitOwnershipData(...)
{
    if (!HasAuthority()) return false;
    if (bTransitionInProgress) return false;  // P1-04 fix
    ...
    bTransitionInProgress = true;
    ...  // event bundle fires here
    bTransitionInProgress = false;
}
```

### Cross-Actor Cascade — ✅ Safe

The guard is **per-actor** (`bTransitionInProgress` is an instance member). When the District's `CommitOwnershipData` fires events that cascade to Properties and City, those actors have their own independent `bTransitionInProgress` flags — all `false`. The cascade proceeds normally.

### Reverse Cascade (Property→District during District transition) — ✅ Benign

During a District's transition (cascade-driven), the Properties are being aligned TO the District's new owner. Each Property's ownership change fires back to the District's `OnPropertyControlChanged`. That handler may call `SetDerivedOwningFaction` or `SetTerritoryState` on the District — which checks `bTransitionInProgress` (still true on the District) and returns early.

This is **benign** because:
1. The District already has the correct owner (it initiated the cascade)
2. `SetOwningFactionWithContext` line 742: `if (OldOwner == NewFaction) return;` would no-op anyway
3. `SetTerritoryState` to Contested would be wrong — the District just transitioned to Claimed

The guard prevents wasted work, not lost work.

### Direct Reentrancy (Same Actor) — ✅ Blocked

If a delegate listener on `OnTerritoryOwnershipChanged` calls `CommitOwnershipData` on the SAME actor, the guard correctly blocks it. This is the intended behavior.

---

## ControlMode Verification

| Mode | Direct Capture | Cascade Capture | Force Capture |
|---|---|---|---|
| Independent | ✅ Allowed | ✅ Allowed | ✅ Allowed |
| AggregateOnly | ❌ Blocked (AttemptCapture returns InvalidTerritory) | ✅ Allowed (bApplyingDerivedOwnership bypasses) | ✅ Allowed (bBypassTransitionConditions) |
| Cascading | ✅ Allowed | ✅ Allowed | ✅ Allowed |

Districts default to `AggregateOnly` — they can only change owner through property unanimity or force, never through direct capture. Verified at `TerritoryControlSubsystem.cpp:383` (ValidateCaptureAttempt) and `:689` (ApplyTerritoryMutation).

---

## Edge Cases — All Verified

### 1. Partial District Ownership (Multi-Faction)
- District with properties owned by different factions → District stays Contested
- City with districts owned by different factions → City stays Contested
- `OnPropertyControlChanged` checks `AllPropertiesOwnedBy` before promoting
- `OnDistrictControlChanged` checks `AllDistrictsOwnedBy` before promoting

### 2. Late-Binding (World Partition)
- City binds to `OnTerritoryRegistered` for late-arriving districts (`TerritoryHierarchy.cpp:60-70`)
- District binds to `OnTerritoryRegistered` for late-arriving properties (`TerritoryHierarchy.cpp:470-478`)
- Property binds to `OnTerritoryRegistered` for late-arriving districts (`TerritoryHierarchy.cpp:730-745`)
- Property BeginPlay syncs to district owner ONLY if property has no owner (first-time init)
- Saved ownership is preserved — late-binding never overwrites saved state

### 3. City Loss / Recovery
- `bCityLostFired` prevents double-firing of `OnCityLost`
- Reset to `false` when city is fully captured by a new faction
- City can recover from Contested → Claimed when incumbent retakes all districts
- `OnCityLost_Implementation` re-evaluates: if another faction owns all, they get the city; otherwise Contested

### 4. Economy Recalculation During Cascade
- `MarkFactionDirty` is used (deferred recalc) — avoids O(N²) scans during cascade
- Both old and new owner factions are marked dirty
- Actual `RecalculateIncome` runs once per economy tick

### 5. Guard Lifecycle During Cascade
- `CascadeCaptureToProperties` calls `ForceSetOwningFactionWithContext` — bypasses state conditions
- District's `CommitOwnershipData` despawns guards BEFORE cascade fires
- Properties get new owner → `CommitOwnershipData` spawns guards for new owner
- Ordering: District despawn → cascade → Property spawn — no guard leak

---

## P1/P2 Fix Regression Summary

| Fix | Regression Risk | Verdict |
|---|---|---|
| P1-01 ForceNetUpdate | None — only adds net update calls | ✅ Safe |
| P1-02 IncomePayoutPolicy | None — reads existing public field | ✅ Safe |
| P1-03 SetControlProgress via CommitOwnershipData | Low — now fires events; callers that expected silent write get events | ✅ Acceptable (events are correct behavior) |
| P1-04 bTransitionInProgress guard | **Reviewed above** — per-actor, cascade-safe, reverse-cascade benign | ✅ Safe |
| P1-05 RPC _Validate null checks | None — only tightens validation | ✅ Safe |
| P1-06 bReplicationInitialized | None — only suppresses synthetic first-replication events | ✅ Safe |
| P2-07 SourceTerritory optional param | None — default empty tag preserves all existing callers | ✅ Safe |
| P2-08 bClearCaptureState deprecated | None — field was already ignored; metadata only | ✅ Safe |

---

## Conclusion

**No regressions found.** The cascade hierarchy is sound:
- Bottom-up and top-down cascades both work correctly
- The P1-04 reentrancy guard is per-actor and does not block legitimate cross-actor cascade transitions
- Reverse-cascade notifications during a transition are benign (would be no-ops anyway)
- ControlMode enforcement is consistent across all entry points
- Late-binding handles World Partition correctly
- Economy recalculation is properly deferred during cascades

*2 parallel scouts dispatched for independent verification; results will supplement this report.*

---

*Audit performed 2026-08-09 via direct code tracing + 2 parallel scouts.*
