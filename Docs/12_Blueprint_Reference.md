# Blueprint Reference — All Exposed Functions, Properties, Delegates

## Operations UI

### `UTerritoryUIBlueprintLibrary`

| Function | Returns | Notes |
|---|---|---|
| `OpenTerritoryMenu(PlayerController, WidgetClass, LayerTag)` | `UTerritoryActivatableWidget*` | Pushes into a registered Narrative HUD CommonUI layer |
| `BuildDistrictOperationsView(Context, District, Viewer, OutView)` | bool | Viewer-relative district/security/finance/threat projection |
| `GetDistrictOperationsViews(Context, Viewer, Filter)` | Array | Sorted district list for one operational filter |
| `GetPlayerVisibleDistrictOperationsViews(Context, Viewer, Filter)` | Array | Player list with locked/unloaded parent branches removed and City grouping order applied |
| `BuildHierarchyOperationsView(Context, Territory, Viewer, OutView)` | bool | One City, District, or Place read model |
| `GetDistrictHierarchyOperationsViews(Context, District, Viewer)` | Array | Visible City -> District -> Place branch for a selected District |
| `IsTerritoryVisibleToPlayer(Context, Territory)` | bool | True only when the Territory and every required loaded parent are registered and unlocked |
| `DoesDistrictMatchFilter(View, Filter)` | bool | Pure filter predicate |
| `DoesDistrictMatchSearch(View, SearchText)` | bool | Case-insensitive tokenized District/read-model search |
| `GetDistrictOperationsRevision(View)` | int32 | UI invalidation key covering displayed authorities |
| `BuildEconomyOperationsView(Context, Viewer, Faction, MaxRecent)` | struct | Narrative funds plus Territory economy projection |
| `BuildProductionSiteOperationsView(Context, TerritoryTag, OutView)` | bool | Server/client read model for one production Property |
| `GetThreatLevelText(Level)` | Text | Localizable threat label |
| `GetAssaultStateText(State)` | Text | Localizable assault-state label |
| `GetDiplomacyStateText(State)` | Text | Localizable Territory diplomacy label |

### UI enums

- `ETerritoryOperationsFilter`: `All`, `Unlocked`, `Available`, `Owned`, `Manageable`, `UnderAttack`, `Contested`, `Locked`, `FinancialRisk`, `Producing`, `ProductionBlocked`, `MissingInputs`, `StorageFull`.
- `ETerritoryThreatLevel`: `None`, `Watch`, `Warning`, `Critical`.

### Interactive widgets

| Class | Blueprint API |
|---|---|
| `UTerritoryActivatableWidget` | `CloseTerritoryWidget`, `GetTerritoryPlayerController`, `DesiredFocusTargetName` |
| `UTerritoryJournalWidget` | `RefreshDistrictList`, `SelectDistrict`, `SetOperationsFilter`, `GetSelectedDistrictOperationsView` |
| `UTerritoryDistrictManagementWidget` | `GetOperationsView`, `CanPurchaseGuard`, `CanRemoveGuard`, `RequestAddGuards`, `RequestRemoveGuards` |
| `UTerritoryEconomyWidget` | `GetNetIncome`, `IsOperatingAtDeficit`, `GetEconomyOperationsView` |

Currency is read from the owning pawn's Narrative inventory/account. Guard mutations use the player-owned server RPC bridge. A client widget never owns or trusts owner, faction, price, balance, reserve, capture, or assault state.

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
| GetResolvedInitialState() | ETerritoryState | Territory |
| GetSpawnedGuardCount() | int32 | Territory\|Guards |
| HasGuardsAlive() | bool | Territory\|Guards |
| GetGuardSpawnPoints() | Array<GuardSpawnPoint*> | Territory\|Guards |
| GetRegisteredDefenders() | Array<Actor*> | Territory |
| GetMapMarkerComponent() | TerritoryNavigationMarkerComponent* | Territory\|Visual |

