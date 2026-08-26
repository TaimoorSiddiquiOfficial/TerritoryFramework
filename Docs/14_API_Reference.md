# API Reference — Complete Function, Property, Delegate Catalog

## Table of Contents
- [TerritoryBlueprintLibrary](#territoryblueprintlibrary)
- [ATerritoryVolume](#aterritoryvolume)
- [ATerritoryCity](#aterritorycity)
- [ATerritoryDistrict](#aterritorydistrict)
- [ATerritoryProperty](#aterritoryproperty)
- [ATerritoryGuardCharacter](#aterritoryguardcharacter)
- [ATerritoryGuardSpawnPoint](#aterritoryguardspawnpoint)
- [UTerritoryGuardPostDefinition](#uterritoryguardpostdefinition-uprimarydataasset)
- [ATerritoryWorldState](#aterritoryworldstate)
- [ATerritorySavableData](#aterritorysavabledata)
- [UTerritoryRegistrySubsystem](#uterritoryregistrysubsystem)
- [UTerritoryControlSubsystem](#uterritorycontrolsubsystem)
- [UTerritoryEconomySubsystem](#uterritoryeconomysubsystem)
- [UTerritoryDiplomacySubsystem](#uterritorydiplomacysubsystem)
- [UTerritoryCombatDirector](#uterritorycombatdirector)
- [UTerritoryNavigationMarkerComponent](#uterritorynavigationmarkercomponent)
- [UTerritoryMapMarker](#uterritorymapmarker)
- [UTerritoryInfoWidget](#uterritoryinfowidget)
- [UTerritoryEconomyWidget](#uterritoryeconomywidget)
- [UTerritoryUIBlueprintLibrary](#uterritoryuiblueprintlibrary)
- [UTerritoryJournalWidget](#uterritoryjournalwidget)
- [UTerritoryDistrictManagementWidget](#uterritorydistrictmanagementwidget)
- [UTerritoryDebugWidget](#uterritorydebugwidget)
- [UTerritoryDebugger](#uterritorydebugger)
- [UTerritoryPatrolGoal](#uterritorypatrolgoal)
- [BTTask_RequestTerritoryPermission](#btask_requestterritorypermission)
- [BTTask_ReleaseTerritoryPermission](#btask_releaseterritorypermission)
- [UTerritoryCaptureTask](#uterritorycapturetask)
- [UTerritoryCaptureEvent](#uterritorycaptureevent)
- [UTerritoryOwnershipCondition](#uterritoryownershipcondition)
- [ITerritoryOwnershipInterface](#iterritoryownershipinterface)
- [ITerritoryEconomyInterface](#iterritoryeconomyinterface)
- [ITerritoryEventReceiverInterface](#iterritoryeventreceiverinterface)
- [Enums](#enums)
- [Structs](#structs)
- [Delegates](#delegates)
- [DeveloperSettings](#developersettings)

---

## TerritoryBlueprintLibrary

Static Blueprint-callable helpers. Use these as the primary entry point from Blueprint.

### Functions (BlueprintPure)

| Function | Parameters | Returns | Description |
|---|---|---|---|
| GetTerritoryRegistry | WorldContextObject | UTerritoryRegistrySubsystem* | Get registry subsystem |
| GetTerritoryControl | WorldContextObject | UTerritoryControlSubsystem* | Get control subsystem |
| GetTerritoryEconomy | WorldContextObject | UTerritoryEconomySubsystem* | Get economy subsystem |
| GetTerritoryCombatDirector | WorldContextObject | UTerritoryCombatDirector* | Get combat director |
| GetTerritoryDiplomacy | WorldContextObject | UTerritoryDiplomacySubsystem* | Get diplomacy subsystem |
| GetAllFactions | WorldContextObject | TArray<FGameplayTag> | All factions known to subsystems |
| GetTerritoryAtLocation | WorldContextObject, Location (FVector) | ATerritoryVolume* | Find territory at point |
| GetTerritoryByTag | WorldContextObject, Tag (GameplayTag) | ATerritoryVolume* | Find territory by tag |
| GetAllTerritories | WorldContextObject | TArray<ATerritoryVolume*> | All registered territories |
| GetTerritoriesByFaction | WorldContextObject, Faction (GameplayTag) | TArray<ATerritoryVolume*> | Territories owned by faction |
| GetChildTerritories | WorldContextObject, ParentTag (GameplayTag) | TArray<ATerritoryVolume*> | Child territories (districts of city, properties of district) |
| GetTerritoryCount | WorldContextObject | int32 | Total registered territory count |
| GetFactionTerritoryCount | WorldContextObject, Faction (GameplayTag) | int32 | Count owned by faction |
| IsTerritoryAtLocation | WorldContextObject, Location (FVector) | bool | True if any territory contains point |
| GetFactionGold | WorldContextObject, Faction (GameplayTag) | int32 | **Deprecated** — always returns 0. Use GetActorCurrency instead |
| GetFactionIncome | WorldContextObject, Faction (GameplayTag) | int32 | Sum of periodic income for faction |
| GetTerritoryState | WorldContextObject, TerritoryTag (GameplayTag) | ETerritoryState | Query state by tag |
| GetCaptureProgress | WorldContextObject, TerritoryTag (GameplayTag) | float | Query progress by tag |
| GetTreatyState | WorldContextObject, FactionA, FactionB | EDiplomacyState | Diplomacy state between factions |
| IsAllied | WorldContextObject, FactionA, FactionB | bool | True if alliance or higher |
| IsAtWar | WorldContextObject, FactionA, FactionB | bool | True if at war |
| IsSameFaction | A (GameplayTag), B (GameplayTag) | bool | Check if two factions are the same |
| GetFriendlyTagDisplayName | Tag (GameplayTag) | FText | UI-friendly tag text |
| GetActorFactions | WorldContextObject, Actor | FGameplayTagContainer | All factions via Narrative team interface |
| IsActorInFaction | WorldContextObject, Actor, Faction | bool | True if actor belongs to faction |
| GetActorPrimaryFaction | WorldContextObject, Actor | GameplayTag | Actor's primary faction |
| AreActorsAllied | A (Actor), B (Actor) | bool | Shared Narrative faction-tag check, matching Narrative `IsSameTeam` semantics |
| GetAllCities | WorldContextObject | TArray<ATerritoryCity*> | All registered cities |
| GetAllDistricts | WorldContextObject | TArray<ATerritoryDistrict*> | All registered districts |
| GetCityForDistrict | WorldContextObject, District | ATerritoryCity* | Parent city for district |
| DoesFactionControlCity | WorldContextObject, City, Faction | bool | True if faction owns all districts |
| GetFactionCityCount | WorldContextObject, Faction | int32 | Cities fully controlled by faction |
| GetFactionDistrictCount | WorldContextObject, Faction | int32 | Districts owned by faction |
| GetCapitalDistricts | WorldContextObject | TArray<ATerritoryDistrict*> | Districts marked as capital |

### Functions (BlueprintCallable)

| Function | Parameters | Authority | Description |
|---|---|---|---|
| ForceCaptureTerritory | WorldContextObject, TerritoryTag, NewOwner | BlueprintAuthorityOnly, DevelopmentOnly | Resolve the territory and invoke ControlSubsystem `ForceCapture` |
| PrintTerritoryDebug | WorldContextObject, Territory, Duration | DevelopmentOnly | Print debug info for one territory |
| PrintAllTerritoryDebug | WorldContextObject, Duration | DevelopmentOnly | Print debug info for all territories |

---

## ATerritoryVolume

Base territory actor. Place in level to define a capturable zone.

### Properties (BlueprintReadWrite)

| Property | Type | Category | SaveGame | Replicated | Notes |
|---|---|---|---|---|---|
| TerritoryTag | FGameplayTag | Territory | — | — | Unique identifier tag |
| TerritoryDisplayName | FText | Territory | — | — | Display name for UI |
| InitialOwningFaction | FGameplayTag | Territory | — | — | Set at design time, applied in BeginPlay |
| InitialState | ETerritoryInitialState | Territory | — | — | Automatic, Unclaimed, Claimed, Contested, or Locked new-campaign state |
| InitialMaxConcurrentAttackers | int32 | Territory\|Capture | — | — | Design-time default |
| InitialPeriodicIncome | int32 | Territory\|Economy | — | — | Design-time default |
| InitialGuardCost | int32 | Territory\|Economy | — | — | Recurring upkeep per assigned guard per cycle |
| InitialGuardRecruitmentCost | int32 | Territory\|Economy | — | — | One-time Narrative inventory debit per target increase |
| ParentTerritoryTag | FGameplayTag | Territory | — | — | Parent city tag (for districts) |
| TerritoryGUID | FGuid | Territory | ✅ | — | Editor-stable unique ID |
| BoundsShape | UShapeComponent* | Territory\|Bounds | — | — | Collision shape for bounds |
| GuardNPCDefinition | UNarrativeNPCDefinition* | Territory\|Guards | — | — | Default NPC definition for guards |
| FactionGuardDefinitions | TArray<FTerritoryFactionGuardDefinition> | Territory\|Guards | — | — | Per-faction NPC definition overrides |
| GuardSpawnCount | int32 | Territory\|Guards | — | — | Authored initial target for non-player captures |
| PostCaptureGarrisonPolicy | ETerritoryPostCaptureGarrisonPolicy | Territory\|Guards | — | — | Default `PlayerChooses` starts captures by a resolved matching live Narrative player faction at zero |
| GuardSpawnRadius | float | Territory\|Guards | — | — | Deprecated/ignored; no random active-guard fallback |
| GuardSpawnPoints | TArray<ATerritoryGuardSpawnPoint*> | Territory\|Guards | — | — | Explicit post references; the unique resolved union is active capacity, one guard per point |
| ControlMode | ETerritoryControlMode | Territory\|Hierarchy | — | — | Independent (default), AggregateOnly, or Cascading |
| StateConfigs | TMap<ETerritoryState, FTerritoryStateConfig> | Territory\|State | — | — | Per-state entry/exit conditions and entry/exit events; designer-configured |

Legacy serialized `bStartsLocked` and `LockConditions` remain readable only for bounded
migration. New assets use `InitialState` and the Locked row's Exit Conditions.

### OwnershipData (Replicated, RepNotify)

| Field | Type | Notes |
|---|---|---|
| OwningFaction | FGameplayTag | Stable owner; retains the incumbent defender while Contested |
| TerritoryState | ETerritoryState | Current state |
| ControlProgress | float | 0.0–1.0 |
| ContestingFaction | FGameplayTag | Who is attacking |
| DefenderCount | int32 | Active defenders |
| MaxConcurrentAttackers | int32 | Budget limit |
| PeriodicIncome | int32 | Current income value |
| GuardCost | int32 | Current upkeep cost |
| GuardRecruitmentCost | int32 | Current one-time recruitment price |
| DesiredGuardCount | int32 | Persistent staffing target |

### BlueprintPure Functions

| Function | Returns | Category |
|---|---|---|
| GetOwningFaction | GameplayTag | Territory |
| GetTerritoryState | ETerritoryState | Territory |
| GetControlProgress | float | Territory |
| IsContested | bool | Territory |
| IsOwnedByFaction(Faction) | bool | Territory |
| GetTerritoryTag | GameplayTag | Territory |
| GetTerritoryDisplayName | Text | Territory |
| GetMaxConcurrentAttackers | int32 | Territory |
| GetDefenderCount | int32 | Territory |
| GetPeriodicIncome | int32 | Territory |
| GetGuardCost | int32 | Territory |
| GetTerritoryBounds | FBox | Territory |
| ContainsPoint(Point) | bool | Territory |
| GetParentTerritoryTag | GameplayTag | Territory |
| GetInitialOwningFaction | GameplayTag | Territory |
| GetSpawnedGuardCount | int32 | Territory\|Guards |
| HasGuardsAlive | bool | Territory\|Guards |
| GetGuardSpawnPoints | Array<TerritoryGuardSpawnPoint*> | Territory\|Guards |
| GetDesiredGuardCount | int32 | Territory\|Guards |
| GetMaxGuardCount | int32 | Territory\|Guards (unique loaded spawn-point count) |
| GetPostCaptureGuardCount | int32 | Territory\|Guards |
| GetGarrisonSnapshot | FTerritoryGarrisonSnapshot | Territory\|Guards |
| GetGuardRecruitmentCost | int32 | Territory\|Guards |
| GetEffectiveIncome | int32 | Territory\|Economy |
| IsFullyCaptured | bool | Territory |
| GetCapturingFaction | GameplayTag | Territory |
| IsLocked | bool | Territory |
| GetUpgradeLevel | int32 | Territory |
| GetContestingFaction | GameplayTag | Territory |
| GetRegisteredDefenders | Array<Actor*> | Territory |
| GetOwnershipData | FTerritoryOwnershipData | Territory — returns copy of current ownership data struct (C++ only) |
| GetControlMode | ETerritoryControlMode | Territory\|Hierarchy |

`IsOwnedByFaction(Faction)` requires both a matching owner tag and `TerritoryState == Claimed`. It returns false while Contested, Locked, or Unclaimed even when `GetOwningFaction()` still reports the incumbent.

### BlueprintCallable (AuthorityOnly)

| Function | Parameters | Description |
|---|---|---|
| SetOwningFaction | NewFaction (GameplayTag) | Validated compatibility wrapper through `ApplyTerritoryMutation`; use that subsystem API for an explicit context/result |
| SetControlProgress | Progress (float) | Set capture progress |
| SetTerritoryState | NewState (ETerritoryState) | Force-set state |
| RegisterDefender | Defender (Actor*) | Add to defender list |
| UnregisterDefender | Defender (Actor*) | Remove from defender list |
| SpawnGuards | — | Spawn all guards per config |
| DespawnGuards | — | Despawn all guards |
| TryPurchaseGuards | RequestingPawn, Count | Compatibility delta wrapper over absolute staffing target |
| TryRemoveGuards | RequestingPawn, Count | Compatibility delta wrapper; does not require missing/dead guards to be live |
| TrySetDesiredGuardCount | Requester, NewDesiredGuardCount | Atomically debit recruitment, deploy/withdraw, update upkeep, or fully roll back |
| SetUpgradeLevel | Level (int32) | Force-set property upgrade level |
| CommitOwnershipData | NewData (FTerritoryOwnershipData), TransitionContext (FTerritoryTransitionContext) | bool | Atomically commits new ownership data — single struct write, one ordered event bundle (guards → state events → ownership delegates → state delegates). Returns false if no-op. |
| LockTerritory | Reason (FText) | void | Lock the territory with optional reason |
| TryUnlock | bForce (bool) | bool | Unlock — force bypasses conditions |
| SpawnSingleGuard | SpawnPoint (ATerritoryGuardSpawnPoint*) | void | Spawn one guard at the given spawn point |
| TrySpawnSingleGuard | SpawnPoint (ATerritoryGuardSpawnPoint*), bRequireConcealment (bool) | bool | Attempt one guard spawn with optional camera avoidance |

### BlueprintPure (Guard Queries)

| Function | Parameters | Returns | Description |
|---|---|---|---|
| CanUnlock | (none) | bool | Read-only check if unlock would succeed |
| GetLockReason | (none) | FText | Returns the lock reason text |
| CanPurchaseGuards | Requester (AActor*), Count (int32), OutFailureReason (FText&) | bool | Check if guard purchase is allowed |
| CanRemoveGuards | Requester (AActor*), Count (int32), OutFailureReason (FText&) | bool | Check if guard removal is allowed |
| GetGuardPurchaseCost | Count (int32) | int32 | Calculate cost for N guards |
| CanSetDesiredGuardCount | Requester, NewDesiredGuardCount, OutFailureReason, OutRecruitmentCost | bool | Validate one absolute staffing target |

### BlueprintNativeEvent

| Event | Parameters | When |
|---|---|---|
| OnOwnershipChanged | OldOwner (GameplayTag), NewOwner (GameplayTag) | Ownership changes |
| OnStateChanged | OldState (ETerritoryState), NewState (ETerritoryState) | State changes |

### Delegates (BlueprintAssignable)

| Delegate | Signature |
|---|---|
| OnTerritoryOwnershipChanged | (ATerritoryVolume*, OldOwner, NewOwner) |
| OnTerritoryStateChangedDelegate | (ATerritoryVolume*, NewState) |
| OnGuardKilled | (ATerritoryVolume*, Guard, Killer, RemainingDefenders) |
| OnAllGuardsDefeatedDelegate | (ATerritoryVolume*) |

---

## ATerritoryCity

Extends `ATerritoryVolume`. Represents a city that controls districts.

### Properties

Cities inherit all `ATerritoryVolume` properties. Parent-child relationships are established via `ParentTerritoryTag` on each `ATerritoryDistrict`.

### Functions

| Function | Returns | Description |
|---|---|---|
| GetDistricts | TArray<ATerritoryVolume*> | All districts belonging to this city (resolved via Registry) |
| HasCapitalDistrict | bool | True if any district in this city has `bIsCapital` set |
| IsFullyCaptured | bool | City owns all its districts |
| GetCityControlPercentage | float | 0.0–1.0 of districts owned by faction |

### Behavior

On BeginPlay, binds to `Registry->OnTerritoryRegistered` to catch late-spawning districts. When all districts are captured by the same faction, city ownership updates.

---

## ATerritoryDistrict

Extends `ATerritoryVolume`. A sub-zone within a city.

### Properties

| Property | Type | Notes |
|---|---|---|
| bIsCapital | bool | BlueprintReadWrite — capital district flag (only district, not city has this; city uses `HasCapitalDistrict()` instead) |

---

## ATerritoryProperty

Extends `ATerritoryVolume`. An upgradeable property within a district.

### Properties (BlueprintReadWrite)

| Property | Type | SaveGame | Replicated | RepNotify | Notes |
|---|---|---|---|---|---|
| UpgradeLevel | int32 | ✅ | ✅ | ✅ OnRep_UpgradeLevel | Current upgrade tier |
| MaxUpgradeLevel | int32 | — | — | — | Cap (default 3) |
| UpgradeCostPerLevel | int32 | — | — | — | Base cost per level |
| IncomeBonusPerLevel | int32 | — | — | — | Income added per level |
| ProductionProfile | UTerritoryProductionProfile* | — | — | — | Optional item-resource recipe asset; capture does not require one |

### BlueprintPure

| Function | Returns | Description |
|---|---|---|
| CanUpgrade | bool | Level < Max and enough gold |
| GetUpgradeCost | int32 | Cost for next level |
| GetEffectiveIncome | int32 | Periodic + (Level × BonusPerLevel) |
| GetOwningDistrict | ATerritoryDistrict* | Parent district |

### BlueprintCallable (AuthorityOnly)

| Function | Returns | Description |
|---|---|---|
| TryUpgrade | bool | Debits requester's Narrative inventory, increments level |
| SetUpgradeLevel | void | Force-set level |

### BlueprintImplementableEvent

| Event | Parameters |
|---|---|
| OnUpgradeLevelChanged | NewLevel (int32) |

---

## ATerritoryGuardCharacter

Extends `ANarrativeNPCCharacter`. Guard NPC spawned by territory volumes.

### Stable GUID Override

```cpp
virtual FGuid GetActorGUID_Implementation() const override
{
    if (SpawnInfo.SpawnAssignedSaveGUID.IsValid())
    {
        return SpawnInfo.SpawnAssignedSaveGUID;
    }
    if (!CachedFallbackGUID.IsValid())
    {
        const_cast<ATerritoryGuardCharacter*>(this)->CachedFallbackGUID = FGuid::NewGuid();
    }
    return CachedFallbackGUID;
}
```

This returns the save-system-assigned GUID when valid. Otherwise it generates and caches one fallback GUID, preventing `NarrativeStableActor` assertion crashes without changing identity on repeated calls.

### Replicated Context

| Property | Type | Description |
|---|---|---|
| TerritoryHomeTransform | FTransform | Resolved home transform used by return activities |
| OwningTerritory | ATerritoryVolume* | Owning territory back-reference |
| OwningTerritorySpawnPoint | ATerritoryGuardSpawnPoint* | Required authoritative active-slot back-reference |

### Blueprint API

| Function | Returns | Description |
|---|---|---|
| ConfigureTerritorySpawnWithContext(..., OwningTerritory, OwningSpawnPoint) | bool | Authority-only deferred-spawn configuration. Validates typed ownership and stable identities before applying the Narrative definition. |
| ConfigureTerritorySpawn(...) | void | Deprecated migration node. Resolves typed ownership by Territory GUID and spawn-point name, then calls `ConfigureTerritorySpawnWithContext`; fails closed when resolution is ambiguous. |
| GetTerritoryPatrolRoute | TArray<FTerritoryPatrolNode> | Copy assigned patrol route |
| HasTerritoryPatrolRoute | bool | True when an assigned route has at least two nodes |
| GetPatrolNodeCount | int32 | Number of assigned nodes |
| GetSafePatrolNode | bool | Bounds-checked node lookup |
| GetSpawnTransform | FTransform | Stored `TerritoryHomeTransform` |
| GetOwningTerritory | ATerritoryVolume* | Replicated owning territory |
| GetGuardFaction | FGameplayTag | Narrative faction, including spawn overrides |
| IsSpawnPointGuard | bool | Whether a spawn point is assigned |

### Behavior

- Core garrisons call the native `SpawnThroughNarrative` adapter, which uses
  `UNarrativeCharacterSubsystem::SpawnNPC` and supplies complete `SpawnInfo` plus Territory
  context before `SetNPCDefinition`
- New Blueprint deferred-spawn graphs must use `ConfigureTerritorySpawnWithContext` so the
  complete typed ownership context exists before `SetNPCDefinition`
- Existing `ConfigureTerritorySpawn` nodes remain as an explicitly deprecated migration path
- Exact faction overrides come from the selected spawn point or owning territory
- Multi-slot definitions must allow multiple Narrative instances; class, Narrative
  controller, spawned auto-possession, and instance policy are runtime/editor validated
- `ShouldRespawn_Implementation()` returns false so Narrative does not restore stale individual guard actors

---

## ATerritoryGuardSpawnPoint

Actor placed in level to define guard spawn locations and patrol routes.

### Properties

| Property | Type | Default | Notes |
|---|---|---|---|
| OwnerTerritoryTag | FGameplayTag | — | Optional explicit owner; territory `GuardSpawnPoints` references take precedence, then this tag, then proximity |
| MaxGuards | int32 | 1 | Deprecated/ignored legacy value; each point is one active slot |
| ReserveSlots | int32 | 1 | Guards that only spawn when active guards die |
| PatrolRoute | TArray<FTerritoryPatrolNode> | empty | Ordered waypoints for patrol. Empty = guard stays at spawn point |
| bLoopPatrol | bool | true | Whether patrol route loops back to start |
| FactionOverride | FGameplayTag | — | Override territory's faction for this point |
| Priority | int32 | 50 | Higher priority spawn points fill first |
| bAutoSpawnReserves | bool | true | Auto-deploy reserves on guard death |
| ReserveSpawnDelay | float | 3.0 | Delay before reserve deployment |
| ReserveSpawnRetryInterval | float | 2.0 | Retry interval for blocked spawns |
| ReserveSpawnRadius | float | 600.0 | Random placement radius (uu) |
| ReserveMinimumPlayerDistance | float | 500.0 | Minimum distance from players for reserve spawns |
| ReserveSpawnCandidateCount | int32 | 12 | NavMesh candidate locations per spawn attempt |
| GuardPostDefinition | UTerritoryGuardPostDefinition* | null | Data asset for reusable guard configuration |
| NPCDefinitionOverride | UNPCDefinition* | null | Per-point NPC definition override |
| ActivityConfigurationOverride | UNPCActivityConfiguration* | null | Per-point activity configuration override |
| TriggerSetOverrides | TArray<TSoftObjectPtr<UTriggerSet>> | empty | Per-point trigger set overrides |

### Struct: FTerritoryPatrolNode

| Field | Type | Notes |
|---|---|---|
| Location | FVector | World position |
| Rotation | FRotator | Face direction |
| WaitTime | float | Seconds to wait at node |
| ActivityTag | FGameplayTag | Optional activity tag (e.g., Guard.Activity.Inspect) |

### Functions

| Function | Returns | Description |
|---|---|---|
| HasAvailableSlot | bool | Whether this point's single active slot is empty |
| HasReserveAvailable | bool | Whether reserve guards remain |
| GetActiveGuardCount | int32 | Currently alive spawned guards |
| GetReserveCount | int32 | Remaining reserve guards |
| RegisterSpawnedGuard | void | Notify that a guard was spawned from this point |
| UnregisterGuard | void | Notify that a guard died/was removed from this point |
| GetPatrolRoute | TArray<FTerritoryPatrolNode> | Full patrol node array, returned by value |
| GetPatrolRouteAsTransforms | TArray<FTransform> | Patrol route as transforms (for behavior trees) |
| GetPatrolWaitTimes | TArray<float> | Wait times parallel to GetPatrolRouteAsTransforms |
| HasPatrolRoute | bool | Whether PatrolRoute contains at least two nodes |
| GetSpawnTransform | FTransform | Exact authored foot-marker transform |
| ResolveGuardDeploymentTransform | bool + FTransform | Preserves exact X/Y/facing and aligns Z for the guard capsule |
| HasPendingReserveSpawn | bool | Whether a reserve guard spawn is pending |
| SpawnReserveGuard | bool | Manually trigger a reserve guard spawn. Returns true if a reserve was consumed and spawn initiated |
| GetLoopPatrol | bool | Whether patrol route loops back to start |
| IsLoopingPatrol | bool | Whether patrol is currently looping |

### Save System

`ATerritoryGuardSpawnPoint` implements `INarrativeSavableActor` — persists `SpawnPointGUID`, `CurrentReserveCount`, `PendingReserveSpawns`, and `SavedActiveGuardCount` across save/load cycles. The spawn point uses an editor-baked GUID for stable identity.

### Death Handling

When a guard dies:
1. `ATerritoryGuardSpawnPoint::UnregisterGuard()` is called
2. If `bAutoSpawnReserves` is true and a reserve is available, consume one and call `Territory->SpawnSingleGuard(this)` for one replacement

---

## UTerritoryGuardPostDefinition (UPrimaryDataAsset)

Reusable guard configuration asset. Assign to `ATerritoryGuardSpawnPoint::GuardPostDefinition` to share guard settings across multiple spawn points.

### Properties

| Property | Type | Default | Description |
|---|---|---|---|
| DisplayName | FText | — | Display name for editor identification |
| FactionOverride | FGameplayTag | — | Override territory faction for guards spawned from this post |
| NPCDefinition | UNPCDefinition* | null | NPC definition for guards |
| ActivityConfiguration | UNPCActivityConfiguration* | null | Activity config override |
| TriggerSetOverrides | TArray<TSoftObjectPtr<UTriggerSet>> | empty | Trigger set overrides |
| PatrolRoute | TArray<FTerritoryPatrolNode> | empty | Patrol waypoints |
| bLoopPatrol | bool | true | Loop patrol route |
| MaxGuards | int32 | 1 | Deprecated/ignored; capacity comes from placed spawn-point count |
| ReserveSlots | int32 | 1 | Reserve guard count |
| ReserveSpawnDelay | float | 3.0 | Delay before reserve deployment |
| ReserveSpawnRetryInterval | float | 2.0 | Retry interval for blocked spawns |
| ReserveSpawnRadius | float | 600.0 | Random placement radius (uu) |
| ReserveMinimumPlayerDistance | float | 500.0 | Minimum distance from players |
| ReserveSpawnCandidateCount | int32 | 12 | NavMesh candidates per attempt |

Primary asset type: `TerritoryGuardPost`.

---

## ATerritoryWorldState

Global persistence and late-join projection for economy, diplomacy, capture summaries, and counterattacks. Place exactly one per world. Authority delegates keep its arrays current and RepNotify hydrates client query subsystems.

### Replicated Arrays

| Array | Struct | Purpose |
|---|---|---|
| ReplicatedTreasuries | FReplicatedFactionEconomy | Faction treasuries |
| ReplicatedTransactions | FReplicatedTransaction | Transaction history |
| ReplicatedProductionSites | FTerritoryProductionSiteRecord | World Partition-safe site and per-rule status projection |
| ReplicatedResourceSnapshots | FTerritoryFactionResourceSnapshot | Read-only Narrative stockpile projection |
| ReplicatedTreaties | FReplicatedTreaty | Active treaties |
| ReplicatedCaptureSummaries | FReplicatedCaptureSummary | Per-territory state at the last export/setter update |
| ReplicatedReputation | FReplicatedFactionReputation | Faction reputation |
| ReplicatedDiplomacyHistory | FDiplomacyEvent | Diplomacy event history |
| ReplicatedAssaults | FTerritoryAssaultRecord | Deterministic decisions, lifecycle, and finite force counts |

`SavedAssaultCycles` is a server-only `SaveGame` array of `FTerritoryAssaultCycleRecord`; it is deliberately not replicated because clients do not schedule assaults.

### Functions

| Function | Type | Description |
|---|---|---|
| ExportPersistentState | AuthorityOnly | Copy subsystem state → replicated arrays |
| ImportPersistentState | AuthorityOnly | Copy replicated arrays → subsystems |

`ExportPersistentState` rebuilds economy, transaction, treaty, reputation, capture projections, assault records, and the counterattack cycle ledger. Import restores the economy ledger, rich treaty metadata, finite assault state, and deterministic high-water marks. TerritoryVolume owns durable ownership; participant actor identities are not persisted.

### Delegates

| Delegate | Signature |
|---|---|
| OnTransactionRecorded | (FTerritoryTransaction) |

---

## ATerritorySavableData

Legacy single-player save adapter. Place exactly 1 per level for single-player. Implements `INarrativeSavableActor` overrides.

### Functions (INarrativeSavableActor overrides)

| Function | Description |
|---|---|
| PrepareForSave_Implementation | Save all subsystem state to SaveGame properties |
| Load_Implementation | Restore all subsystem state from SaveGame properties |
| ShouldRespawn_Implementation | Returns false (persistent save-game adapter never respawns) |

### Editor-Stable GUID

Uses `PostEditChangeProperty` and `PostDuplicate` to maintain stable GUIDs across edits and duplications.

---

## UTerritoryRegistrySubsystem

### Functions

| Function | Type | Returns | Description |
|---|---|---|---|
| RegisterTerritory | — | ETerritoryRegistrationResult | Called by ATerritoryVolume::BeginPlay; rejects invalid/duplicate tag or GUID |
| UnregisterTerritory | — | void | Called by ATerritoryVolume::EndPlay (identity-safe) |
| GetTerritoryByTag | Pure | ATerritoryVolume* | O(1) lookup |
| GetTerritoryByGUID | Pure | ATerritoryVolume* | O(1) lookup |
| GetTerritoryAtLocation | Pure | ATerritoryVolume* | O(1) spatial grid |
| GetAllTerritories | Pure | TArray<ATerritoryVolume*> | All registered |
| GetChildTerritories | Pure | TArray<ATerritoryVolume*> | Districts under parent |
| GetTerritoriesByFaction | Pure | TArray<ATerritoryVolume*> | Owned by faction |

### Delegates

| Delegate | Signature |
|---|---|
| OnTerritoryRegistered | (ATerritoryVolume*, bool bWasUnregistered) |
| OnTerritoryUnregistered | (ATerritoryVolume*, bool bWasUnregistered) |

---

## UTerritoryControlSubsystem

### Actions (BlueprintAuthorityOnly)

| Function | Parameters | Returns |
|---|---|---|
| AttemptCapture | Territory, Faction | ECaptureResult |
| ForceCapture | Territory, Faction | bool; backward-compatible force path that resolves a matching live Narrative player context for `PlayerChooses` |
| ForceCaptureWithContext | Territory, Faction, FTerritoryTransitionContext | bool; preferred exact-instigator force path; normalizes RequestingFaction and applies the same explicit bypasses |
| ResetCapture | Territory | void |
| AddCaptureProgress | Territory, Faction, Delta | void |
| RegisterAttacker | Territory, Actor, Faction | void; invalid, duplicate, blocked, or over-budget registrations are ignored |
| UnregisterAttacker | Territory, Actor, Faction | void |
| ApplyTerritoryMutation | FTerritoryMutationRequest | FTerritoryMutationResponse | Atomic territory mutation — validates authority, diplomacy, invariants, commits ownership atomically, fires one ordered event bundle |

### Queries (BlueprintPure)

| Function | Returns |
|---|---|
| IsCaptureInProgress(Territory) | bool |
| GetCaptureProgress(Territory) | float |
| GetContestingFaction(Territory) | GameplayTag |
| HasAttackBudget(Territory, Faction) | bool | Checks CombatDirector slot availability |
| GetActiveAttackers(Territory, Faction) | int32 | Count of identity-based attackers |
| CanFactionCaptureTerritory(Territory, Faction) | bool | Whether faction can initiate capture |

### Delegates

| Delegate | Signature | BlueprintAssignable |
|---|---|---|
| OnTerritoryControlChanged | (ATerritoryVolume*, FGameplayTag OldOwner, FGameplayTag NewOwner) | ✅ |
| OnCaptureAttempted | (const FCaptureAttempt& Attempt) | ✅ |

---

## UTerritoryEconomySubsystem

### Actions (BlueprintAuthorityOnly)

| Function | Parameters | Returns |
|---|---|---|
| CreditCurrency | Beneficiary, Amount, Faction, Reason, Type | bool |
| TryDebitCurrency | Requester, Amount, Faction, Reason, Type | bool |
| CreditCurrencyToFaction | Faction, Amount, Policy, Reason, Type, PreferredBeneficiary (optional) | int32 paid |
| RegisterFactionCurrencyAccount | Faction, Policy, AccountActor | bool |
| UnregisterFactionCurrencyAccount | Faction, AccountActor | void |
| RegisterFactionResourceAccount | Faction, AccountActor | bool |
| UnregisterFactionResourceAccount | Faction, AccountActor | void |
| ProcessResourceProduction | none | void |
| ExecuteResourceRecipe | Requester, Faction, Rule, UpgradeLevel, BatchCount, SourceTerritory, OutResult | bool |
| SetFactionTreasury | Faction, Treasury | void |
| RecalculateIncome | Faction | void |

### Queries (BlueprintPure)

| Function | Returns |
|---|---|
| GetActorCurrency(Requester) | int32 |
| GetIncome(Faction) | int32 |
| GetCosts(Faction) | int32 |
| CanActorAfford(Requester, Cost) | bool |
| GetOnlineFactionPlayers(Faction) | TArray\<ANarrativeCharacter*\> |
| GetFactionEconomy(Faction) | FTerritoryTreasury |
| GetAllFactionsWithTreasury | TArray\<FGameplayTag\> |
| GetFactionResourceSnapshot(Faction) | FTerritoryFactionResourceSnapshot |
| GetProductionSitesForFaction(Faction) | TArray\<FTerritoryProductionSiteRecord\> |
| GetProductionSite(TerritoryTag) | FTerritoryProductionSiteRecord |

### Other

| Function | Type | Returns |
|---|---|---|
| GetTransactionHistory | — | TArray<FTerritoryTransaction> |

### Properties

| Property | Type | Default | Notes |
|---|---|---|---|
| MaxTransactionHistory | int32 | 500 | Global ledger cap |
| ProductionCycleLength | float | 2400 | Narrative accumulated-time units per production day |
| MaxProductionCatchupCycles | int32 | 7 | Maximum ordered missed days processed in one evaluation |

### Delegates

| Delegate | Signature |
|---|---|
| OnEconomyTickFired | (Faction, FTerritoryEconomySnapshot) |
| OnTransactionRecorded | (FTerritoryTransaction) |
| OnFactionUpkeepDeficit | (Faction, Deficit) — fires when a faction can't pay full guard upkeep; Deficit = required − paid |
| OnProductionSettled | (FTerritoryProductionResult) - success and evaluated failure outcomes |

---

## UTerritoryDiplomacySubsystem

### Actions (BlueprintAuthorityOnly)

| Function | Parameters | Returns |
|---|---|---|
| DeclareWar | FactionA, FactionB | void |
| DeclarePeace | FactionA, FactionB | void |
| FormAlliance | FactionA, FactionB | void |
| BreakAlliance | FactionA, FactionB | void |
| SignTradeAgreement | FactionA, FactionB, Duration | void |
| SignNonAggression | FactionA, FactionB | void |
| BreakCeasefire | FactionA, FactionB | void |
| SetDiplomacyState | FactionA, FactionB, State | void |
| AddReputation | Faction, Amount | void |
| SetReputation | Faction, Value | void |

### Queries (BlueprintPure)

| Function | Returns |
|---|---|
| GetDiplomacyState(A, B) | EDiplomacyState |
| IsAtWar(A, B) | bool |
| IsAllied(A, B) | bool |
| HasTradeAgreement(A, B) | bool |
| GetReputation(Faction) | int32 |

### Other

| Function | Returns |
|---|---|
| GetAllTreaties | TArray<FTreatyRecord> |
| GetTreatiesForFaction(Faction) | TArray<FTreatyRecord> |
| GetDiplomacyHistory | TArray<FDiplomacyEvent> |
| GetAllReputation | TMap<FGameplayTag, int32> (BlueprintPure) |

### Save/Sync (BlueprintCallable)

| Function | Description |
|---|---|
| SyncToGameState | Push treaty-derived attitudes into both `ANarrativeGameState` directions |
| LoadFromGameState | Reconcile treaty state from Narrative attitudes while preserving compatible rich metadata |

`SyncToGameState` is `BlueprintAuthorityOnly`; `LoadFromGameState` remains a read/reconciliation operation available on clients.

The native `SetNarrativeAttitude(A, B, Attitude)` API directly applies the requested attitude symmetrically. Treaty-derived sync is separately reconciled after startup and every Narrative `OnFinishedLoad` event.

### Native Persistence Bridge

| Function | Parameters | Returns | Description |
|---|---|---|---|
| RestorePersistentState | Treaties (TArray<FTreatyRecord>&), Reputation (TMap<FGameplayTag, int32>&), History (TArray<FDiplomacyEvent>&) | void | Restore diplomacy state from WorldState without recording gameplay events. C++ only (not UFUNCTION). |

### Delegates

| Delegate | Signature |
|---|---|
| OnDiplomacyStateChanged | (FactionA, FactionB, EDiplomacyState) |
| OnDiplomacyEvent | (const FDiplomacyEvent&) |
| OnReputationChanged | (Faction, NewReputation) |
| OnTreatyExpired | (FactionA, FactionB, EDiplomacyState) — fires when a timed treaty expires; reuses `FOnDiplomacyStateChanged` delegate type |

---

## UTerritoryCombatDirector

### Actions (BlueprintAuthorityOnly)

| Function | Parameters | Returns |
|---|---|---|
| RequestAssaultSlot | Territory, NPCController | bool | Returns false unless the pawn has a configured assault participant for this exact Territory, or when the budget is exhausted/locked |
| ReleaseAssaultSlot | Territory, NPCController | void | Frees one slot |
| ReleaseAllSlots | NPCController | void | Frees all slots across all territories |

### Queries (BlueprintPure)

| Function | Returns |
|---|---|
| GetGrantedSlots(Territory) | int32 | Active slots (filters dead controllers) |
| GetAvailableSlots(Territory) | int32 | MaxSlots - GrantedSlots |
| HasAssaultSlot(Territory, Controller) | bool | Does controller hold a slot? |
| IsEligibleAssaultController(Territory, Controller) | bool | Configured physical counterattacker targeting this exact Territory |

### Notes

CombatDirector has no `BlueprintAssignable` delegates. Slot lifecycle events are reported through the subsystem actions (`RequestAssaultSlot` returns true/false).

---

## UTerritoryNavigationMarkerComponent

### Properties

| Property | Type | Default |
|---|---|---|
| bAutoCreateMarker | bool | true |
| MarkerDisplayText | FText | (from territory) *(inherited from UNavigationMarkerComponent)* |
| bShowOwnerColor | bool | true *(inherited from UNavigationMarkerComponent)* |
| bShowContestedFlash | bool | true *(inherited from UNavigationMarkerComponent)* |
| bShowOutline | bool | true *(inherited from UNavigationMarkerComponent)* |

### Functions

| Function | Type | Description |
|---|---|---|
| SetMarkerDisplayText | Callable | Override label |
| SetShowOwnerColor | Callable | Toggle faction color |
| SetShowContestedFlash | Callable | Toggle contested flash |
| SetShowOutline | Callable | Toggle border outline |
| RefreshMarker | Callable | Force redraw |

---

## UTerritoryMapMarker

Extends `UMapMarker` (Narrative Pro).

### Properties

| Property | Type | Notes |
|---|---|---|
| FactionColorMap | TMap<GameplayTag, LinearColor> | Faction → color |
| TerritoryVolume | TWeakObjectPtr<ATerritoryVolume> | Bound territory |

### Key Methods

| Method | Description |
|---|---|
| MarkerOnPaint | Draws colored polygon on map canvas |
| ClearTerritoryBinding | Unbinds all delegates |
| RefreshMarker | Updates colors from territory state |

---

## UTerritoryInfoWidget

UMG widget for displaying territory info.

### Functions

| Function | Description |
|---|---|
| BindToTerritory(Tag) | Bind to territory by FGameplayTag |
| BindToTerritoryAtPlayer() | Bind to territory at player's current location |
| UnbindFromTerritory() | Remove delegate bindings, clear territory reference, and clear stale display fields |
| GetBoundTerritory | Get bound ATerritoryVolume* |

### Blueprint Events

| Event | Parameters |
|---|---|
| OnTerritoryOwnershipChanged | OldOwner (FGameplayTag), NewOwner (FGameplayTag) |
| OnTerritoryStateChanged | NewState (ETerritoryState) |
| OnTerritoryBound | Territory (ATerritoryVolume*) |
| OnTerritoryUnbound | none |

---

## UTerritoryEconomyWidget

UMG widget for displaying faction economy.

### Functions

| Function | Returns | Description |
|---|---|---|
| SetDisplayFaction | void | Set faction to display |
| GetCurrentGold | int32 | Owning pawn's Narrative inventory/account balance |
| GetCurrentIncome | int32 | Per-tick income |
| GetCurrentCosts | int32 | Per-tick costs |
| GetTerritoryCount | int32 | Owned territories |
| GetNetIncome | int64 | Income minus costs |
| IsOperatingAtDeficit | bool | True when costs exceed income |
| GetEconomyOperationsView | FTerritoryEconomyOperationsView | Funds, rates, deficit, activity, stockpile, and production-site projections |

### Blueprint Events

| Event | Parameters |
|---|---|
| OnEconomyUpdated | (Faction, FTerritoryEconomySnapshot) |
| OnTransactionRecorded | (FTerritoryTransaction) |
| OnEconomyOperationsUpdated | (FTerritoryEconomyOperationsView) |

---

## UTerritoryUIBlueprintLibrary

Shared read-model and Narrative CommonUI bridge. It owns no gameplay state.

| Function | Returns | Description |
|---|---|---|
| OpenTerritoryMenu | UTerritoryActivatableWidget* | Push a Territory screen into a registered Narrative HUD layer |
| BuildDistrictOperationsView | bool + OutView | Build one viewer-relative district projection |
| BuildHierarchyOperationsView | bool + OutView | Build one City, District, or Place projection |
| IsTerritoryVisibleToPlayer | bool | Require the Territory and every loaded parent to be registered and unlocked |
| GetDistrictHierarchyOperationsViews | Array<HierarchyView> | Visible City -> District -> Place branch |
| BuildGarrisonOperationsView | bool + OutView | Build one independently managed District/Property garrison and local P&L |
| GetDistrictGarrisonOperationsViews | Array<GarrisonView> | District garrison plus loaded registered child Properties |
| GetDistrictOperationsViews | Array<View> | List districts matching an operational filter |
| GetPlayerVisibleDistrictOperationsViews | Array<View> | Player-safe City-grouped District list with locked/unloaded parent branches removed |
| DoesDistrictMatchFilter | bool | Evaluate one projection against a filter |
| GetDistrictOperationsRevision | int32 | Hash all displayed state used for list invalidation |
| BuildEconomyOperationsView | EconomyView | Narrative funds plus Territory income/cost/activity projection |
| GetProductionStatusText | FText | Localizable production-state label |
| GetThreatLevelText | FText | Localizable threat label |
| GetAssaultStateText | FText | Localizable assault-state label |
| GetDiplomacyStateText | FText | Localizable diplomacy-state label |

`ETerritoryOperationsFilter` values are All, Unlocked, Available, Owned, Manageable, UnderAttack, Contested, Locked, FinancialRisk, Producing, ProductionBlocked, MissingInputs, and StorageFull. `ETerritoryThreatLevel` values are None, Watch, Warning, and Critical.

---

## UTerritoryJournalWidget

Narrative-activatable operations dashboard.

| Function | Returns | Description |
|---|---|---|
| RefreshDistrictList | void | Rebuild when the complete operations revision changes |
| SelectDistrict | void | Select one visible District, synchronize entry highlighting, reveal known Places, and update the detail pane |
| SetOperationsFilter | void | Apply the viewer-relative operational filter |
| GetSelectedDistrictOperationsView | View | Current detail/security/finance/threat projection |
| GetActiveTerritoryEntryCount | int32 | Count real District rows under Active Territories; empty-state text is not counted |
| GetCapturedTerritoryEntryCount | int32 | Count real District rows under Captured Territories; empty-state text is not counted |

The supplied widget follows Narrative Pro's Quest Journal contract: Active and Captured
Territory `ScrollBox` lists, one reusable entry class, one selected District, and one persistent
detail pane. It provides Overview, Places, Garrison, Economy, Production, Threats, and
Diplomacy selected-detail tabs. Locked hierarchy branches never enter the player lists. Its
selector/target/Apply/0/Max controls and delta shortcuts route through
`UTerritoryPlayerManagementComponent` and can manage visible child Property garrisons.

Easy diagnostic example: Market Square can report `Active = 1, Captured = 0` before capture and
`Active = 0, Captured = 1` after ownership moves to the viewer faction. Both values count entry
widgets, not the explanatory text displayed when a queue is empty.

---

## UTerritoryDistrictManagementWidget

Narrative-activatable in-world district command screen.

| Function | Returns | Description |
|---|---|---|
| InitializeManagement | void | Bind a management point |
| GetManagedDistrict | ATerritoryDistrict* | Resolve the current district |
| GetManagedFaction | FGameplayTag | Faction resolved from the owning pawn |
| GetDistrictIncome | int32 | Effective district income |
| CanPurchaseGuard | bool + reason | Exact read-only add validation |
| CanRemoveGuard | bool + reason | Exact read-only remove validation |
| GetOperationsView | View | Complete current projection |
| RequestAddGuards | void | Submit server-validated bulk addition |
| RequestRemoveGuards | void | Submit server-validated bulk removal |
| RefreshManagementDisplay | void | Refresh presentation |

---

## UTerritoryDebugWidget

Tick-based debug overlay.

### Functions

| Function | Description |
|---|---|
| SetDebugEnabled(bool) | Toggle live territory and counterattack debug display |

### Blueprint Events

| Event | Parameters |
|---|---|
| OnUpdateDebugText | FString (multi-line debug summary) |

---

## UTerritoryDebugger

`BuildTerritoryDebugSummary(WorldContextObject, DebugActor)` is BlueprintPure and resolves
either a selected Territory actor or the Territory containing the selected actor. It reports
tag, owner, state, progress, garrison counts, capacity, and finite assault records. The runtime
module registers the same data as the `Territory` Gameplay Debugger category in developer builds.

---

## UTerritoryPatrolGoal

Extends `UNPCGoalItem` (Narrative Pro). Goal instance populated from a territory guard's assigned spawn point patrol route. Automatically created during `ATerritoryGuardCharacter::InitializeTerritoryPatrolGoal()` when the guard has a valid patrol route.

### Properties

| Property | Type | Description |
|---|---|---|
| TerritoryPatrol | TArray<FTerritoryPatrolNode> | Patrol waypoints copied from the spawn point's `PatrolRoute` |

### Overrides

| Function | Behavior |
|---|---|
| GetGoalScore_Implementation | Returns parent score when ≥2 patrol nodes; 0 otherwise (prevents goal activation with insufficient route) |
| ShouldCleanup_Implementation | Returns true when no patrol nodes remain |

---

## BTTask_RequestTerritoryPermission

Legacy behavior tree task for physical counterattack participants. It requests a strategic assault slot from the CombatDirector before allowing that attacker to enter its attack branch. Ordinary defenders use Narrative attack tokens only and must use the shipped `BTService_TerritoryAssaultPermission`, which grants their branch without consuming a strategic slot.

### Blackboard Keys

| Key | Type | Direction | Notes |
|---|---|---|---|
| TerritoryKey | UObject (ATerritoryVolume) | Read | Target territory. Falls back to spatial lookup if not set. |
| bPermissionGrantedKey | bool | Write | True if slot granted, false if denied |

### Behavior

1. Gets NPCController from AI owner and requires its pawn to have configured assault identity
2. Reads TerritoryKey from blackboard (or finds territory at NPC location)
3. Calls `CombatDirector->RequestAssaultSlot(Territory, NPCController)`
4. Writes result to `bPermissionGrantedKey`
5. Returns Succeeded (granted) or Failed (denied)

**Note:** Fails immediately if `bPermissionGrantedKey` is not configured or the pawn is not a physical participant for the exact target Territory.

---

## BTTask_ReleaseTerritoryPermission

Behavior tree task. Releases assault slot(s) for an NPC.

### Blackboard Keys

| Key | Type | Direction | Notes |
|---|---|---|---|
| TerritoryKey | UObject (ATerritoryVolume) | Read | Target territory for targeted release |

### Behavior

1. If TerritoryKey is configured and resolves: calls `ReleaseAssaultSlot(Territory, NPCController)` for that territory only
2. If TerritoryKey is not configured: calls `ReleaseAllSlots(NPCController)` across all territories (legacy fallback)

---

## UTerritoryCaptureTask

Extends `UNarrativeTask`. Quest task for territory capture objectives.

### Properties

| Property | Type | Notes |
|---|---|---|
| TargetTerritoryTag | FGameplayTag | Which territory to capture |
| RequiredCapturingFaction | FGameplayTag | Which faction must capture |
| bCompleteOnLoss | bool | Complete if ownership changes from initial owner (any direction) |

### Behavior

- Tracks `InitialOwner` at activation
- Completes when `OnTerritoryOwnershipChanged` fires and new owner == `RequiredCapturingFaction`
- If `bCompleteOnLoss`, completes on ANY ownership change from `InitialOwner`

---

## UTerritoryCaptureEvent

Extends `UNarrativeEvent`. Fires when territory is captured.

### Properties

| Property | Type | Notes |
|---|---|---|
| TargetTerritoryTag | FGameplayTag | Territory to capture |
| bForceCapture | bool | Bypass gameplay AttemptCapture rules |

### Behavior

- Both modes submit one contextual `FTerritoryMutationRequest`.
- If `bForceCapture` → enables explicit condition, diplomacy, and lock bypass flags while preserving the Tales/player context.
- If not → uses the same atomic mutation without bypass flags; Narrative state conditions and diplomacy remain authoritative.

---

## UTerritoryOwnershipCondition

Extends `UNarrativeCondition`. Checks territory ownership in quest graphs.

### Properties

| Property | Type | Notes |
|---|---|---|
| RequiredOwner | FGameplayTag | Required owning faction |
| TargetTerritoryTag | FGameplayTag | Territory to check |
| bPassWhenContested | bool | Pass if territory is contested |
| bPassWhenUnclaimed | bool | Pass if no owner |
| bPassWhenLocked | bool | Pass if locked |

---

## ITerritoryOwnershipInterface

Implement on actors that need to expose territory ownership.

### Functions

| Function | Returns | Notes |
|---|---|---|
| GetTerritoryOwner_Implementation | FGameplayTag | Stable owner, including the incumbent defender while Contested |
| GetTerritoryControlProgress_Implementation | float | 0.0–1.0 |
| IsTerritoryContested_Implementation | bool | Whether territory is contested |
| GetContestingFaction_Implementation | FGameplayTag | Contester (invalid if not contested) |

---

## ITerritoryEconomyInterface

Implement on actors that participate in economy.

### Functions (BlueprintNativeEvent)

| Function | Returns | Notes |
|---|---|---|
| GetTreasury(Faction) | int32 | Deprecated; no faction wallet |
| GetPeriodicIncome(Faction) | int32 | Per-tick income for faction |
| CanAfford(Faction, Cost) | bool | Deprecated; use CanActorAfford |
| GetActorCurrency(Requester) | int32 | Narrative inventory currency |
| CanActorAfford(Requester, Cost) | bool | Exact-account affordability |

---

## ITerritoryEventReceiverInterface

BlueprintNativeEvent — implement to receive territory events.

### Functions (BlueprintNativeEvent)

| Function | Notes |
|---|---|
| OnTerritoryControlChanged | Called when ownership changes (TerritoryTag, OldOwner, NewOwner) |
| OnTerritoryContested | Called when attack starts (TerritoryTag, ContestingFaction) |
| OnTerritoryUncontested | Called when attack ends (TerritoryTag) |
| OnTerritoryStateChanged | Called when territory state transitions (TerritoryTag, NewState) |

---

## UTerritoryCounterAttackSubsystem

Server-authoritative scheduler for deterministic, finite physical assaults. It never changes ownership directly.

| Function | Type | Returns |
|---|---|---|
| ScheduleCounterAttack(Territory, AttackingFaction) | AuthorityOnly | bool |
| ScheduleBestCounterAttack(Territory, PreferredFaction) | AuthorityOnly | bool; evaluates every configured diplomacy-eligible force and schedules the strongest |
| CancelAssault(AssaultID, Reason) | AuthorityOnly | bool |
| GetAssault(AssaultID, OutAssault) | Pure | bool |
| GetAllAssaults() | Pure | Array<AssaultRecord> |
| GetAssaultsForTerritory(Tag) | Pure | Array<AssaultRecord> |
| GetAssaultsForTerritoryActor(Territory) | Pure | Array<AssaultRecord>; exact loaded actor/GUID query |
| IsAssaultActive(AssaultID) | Pure | bool |
| IsAssaultPendingOrActive(AssaultID) | Pure | bool |
| GetAssaultDebugString(AssaultID) | Pure | string |
| GetBestEligibleAttackerPreview(Territory, PreferredFaction, OutFaction, OutInput, OutResult, OutReason) | Pure | bool; planning only, no cycle reservation or decision roll |

| Delegate | Delivery | Payload |
|---|---|---|
| `OnAssaultChanged` | Server subsystem; every record/casualty update | `FTerritoryAssaultRecord` |
| `OnAssaultWarning` | Server subsystem; targeted warning admission | Controller + record |
| `OnCounterHappened` | Server subsystem; once after each committed state transition | `FTerritoryCounterAttackStateEvent` |

`UTerritoryPlayerManagementComponent::OnCounterHappened` receives the same state-event payload on the relevant owning client through a reliable RPC. `UTerritoryHUDWidget::OnCounterHappened` forwards it as a Blueprint event suitable for Narrative HUD notification nodes. Save/load hydration never replays the event; late join reads the replicated WorldState snapshot.

Native persistence functions are `GetPersistentState`, `GetPersistentCycleState`, and the `RestorePersistentState` overload accepting both arrays. The record-only restore overload remains for client read-model hydration and compatibility. `CalculateEvaluation` is a deterministic pure native calculator.

## UTerritoryCounterAttackProfile

`UPrimaryDataAsset` containing grace/proximity/notification policy, launch probability
weights, maximum approaches, and per-faction `FTerritoryFactionAssaultConfig` values.
`UnguardedLaunchProbability` defaults to `1.0` and applies only when the complete local
same-owner District/Property defence cascade has zero active guards after hard
diplomacy/admission gates.

## ATerritoryAssaultCharacter and Narrative activity

`ATerritoryAssaultCharacter` derives from `ANarrativeNPCCharacter`. `UTerritoryAssaultParticipantComponent` owns replicated assault identity and exact-once capture/death registration. `UTerritoryAssaultGoal` derives from `UNPCGoalItem` and scores below Narrative's attack goal; `UTerritoryAssaultActivity` derives from `UNPCActivity`. `TerritoryAssaultTargetPolicy` is a transient adapter over Narrative's public goal-key/score contract: while registered hostile defenders remain, it suppresses non-defender attack goals, preserves defender scores, and restores exact original scores afterward. It does not persist or replicate goal pointers/scores and does not replace Narrative perception, activities, behavior trees, GAS, or attack tokens. Combat priority is score-driven rather than dependent on Narrative's unused interrupt flag.

See [17_Counterattack_System.md](17_Counterattack_System.md) for lifecycle and configuration.

## Enums

### ETerritoryControlMode

| Value | Description |
|---|---|
| Independent | Standard territory — owns its own state, can be captured directly |
| AggregateOnly | Parent-only — state derived from children, cannot be directly captured |
| Cascading | Capture cascades to children; direct capture allowed and child ownership follows |

### ETerritoryState

| Value | Description |
|---|---|
| Unclaimed | No owner |
| Claimed | Owned and stable |
| Contested | Capture in progress |
| Locked | Cannot be captured |

### ETerritoryRegistrationResult

| Value | Description |
|---|---|
| Success | Territory registered normally |
| DuplicateTag | Another territory already has this tag |
| DuplicateGUID | Another territory already has this GUID |
| InvalidTerritory | Null or invalid territory actor |

### ECaptureResult

| Value | Description |
|---|---|
| Success | Capture initiated successfully |
| AlreadyOwned | Attacker already owns territory |
| Locked | Territory is locked |
| DefendersRemain | Guards still alive |
| DiplomaticallyBlocked | Factions have a Friendly Narrative attitude (Alliance, TradeAgreement, or NonAggression) |
| InvalidTerritory | No valid territory provided |

### ETerritoryTransactionType

| Value | Description |
|---|---|
| Income | Economy tick income |
| GuardUpkeep | Economy tick guard cost |
| UpgradeCost | Property upgrade |
| Purchase | Territory or asset purchase |
| Reward | External reward (quest, event) |
| Scripted | Scripted economy change |
| ManualCredit | Manual credit via API |
| ManualDebit | Manual debit via API |

### EDiplomacyState

| Value | Description |
|---|---|
| None | Default / neutral |
| Alliance | Friendly, can't capture |
| TradeAgreement | Friendly, timed |
| NonAggression | Friendly, permanent |
| Ceasefire | Neutral; permanent until explicitly changed or broken |
| War | Hostile, can capture |

### EDiplomacyEventType

| Value | Description |
|---|---|
| DeclaredWar | War started |
| DeclaredPeace | War ended |
| FormedAlliance | Alliance created |
| BrokeAlliance | Alliance dissolved |
| SignedTradeAgreement | Trade agreement signed |
| ExpiredTreaty | Any treaty expired |
| BrokeCeasefire | Ceasefire broken |
| SignedNonAggression | Non-aggression pact signed |

---

## Structs

### FTerritoryOwnershipData

| Field | Type | SaveGame | Replicated | Notes |
|---|---|---|---|---|
| OwningFaction | FGameplayTag | ✅ | ✅ | Stable owner |
| TerritoryState | ETerritoryState | ✅ | ✅ | Current state |
| ControlProgress | float | ✅ | ✅ | 0.0–1.0 |
| ContestingFaction | FGameplayTag | ✅ | ✅ | Who is attacking |
| DefenderCount | int32 | ✅ | ✅ | Active defenders |
| MaxConcurrentAttackers | int32 | ✅ | ✅ | Budget limit |
| PeriodicIncome | int32 | ✅ | ✅ | Current income value |
| GuardCost | int32 | ✅ | ✅ | Current upkeep cost |
| GuardRecruitmentCost | int32 | ✅ | ✅ | One-time price per newly assigned guard |
| DesiredGuardCount | int32 | ✅ | ✅ | Absolute target garrison size |
| LockReason | FText | ✅ | ✅ | Reason text when territory is locked; empty when not locked |

### FTerritoryStateConfig

Per-state configuration for territory transitions. Designer-authored via `StateConfigs` TMap on ATerritoryVolume.

| Field | Type | Description |
|-------|------|-------------|
| EntryConditions | TArray<UNarrativeCondition*> | Conditions that must pass before entering this state |
| EntryEvents | TArray<UNarrativeEvent*> | Events fired when entering this state |
| ExitEvents | TArray<UNarrativeEvent*> | Events fired when leaving this state |

### FTerritoryTransitionContext

Explicit context for territory state transitions. Replaces `GetFirstPlayerController()` with the actual instigator. Pass to `CommitOwnershipData` and `ApplyTerritoryMutation`.

| Field | Type | Description |
|-------|------|-------------|
| Instigator | AActor* | The actor that initiated the transition |
| TargetPawn | APawn* | The pawn involved in condition/event evaluation |
| PlayerController | APlayerController* | The exact controller for a player-driven transition; null for deliberate world/AI context |
| TalesComponent | UTalesComponent* | Tales component for quest/dialogue event context |
| RequestingFaction | FGameplayTag | The faction requesting or causing the transition |

### FTerritoryMutationRequest

Request struct for `ApplyTerritoryMutation`.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| Territory | ATerritoryVolume* | null | The territory to mutate |
| NewOwner | FGameplayTag | Invalid | Faction that should own the territory |
| DesiredState | ETerritoryState | Claimed | Target state after mutation |
| bClearCaptureState | bool | true | Clear contesting faction and progress |
| bBypassConditions | bool | false | Explicitly bypass Narrative state-entry conditions |
| bBypassDiplomacy | bool | false | Explicitly bypass treaty/capture policy |
| bBypassLock | bool | false | Explicitly bypass a locked Territory; force capture only |
| TransitionContext | FTerritoryTransitionContext | Default | Context for Narrative conditions/events |

### FTerritoryMutationResponse

Response struct from `ApplyTerritoryMutation`.

| Field | Type | Description |
|-------|------|-------------|
| Result | ETerritoryMutationResult | Success, Rejected_Authority, Rejected_NullTerritory, Rejected_AggregateOnly, Rejected_InvalidFaction, Rejected_DiplomacyBlocked, Rejected_Locked, Rejected_StateUnchanged, Failed_FinalStateMismatch |
| Territory | ATerritoryVolume* | The territory that was mutated |
| OldOwner | FGameplayTag | Owner before mutation |
| NewOwner | FGameplayTag | Owner after mutation |
| OldState | ETerritoryState | State before mutation |
| NewState | ETerritoryState | State after mutation |
| Explanation | FText | Human-readable explanation |

### FTerritoryTransaction

| Field | Type | Notes |
|---|---|---|
| TransactionID | FGuid | Auto-generated |
| Faction | FGameplayTag | Which faction |
| Type | ETerritoryTransactionType | Category |
| Amount | int32 | +credit or -debit |
| BalanceAfter | int32 | Post-transaction balance |
| GameTime | double | Accumulated game time |
| Reason | FString | Human-readable |
| SourceTerritory | FGameplayTag | Optional source |

### FTerritoryEconomySnapshot

| Field | Type | Notes |
|---|---|---|
| Treasury | int32 | Deprecated compatibility field; always `0` because Narrative inventories own every balance |
| TotalIncome | int32 | Per-tick income |
| TotalCosts | int32 | Per-tick costs |
| TerritoryCount | int32 | Owned territories |

### FTerritoryTreasury

| Field | Type | Notes |
|---|---|---|
| IncomePerTick | int32 | Calculated income |
| CostsPerTick | int32 | Calculated costs |
| TerritoryCount | int32 | Owned territories |

> **Note**: Narrative Pro's `UInventoryComponent::Currency` is the sole balance. TerritoryFramework stores only income/cost rates and transaction metadata; it does not maintain a faction wallet.

### FTerritoryPatrolNode

| Field | Type | Notes |
|---|---|---|
| Location | FVector | World position |
| WaitTime | float | Seconds at node |
| Rotation | FRotator | Face direction |

### FTreatyRecord

| Field | Type | Notes |
|---|---|---|
| FactionA | FGameplayTag | Party A |
| FactionB | FGameplayTag | Party B |
| State | EDiplomacyState | Treaty type |
| SignedGameTime | float | When signed |
| ExpiryGameTime | float | Absolute expiry time; -1 when not timed |
| bPermanent | bool | Permanent flag |

### FDiplomacyEvent

| Field | Type | Notes |
|---|---|---|
| EventType | EDiplomacyEventType | What happened |
| FactionA | FGameplayTag | Party A |
| FactionB | FGameplayTag | Party B |
| GameTime | float | When |

### FCaptureAttempt

| Field | Type | Notes |
|---|---|---|
| Territory | ATerritoryVolume* | Target |
| AttackingFaction | FGameplayTag | Who |
| DefendingFaction | FGameplayTag | Incumbent owner at attempt time |
| Result | ECaptureResult | Outcome |
| AttackersPresent | int32 | Registered attackers for the attacking faction |
| DefendersPresent | int32 | Territory defender count |

### FReplicatedFactionEconomy

| Field | Type | Notes |
|---|---|---|
| Faction | FGameplayTag | Faction |
| Treasury | int32 | Reserved snapshot field; current export leaves this at 0 because wealth lives in Narrative inventories |
| IncomePerTick | int32 | Income |
| CostsPerTick | int32 | Costs |
| TerritoryCount | int32 | Owned territories |

### FReplicatedTransaction

Mirrors `FTerritoryTransaction` for network replication.

### FReplicatedTreaty

Replicates the treaty parties, state, signed/expiry times, permanence, and a canonical `TreatyID`. WorldState import restores the rich metadata directly to the DiplomacySubsystem.

### FReplicatedCaptureSummary

| Field | Type | Notes |
|---|---|---|
| TerritoryTag | FGameplayTag | Which territory |
| CurrentOwner | FGameplayTag | Stable owner/incumbent defender |
| ContestingFaction | FGameplayTag | Leading attacker |
| ControlProgress | float | Leading capture progress |
| State | ETerritoryState | Territory state |

### FReplicatedFactionReputation

| Field | Type | Notes |
|---|---|---|
| Faction | FGameplayTag | Faction |
| Reputation | int32 | Score |

---

## Delegates

### Territory Volume Delegates

| Delegate | Signature | BlueprintAssignable |
|---|---|---|
| FOnTerritoryControlChanged | (ATerritoryVolume*, OldOwner, NewOwner) | ✅ |
| FOnTerritoryStateChanged | (ATerritoryVolume*, ETerritoryState) | ✅ |
| FOnGuardKilled | (ATerritoryVolume*, AActor* Guard, AActor* Killer, int32 RemainingDefenders) | ✅ |
| FOnAllGuardsDefeated | (ATerritoryVolume*) | ✅ |
| FOnUpgradeLevelChanged | (ATerritoryProperty*, int32) | ✅ |

### Subsystem Delegates

| Delegate | Signature | Source |
|---|---|---|
| FOnTerritoryRegistered | (ATerritoryVolume*, bool bWasUnregistered) | Registry |
| FOnTerritoryUnregistered | (ATerritoryVolume*, bool bWasUnregistered) | Registry |
| FOnTerritoryControlChanged | (ATerritoryVolume*, FGameplayTag, FGameplayTag) | Control |
| FOnCaptureAttempted | (const FCaptureAttempt&) | Control |
| FOnEconomyTickFired | (FGameplayTag, FTerritoryEconomySnapshot) | Economy |
| FOnTransactionRecorded | (FTerritoryTransaction) | Economy + WorldState |
| FOnFactionUpkeepDeficit | (FGameplayTag Faction, int32 Deficit) | Economy |
| FOnDiplomacyStateChanged | (FGameplayTag, FGameplayTag, EDiplomacyState) | Diplomacy |
| FOnDiplomacyEvent | (const FDiplomacyEvent&) | Diplomacy |
| FOnReputationChanged | (FGameplayTag, int32) | Diplomacy |
| FOnTreatyExpired | (FGameplayTag, FGameplayTag, EDiplomacyState) | Diplomacy — fires when timed treaty expires |

> **Note:** `UTerritoryCombatDirector` has no `BlueprintAssignable` delegates. Slot grant/denial is signaled via return values on `RequestAssaultSlot`.

---

## DeveloperSettings

`UTerritoryDeveloperSettings` — configure via Project Settings → Plugins → Territory Framework.

### Runtime Settings

| Category | Settings and defaults |
|---|---|
| Economy | `EconomyTickIntervalSeconds=300`, `DefaultTerritoryIncome=100`, `DefaultGuardCost=50` |
| Capture | `CaptureProgressPerSecond=0.1`, `CaptureProgressDecayPerSecond=0.05`, `DefaultMaxConcurrentAttackers=3`, `CaptureTickInterval=0.1`, `TreatyExpirationCheckInterval=10` |
| Counterattack | `CounterAttackCampaignSeed=1337`, `CounterAttackUpdateInterval=2`, `MaxConcurrentScheduledAssaults=8`, `MaxConcurrentAssaultsPerFaction=2`, `MaxLiveCounterAttackNPCs=24`, `MaxRetainedAssaultRecords=100` |
| Spatial | `SpatialCellSize=2000` Unreal units |
| Guards | `DefaultPatrolArrivalThreshold=100`, `DefaultPatrolAcceptanceRadius=50`, `DefaultPatrolWaitTime=2`, `MaxPatrolRouteNodes=32` |
| Identity/UI | `DefaultPlayerFaction`, `DefaultNarrativeButtonClass` |

`EconomyStartingGold` and `MaxCaptureHistory` are deprecated, unused compatibility properties scheduled for removal in v0.3.0.

### Debug Settings

All category flags require the master `bEnableDebug` switch.

| Group | Exact property names |
|---|---|
| Registry/capture | `bDebugRegistry`, `bDebugCapture`, `bDebugCaptureAttempts` |
| Ownership | `bDebugOwnershipChanges`, `bDebugStateTransitions` |
| Economy | `bDebugEconomyTicks`, `bDebugTransactions` |
| Guards/AI | `bDebugGuardSpawning`, `bDebugGuardDeaths`, `bDebugBT` |
| Diplomacy | `bDebugDiplomacy`, `bDebugFactionAttitudes` |
| Integration | `bDebugSaveLoad`, `bDebugSpatialIndex`, `bDebugMapMarkers`, `bDebugTales`, `bDebugCombat` |
| Visual | `bDrawTerritoryBounds`, `bDrawOwnershipOverlay`, `bDrawCaptureProgress`, `bDrawGuardSpawnPoints`, `bDrawSpatialGrid` |
| Verbosity | `DebugVerbosityLevel=5` (0–6) |

### Debug Helper Functions

Each debug toggle has a matching `ShouldDebug*()` helper:

```cpp
bool ShouldDebugCapture() const;
bool ShouldDebugEconomy() const;
bool ShouldDebugGuardDeaths() const;
bool ShouldDebugCombat() const;
```

Access in C++:
```cpp
const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
if (Settings->ShouldDebugCapture())
{
    UE_LOG(LogTerritory, Log, TEXT("Capture tick processing..."));
}
```
