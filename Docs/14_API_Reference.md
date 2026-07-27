# API Reference — Complete Function, Property, Delegate Catalog

## Table of Contents
- [TerritoryBlueprintLibrary](#territoryblueprintlibrary)
- [ATerritoryVolume](#aterritoryvolume)
- [ATerritoryCity](#aterritorycity)
- [ATerritoryDistrict](#aterritorydistrict)
- [ATerritoryProperty](#aterritoryproperty)
- [ATerritoryGuardCharacter](#aterritoryguardcharacter)
- [ATerritoryGuardSpawnPoint](#aterritoryguardspawnpoint)
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
- [UTerritoryDebugWidget](#uterritorydebugwidget)
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
| GetAllFactions | WorldContextObject | TArray<FGameplayTag> | All factions known to subsystems |
| GetTerritoryAtLocation | WorldContextObject, Location (FVector) | ATerritoryVolume* | Find territory at point |
| GetTerritoryByTag | WorldContextObject, Tag (GameplayTag) | ATerritoryVolume* | Find territory by tag |
| IsSameFaction | A (GameplayTag), B (GameplayTag) | bool | Check if two factions are the same |

### Functions (BlueprintCallable)

| Function | Parameters | Authority | Description |
|---|---|---|---|
| ForceCaptureTerritory | WorldContextObject, TerritoryTag, NewOwner | BlueprintAuthorityOnly | Resolve the territory and invoke ControlSubsystem `ForceCapture` |

---

## ATerritoryVolume

Base territory actor. Place in level to define a capturable zone.

### Properties (BlueprintReadWrite)

| Property | Type | Category | SaveGame | Replicated | Notes |
|---|---|---|---|---|---|
| TerritoryTag | FGameplayTag | Territory | — | — | Unique identifier tag |
| TerritoryDisplayName | FText | Territory | — | — | Display name for UI |
| InitialOwningFaction | FGameplayTag | Territory | — | — | Set at design time, applied in BeginPlay |
| InitialMaxConcurrentAttackers | int32 | Territory\|Capture | — | — | Design-time default |
| InitialPeriodicIncome | int32 | Territory\|Economy | — | — | Design-time default |
| InitialGuardCost | int32 | Territory\|Economy | — | — | Design-time default |
| bStartsLocked | bool | Territory | — | — | If true, can't be captured |
| ParentTerritoryTag | FGameplayTag | Territory | — | — | Parent city tag (for districts) |
| TerritoryGUID | FGuid | Territory | ✅ | — | Editor-stable unique ID |
| BoundsShape | UShapeComponent* | Territory\|Bounds | — | — | Collision shape for bounds |
| GuardNPCDefinition | UNarrativeNPCDefinition* | Territory\|Guards | — | — | Default NPC definition for guards |
| FactionGuardDefinitions | TArray<FTerritoryFactionGuardDefinition> | Territory\|Guards | — | — | Per-faction NPC definition overrides |
| GuardSpawnCount | int32 | Territory\|Guards | — | — | Number to spawn |
| GuardSpawnRadius | float | Territory\|Guards | — | — | Radius around spawn point |
| GuardSpawnPoints | TArray<AActor*> | Territory\|Guards | — | — | Spawn point actors |

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
| GetGuardSpawnPoints | Array<Actor*> | Territory\|Guards |
| GetRegisteredDefenders | Array<Actor*> | Territory |

`IsOwnedByFaction(Faction)` requires both a matching owner tag and `TerritoryState == Claimed`. It returns false while Contested, Locked, or Unclaimed even when `GetOwningFaction()` still reports the incumbent.

### BlueprintCallable (AuthorityOnly)

| Function | Parameters | Description |
|---|---|---|
| SetOwningFaction | NewFaction (GameplayTag) | Force-set owner |
| SetControlProgress | Progress (float) | Set capture progress |
| SetTerritoryState | NewState (ETerritoryState) | Force-set state |
| RegisterDefender | Defender (Actor*) | Add to defender list |
| UnregisterDefender | Defender (Actor*) | Remove from defender list |
| SpawnGuards | — | Spawn all guards per config |
| DespawnGuards | — | Despawn all guards |

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
| TryUpgrade | bool | Debits treasury, increments level |
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
| OwningTerritorySpawnPoint | ATerritoryGuardSpawnPoint* | Spawn point back-reference; null for random fallback guards |

### Blueprint API

| Function | Returns | Description |
|---|---|---|
| ConfigureTerritorySpawn(...) | void | Configure Narrative definition, exact faction, stable IDs, home transform, and optional activity/trigger overrides during deferred spawn |
| GetTerritoryPatrolRoute | TArray<FTerritoryPatrolNode> | Copy assigned patrol route |
| HasTerritoryPatrolRoute | bool | True when an assigned route has at least two nodes |
| GetPatrolNodeCount | int32 | Number of assigned nodes |
| GetSafePatrolNode | bool | Bounds-checked node lookup |
| GetSpawnTransform | FTransform | Stored `TerritoryHomeTransform` |
| GetOwningTerritory | ATerritoryVolume* | Replicated owning territory |
| GetGuardFaction | FGameplayTag | Narrative faction, including spawn overrides |
| IsSpawnPointGuard | bool | Whether a spawn point is assigned |

### Behavior

- `ConfigureTerritorySpawn` sets Narrative `SpawnInfo` before `SetNPCDefinition`
- Exact faction overrides come from the selected spawn point or owning territory
- `ShouldRespawn_Implementation()` returns false so Narrative does not restore stale individual guard actors

---

## ATerritoryGuardSpawnPoint

Actor placed in level to define guard spawn locations and patrol routes.

### Properties

| Property | Type | Notes |
|---|---|---|
| OwnerTerritoryTag | FGameplayTag | Optional explicit owner; territory `GuardSpawnPoints` references take precedence, then this tag, then proximity |
| MaxGuards | int32 | Maximum guards that can spawn at this point (default 3) |
| ReserveSlots | int32 | Guards that only spawn when active guards die (default 1) |
| PatrolRoute | TArray<FTerritoryPatrolNode> | Ordered waypoints for patrol. Empty = guard stays at spawn point |
| bLoopPatrol | bool | Whether patrol route loops back to start (default true) |
| FactionOverride | FGameplayTag | Override territory's faction for this point |
| Priority | int32 | Higher priority spawn points fill first (default 50) |

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
| HasAvailableSlot | bool | Whether active guard count < MaxGuards |
| HasReserveAvailable | bool | Whether reserve guards remain |
| GetActiveGuardCount | int32 | Currently alive spawned guards |
| GetReserveCount | int32 | Remaining reserve guards |
| RegisterSpawnedGuard | void | Notify that a guard was spawned from this point |
| UnregisterGuard | void | Notify that a guard died/was removed from this point |
| GetPatrolRoute | TArray<FTerritoryPatrolNode> | Full patrol node array, returned by value |
| GetPatrolRouteAsTransforms | TArray<FTransform> | Patrol route as transforms (for behavior trees) |
| GetPatrolWaitTimes | TArray<float> | Wait times parallel to GetPatrolRouteAsTransforms |
| HasPatrolRoute | bool | Whether PatrolRoute contains at least two nodes |
| GetSpawnTransform | FTransform | Actor transform with its location projected to NavMesh when possible |

### Death Handling

When a guard dies:
1. `ATerritoryGuardSpawnPoint::UnregisterGuard()` is called
2. If a reserve is available, consume one and call `Territory->SpawnSingleGuard(this)` for one replacement

---

## ATerritoryWorldState

Replicated save snapshot for multiplayer-visible economy/diplomacy state. Place at most one per level. It is not continuously synchronized with subsystem mutations.

### Replicated Arrays

| Array | Struct | Purpose |
|---|---|---|
| ReplicatedTreasuries | FReplicatedFactionEconomy | Faction treasuries |
| ReplicatedTransactions | FReplicatedTransaction | Transaction history |
| ReplicatedTreaties | FReplicatedTreaty | Active treaties |
| ReplicatedCaptureSummaries | FReplicatedCaptureSummary | Per-territory state at the last export/setter update |
| ReplicatedReputation | FReplicatedFactionReputation | Faction reputation |
| ReplicatedDiplomacyHistory | FDiplomacyEvent | Diplomacy event history |

### Functions

| Function | Type | Description |
|---|---|---|
| ExportPersistentState | AuthorityOnly | Copy subsystem state → replicated arrays |
| ImportPersistentState | AuthorityOnly | Copy replicated arrays → subsystems |

`ExportPersistentState` rebuilds economy, transaction, treaty, reputation, and capture-summary arrays. Import restores the economy ledger and rich treaty metadata. TerritoryVolume load resumes the leading saved contest for decay, but actor identities and non-leading faction progress are not persisted.

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
| RegisterTerritory | — | void | Called by ATerritoryVolume::BeginPlay |
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
| ForceCapture | Territory, Faction | void; validates authority/inputs, bypasses gameplay capture rules, sets progress to 1.0 and state to Claimed |
| ResetCapture | Territory | void |
| AddCaptureProgress | Territory, Faction, Delta | void |
| RegisterAttacker | Territory, Actor, Faction | void; invalid, duplicate, blocked, or over-budget registrations are ignored |
| UnregisterAttacker | Territory, Actor, Faction | void |

### Queries (BlueprintPure)

| Function | Returns |
|---|---|
| IsCaptureInProgress(Territory) | bool |
| GetCaptureProgress(Territory) | float |
| GetContestingFaction(Territory) | GameplayTag |
| HasAttackBudget(Territory, Faction) | bool | Checks CombatDirector slot availability |
| GetActiveAttackers(Territory, Faction) | int32 | Count of identity-based attackers |

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
| CreditCurrencyToFaction | Faction, Amount, Policy, Reason, Type | int32 paid |
| SetFactionTreasury | Faction, Treasury | void |
| RecalculateIncome | Faction | void |

### Queries (BlueprintPure)

| Function | Returns |
|---|---|
| GetActorCurrency(Requester) | int32 |
| GetIncome(Faction) | int32 |
| GetCosts(Faction) | int32 |
| CanActorAfford(Requester, Cost) | bool |
| GetFactionEconomy(Faction) | FTerritoryTreasury |
| GetAllFactionsWithTreasury | TArray<FGameplayTag> |

### Other

| Function | Type | Returns |
|---|---|---|
| GetTransactionHistory | — | TArray<FTerritoryTransaction> |

### Properties

| Property | Type | Default | Notes |
|---|---|---|---|
| MaxTransactionHistory | int32 | 500 | Per-faction cap |

### Delegates

| Delegate | Signature |
|---|---|
| OnEconomyTickFired | (Faction, FTerritoryEconomySnapshot) |
| OnTransactionRecorded | (FTerritoryTransaction) |

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
| GetAllReputation | TMap<FGameplayTag, int32> |

### Save/Sync (BlueprintCallable)

| Function | Description |
|---|---|
| SyncToGameState | Push treaty-derived attitudes into both `ANarrativeGameState` directions |
| LoadFromGameState | Reconcile treaty state from Narrative attitudes while preserving compatible rich metadata |

`SyncToGameState` is `BlueprintAuthorityOnly`; `LoadFromGameState` remains a read/reconciliation operation available on clients.

The native `SetNarrativeAttitude(A, B, Attitude)` API directly applies the requested attitude symmetrically. Treaty-derived sync is separately reconciled after startup and every Narrative `OnFinishedLoad` event.

### Delegates

| Delegate | Signature |
|---|---|
| OnDiplomacyStateChanged | (FactionA, FactionB, EDiplomacyState) |
| OnDiplomacyEvent | (const FDiplomacyEvent&) |
| OnReputationChanged | (Faction, NewReputation) |

---

## UTerritoryCombatDirector

### Actions (BlueprintAuthorityOnly)

| Function | Parameters | Returns |
|---|---|---|
| RequestAssaultSlot | Territory, NPCController | bool | Returns false if budget exhausted or locked |
| ReleaseAssaultSlot | Territory, NPCController | void | Frees one slot |
| ReleaseAllSlots | NPCController | void | Frees all slots across all territories |

### Queries (BlueprintPure)

| Function | Returns |
|---|---|
| GetGrantedSlots(Territory) | int32 | Active slots (filters dead controllers) |
| GetAvailableSlots(Territory) | int32 | MaxSlots - GrantedSlots |
| HasAssaultSlot(Territory, Controller) | bool | Does controller hold a slot? |

### Notes

CombatDirector has no `BlueprintAssignable` delegates. Slot lifecycle events are reported through the subsystem actions (`RequestAssaultSlot` returns true/false).

---

## UTerritoryNavigationMarkerComponent

### Properties

| Property | Type | Default |
|---|---|---|
| bAutoCreateMarker | bool | true |
| MarkerDisplayText | FText | (from territory) |
| bShowOwnerColor | bool | true |
| bShowContestedFlash | bool | true |
| bShowOutline | bool | true |

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
| UnbindFromTerritory() | Remove delegate bindings and clear territory reference |
| GetBoundTerritory | Get bound ATerritoryVolume* |

### Blueprint Events

| Event | Parameters |
|---|---|
| OnTerritoryOwnershipChanged | OldOwner (FGameplayTag), NewOwner (FGameplayTag) |
| OnTerritoryStateChanged | NewState (ETerritoryState) |
| OnTerritoryBound | Territory (ATerritoryVolume*) |

---

## UTerritoryEconomyWidget

UMG widget for displaying faction economy.

### Functions

| Function | Returns | Description |
|---|---|---|
| SetDisplayFaction | void | Set faction to display |
| GetCurrentGold | int32 | Treasury gold |
| GetCurrentIncome | int32 | Per-tick income |
| GetCurrentCosts | int32 | Per-tick costs |
| GetTerritoryCount | int32 | Owned territories |

### Blueprint Events

| Event | Parameters |
|---|---|
| OnEconomyUpdated | (Faction, FTerritoryEconomySnapshot) |
| OnTransactionRecorded | (FTerritoryTransaction) |

---

## UTerritoryDebugWidget

Tick-based debug overlay.

### Functions

| Function | Description |
|---|---|
| SetDebugEnabled(bool) | Toggle live debug display |

### Blueprint Events

| Event | Parameters |
|---|---|
| OnUpdateDebugText | FString (multi-line debug summary) |

---

## BTTask_RequestTerritoryPermission

Behavior tree task. Requests a strategic assault slot from the CombatDirector before allowing an NPC to attack within a territory.

### Blackboard Keys

| Key | Type | Direction | Notes |
|---|---|---|---|
| TerritoryKey | UObject (ATerritoryVolume) | Read | Target territory. Falls back to spatial lookup if not set. |
| bPermissionGrantedKey | bool | Write | True if slot granted, false if denied |

### Behavior

1. Gets NPCController from AI owner
2. Reads TerritoryKey from blackboard (or finds territory at NPC location)
3. Calls `CombatDirector->RequestAssaultSlot(Territory, NPCController)`
4. Writes result to `bPermissionGrantedKey`
5. Returns Succeeded (granted) or Failed (denied)

**Note:** Fails immediately if `bPermissionGrantedKey` is not configured (prevents silent success).

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

- If `bForceCapture` → calls `ForceCapture` (still requires server authority, a resolved territory, and a valid owner tag; bypasses lock, defenders, diplomacy, and AttemptCapture)
- If not → calls `AttemptCapture` (respects diplomacy, lock, defender checks)

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

## Enums

### ETerritoryState

| Value | Description |
|---|---|
| Unclaimed | No owner |
| Claimed | Owned and stable |
| Contested | Capture in progress |
| Locked | Cannot be captured |

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

| Field | Type | SaveGame | Replicated |
|---|---|---|---|
| OwningFaction | FGameplayTag | ✅ | ✅ |
| TerritoryState | ETerritoryState | ✅ | ✅ |
| ControlProgress | float | ✅ | ✅ |
| ContestingFaction | FGameplayTag | ✅ | ✅ |
| DefenderCount | int32 | ✅ | ✅ |
| MaxConcurrentAttackers | int32 | ✅ | ✅ |
| PeriodicIncome | int32 | ✅ | ✅ |
| GuardCost | int32 | ✅ | ✅ |

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
| Treasury | int32 | Current aggregate faction wealth (sum of online members' Currency) |
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
| FOnDiplomacyStateChanged | (FGameplayTag, FGameplayTag, EDiplomacyState) | Diplomacy |
| FOnDiplomacyEvent | (const FDiplomacyEvent&) | Diplomacy |
| FOnReputationChanged | (FGameplayTag, int32) | Diplomacy |

> **Note:** `UTerritoryCombatDirector` has no `BlueprintAssignable` delegates. Slot grant/denial is signaled via return values on `RequestAssaultSlot`.

---

## DeveloperSettings

`UTerritoryDeveloperSettings` — configure via Project Settings → Plugins → Territory Framework.

### Debug Toggles

| Setting | Type | Default | Description |
|---|---|---|---|
| bDebugTerritoryRegistration | bool | false | Log register/unregister |
| bDebugCaptureFlow | bool | false | Log capture tick + progress |
| bDebugEconomyTick | bool | false | Log economy tick + transactions |
| bDebugDiplomacy | bool | false | Log diplomacy changes |
| bDebugGuardSpawning | bool | false | Log spawn/despawn |
| bDebugGuardDeath | bool | false | Log guard death + reserves |
| bDebugAttitudes | bool | false | Log faction attitude checks |
| bDebugSpatialIndex | bool | false | Log spatial index queries |
| bDebugCombatDirector | bool | false | Log attack permissions |
| bDebugSaveLoad | bool | false | Log save/load operations |
| bDebugTransactions | bool | false | Log every transaction |
| bDebugUpgrades | bool | false | Log property upgrades |

### Visual Debug Toggles

| Setting | Type | Default | Description |
|---|---|---|---|
| bShowTerritoryBounds | bool | false | Draw debug boxes in-game |
| bShowCaptureProgress | bool | false | Draw progress bars |
| bShowAttackerCount | bool | false | Draw attacker counts |
| bShowDefenderCount | bool | false | Draw defender counts |
| bShowTerritoryLabels | bool | false | Draw territory names |

### Timer Settings

| Setting | Type | Default | Range | Description |
|---|---|---|---|---|
| CaptureTickInterval | float | 0.1 | 0.01–1.0 | Seconds between capture ticks |
| TreatyExpirationCheckInterval | float | 10.0 | 1.0–60.0 | Seconds between treaty checks |
| SpatialCellSize | float | 2000.0 | 500–10000 | Spatial grid cell size (uu) |

### Debug Helper Functions

Each debug toggle has a matching `ShouldDebug*()` helper:

```cpp
bool ShouldDebugCaptureFlow() const { return bDebugCaptureFlow; }
bool ShouldDebugEconomyTick() const { return bDebugEconomyTick; }
// ... etc for all 17 toggles
```

Access in C++:
```cpp
const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
if (Settings->ShouldDebugCaptureFlow())
{
    UE_LOG(LogTerritory, Log, TEXT("Capture tick processing..."));
}
```
