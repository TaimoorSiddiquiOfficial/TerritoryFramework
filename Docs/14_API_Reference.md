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
- [BTTask_MoveToPatrolNode](#btask_movetopatrolnode)
- [BTService_UpdatePatrolRoute](#btservice_updatepatrolroute)
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
| GuardNPCDefinition | UNarrativeNPCDefinition* | Territory\|Guards | — | — | NPC definition for guards |
| GuardBehaviorTree | UBehaviorTree* | Territory\|Guards | — | — | BT for guard AI |
| GuardBlackboardAsset | UBlackboardData* | Territory\|Guards | — | — | Blackboard for guard AI |
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
| OnTerritoryControlChanged | (ATerritoryVolume*, OldOwner, NewOwner) |
| OnTerritoryStateChanged | (ATerritoryVolume*, NewState) |
| OnGuardDied | (ATerritoryVolume*, Faction, EmptyTag) |

---

## ATerritoryCity

Extends `ATerritoryVolume`. Represents a city that controls districts.

### Properties

| Property | Type | Notes |
|---|---|---|
| bIsCapital | bool | Capital city flag |
| RequiredDistrictTags | TArray<FGameplayTag> | Districts that belong to this city |

### Functions

| Function | Returns | Description |
|---|---|---|
| GetOwnedDistricts | TArray<ATerritoryDistrict*> | All districts owned by this city's faction |
| IsFullyControlled | bool | City owns all its districts |
| GetControlPercentage | float | 0.0–1.0 of districts controlled |

### Behavior

On BeginPlay, binds to `Registry->OnTerritoryRegistered` to catch late-spawning districts. When all districts are captured by the same faction, city ownership updates.

---

## ATerritoryDistrict

Extends `ATerritoryVolume`. A sub-zone within a city.

### Properties

| Property | Type | Notes |
|---|---|---|
| bIsCapital | bool | BlueprintReadWrite — capital district flag |

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
| PatrolRoute | TArray<FTerritoryPatrolNode> | Waypoints for patrol |
| MaxReserveSlots | int32 | Total guards this point can produce |
| Priority | int32 | Lower = higher priority |
| FactionOverride | GameplayTag | Override territory's faction for this point |

### Struct: FTerritoryPatrolNode

| Field | Type | Notes |
|---|---|---|
| Location | FVector | World position |
| WaitTime | float | Seconds to wait at node |
| Rotation | FRotator | Face direction |

### Functions

| Function | Returns | Description |
|---|---|---|
| GetLoopPatrol | bool | Whether patrol loops |
| GetNextPatrolNode | FTerritoryPatrolNode | Next waypoint |
| GetRemainingReserves | int32 | Guards still available |
| ConsumeReserve | bool | Take one reserve slot |
| ResetReserves | void | Restore all slots |

### Death Handling

When a guard dies:
1. `ATerritoryVolume::OnDefenderDied` is called
2. Checks `bWasTracked` — only processes if guard was registered from this spawn point
3. Calls `SpawnPoint->ConsumeReserve()`
4. If reserves depleted → `Territory->SpawnGuards()` to produce replacement

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

Legacy single-player save adapter. Place exactly 1 per level for single-player.

### Functions

| Function | Description |
|---|---|
| SaveState | Save all subsystem state to SaveGame properties |
| LoadState | Restore all subsystem state from SaveGame properties |

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
| HasAttackBudget(Territory, Faction) | bool |
| GetActiveAttackers(Territory, Faction) | int32 |

### Delegates

| Delegate | Signature |
|---|---|
| OnCaptureStarted | (ATerritoryVolume*, FGameplayTag) |
| OnCaptureProgressUpdated | (ATerritoryVolume*, FGameplayTag, float) |
| OnCaptureCompleted | (ATerritoryVolume*, FGameplayTag, FGameplayTag) |

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
| RequestAttackPermission | Territory, Faction, Actor | bool |
| ReleaseAttackPermission | Territory, Faction, Actor | void |
| ReleaseAllPermissions | Territory, Faction | void |

### Queries (BlueprintPure)

| Function | Returns |
|---|---|
| GetActiveAttackers(Territory, Faction) | int32 |
| GetAttackBudget(Territory, Faction) | int32 |
| HasAttackBudget(Territory, Faction) | bool |

### Delegates

| Delegate | Signature |
|---|---|
| OnAttackPermissionGranted | (ATerritoryVolume*, Faction, Actor) |
| OnAttackPermissionDenied | (ATerritoryVolume*, Faction, Actor) |

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
| SetTerritoryByTag(Tag) | Bind to territory by GameplayTag |
| SetTerritoryByLocation(Location) | Bind to nearest territory |
| GetCurrentTerritory | Get bound ATerritoryVolume* |

### Blueprint Events

| Event | Parameters |
|---|---|
| OnUpdateOwnership | OldOwner, NewOwner |
| OnUpdateState | NewState |
| OnUpdateProgress | Progress (float) |

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
| OnEconomyTick | (Faction, FTerritoryEconomySnapshot) |
| OnTransaction | (FTerritoryTransaction) |

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

## BTTask_MoveToPatrolNode

Behavior tree task. Moves guard to next patrol node.

### Blackboard Keys Written

| Key | Type | Value |
|---|---|---|
| TargetLocation | FVector | Patrol node position |
| TargetRotation | FRotator | Patrol node facing |
| PatrolWaitTime | float | Wait time at node |

### Node Properties

| Property | Type | Default | Notes |
|---|---|---|---|
| AcceptanceRadius | float | 50.0 | Move completion distance |

---

## BTService_UpdatePatrolRoute

Behavior tree service. Advances patrol index when guard arrives at node.

### Node Properties

| Property | Type | Default | Notes |
|---|---|---|---|
| ArrivalThreshold | float | 100.0 | Distance to consider "arrived" |
| bLoopPatrol | bool | true | Loop back to first node |

### Blackboard Keys Read

| Key | Type | Notes |
|---|---|---|
| PatrolNodeIndex | int32 | Current node index |
| TargetLocation | FVector | Current move target |

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
- Completes when `OnTerritoryControlChanged` fires and new owner == `RequiredCapturingFaction`
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
| GetOwningFaction_Implementation | FGameplayTag | Current owner |
| GetTerritoryState_Implementation | ETerritoryState | Current state |
| GetControlProgress_Implementation | float | 0.0–1.0 |

---

## ITerritoryEconomyInterface

Implement on actors that participate in economy.

### Functions

| Function | Returns | Notes |
|---|---|---|
| GetPeriodicIncome_Implementation | int32 | Income contribution |
| GetGuardCost_Implementation | int32 | Upkeep cost |
| GetEffectiveIncome_Implementation | int32 | Including upgrades |

---

## ITerritoryEventReceiverInterface

BlueprintNativeEvent — implement to receive territory events.

### Functions

| Function | Notes |
|---|---|
| OnTerritoryCaptured_Implementation | Called when territory is captured |
| OnTerritoryContested_Implementation | Called when capture starts |
| OnTerritoryUpgraded_Implementation | Called when property upgrades |

---

## Enums

### ETerritoryState

| Value | Description |
|---|---|
| Unclaimed | No owner |
| Controlled | Owned and stable |
| Contested | Capture in progress |
| Locked | Cannot be captured |

### ECaptureResult

| Value | Description |
|---|---|
| Success | Capture initiated |
| AlreadyOwned | Attacker already owns |
| Locked | Territory is locked |
| DiplomaticallyBlocked | Factions are Friendly |
| DefendersRemain | Guards still alive |
| NoAttackBudget | Too many attackers |

### ETerritoryTransactionType

| Value | Description |
|---|---|
| Income | Economy tick income |
| GuardUpkeep | Economy tick cost |
| UpgradeCost | Property upgrade |
| Reward | External reward (quest, etc.) |
| Manual | Direct add/debit |
| Capture | Capture-related |

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
| WarDeclared | War started |
| PeaceDeclared | War ended |
| AllianceFormed | Alliance created |
| AllianceBroken | Alliance dissolved |
| TradeSigned | Trade agreement |
| TradeExpired | Trade timed out |
| TreatyExpired | Any treaty expired |
| AttitudeChanged | Narrative attitude shifted |

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
| Faction | FGameplayTag | Faction tag |
| Gold | int32 | Current treasury |
| Income | int32 | Per-tick income |
| Costs | int32 | Per-tick costs |
| TerritoryCount | int32 | Owned territories |

### FTerritoryTreasury

| Field | Type | Notes |
|---|---|---|
| Gold | int32 | Current balance |
| IncomePerTick | int32 | Calculated income |
| CostsPerTick | int32 | Calculated costs |
| TerritoryCount | int32 | Owned territories |

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
| Gold | int32 | Treasury |
| Income | int32 | Income |
| Costs | int32 | Costs |

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
| FOnTerritoryControlChanged | (ATerritoryVolume*, OldFaction, NewFaction) | ✅ |
| FOnTerritoryStateChanged | (ATerritoryVolume*, ETerritoryState) | ✅ |
| FOnGuardDied | (ATerritoryVolume*, FGameplayTag, FGameplayTag) | ✅ |
| FOnUpgradeLevelChanged | (ATerritoryProperty*, int32) | ✅ |

### Subsystem Delegates

| Delegate | Signature | Source |
|---|---|---|
| FOnTerritoryRegistered | (ATerritoryVolume*) | Registry |
| FOnTerritoryUnregistered | (ATerritoryVolume*) | Registry |
| FOnCaptureStarted | (ATerritoryVolume*, FGameplayTag) | Control |
| FOnCaptureProgressUpdated | (ATerritoryVolume*, FGameplayTag, float) | Control |
| FOnCaptureCompleted | (ATerritoryVolume*, FGameplayTag, FGameplayTag) | Control |
| FOnEconomyTickFired | (FGameplayTag, FTerritoryEconomySnapshot) | Economy |
| FOnTransactionRecorded | (FTerritoryTransaction) | Economy + WorldState |
| FOnDiplomacyStateChanged | (FGameplayTag, FGameplayTag, EDiplomacyState) | Diplomacy |
| FOnDiplomacyEvent | (const FDiplomacyEvent&) | Diplomacy |
| FOnReputationChanged | (FGameplayTag, int32) | Diplomacy |
| FOnAttackPermissionGranted | (ATerritoryVolume*, FGameplayTag, AActor*) | CombatDirector |
| FOnAttackPermissionDenied | (ATerritoryVolume*, FGameplayTag, AActor*) | CombatDirector |

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