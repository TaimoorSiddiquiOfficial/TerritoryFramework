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
| GetMapMarkerComponent() | TerritoryNavigationMarkerComponent* | Territory\|Visual |

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

### BlueprintNativeEvent

| Event | Parameters |
|---|---|
| OnOwnershipChanged | OldOwner (GameplayTag), NewOwner (GameplayTag) |
| OnStateChanged | OldState (ETerritoryState), NewState (ETerritoryState) |
| OnAllGuardsDefeated | (none) |
| OnTerritoryInitialized | (none) |

### BlueprintAssignable Delegates

| Delegate | Signature |
|---|---|
| OnTerritoryOwnershipChanged | (Volume*, OldOwner, NewOwner) |
| OnTerritoryStateChangedDelegate | (Volume*, NewState) |
| OnGuardKilled | (Volume*, Guard, Killer, RemainingDefenders) |
| OnAllGuardsDefeatedDelegate | (Volume*) |

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
| bAutoCreateMapMarker | bool |
| TerritoryGUID | FGuid |
| BoundsShape | ShapeComponent* |
| GuardNPCDefinition | NPCDefinition* |
| FactionGuardDefinitions | Array<FTerritoryFactionGuardDefinition> |
| GuardSpawnCount | int32 |
| GuardSpawnRadius | float |
| GuardSpawnPoints | Array<Actor*> |

## ATerritoryCity (extends ATerritoryVolume)

### BlueprintPure

| Function | Returns |
|---|---|
| GetDistricts() | Array<Volume*> |
| GetDistrictCount() | int32 |
| AllDistrictsOwnedBy(Faction) | bool |
| GetCityControlPercentage(Faction) | float |
| GetMajorityOwner() | GameplayTag |
| IsFullyCaptured() | bool |
| GetCapturingFaction() | GameplayTag |
| GetCapitalDistrictCount() | int32 |
| HasCapitalDistrict() | bool |

### BlueprintNativeEvent

| Event | Parameters |
|---|---|
| OnCityFullyCaptured | CapturingFaction (GameplayTag) |
| OnCityLost | PreviousFaction (GameplayTag) |
| OnDistrictCapturedInCity | District (Volume*), OldOwner, NewOwner |

### BlueprintAssignable Delegates

| Delegate | Signature |
|---|---|
| OnCityCapturedDelegate | (City*, CapturingFaction) |
| OnCityLostDelegate | (City*, PreviousFaction) |

## ATerritoryDistrict (extends ATerritoryVolume)

### BlueprintPure

| Function | Returns |
|---|---|
| GetOwningCity() | City* |
| GetProperties() | Array<Volume*> |
| IsCapitalDistrict() | bool |
| GetPropertyCountForFaction(Faction) | int32 |
| AllPropertiesOwnedBy(Faction) | bool |

### BlueprintNativeEvent

| Event | Parameters |
|---|---|
| OnDistrictFullyCaptured | CapturingFaction (GameplayTag) |

### BlueprintAssignable Delegates

| Delegate | Signature |
|---|---|
| OnDistrictCapturedDelegate | (District*, OldOwner, NewOwner) |

### BlueprintReadWrite Properties

| Property | Type |
|---|---|
| bIsCapital | bool |
| CapitalIncomeMultiplier | float |

## ATerritoryProperty (extends ATerritoryVolume)

### BlueprintPure

| Function | Returns |
|---|---|
| CanUpgrade() | bool |
| GetUpgradeCost() | int32 |
| GetEffectiveIncome() | int32 |
| GetOwningDistrict() | District* |

### BlueprintCallable (AuthorityOnly)

| Function |
|---|
| TryUpgrade() → bool |
| SetUpgradeLevel(NewLevel) |

### BlueprintNativeEvent

| Event | Parameters |
|---|---|
| OnPropertyCaptured | NewOwner (GameplayTag) |

### BlueprintImplementableEvent

| Event |
|---|
| OnUpgradeLevelChanged(NewLevel) |

### BlueprintAssignable Delegates

| Delegate | Signature |
|---|---|
| OnPropertyCapturedDelegate | (Property*, NewOwner) |

### BlueprintReadWrite