`GetOwningFaction()` returns the incumbent defending faction while a claimed territory is contested. `IsOwnedByFaction()` returns true only in the `Claimed` state, so it is false while Contested, Locked, or Unclaimed.

### BlueprintCallable (AuthorityOnly) Functions

| Function | Category |
|---|---|
| SetOwningFaction(NewFaction) | Territory — validated wrapper through ControlSubsystem; use Apply Territory Mutation for a result/context |
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
| InitialState | ETerritoryInitialState |
| InitialMaxConcurrentAttackers | int32 |
| InitialPeriodicIncome | int32 |
| InitialGuardCost | int32 |
| ParentTerritoryTag | GameplayTag |
| TerritoryGUID | FGuid |
| BoundsShape | ShapeComponent* |
| GuardNPCDefinition | NPCDefinition* |
| FactionGuardDefinitions | Array<FTerritoryFactionGuardDefinition> |
| GuardSpawnCount | int32 |
| GuardSpawnRadius | float (deprecated/ignored) |
| GuardSpawnPoints | Array<TerritoryGuardSpawnPoint*> |

## ATerritoryGuardCharacter

### BlueprintPure Functions

| Function | Returns | Category |
|---|---|---|
| GetTerritoryPatrolRoute() | Array<TerritoryPatrolNode> | Territory\|Guard\|Patrol |
| HasTerritoryPatrolRoute() | bool | Territory\|Guard\|Patrol |
| GetPatrolNodeCount() | int32 | Territory\|Guard\|Patrol |
| GetSafePatrolNode(Index, OutNode) | bool | Territory\|Guard\|Patrol |
| GetSpawnTransform() | Transform | Territory\|Guard |
| GetOwningTerritory() | TerritoryVolume* | Territory\|Guard |
| GetGuardFaction() | GameplayTag | Territory\|Guard |
| IsSpawnPointGuard() | bool | Territory\|Guard |

### BlueprintCallable Functions

| Function | Notes |
|---|---|
| ConfigureTerritorySpawnWithContext(...) | Authority-only external deferred-spawn entrypoint. Supply the exact Territory and guard spawn point and branch on the Boolean result. |
| ConfigureTerritorySpawn(...) | Deprecated migration node. It resolves typed context from stable identity or fails closed; core garrisons already spawn through Narrative's subsystem. |

### BlueprintReadOnly Replicated Properties

| Property | Type |
|---|---|
| TerritoryHomeTransform | Transform |
| OwningTerritory | TerritoryVolume* |
| OwningTerritorySpawnPoint | TerritoryGuardSpawnPoint* |

## ATerritoryGuardSpawnPoint

### BlueprintPure Functions

| Function | Returns |
|---|---|
| HasAvailableSlot() | bool |
| HasReserveAvailable() | bool |
| GetActiveGuardCount() | int32 |
| GetReserveCount() | int32 |
| HasPendingReserveSpawn() | bool |
| GetPendingReserveCount() | int32 |
| GetSpawnTransform() | Transform |
| ResolveGuardDeploymentTransform(GuardClass) | Transform (out) |
| GetPatrolRoute() | Array<TerritoryPatrolNode> |
| HasPatrolRoute() | bool (requires at least two nodes) |
| GetLoopPatrol() | bool |
| GetPatrolRouteAsTransforms() | Array<Transform> |
| GetPatrolWaitTimes() | Array<float> |
| GetOwningTerritory() | TerritoryVolume* |
| **Effective Config** (resolves inline vs GuardPostDefinition precedence) | |
| GetEffectiveMaxGuards() | int32 |
| GetEffectiveReserveSlots() | int32 |
| GetEffectiveReserveSpawnDelay() | float |
| GetEffectiveReserveRetryInterval() | float |
| GetEffectiveReserveRadius() | float |
| GetEffectiveMinimumPlayerDistance() | float |
| GetEffectiveCandidateCount() | int32 |
| GetEffectiveFactionOverride() | GameplayTag |
| GetEffectivePatrolRoute() | Array<TerritoryPatrolNode> |
| GetEffectiveLoopPatrol() | bool |

