# Core Actors — City, District, Property, GuardSpawnPoint

## Hierarchy

```
ATerritoryVolume (base — placed in level for any territory)
├── ATerritoryCity          — top-level territory (aggregates districts)
│   ├── ATerritoryDistrict  — mid-level (contains properties)
│   │   └── ATerritoryProperty — leaf (upgradeable, income-generating)
│   └── (more districts...)
├── ATerritoryGuardSpawnPoint — placed inside a volume (guard staging)
├── ATerritoryGuardCharacter  — spawned at runtime by territory volumes
└── ATerritoryWorldState      — global persistence actor (1 per level)
```

## ATerritoryVolume — Base Class

### Key Properties (all BlueprintReadWrite)

| Property | Type | Default | Purpose |
|---|---|---|---|
| TerritoryTag | GameplayTag | — | Unique identity (e.g., `Territory.HavenReach.MarketSquare`) |
| TerritoryDisplayName | Text | — | Player-facing name |
| InitialOwningFaction | GameplayTag | — | Who owns at game start |
| InitialMaxConcurrentAttackers | int32 | 3 | NPC attack slot limit |
| InitialPeriodicIncome | int32 | 100 | Gold per economy tick |
| InitialGuardCost | int32 | 50 | Upkeep cost per tick |
| bStartsLocked | bool | false | If true, territory can't be captured until unlocked |
| ParentTerritoryTag | GameplayTag | — | Parent territory for hierarchy |
| GuardNPCDefinition | NPCDefinition* | — | NPC template for guards |
| GuardBehaviorTree | BehaviorTree* | — | BT to run on spawned guards |
| GuardBlackboardAsset | BlackboardData* | — | BB override (optional) |
| GuardSpawnCount | int32 | 3 | How many guards to spawn |
| GuardSpawnPoints | Array<Actor> | — | Optional spawn point actors |

### Key Events (BlueprintNativeEvent)

| Event | When | Override For |
|---|---|---|
| OnOwnershipChanged(Old, New) | Faction changes | Custom capture effects, sounds |
| OnStateChanged(OldState, NewState) | State transition | Custom state-specific behavior |

### Key Delegates (BlueprintAssignable)

| Delegate | Signature |
|---|---|
| OnTerritoryControlChanged | (Volume*, OldOwner, NewOwner) |
| OnTerritoryStateChanged | (Volume*, NewState) |
| OnGuardDied | (Volume*, Faction, EmptyTag) |

## ATerritoryCity

### Additional Properties
None beyond base.

### Additional Functions
| Function | Returns | Purpose |
|---|---|---|
| GetDistricts() | Array<Volume*> | All child districts |
| GetDistrictCount() | int32 | Number of districts |
| AllDistrictsOwnedBy(Faction) | bool | Check if faction controls all |
| GetCityControlPercentage(Faction) | float | 0.0-1.0 |

### Events
| Event | When |
|---|---|
| OnCityFullyCaptured(Faction) | All districts owned by one faction |
| OnCityLost(PreviousFaction) | City loses majority control |

### Setup Example
```
BP_TerritoryCity "Haven Reach"
  TerritoryTag: Territory.HavenReach
  ParentTerritoryTag: (none — top level)
  InitialOwningFaction: Narrative.Factions.Bandits
```

## ATerritoryDistrict

### Additional Properties
| Property | Type | Default | Purpose |
|---|---|---|---|
| bIsCapital | bool | false | Marks the main district of a city |

### Setup Example
```
BP_TerritoryDistrict "Market Square"
  TerritoryTag: Territory.HavenReach.MarketSquare
  ParentTerritoryTag: Territory.HavenReach
  InitialOwningFaction: Narrative.Factions.Bandits
  InitialPeriodicIncome: 200
```

## ATerritoryProperty

