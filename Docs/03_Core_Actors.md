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
| bAutoCreateMapMarker | bool | true | Auto-creates TerritoryNavigationMarkerComponent on BeginPlay |
| GuardNPCDefinition | NPCDefinition* | — | NPC template for guards |
| GuardBehaviorTree | BehaviorTree* | — | BT to run on spawned guards |
| GuardBlackboardAsset | BlackboardData* | — | BB override (optional) |
| GuardSpawnCount | int32 | 3 | How many guards to spawn |
| GuardSpawnPoints | Array<Actor> | — | Optional spawn point actors |

### Key Events (BlueprintNativeEvent)

| Event | When | Override For |
|---|---|---|
| OnOwnershipChanged(Old, New) | Faction changes | Custom capture effects, sounds |
| OnStateChanged(OldState, NewState) | State transition | Guard despawn on Contested, respawn on Claimed, despawn on Locked |
| OnAllGuardsDefeated() | All guards are dead | Trigger undefended state, open capture |
| OnTerritoryInitialized() | BeginPlay completes | Custom initialization logic |

### Key Delegates (BlueprintAssignable)

| Delegate | Signature |
|---|---|
| OnTerritoryControlChanged | (Volume*, OldOwner, NewOwner) |
| OnTerritoryStateChanged | (Volume*, NewState) |
| OnGuardDied | (Volume*, Faction, ContestingFaction) |
| OnAllGuardsDefeatedDelegate | (Volume*) |

### State Transition Logic (C++)

OnStateChanged_Implementation now handles:
- **Claimed → Contested**: Despawns guards (territory under attack)
- **Contested → Claimed**: Respawns guards (recaptured by owner)
- **Any → Locked**: Despawns all guards (locked territory)

### Map Marker Auto-Creation

When `bAutoCreateMapMarker = true`, the volume automatically creates a `UTerritoryNavigationMarkerComponent` on BeginPlay. This component:
- Creates a `UTerritoryMapMarker` instance
- Subscribes to ownership/state changes for auto-refresh
- Registers with the Narrative navigation subsystem
- Can be retrieved via `GetMapMarkerComponent()`

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
| GetMajorityOwner() | GameplayTag | Faction controlling >50% of districts, or empty |
| IsFullyCaptured() | bool | True if one faction owns all districts |
| GetCapturingFaction() | GameplayTag | Returns capturing faction if fully captured |
| GetCapitalDistrictCount() | int32 | Number of capital districts in this city |
| HasCapitalDistrict() | bool | True if any district is a capital |

### Events (BlueprintNativeEvent)
| Event | When | Override For |
|---|---|---|
| OnCityFullyCaptured(Faction) | All districts owned by one faction | Economy bonus, cascade, rewards |
| OnCityLost(PreviousFaction) | City owner loses majority | Clear ownership, economy recalc |
| OnDistrictCapturedInCity(District, Old, New) | Any district in this city changes owner | Per-district capture effects |

### Delegates (BlueprintAssignable)
| Delegate | Signature |
|---|---|
| OnCityCapturedDelegate | (City*, CapturingFaction) |
| OnCityLostDelegate | (City*, PreviousFaction) |

### City Capture Flow (Complete)

```
District captured by Faction X
  → District.SetOwningFaction(X)
  → District.OnTerritoryControlChanged broadcast
  → City.OnDistrictControlChanged handler:
      1. Fires OnDistrictCapturedInCity BP event
      2. CascadeCaptureToProperties(district, X)
         → All child properties auto-reassigned to X
         → Each property fires OnPropertyCaptured + OnPropertyCapturedDelegate
      3. If AllDistrictsOwnedBy(X):
         → City.SetOwningFaction(X)
         → City.OnCityFullyCaptured(X) — economy bonus, capital reward
         → City.OnCityCapturedDelegate.Broadcast(this, X)
      4. If city owner no longer controls all districts:
         → City.OnCityLost(oldOwner) — clears city ownership
         → City.OnCityLostDelegate.Broadcast(this, oldOwner)
```

### Capital City Bonus

When a city with capital districts is fully captured:
- 1000 gold reward to capturing faction (EconomySubsystem)
- 500 gold reward per capital district captured
- Capital income multiplier applies to property income

## ATerritoryDistrict

### Additional Properties
| Property | Type | Default | Purpose |
|---|---|---|---|
| bIsCapital | bool | false | Marks the main district of a city |
| CapitalIncomeMultiplier | float | 2.0 | Income multiplier for properties in capital districts |

### Additional Functions
| Function | Returns | Purpose |
|---|---|---|
| GetOwningCity() | City* | Parent city via ParentTerritoryTag |
| GetProperties() | Array<Volume*> | All child properties |
| IsCapitalDistrict() | bool | Returns bIsCapital |
| GetPropertyCountForFaction(Faction) | int32 | Properties owned by faction |
| AllPropertiesOwnedBy(Faction) | bool | All properties owned by faction |

### Events (BlueprintNativeEvent)
| Event | When |
|---|---|
| OnDistrictFullyCaptured(Faction) | District captured by a new faction (capital bonus) |

### Delegates (BlueprintAssignable)
| Delegate | Signature |
|---|---|
| OnDistrictCapturedDelegate | (District*, OldOwner, NewOwner) |

### Hierarchy Collapse

When a district changes owner:
1. City's `CascadeCaptureToProperties` iterates all child properties
2. Each property's `SetOwningFaction` is called with the new district owner
3. Each property fires `OnPropertyCaptured` + `OnPropertyCapturedDelegate`
4. Property upgrade level resets to 0 on capture by a new faction
5. Economy income recalculated for both old and new owners

### Setup Example
```
BP_TerritoryDistrict "Market Square"
  TerritoryTag: Territory.HavenReach.MarketSquare
  ParentTerritoryTag: Territory.HavenReach
  InitialOwningFaction: Narrative.Factions.Bandits
  InitialPeriodicIncome: 200
  bIsCapital: true
  CapitalIncomeMultiplier: 2.0
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
| GetEffectiveIncome() | int32 | Base + upgrade bonus + capital multiplier |
| TryUpgrade() | bool | Authority-only upgrade (debits treasury) |
| SetUpgradeLevel(Level) | void | Authority-only direct set |
| GetOwningDistrict() | District* | Parent district via ParentTerritoryTag |

### Events (BlueprintNativeEvent)
| Event | When |
|---|---|
| OnUpgradeLevelChanged(NewLevel) | Client receives replicated upgrade change |
| OnPropertyCaptured(NewOwner) | Property captured by new faction (resets upgrade level) |

### Delegates (BlueprintAssignable)
| Delegate | Signature |
|---|---|
| OnPropertyCapturedDelegate | (Property*, NewOwner) |

### Property BeginPlay

On authority BeginPlay, properties auto-sync their ownership to their parent district's owner. This ensures properties start aligned with their district's faction even after save/load.

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

## ATerritoryGuardCharacter

Extends `ANarrativeNPCCharacter` from Narrative Pro.

### Additional Functions
| Function | Purpose |
|---|---|
| SetTerritorySaveGUID(GUID) | Set save GUID before FinishSpawning |
| SetOwningTerritoryGUID(GUID) | Link to parent territory |

## ATerritoryWorldState

**Place ONE in the level** for multiplayer-persistent territory state.

### What It Stores (replicated)
- Faction treasuries (gold, income, costs, territory count)
- Transaction history (audit trail)
- Active treaties (with timing, expiry, permanence)
- Faction reputation
- Capture summaries (per territory)