### BlueprintCallable Functions

| Function | Notes |
|---|---|
| RegisterSpawnedGuard(Guard) | Internal spawn bookkeeping; normally called by TerritoryVolume |
| UnregisterGuard(Guard) | Frees the slot and may consume one reserve replacement |

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
| MaxUpgradeLevel | int32 (default 3, no SaveGame, no Replicated) |
| UpgradeCostPerLevel | int32 |
| IncomeBonusPerLevel | int32 |
| ProductionProfile | UTerritoryProductionProfile* (optional; captured Properties may produce no resources) |

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
| GetFactionGold(WorldContext, Faction) | int32 | Deprecated; no faction wallet |
| GetFactionIncome(WorldContext, Faction) | int32 |
| GetAllFactions(WorldContext) | Array<GameplayTag> |

### Capture Shortcuts

| Function | Returns |
|---|---|
| GetTerritoryState(WorldContext, Tag) | ETerritoryState |
| GetCaptureProgress(WorldContext, Tag) | float |
| ForceCaptureTerritory(WorldContext, Tag, Faction) | AuthorityOnly → void |

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
| PrintTerritoryDebug(WorldContext, Territory, Duration) | void |
| PrintAllTerritoryDebug(WorldContext, Duration) | void |

## Subsystem API Summary

### UTerritoryControlSubsystem

| Function | Type |
|---|---|
| AttemptCapture(Territory, Faction) | AuthorityOnly → ECaptureResult |
| ForceCapture(Territory, Faction) | AuthorityOnly → bool; resolves a matching live Narrative player context |
| ForceCaptureWithContext(Territory, Faction, Context) | AuthorityOnly → bool; preferred exact-instigator path |
| ResetCapture(Territory) | AuthorityOnly |
| AddCaptureProgress(Territory, Faction, Delta) | AuthorityOnly |
| RegisterAttacker(Territory, Actor, Faction) | AuthorityOnly |
| UnregisterAttacker(Territory, Actor, Faction) | AuthorityOnly |
| IsCaptureInProgress(Territory) | Pure → bool |
| GetCaptureProgress(Territory) | Pure → float |
| GetContestingFaction(Territory) | Pure → GameplayTag |
| HasAttackBudget(Territory, Faction) | Pure → bool |
| GetActiveAttackers(Territory, Faction) | Pure → int32 |

`TryRegisterAttacker(Territory, Actor, Faction)` is the result-bearing registration API. The legacy `RegisterAttacker` wrapper remains for Blueprint compatibility but callers that must know whether capture pressure was admitted should use `TryRegisterAttacker`.

### UTerritoryEconomySubsystem

| Function | Type |
|---|---|
| CreditCurrency(Beneficiary, Amount, Faction, Reason, Type) | AuthorityOnly → bool |
| TryDebitCurrency(Requester, Amount, Faction, Reason, Type) | AuthorityOnly → bool |
| CreditCurrencyToFaction(Faction, Amount, Policy, Reason, Type) | AuthorityOnly → int32 |
| RegisterFactionCurrencyAccount(Faction, Role, AccountActor) | AuthorityOnly → bool |
| UnregisterFactionCurrencyAccount(Faction, AccountActor) | AuthorityOnly |
| RegisterFactionResourceAccount(Faction, AccountActor) | AuthorityOnly → bool |
| UnregisterFactionResourceAccount(Faction, AccountActor) | AuthorityOnly |
| ProcessResourceProduction() | AuthorityOnly |
| ExecuteResourceRecipe(Requester, Faction, Rule, UpgradeLevel, BatchCount, SourceTerritory, OutResult) | AuthorityOnly → bool |
| SetFactionTreasury(Faction, Treasury) | AuthorityOnly |
| RecalculateIncome(Faction) | AuthorityOnly |
| GetActorCurrency(Requester) | Pure → int32 |
| GetIncome(Faction) | Pure → int32 |
| GetCosts(Faction) | Pure → int32 |
| CanActorAfford(Requester, Cost) | Pure → bool |
| GetFactionEconomy(Faction) | Pure → TerritoryTreasury |
| GetAllFactionsWithTreasury() | Pure → Array<GameplayTag> |
| GetTransactionHistory(Faction, Max) | → Array<Transaction> |
| GetFactionResourceSnapshot(Faction) | Pure → ResourceSnapshot |
| GetProductionSitesForFaction(Faction) | Pure → Array<ProductionSiteRecord> |
| GetProductionSite(TerritoryTag) | Pure → ProductionSiteRecord |

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

