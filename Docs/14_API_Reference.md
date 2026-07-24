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
| OwningFaction | FGameplayTag | Current owner |
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
| MaxUpgradeLevel | int32 | — | — | — | Cap (default 5) |
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

### Key Override

```cpp
virtual FGuid GetActorGUID_Implementation() const override
{
    return SpawnInfo.SpawnAssignedSaveGUID;
}
```

This returns the save-system-assigned GUID instead of generating one — prevents `NarrativeStableActor` assertion crashes on deferred spawn.

### Behavior

- BeginPlay binds `OnTakeAnyDamage` to force `MOVE_Walking` movement mode (fixes floating-on-hit)
- Faction set from owning territory (not NPC definition's default)

---

## ATerritoryGuardSpawnPoint

Actor placed in level to define guard spawn locations and patrol routes.

### Properties

| Property | Type | Notes |
|---|---|---|
| OwnerTerritoryTag | FGameplayTag | Auto-resolved to owning territory at BeginPlay |
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
| GetPatrolRoute | const TArray<FTerritoryPatrolNode>& | Full patrol node array |
| GetPatrolRouteAsTransforms | TArray<FTransform> | Patrol route as transforms (for behavior trees) |
| GetPatrolWaitTimes | TArray<float> | Wait times parallel to GetPatrolRouteAsTransforms |
| HasPatrolRoute | bool | Whether PatrolRoute is non-empty |
| GetSpawnTransform | FTransform | Transform for the next available slot |

### Death Handling

When a guard dies:
1. `ATerritoryGuardSpawnPoint::UnregisterGuard()` is called
2. If reserves depleted → `Territory->SpawnGuards()` to produce replacement

---

## ATerritoryWorldState

Replicated actor for multiplayer economy/diplomacy state. Place exactly 1 per level for multiplayer.

### Replicated Arrays

| Array | Struct | Purpose |
|---|---|---|
| ReplicatedTreasuries | FReplicatedFactionEconomy | Faction treasuries |
| ReplicatedTransactions | FReplicatedTransaction | Transaction history |
| ReplicatedTreaties | FReplicatedTreaty | Active treaties |
| ReplicatedCaptureSummaries | FReplicatedCaptureSummary | Recent captures |
| ReplicatedReputation | FReplicatedFactionReputation | Faction reputation |

### Functions

| Function | Type | Description |
|---|---|---|
| ExportPersistentState | AuthorityOnly | Copy subsystem state → replicated arrays |
| ImportPersistentState | AuthorityOnly | Copy replicated arrays → subsystems |
| SyncSubsystemsFromReplicatedState | AuthorityOnly | Push to Economy/Diplomacy subsystems |

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
| OnTerritoryRegistered | (ATerritoryVolume*) |
| OnTerritoryUnregistered | (ATerritoryVolume*) |

---

## UTerritoryControlSubsystem

### Actions (BlueprintAuthorityOnly)

| Function | Parameters | Returns |
|---|---|---|
| AttemptCapture | Territory, Faction | ECaptureResult |
| ForceCapture | Territory, Faction | void |
| ResetCapture | Territory | void |
| AddCaptureProgress | Territory, Faction, Delta | void |
| RegisterAttacker | Territory, Actor, Faction | bool |
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
| OnCaptureAttempted | (ATerritoryVolume*, FGameplayTag Attacker) | ✅ |

---

## UTerritoryEconomySubsystem

### Actions (BlueprintAuthorityOnly)

| Function | Parameters | Returns |
|---|---|---|
| AddToTreasury | Faction, Amount, Reason, Type | void |
| TryDebitTreasury | Faction, Amount, Reason, Type | bool |
| SetFactionTreasury | Faction, Treasury | void |
| RecalculateIncome | Faction | void |

### Queries (BlueprintPure)

| Function | Returns |
|---|---|
| GetTreasury(Faction) | int32 |
| GetIncome(Faction) | int32 |
| GetCosts(Faction) | int32 |
| CanAfford(Faction, Cost) | bool |
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
| SyncToGameState | Push diplomacy state into ATerritoryWorldState for replication |
| LoadFromGameState | Pull diplomacy state back from ATerritoryWorldState after load |

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
| bForceCapture | bool | Bypass AttemptCapture rules |

### Behavior

- If `bForceCapture` → calls `ForceCapture` (bypasses all checks)
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
| GetTerritoryOwner_Implementation | FGameplayTag | Current owner |
| GetTerritoryControlProgress_Implementation | float | 0.0–1.0 |
| IsTerritoryContested_Implementation | bool | Whether territory is contested |
| GetContestingFaction_Implementation | FGameplayTag | Contester (invalid if not contested) |

---

## ITerritoryEconomyInterface

Implement on actors that participate in economy.

### Functions (BlueprintNativeEvent)

| Function | Returns | Notes |
|---|---|---|
| GetTreasury(Faction) | int32 | Current treasury for faction |
| GetPeriodicIncome(Faction) | int32 | Per-tick income for faction |
| CanAfford(Faction, Cost) | bool | Whether faction can afford the cost |

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
| DiplomaticallyBlocked | Factions diplomatically blocked (e.g., alliance, ceasefire) |
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
| Ceasefire | Neutral, temporary |
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

> **Note**: No `Gold` field — faction wealth is the aggregate of all online faction members' `UInventoryComponent::Currency` (NarrativePro). `GetTreasury()` reads live from player inventories.

### FTerritoryPatrolNode

| Field | Type | Notes |
|---|---|---|
| Location | FVector | World position |
| WaitTime | float | Seconds at node |
| Rotation | FRotator | Face direction |

### FTreatyRecord

| Field | Type | Notes |
|---|---|---|
| TreatyID | FGuid | Unique ID |
| FactionA | FGameplayTag | Party A |
| FactionB | FGameplayTag | Party B |
| Type | EDiplomacyState | Treaty type |
| StartGameTime | double | When signed |
| DurationGameTime | double | 0 = permanent |
| bIsPermanent | bool | Permanent flag |

### FDiplomacyEvent

| Field | Type | Notes |
|---|---|---|
| EventType | EDiplomacyEventType | What happened |
| FactionA | FGameplayTag | Party A |
| FactionB | FGameplayTag | Party B |
| GameTime | double | When |
| Description | FString | Details |

### FCaptureAttempt

| Field | Type | Notes |
|---|---|---|
| Territory | ATerritoryVolume* | Target |
| AttackingFaction | FGameplayTag | Who |
| Result | ECaptureResult | Outcome |
| GameTime | double | When |

### FReplicatedFactionEconomy

| Field | Type | Notes |
|---|---|---|
| Faction | FGameplayTag | Faction |
| Treasury | int32 | Aggregate faction wealth (read from player inventories) |
| IncomePerTick | int32 | Income |
| CostsPerTick | int32 | Costs |
| TerritoryCount | int32 | Owned territories |

### FReplicatedTransaction

Mirrors `FTerritoryTransaction` for network replication.

### FReplicatedTreaty

Mirrors `FTreatyRecord` for network replication.

### FReplicatedCaptureSummary

| Field | Type | Notes |
|---|---|---|
| TerritoryTag | FGameplayTag | Which territory |
| OldOwner | FGameplayTag | Previous owner |
| NewOwner | FGameplayTag | New owner |
| GameTime | double | When |

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
| FOnTerritoryRegistered | (ATerritoryVolume*) | Registry |
| FOnTerritoryUnregistered | (ATerritoryVolume*) | Registry |
| FOnTerritoryControlChanged | (ATerritoryVolume*, FGameplayTag, FGameplayTag) | Control |
| FOnCaptureAttempted | (ATerritoryVolume*, FGameplayTag) | Control |
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
| bDebugHierarchy | bool | false | Log city/district bindings |
| bDebugNavigation | bool | false | Log map marker creation |
| bDebugReplication | bool | false | Log replicated state sync |
| bDebugTransactions | bool | false | Log every transaction |
| bDebugUpgrades | bool | false | Log property upgrades |
| bDebugBlueprintBridges | bool | false | Log BP bridge calls |

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
// ... etc for all 16 toggles
```

Access in C++:
```cpp
const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
if (Settings->ShouldDebugCaptureFlow())
{
    UE_LOG(LogTerritory, Log, TEXT("Capture tick processing..."));
}
```