### Additional Properties (all BlueprintReadWrite)
| Property | Type | Default | Purpose |
|---|---|---|---|
| UpgradeLevel | int32 (SaveGame, Replicated) | 0 | Current upgrade tier |
| MaxUpgradeLevel | int32 | 3 | Maximum upgrades |
| UpgradeCostPerLevel | int32 | 500 | Cost = level × this |
| IncomeBonusPerLevel | int32 | 25 | Income bonus per level |

### Functions
| Function | Returns | Purpose |
|---|---|---|
| CanUpgrade() | bool | Check if upgradeable |
| GetUpgradeCost() | int32 | Current upgrade cost |
| GetEffectiveIncome() | int32 | Base + upgrade bonus |
| TryUpgrade() | bool | Authority-only upgrade (debits treasury) |
| SetUpgradeLevel(Level) | void | Authority-only direct set |

### Events
| Event | When |
|---|---|
| OnUpgradeLevelChanged(NewLevel) | Client receives replicated upgrade change |

### Setup Example
```
BP_TerritoryProperty "Blacksmith"
  TerritoryTag: Territory.HavenReach.MarketSquare.Blacksmith
  ParentTerritoryTag: Territory.HavenReach.MarketSquare
  InitialOwningFaction: Narrative.Factions.Bandits
  InitialPeriodicIncome: 50
  MaxUpgradeLevel: 3
  UpgradeCostPerLevel: 500
  IncomeBonusPerLevel: 25
```

## ATerritoryGuardSpawnPoint

### Properties
| Property | Type | Default | Purpose |
|---|---|---|---|
| OwnerTerritoryTag | GameplayTag | — | Which territory this belongs to |
| MaxGuards | int32 | 3 | Active guard slots |
| ReserveSlots | int32 | 1 | Replacement guards |
| PatrolRoute | Array<PatrolNode> | — | Ordered waypoints |
| bLoopPatrol | bool | true | Loop back to start |
| FactionOverride | GameplayTag | — | Override territory owner faction |
| Priority | int32 | 50 | Higher = fills first |

### Patrol Node
| Field | Type | Purpose |
|---|---|---|
| Location | Vector | World position |
| Rotation | Rotator | Facing direction |
| WaitTime | float | Seconds to wait |
| ActivityTag | GameplayTag | e.g., `Guard.Activity.Rest` |

### Setup Example
```
BP_GuardSpawnPoint "Market Patrol A"
  OwnerTerritoryTag: Territory.HavenReach.MarketSquare
  MaxGuards: 2
  ReserveSlots: 1
  PatrolRoute:
    [0] Location=(0,0,200) Rotation=(0,0,0) WaitTime=2.0 Activity=Guard.Activity.Patrol
    [1] Location=(1000,0,200) Rotation=(0,90,0) WaitTime=5.0 Activity=Guard.Activity.Inspect
    [2] Location=(1000,1000,200) Rotation=(0,180,0) WaitTime=2.0 Activity=Guard.Activity.Rest
    [3] Location=(0,1000,200) Rotation=(0,270,0) WaitTime=2.0 Activity=Guard.Activity.Patrol
  bLoopPatrol: true
```

## ATerritoryGuardCharacter

Extends `ANarrativeNPCCharacter` from Narrative Pro.

### Additional Functions
| Function | Purpose |
|---|---|
| SetTerritorySaveGUID(GUID) | Set save GUID before FinishSpawning |
| SetOwningTerritoryGUID(GUID) | Link to parent territory |

This class prevents the `INarrativeStableActor::GetActorGUID()` assertion crash by returning a valid GUID from `SpawnInfo.SpawnAssignedSaveGUID`.

## ATerritoryWorldState

**Place ONE in the level** for multiplayer-persistent territory state.

### What It Stores (replicated)
- Faction treasuries (gold, income, costs, territory count)
- Transaction history (audit trail)
- Active treaties (with timing, expiry, permanence)
- Faction reputation
- Capture summaries (per territory)

### Save/Load
- Implements `INarrativeSavableActor`
- `ExportPersistentState()` → copies replicated state to SaveGame
- `ImportPersistentState()` → restores from save, syncs subsystems
