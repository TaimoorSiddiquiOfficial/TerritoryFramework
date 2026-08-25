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
| GuardRecruitmentCost | ✅ | SaveGame on OwnershipData; old saves migrate to authored initial value |
| DesiredGuardCount | ✅ | SaveGame on OwnershipData; absolute player staffing target |
| TerritoryGUID | ✅ | SaveGame (editor-stable) |

### ATerritoryGuardSpawnPoint

| Property | Saved? |
|---|---|
| SpawnPointGUID | ✅ editor-stable identity |
| CurrentReserveCount | ✅ |
| PendingReserveSpawns | ✅ |
| SavedActiveGuardCount | ✅ finite reconstruction count |

### ATerritoryProperty (extends Volume)

| Property | Saved? |
|---|---|
| UpgradeLevel | ✅ SaveGame |
| ProductionProfile | Authored asset reference; durable soft reference copied into WorldState site records |

### ATerritoryWorldState (global)

| Property | Saved? |
|---|---|
| Faction treasuries | ✅ |
| Transaction history | ✅ |
| Active treaties (with timing) | ✅ |
| Faction reputation | ✅ |
| Diplomacy history | ✅ |
| Capture summaries | Replicated projection; ownership persists on each Volume |
| Counterattack records | ✅ SaveGame + replicated |
| Resource production checkpoints | ✅ SaveGame; stable Territory GUID + RuleTag + owner + last consumed cycle |
| Production site records and per-rule outcomes | ✅ SaveGame + replicated read model; supports unloaded World Partition Properties |
| Resource stockpile snapshots | ✅ SaveGame + replicated read model; actual items remain in Narrative inventory |

Server subsystem delegates keep WorldState projections current between saves. `ExportPersistentState()` rebuilds them at the save boundary. Capture summaries are not a second durable ownership source; `ATerritoryVolume::OwnershipData` remains authoritative.

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
| PostActorCreated (new editor placement) | Generate a NEW instance GUID even when the Blueprint CDO has a serialized default |
| PostEditChangeProperty | Auto-generate if invalid |
| PostDuplicate (Ctrl+D) | Generate NEW GUID (prevents save conflicts) |
| Load / PIE duplication | Retain the saved editor-authored GUID |
| BeginPlay with invalid GUID | Log an error and fail closed for persistence; runtime identity is never invented |

### What This Prevents

Before the fix: each session generated a random GUID → save records couldn't be found on reload.

After the fix: each placement receives a unique GUID, that GUID persists in the map, and Narrative Save System finds the same record on reload. Existing placed actors keep their saved identities; only newly created editor actors receive a new value.

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
3. ATerritoryVolume::Load — hydrates referenced guard posts, reconstructs finite active guards, and restores capture decay state
4. ATerritoryWorldState::Load — ImportPersistentState:
   a. Direct assignment (no artificial transactions)
    b. SyncSubsystemsFromReplicatedState:
        - Push income/cost/count parameters to EconomySubsystem via SetFactionTreasury (Narrative inventory currency is separate)
        - Restore production checkpoints, site records, per-rule outcomes, and resource read models without restoring item balances
        - Replay treaty states and reputation to DiplomacySubsystem
        - Diplomacy syncs to Narrative GameState attitudes
        - Hydrate client-side economy/diplomacy query models from RepNotify
        - Restore counterattack decisions, the per-territory/faction evaluation-cycle high-water ledger, and reconstruct saved active survivors as finite pending force

**Important:** `ATerritorySavableData` is **deprecated** — use `ATerritoryWorldState` instead.
`ATerritoryWorldState` handles both single-player and multiplayer. `ATerritorySavableData` will be removed in a future version; do not use it for new projects.

### State Reconstructed on Load

- A contested TerritoryVolume restores its leading faction/progress into ControlSubsystem via `RestoreCaptureState` so capture decay resumes correctly.
- Claimed territories restore their owning faction and only the saved finite active guard count.
- Guard posts restore reserve and pending deployment counts.
- `FTerritoryGarrisonSnapshot` is reconstructed from live/restored guard posts and replicated; it does not save pawn pointers.
- WorldState restores economy rate parameters, transaction history, treaties, reputation, counterattack records, and the server-only decision-cycle ledger. Older saves without the ledger rebuild the best available high-water marks from retained assault records.
- Production checkpoints prevent same-day duplication and ownership-change windfalls. Missing-input days are consumed; storage-unavailable/full outcomes remain pending inside the bounded catch-up window.
- Actual Meat, Grain, tools, or other resources restore through the registered `UNarrativeInventoryComponent`, never from the WorldState snapshot.
- A saved active assault preserves its decision/casualties and moves surviving live count into pending reconstruction.

### State Not Reconstructed

- Attacker actor identities and non-leading faction progress from a multi-faction contest are not persisted.
- Individual pawn identities, health, controllers, ASC pointers, and activity UObject instances are not persisted.
- Recreated guards and assault participants receive new runtime spawn GUIDs while durable Territory/guard-post/assault IDs remain stable.

### Garrison migration

Saves created before `GuardRecruitmentCost` use the Territory Blueprint's `InitialGuardRecruitmentCost` on load. Existing `DesiredGuardCount` values remain authoritative, including the legacy value 3; players can set those owned garrisons to zero in the District Command Center. New physical player captures use the default `PlayerChooses` policy and therefore begin at zero without rewriting old campaign state.

### PIE Safety

`PostDuplicate` on all territory actors checks `DuplicateMode == Normal` before regenerating GUIDs.
PIE world creation uses `StaticDuplicateObject` which would otherwise generate new GUIDs, breaking save/load matching.
```

## Placement Requirements

| Actor | Count | Required? |
|---|---|---|
| ATerritoryVolume | 1+ per district | Yes |
| ATerritoryWorldState | Exactly 1 | Persists economy, diplomacy, and assault state; replicates late-join projections |
| ATerritorySavableData | 0 | **Deprecated** — never place beside WorldState |

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
