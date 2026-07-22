# Blueprint Reference — All Exposed Functions, Properties, Delegates

## ATerritoryVolume

### BlueprintPure Functions

| Function | Returns | Category |
|---|---|---|
| GetOwningFaction() | GameplayTag | Territory |
| GetTerritoryState() | ETerritoryState | Territory |
| GetControlProgress() | float | Territory |
| IsContested() | bool | Territory |
| IsOwnedByFaction(Faction) | bool | Territory |
| GetTerritoryTag() | GameplayTag | Territory |
| GetTerritoryDisplayName() | Text | Territory |
| GetMaxConcurrentAttackers() | int32 | Territory |
| GetDefenderCount() | int32 | Territory |
| GetPeriodicIncome() | int32 | Territory |
| GetGuardCost() | int32 | Territory |
| GetTerritoryBounds() | Box | Territory |
| ContainsPoint(Point) | bool | Territory |
| GetParentTerritoryTag() | GameplayTag | Territory |
| GetInitialOwningFaction() | GameplayTag | Territory |
| GetSpawnedGuardCount() | int32 | Territory\|Guards |
| HasGuardsAlive() | bool | Territory\|Guards |
| GetGuardSpawnPoints() | Array<GuardSpawnPoint*> | Territory\|Guards |
| GetRegisteredDefenders() | Array<Actor*> | Territory |

### BlueprintCallable (AuthorityOnly) Functions

| Function | Category |
|---|---|
| SetOwningFaction(NewFaction) | Territory |
| SetControlProgress(Progress) | Territory |
| SetTerritoryState(NewState) | Territory |
| RegisterDefender(Defender) | Territory |
| UnregisterDefender(Defender) | Territory |
| SpawnGuards() | Territory\|Guards |
| DespawnGuards() | Territory\|Guards |

### BlueprintCallable Functions

| Function | Category |
|---|---|
| GetRegisteredDefenders() | Territory |

### BlueprintNativeEvent

| Event | Parameters |
|---|---|
| OnOwnershipChanged | OldOwner (GameplayTag), NewOwner (GameplayTag) |
| OnStateChanged | OldState (ETerritoryState), NewState (ETerritoryState) |

### BlueprintAssignable Delegates

| Delegate | Signature |
|---|---|
| OnTerritoryControlChanged | (Volume*, OldOwner, NewOwner) |
| OnTerritoryStateChanged | (Volume*, NewState) |
| OnGuardDied | (Volume*, Faction, EmptyTag) |

### BlueprintReadWrite Properties

| Property | Type |
|---|---|
| TerritoryTag | GameplayTag |
| TerritoryDisplayName | Text |
| InitialOwningFaction | GameplayTag |
| InitialMaxConcurrentAttackers | int32 |
| InitialPeriodicIncome | int32 |
| InitialGuardCost | int32 |
| bStartsLocked | bool |
| ParentTerritoryTag | GameplayTag |
| TerritoryGUID | FGuid |
| BoundsShape | ShapeComponent* |
| GuardNPCDefinition | NPCDefinition* |
| GuardBehaviorTree | BehaviorTree* |
| GuardBlackboardAsset | BlackboardData* |
| GuardSpawnCount | int32 |
| GuardSpawnRadius | float |
| GuardSpawnPoints | Array<Actor*> |

## ATerritoryProperty (extends ATerritoryVolume)

### BlueprintPure

| Function | Returns |
|---|---|
| CanUpgrade() | bool |
| GetUpgradeCost() | int32 |
| GetEffectiveIncome() | int32 |
| GetOwningDistrict() | TerritoryDistrict* |

### BlueprintCallable (AuthorityOnly)

| Function |
|---|
| TryUpgrade() → bool |
| SetUpgradeLevel(NewLevel) |

### BlueprintImplementableEvent

| Event |
|---|
| OnUpgradeLevelChanged(NewLevel) |

### BlueprintReadWrite

| Property | Type |
|---|---|
| UpgradeLevel | int32 (SaveGame, Replicated) |
| MaxUpgradeLevel | int32 |
| UpgradeCostPerLevel | int32 |
| IncomeBonusPerLevel | int32 |

## UTerritoryBlueprintLibrary (Static)

| Function | Returns |
|---|---|
| GetTerritoryRegistry(WorldContext) | RegistrySubsystem* |
| GetTerritoryControl(WorldContext) | ControlSubsystem* |
| GetTerritoryEconomy(WorldContext) | EconomySubsystem* |
| GetTerritoryCombatDirector(WorldContext) | CombatDirector* |
| GetTerritoryAtLocation(WorldContext, Location) | TerritoryVolume* |
| GetTerritoryByTag(WorldContext, Tag) | TerritoryVolume* |
| IsSameFaction(A, B) | bool |

## Subsystem API Summary

### UTerritoryControlSubsystem