### UTerritoryCounterAttackSubsystem

| Function / delegate | Type |
|---|---|
| ScheduleCounterAttack(Territory, AttackingFaction) | AuthorityOnly → bool |
| ScheduleBestCounterAttack(Territory, PreferredFaction) | AuthorityOnly → bool; diplomacy-first strongest configured eligible faction |
| CancelAssault(AssaultID, Reason) | AuthorityOnly → bool |
| GetAssault(AssaultID, OutAssault) | Pure → bool |
| GetAllAssaults() | Pure → Array<AssaultRecord> |
| GetAssaultsForTerritory(TerritoryTag) | Pure → Array<AssaultRecord> |
| GetAssaultsForTerritoryActor(Territory) | Pure → Array<AssaultRecord>; exact stable GUID for a loaded actor |
| IsAssaultActive(AssaultID) | Pure → bool |
| IsAssaultPendingOrActive(AssaultID) | Pure → bool |
| GetAssaultDebugString(AssaultID) | Pure → String |
| GetBestEligibleAttackerPreview(Territory, PreferredFaction, ...) | Pure → bool + attacker/input/result/reason; no schedule or roll |
| OnAssaultChanged | BlueprintAssignable |
| OnAssaultWarning | BlueprintAssignable |
| OnCounterHappened | BlueprintAssignable; post-commit state transition payload (`Assault`, `PreviousState`, `NewState`, `Resolution`, `EventGameTime`) |

The warning state is notification-only: it creates no physical NPCs and no capture pressure. Physical activation occurs once, on the server, when a relevant player enters the configured radius.

### Context-aware territory transitions

Use `LockTerritoryWithContext`, `TryUnlockWithContext`, and `CanUnlockWithContext` whenever conditions or Tales hooks depend on the actual instigator. The context carries pawn, controller, Tales component, and requesting faction; world-level transitions may deliberately pass an empty context.

Counterattack setup queries on `ATerritoryVolume` are `GetCounterAttackProfile`,
`GetCounterAttackApproaches`, `GetGuardQuality`, `GetFortificationStrength`,
`GetNearbyAlliedSupport`, and `GetStrategicValue`. The profile's
`UnguardedLaunchProbability` defaults to `1.0` after diplomacy/admission gates.

### Narrative State Config extensions

These `EditInlineNew` Narrative classes can be added directly under a Territory's
`State Configs -> Entry/Exit Conditions/Events` arrays:

