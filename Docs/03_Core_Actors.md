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
├── ATerritoryCapturePoint    — physical player-to-capture adapter for an independent Place
└── ATerritoryWorldState      — global persistence actor (1 per level)
```

## ATerritoryVolume — Base Class

### Definition-authored setup

The values in this table are authored in the assigned City, District, or Place Definition. The
placed actor displays the applied values for inspection, but it is not a second authoring source.

| Property | Type | Default | Purpose |
|---|---|---|---|
| TerritoryTag | GameplayTag | — | Unique identity (e.g., `Territory.HavenReach.MarketSquare`) |
| TerritoryDisplayName | Text | — | Player-facing name |
| InitialOwningFaction | GameplayTag | — | Who owns at game start |
| InitialState | ETerritoryInitialState | Automatic | New-campaign state. Automatic derives Claimed/Unclaimed from InitialOwningFaction |
| InitialMaxConcurrentAttackers | int32 | 3 | NPC attack slot limit |
| InitialPeriodicIncome | int32 | 100 | Gold per economy tick |
| InitialGuardCost | int32 | 50 | Recurring upkeep per assigned guard per economy cycle |
| InitialGuardRecruitmentCost | int32 | 50 | One-time Narrative inventory price per target increase |
| PostCaptureGarrisonPolicy | enum | PlayerChooses | Resolves the new owner's initial desired garrison |
| ControlMode | ETerritoryControlMode | Independent | How ownership is resolved: Independent (direct capture), AggregateOnly (derived from children), Cascading (direct + child cascade) |
| StateConfigs | TMap\<ETerritoryState, FTerritoryStateConfig\> | — | Modular Narrative entry/exit conditions and entry/exit events for every state |
| CounterAttackProfile | UTerritoryCounterAttackProfile* | — | Counterattack data asset (see [Counterattack System](17_Counterattack_System.md)) |
| CounterAttackApproaches | TArray\<FTerritoryAssaultApproach\> | — | Typed attack approaches for this territory |
| GuardQuality | float | 0 | Guard quality rating for counterattack evaluation |
| FortificationStrength | float | 0 | Fortification rating |
| NearbyAlliedSupport | float | 0 | Nearby allied support rating |
| StrategicValue | float | 0 | District strategic value for attack prioritization |
| ParentTerritoryTag | GameplayTag | — | Parent territory for hierarchy |
| GuardNPCDefinition | NPCDefinition* | — | NPC template for guards |
| FactionGuardDefinitions | Array<FTerritoryFactionGuardDefinition> | — | Per-faction NPC definition overrides |
| GuardSpawnCount | int32 | 3 | Authored initial target for existing ownership and non-player capture |
| GuardSpawnPoints | Array<ATerritoryGuardSpawnPoint*> | — | Explicit posts; their unique union with tag/proximity posts is the exact active capacity, one guard per point |

To start locked, set **Initial State = Locked** in the Definition, then put quest or diplomacy
requirements in `State Configs -> Locked -> Exit Conditions`. The former `bStartsLocked`,
`LockConditions`, and actor-side `StateConfigs` properties have been removed after migration.
There is one modular state configuration and it belongs to the Definition asset.

### Key Events (BlueprintNativeEvent)

| Event | When | Super Required? | Override For |
|---|---|---|---|
| OnOwnershipChanged(Old, New) | After faction changes + guard lifecycle | No | Custom capture effects, sounds |
| OnStateChanged(OldState, NewState) | After state transition + guard lifecycle | No | Custom state reactions |
| OnAllGuardsDefeated() | All RegisteredDefenders dead | Recommended | Custom zero-defence reactions (plays VFX/sound). Super only calls ForceNetUpdate; DefenderCount is already 0 by the time this fires. Super never changes ownership. |
| OnTerritoryInitialized() | BeginPlay completes | No | Custom initialization logic |

See [Blueprint_Extension_Guide.md](Blueprint_Extension_Guide.md) for full Super-call requirements.

> **Missing GUID:** If `TerritoryGUID` is not baked (missed editor save), the territory logs an error and skips save/load. Always save the level after placement to bake a proper GUID.
>
> **Actor Tick:** `bStartWithTickEnabled` is `false` on all territory volumes. All periodic logic (capture, economy, diplomacy, bounds tracking) runs through subsystem timers. The volume's native `Tick` is unused and only logs a deprecation warning.

### Key Delegates (BlueprintAssignable)

| Delegate | Signature | Fires After |
|---|---|---|
| OnTerritoryOwnershipChanged | (Volume*, OldOwner, NewOwner) | Verified atomic ownership commit + OnOwnershipChanged BP event |
| OnTerritoryStateChangedDelegate | (Volume*, NewState) | Verified atomic state commit + OnStateChanged BP event |
| OnGuardKilled | (Volume*, Guard, Killer, RemainingDefenders) | Per defender death, before all-defeated check |
| OnAllGuardsDefeatedDelegate | (Volume*) | OnAllGuardsDefeated BP event |

### State Transition Logic (C++)

Guard lifecycle runs in the **non-virtual** atomic commit path before the BP virtual fires.
The Blueprint **Set Owning Faction** node now routes through
`UTerritoryControlSubsystem::ApplyTerritoryMutation`; use **Apply Territory Mutation** when
you need explicit context and a structured result.

- **Claimed → Contested**: Preserves the incumbent `OwningFaction` and surviving registered guards while active ownership is suspended
- **Contested → Claimed**: Keeps surviving guards and does not grant free replacements; an actual owner change applies the selected post-capture garrison policy
- **Locked → Claimed**: Respawns guards for owner (territory was locked, now defended again)
- **Any → Locked**: Despawns all guards
- **Contested → Unclaimed**: Not reached by guard defeat alone. Guard defeat leaves the owner intact and vulnerable (DefenderCount=0); ownership changes only through the capture flow (RegisterAttacker → progress → CompleteCapture)

`GetOwningFaction()` therefore returns the incumbent defender while contested. `IsOwnedByFaction()` requires `State == Claimed`, so it returns false while the territory is Contested, Locked, or Unclaimed.

## ATerritoryCapturePoint

`ATerritoryCapturePoint` is a server-authoritative physical interaction adapter. Set `TargetTerritoryTag` to one `Independent` Territory and author `CaptureRadius` on the placed point. Only a player-controlled pawn with a valid Narrative faction can register.

The point calls `UTerritoryControlSubsystem::TryRegisterAttacker`; it never owns or writes capture progress, Territory owner, save state, or replicated snapshots. It unregisters the pawn on overlap exit, faction/ownership invalidation, Narrative ASC death, disable, or EndPlay. Aggregate-only Districts and Cities are invalid direct targets.

### Haven Reach Example Flow

```text
capture every independent Place in Market Square -> Market Square District derives that owner
capture every independent Place in Castle Hill   -> Castle Hill District derives that owner
all Districts derive the same owner               -> Haven Reach City derives that owner
```

`/Game/HopDistrictTest` places `CapturePoint_Blsmith` for `Territory.HavenReach.MarketSquare.Blacksmith` and `CapturePoint_Farm` for `Territory.HavenReach.CastleHill.Farm`. The Farm retains its independent currency and item-production configuration after capture.

### Map Marker Component

The volume has a `MapMarkerComponent` property (UTerritoryNavigationMarkerComponent) that can be retrieved via `GetMapMarkerComponent()`.

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
| OnCityLost(PreviousFaction) | City owner no longer owns every District | Enter partial-control state, economy recalc |
| OnDistrictCapturedInCity(District, Old, New) | Any district in this city changes owner | Per-district capture effects |

### Delegates (BlueprintAssignable)
| Delegate | Signature |
|---|---|
| OnCityCapturedDelegate | (City*, CapturingFaction) |
| OnCityLostDelegate | (City*, PreviousFaction) |

### City Capture Flow (Complete)

```
Final Place in a District is captured by Faction X
  → District unanimity reducer derives District ownership
  → District.OnTerritoryOwnershipChanged broadcast
  → City.OnDistrictControlChanged handler:
      1. Fires OnDistrictCapturedInCity BP event
      2. If AllDistrictsOwnedBy(X):
         → hierarchy reducer sets derived City ownership
         → City.OnCityFullyCaptured(X) — economy bonus, capital reward
         → City.OnCityCapturedDelegate.Broadcast(this, X)
      3. If the previous city owner no longer controls every District:
         → City becomes Contested until one faction owns every District
         → City.OnCityLost(oldOwner)
         → City.OnCityLostDelegate.Broadcast(this, oldOwner)
