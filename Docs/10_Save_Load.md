# Save/Load — Persistent Territory State

## What Gets Saved

### ATerritoryVolume (per-territory)

| Property | Saved? | Via |
|---|---|---|
| OwningFaction | ✅ | SaveGame on OwnershipData |
| TerritoryState | ✅ | SaveGame on OwnershipData |
| ControlProgress | ✅ | SaveGame on OwnershipData |
| ContestingFaction | ✅ | SaveGame on OwnershipData |
| DefenderCount | ✅ | SaveGame on OwnershipData |
| MaxConcurrentAttackers | ✅ | SaveGame on OwnershipData |
| PeriodicIncome | ✅ | SaveGame on OwnershipData |
| GuardCost | ✅ | SaveGame on OwnershipData |
| TerritoryGUID | ✅ | SaveGame (editor-stable) |

### ATerritoryProperty (extends Volume)

| Property | Saved? |
|---|---|
| UpgradeLevel | ✅ SaveGame |

### ATerritoryWorldState (global)

| Property | Saved? |
|---|---|
| Faction treasuries | ✅ |
| Transaction history | ✅ |
| Active treaties (with timing) | ✅ |
| Faction reputation | ✅ |
| Diplomacy history | ✅ |
| Capture summaries | ✅ |

WorldState arrays are snapshots rebuilt by `ExportPersistentState()` or explicit setters, not continuously synchronized subsystem state. Export rebuilds capture summaries from all registered territories.

### ATerritorySavableData (single-player legacy)

| Property | Saved? |
|---|---|
| SavedTreasuries | ✅ |
| SavedTransactions | ✅ |
| SavedTreaties | ✅ |
| SavedReputation | ✅ |
| SavedDiplomacyHistory | ✅ |

## Stable GUIDs

Territory GUIDs are **editor-stable** — generated once and persisted in the map file.

### GUID Generation Points

| Event | Action |
|---|---|
| PostEditChangeProperty | Auto-generate if invalid |
| PostDuplicate (Ctrl+D) | Generate NEW GUID (prevents save conflicts) |
| BeginPlay (fallback) | Generate if still invalid (shouldn't happen) |

### What This Prevents

Before the fix: each session generated a random GUID → save records couldn't be found on reload.

After the fix: GUID persists in the map → Narrative Save System finds the record → territory state restores correctly.

## Save Flow

```
1. Narrative Save System calls PrepareForSave on all INarrativeSavableActor
2. ATerritoryVolume::PrepareForSave — OwnershipData auto-serialized via SaveGame flags
3. ATerritoryWorldState::PrepareForSave — ExportPersistentState rebuilds subsystem snapshots, then copies replicated → saved arrays
4. Narrative serializes actors to FNarrativeActorRecord
5. UGameplayStatics::SaveGameToSlot
```

## Load Flow

```
1. Narrative Save System loads save game
2. For each INarrativeSavableActor in world: Load_Implementation
3. ATerritoryVolume::Load — re-binds defender death delegates
4. ATerritoryWorldState::Load — ImportPersistentState:
   a. Direct assignment (no artificial transactions)
    b. SyncSubsystemsFromReplicatedState:
        - Push income/cost/count parameters to EconomySubsystem via SetFactionTreasury (Narrative inventory currency is separate)
        - Replay treaty states and reputation to DiplomacySubsystem
        - Diplomacy syncs to Narrative GameState attitudes
        - Restore capture summaries (ownership, state, contesting faction, control progress) back to territories and ControlSubsystem

**Important:** `ATerritorySavableData` is **deprecated** — use `ATerritoryWorldState` instead.
`ATerritoryWorldState` handles both single-player and multiplayer. `ATerritorySavableData` will be removed in a future version; do not use it for new projects.

### State Reconstructed on Load

- A contested TerritoryVolume restores its leading faction/progress into ControlSubsystem via `RestoreCaptureState` so capture decay resumes correctly.
- Claimed territories restore their owning faction and respawn guards.
- WorldState restores economy treasury parameters, transaction history, treaties, reputation, and capture summaries.

### State Not Reconstructed

- Attacker actor identities and non-leading faction progress from a multi-faction contest are not persisted.
- Individual guards, health/activity state, spawn-point reserve counts, and active guard rosters are not persisted.
- Claimed territories destroy/recreate a fresh guard population on load; new guards receive new spawn GUIDs.

### PIE Safety

`PostDuplicate` on all territory actors checks `DuplicateMode == Normal` before regenerating GUIDs.
PIE world creation uses `StaticDuplicateObject` which would otherwise generate new GUIDs, breaking save/load matching.
```

## Placement Requirements

| Actor | Count | Required? |
|---|---|---|
| ATerritoryVolume | 1+ per district | Yes |
| ATerritoryWorldState | Exactly 1 | Persists economy, diplomacy, and capture state |
| ATerritorySavableData | Exactly 1 | **Deprecated** — single-player legacy (use WorldState instead) |

The editor validator detects:
- Multiple WorldState actors → Error
- Multiple SavableData actors → Error
- No persistence actor → Warning

## Manual Save/Load

```cpp
// Save a single territory's state
USaveSystemStatics::SaveSingleActor(TerritoryVolume);

// Load a single territory's state
USaveSystemStatics::LoadSingleActor(TerritoryVolume);

// Remove a territory from save
USaveSystemStatics::RemoveSingleActor(TerritoryVolume);
```