| Class | Important options | Example |
|---|---|---|
| `UTerritoryOwnershipCondition` | Territory tag, required owner, optional special states | Blacksmith must be owned by Heroes |
| `UTerritoryDiplomacyCondition` | Faction A, Faction B, required treaty state | Heroes and Bandits must be at War |
| `UTerritoryGarrisonCondition` | Territory tag, metric, comparison, number | Living Defenders Equal 0 |
| `UTerritoryStateCondition` | Territory tag, required state | District must be Contested |
| `UTerritoryControlProgressCondition` | Territory, comparison, percent, optional tolerance | Control Progress At Least 75% |
| `UTerritoryReputationCondition` | Faction, comparison, signed value | Regime Reputation Less Than -50 |
| `UTerritoryAssaultCondition` | Territory, query, state/result or number comparison | Remaining Attackers Equal 0 |
| `UTerritoryPresenceCondition` | Territory, include child Places | Narrative target is inside Castle Hill |
| `UTerritoryEventContextCondition` | Required target/player/ASC/controller/Tales context | Give XP only to the player pawn that caused capture |
| `UTerritoryProductionStatusCondition` | Property, optional rule, required status | Farm production is Missing Input |
| `UTerritoryResourceCondition` | Faction, Narrative item class, comparison, quantity | Heroes have At Least 10 Medicine |
| `UTerritorySetDiplomacyEvent` | Faction A/B, new state, optional trade duration | Set Heroes and Regime to War after betrayal |
| `UTerritoryModifyReputationEvent` | Faction, Add/Set, value | Add -20 to Bandit reputation |
| `UTerritoryScheduleEnemyWaveEvent` | Target, exact/best attacker | Schedule one finite Bandit assault |
| `UTerritoryCancelEnemyWavesEvent` | Target, optional attacker, include active | Cancel warnings after a peace treaty |
| `UTerritorySetGarrisonTargetEvent` | Target and exact desired guards | Assign two guards through the normal currency mutation |
| `UTerritoryUpgradePropertyEvent` | Property tag | Purchase exactly one normal upgrade |
| `UTerritoryExecuteResourceRecipeEvent` | Faction, source, atomic recipe, finite batches | Convert supplies into one relief shipment |
| `UTerritoryLockEvent` / `UTerritoryUnlockEvent` | Territory tag and explicit lock policy | Unlock a District after its gate quest |
| `UTerritoryCaptureEvent` | Territory, faction, explicit force policy | Award a story outpost through the atomic capture API |

`ETerritoryGarrisonMetric` options are Active Guards, Living Defenders, Desired Guards, Maximum
Guard Capacity, Remaining Reserve, Pending Reserve Deployments, and Guard Shortfall.
`ETerritoryIntegerComparison` provides Equal, Not Equal, At Least, At Most, Greater Than, and
Less Than. `ETerritoryFloatComparison` provides tolerant equality and ordered comparisons.

Every event exposes Narrative's inherited `Conditions` list. All of those conditions must pass,
and each condition's inherited **Not** option is applied. Example:

```text
On All Defenders Defeated Events
  Wave of Enemies
    Target Territory = Blacksmith
    Attacking Faction = Bandits
    Conditions
      Territory Diplomacy = Heroes and Bandits are War
```

`On Defender Died Events` runs after each registered defender death. `On All Defenders Defeated
Events` runs only when living defenders and pending finite replacements are both zero. Death is
the trigger; diplomacy, reputation, garrison, resource, and assault checks are snapshot conditions.
Unregistered or repeated callbacks for the same casualty are ignored.
Enemy-wave scheduling remains finite and uses the existing counterattack profile, Narrative NPC,
route, warning, proximity activation, casualty, save, and replication flow.

State Config entry/exit arrays react to enum-state transitions, not every owner field change.
Normal capture passes through Contested and re-enters Claimed. A direct Claimed-to-Claimed
owner mutation needs `OnTerritoryControlChanged` when an always-run story hook is required.

Do not place a player-only GAS reward in a State Config without an event-level context condition.
`Contested -> Claimed` can also mean that failed capture pressure decayed and the current owner
recovered, so the transition context is deliberately empty. For `NE_GiveXP`, add `Territory Event
Context Condition` and leave Target Pawn, Player Controlled Target, and Ability System Component
enabled. This avoids `Accessed None` and prevents XP from being awarded for world recovery.

### Player management notifications

The server-owned `UTerritoryPlayerManagementComponent` exposes `OnAssaultNotification` for the one-time strategic warning and `OnCounterHappened` for every relevant live state transition. The latter is delivered by reliable owning-client RPC and carries a complete value snapshot, so client UI must use the payload instead of querying server-only subsystem maps. `UTerritoryHUDWidget` also exposes a Blueprint event named `OnCounterHappened`; switch on `NewState` and call Narrative Pro's **Show Narrative HUD Notification** node for the states your project wants to present.

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