```

Districts and Cities default to `AggregateOnly`. Capture points and normal mutation requests
cannot target them directly. Their owner is a summary derived from children, never a shortcut
that grants uncaptured Places.

`GetMajorityOwner` and `GetMajorityPropertyOwner` report influence only when one faction has a
strict majority of securely Claimed children: more than 50 percent. Contested, Locked, and
Unclaimed children do not count as owned. The helpers do not capture the parent. In a 45% Heroes / 55%
Bandits split, Bandits are the dominant faction and Heroes do not dominate or own the parent. A
50/50 split has no majority. Full District or City capture always requires 100% of its immediate
children to share one owner.

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
| GetMajorityPropertyOwner() | GameplayTag | Faction owning >50% of properties |

### Events (BlueprintNativeEvent)
| Event | When |
|---|---|
| OnDistrictFullyCaptured(Faction) | District captured by a new faction (capital bonus) |

### Delegates (BlueprintAssignable)
| Delegate | Signature |
|---|---|
| OnDistrictCapturedDelegate | (District*, OldOwner, NewOwner) |

### Bottom-up hierarchy reduction

When a Place changes owner:

1. The Place commits through the authoritative capture/mutation path and fires its normal events.
2. Its District checks all currently registered child Places.
3. Only unanimous child ownership derives a Claimed District; a partial split is Contested.
4. The City performs the same unanimity check across its Districts.
5. Majority helpers remain read-only domination information and never mutate ownership.
6. Economy is marked dirty for the old and new factions after the committed leaf change.

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

### Events
| Event | Type | When |
|---|---|---|
| OnUpgradeLevelChanged(NewLevel) | BlueprintImplementableEvent | Client receives replicated upgrade change (no C++ implementation — BP provides entire body, no Super call) |
| OnPropertyCaptured(NewOwner) | BlueprintNativeEvent | Property captured by new faction (resets upgrade level) |

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
| TerritoryDefinition | Data Asset | — | Required source containing the matching Guard Post row |
| GuardPostID | Name | — | Stable row ID inside that Definition |
| OwnerTerritoryTag | GameplayTag | — | Read-only value applied from the Definition |
| ReserveSlots | int32 | 1 | Read-only replacement count applied from the row |
| PatrolRoute | Array<PatrolNode> | — | Read-only world route built from the row's relative nodes |
| bLoopPatrol | bool | true | Read-only loop policy applied from the row |
| FactionOverride | GameplayTag | — | Read-only optional faction applied from the row |
| Priority | int32 | 50 | Read-only row priority; higher fills first |

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
| Function | Returns | Purpose |
|---|---|---|
| ConfigureTerritorySpawnWithContext(...) | bool | Authority-only external deferred-spawn adapter. Validates typed Territory/spawn-point ownership and stable identities before applying the Narrative definition. |
| ConfigureTerritorySpawn(...) | void | Deprecated migration node. Resolves typed context by Territory GUID and spawn-point name or fails closed; core garrisons use the Narrative subsystem adapter. |
| GetTerritoryPatrolRoute() | Array<PatrolNode> | Copy the assigned spawn point's route |
| HasTerritoryPatrolRoute() | bool | True when an assigned route has at least two nodes |
| GetPatrolNodeCount() | int32 | Number of assigned patrol nodes |
| GetSafePatrolNode(Index, OutNode) | bool | Bounds-checked patrol node access |
| GetSpawnTransform() | Transform | Stored `TerritoryHomeTransform` used for return-to-home behavior |
| GetOwningTerritory() | TerritoryVolume* | Replicated territory back-reference |
| GetGuardFaction() | GameplayTag | Guard's Narrative faction, including spawn-point overrides |
| IsSpawnPointGuard() | bool | Whether a replicated spawn-point back-reference is assigned |

### Replicated Guard Context

| Property | Type | Purpose |
|---|---|---|
| TerritoryHomeTransform | Transform | Exact authored X/Y/facing with vertical ground/capsule alignment |
| OwningTerritory | TerritoryVolume* | Territory back-reference |
| OwningTerritorySpawnPoint | GuardSpawnPoint* | Required active-slot back-reference |

## ATerritoryWorldState

**Place exactly ONE in the level** for global persistence and late-join read models. Server subsystem delegates maintain the replicated projection between saves.

### What It Stores (replicated)
- Faction economy params (income, costs, territory count) — faction wealth lives in NarrativePro player inventories
- Transaction history (audit trail)
- Active treaties (with timing, expiry, permanence)
- Faction reputation
- Diplomacy history
- Capture summaries (per territory)
- Counterattack decisions, lifecycle state, and finite casualty counts

`ExportPersistentState()` rebuilds the authoritative snapshot at save time. Live economy, diplomacy, capture, and assault delegates keep it current between saves; RepNotify hydrates client query subsystems. Saved contests resume leading progress without attacker identities. Saved active assaults reconstruct surviving finite force without saving pawn pointers.
