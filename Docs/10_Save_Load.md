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
| Capture summaries | ✅ |

### ATerritorySavableData (single-player legacy)

| Property | Saved? |
|---|---|
| SavedTreasuries | ✅ |
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
3. ATerritoryWorldState::PrepareForSave — ExportPersistentState copies replicated → saved arrays
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
      - Push treasuries to EconomySubsystem
      - Push treaties to DiplomacySubsystem
      - Diplomacy syncs to Narrative GameState attitudes
```

## Placement Requirements

| Actor | Count | Required? |
|---|---|---|
| ATerritoryVolume | 1+ per district | Yes |
| ATerritoryWorldState | Exactly 1 | For multiplayer |
| ATerritorySavableData | Exactly 1 | For single-player (alternative to WorldState) |

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