| Property | Type |
|---|---|
| UpgradeLevel | int32 (SaveGame, Replicated) |
| MaxUpgradeLevel | int32 |
| UpgradeCostPerLevel | int32 |
| IncomeBonusPerLevel | int32 |

## UTerritoryBlueprintLibrary (Static)

### Subsystem Access

| Function | Returns |
|---|---|
| GetTerritoryRegistry(WorldContext) | RegistrySubsystem* |
| GetTerritoryControl(WorldContext) | ControlSubsystem* |
| GetTerritoryEconomy(WorldContext) | EconomySubsystem* |
| GetTerritoryCombatDirector(WorldContext) | CombatDirector* |
| GetTerritoryDiplomacy(WorldContext) | DiplomacySubsystem* |

### Territory Queries

| Function | Returns |
|---|---|
| GetTerritoryAtLocation(WorldContext, Location) | TerritoryVolume* |
| GetTerritoryByTag(WorldContext, Tag) | TerritoryVolume* |
| GetAllTerritories(WorldContext) | Array<Volume*> |
| GetTerritoriesByFaction(WorldContext, Faction) | Array<Volume*> |
| GetChildTerritories(WorldContext, ParentTag) | Array<Volume*> |
| GetTerritoryCount(WorldContext) | int32 |
| GetFactionTerritoryCount(WorldContext, Faction) | int32 |
| IsTerritoryAtLocation(WorldContext, Location) | bool |

### Economy Shortcuts

| Function | Returns |
|---|---|
| GetFactionGold(WorldContext, Faction) | int32 |
| GetFactionIncome(WorldContext, Faction) | int32 |
| GetAllFactions(WorldContext) | Array<GameplayTag> |

### Capture Shortcuts

| Function | Returns |
|---|---|
| GetTerritoryState(WorldContext, Tag) | ETerritoryState |
| GetCaptureProgress(WorldContext, Tag) | float |
| ForceCaptureTerritory(WorldContext, Tag, Faction) | void |

### Diplomacy Shortcuts

| Function | Returns |
|---|---|
| GetTreatyState(WorldContext, A, B) | EDiplomacyState |
| IsAllied(WorldContext, A, B) | bool |
| IsAtWar(WorldContext, A, B) | bool |

### Narrative Pro Faction Bridge

| Function | Returns |
|---|---|
| GetActorFactions(WorldContext, Actor) | GameplayTagContainer |
| IsActorInFaction(WorldContext, Actor, Faction) | bool |
| GetActorPrimaryFaction(WorldContext, Actor) | GameplayTag |
| AreActorsAllied(A, B) | bool |

### City / District Queries

| Function | Returns |
|---|---|
| GetAllCities(WorldContext) | Array<City*> |
| GetAllDistricts(WorldContext) | Array<District*> |
| GetCityForDistrict(WorldContext, District) | City* |
| DoesFactionControlCity(WorldContext, City, Faction) | bool |
| GetFactionCityCount(WorldContext, Faction) | int32 |
| GetFactionDistrictCount(WorldContext, Faction) | int32 |
| GetCapitalDistricts(WorldContext) | Array<District*> |

### Utility

| Function | Returns |
|---|---|
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

## ITerritoryOwnershipInterface (Extended)

| Function | Type | Notes |
|---|---|---|
| GetTerritoryOwner | BlueprintNativeEvent | Returns owning faction |
| GetTerritoryControlProgress | BlueprintNativeEvent | 0.0–1.0 |
| IsTerritoryContested | BlueprintNativeEvent | bool |
| GetContestingFaction | BlueprintNativeEvent | Who is attacking |

## ITerritoryEventReceiverInterface (Extended)

| Event | Parameters | Notes |
|---|---|---|
| OnTerritoryControlChanged | (Tag, OldOwner, NewOwner) | Ownership change |
| OnTerritoryContested | (Tag, ContestingFaction) | Capture started |
| OnTerritoryUncontested | (Tag) | Capture ended |
| OnTerritoryStateChanged | (Tag, NewState) | Any state transition |

## UTerritoryMapMarker (Extended)

| Function | Type | Notes |
|---|---|---|
| SetFactionColor(Faction, Color) | Callable | Runtime color override |
| ClearFactionColors() | Callable | Reset all colors |