| Function | Type |
|---|---|
| AttemptCapture(Territory, Faction) | AuthorityOnly → ECaptureResult |
| ForceCapture(Territory, Faction) | AuthorityOnly |
| ResetCapture(Territory) | AuthorityOnly |
| AddCaptureProgress(Territory, Faction, Delta) | AuthorityOnly |
| RegisterAttacker(Territory, Actor, Faction) | AuthorityOnly |
| UnregisterAttacker(Territory, Actor, Faction) | AuthorityOnly |
| IsCaptureInProgress(Territory) | Pure → bool |
| GetCaptureProgress(Territory) | Pure → float |
| GetContestingFaction(Territory) | Pure → GameplayTag |
| HasAttackBudget(Territory, Faction) | Pure → bool |
| GetActiveAttackers(Territory, Faction) | Pure → int32 |

### UTerritoryEconomySubsystem

| Function | Type |
|---|---|
| AddToTreasury(Faction, Amount, Reason, Type) | AuthorityOnly |
| TryDebitTreasury(Faction, Amount, Reason, Type) | AuthorityOnly → bool |
| SetFactionTreasury(Faction, Treasury) | AuthorityOnly |
| RecalculateIncome(Faction) | AuthorityOnly |
| GetTreasury(Faction) | Pure → int32 |
| GetIncome(Faction) | Pure → int32 |
| GetCosts(Faction) | Pure → int32 |
| CanAfford(Faction, Cost) | Pure → bool |
| GetFactionEconomy(Faction) | Pure → TerritoryTreasury |
| GetAllFactionsWithTreasury() | Pure → Array<GameplayTag> |
| GetTransactionHistory(Faction, Max) | → Array<Transaction> |

### UTerritoryDiplomacySubsystem

| Function | Type |
|---|---|
| DeclareWar(FactionA, FactionB) | AuthorityOnly |
| DeclarePeace(FactionA, FactionB) | AuthorityOnly |
| FormAlliance(FactionA, FactionB) | AuthorityOnly |
| BreakAlliance(FactionA, FactionB) | AuthorityOnly |
| SignTradeAgreement(A, B, Duration) | AuthorityOnly |
| SetDiplomacyState(A, B, State) | AuthorityOnly |
| AddReputation(Faction, Amount) | AuthorityOnly |
| SetReputation(Faction, Value) | AuthorityOnly |
| GetDiplomacyState(A, B) | Pure → EDiplomacyState |
| IsAtWar(A, B) | Pure → bool |
| IsAllied(A, B) | Pure → bool |
| HasTradeAgreement(A, B) | Pure → bool |
| GetReputation(Faction) | Pure → int32 |
| GetAllTreaties() | → Array<TreatyRecord> |
| GetTreatiesForFaction(Faction) | → Array<TreatyRecord> |

## UTerritoryBlueprintLibrary (Extended)

### New Helper Functions (Strengthening Pass)

| Function | Type | Returns |
|---|---|---|
| GetTerritoryDiplomacy(WorldContext) | Callable | DiplomacySubsystem* |
| GetAllTerritories(WorldContext) | Pure | Array<TerritoryVolume*> |
| GetTerritoriesByFaction(WorldContext, Faction) | Pure | Array<TerritoryVolume*> |
| GetChildTerritories(WorldContext, ParentTag) | Pure | Array<TerritoryVolume*> |
| GetTerritoryCount(WorldContext) | Pure | int32 |
| GetFactionTerritoryCount(WorldContext, Faction) | Pure | int32 |
| IsTerritoryAtLocation(WorldContext, Location) | Pure | bool |
| GetFactionGold(WorldContext, Faction) | Pure | int32 |
| GetFactionIncome(WorldContext, Faction) | Pure | int32 |
| GetAllFactions(WorldContext) | Pure | Array<GameplayTag> |
| GetTerritoryState(WorldContext, Tag) | Pure | ETerritoryState |
| GetCaptureProgress(WorldContext, Tag) | Pure | float |
| ForceCaptureTerritory(WorldContext, Tag, Faction) | Callable | void |
| GetTreatyState(WorldContext, A, B) | Pure | EDiplomacyState |
| IsAllied(WorldContext, A, B) | Pure | bool |
| IsAtWar(WorldContext, A, B) | Pure | bool |

## ITerritoryOwnershipInterface (Extended)

| Function | Type | Notes |
|---|---|---|
| GetTerritoryOwner | BlueprintNativeEvent | Returns owning faction |
| GetTerritoryControlProgress | BlueprintNativeEvent | 0.0–1.0 |
| IsTerritoryContested | BlueprintNativeEvent | bool |
| GetContestingFaction | BlueprintNativeEvent | New — who is attacking |

## ITerritoryEventReceiverInterface (Extended)

| Event | Parameters | Notes |
|---|---|---|
| OnTerritoryControlChanged | (Tag, OldOwner, NewOwner) | Ownership change |
| OnTerritoryContested | (Tag, ContestingFaction) | Capture started |
| OnTerritoryUncontested | (Tag) | New — capture ended |
| OnTerritoryStateChanged | (Tag, NewState) | New — any state transition |

## UTerritoryMapMarker (Extended)

| Function | Type | Notes |
|---|---|---|
| SetFactionColor(Faction, Color) | Callable | New — runtime color override |
| ClearFactionColors() | Callable | New — reset all colors |
