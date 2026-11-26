# 43. Territory Place — complete reference (config, integration, in-game)

The Place is Territory's **independently capturable unit**. Everything about a Place is authored in a
DataAsset; the Blueprint actor is only the body that carries it. This document is the full picture, read
from source (not from the older docs, which cover the actor only).

## 1. The three layers

| Layer | Asset / class | Role |
|---|---|---|
| Definition (authoring) | **`UTerritoryPlaceDefinition : UTerritoryDefinition : UPrimaryDataAsset`** (`TerritoryDefinition.h:812`) | every gameplay value: identity, economy, capture, guards, floors, counter-attack, benefits, production, story |
| Actor (runtime) | **`ATerritoryProperty : ATerritoryVolume`** (`TerritoryHierarchy.h:255`) | the placed volume in the level; holds owner/state/progress, upgrade level, production |
| Blueprint | **`/Game/TerritoryFramework/Blueprints/BP_Property_Blacksmith`**, **`BP_Property_Farm`** | the concrete placeable classes (`BP_TerritoryVolume` is the generic base) |

Matching definitions: `Definitions/DA_Place_Blacksmith`, `DA_Place_Farm`.
**Duplicates exist** under both `Plugins/TerritoryFramework/Content/...` and `Content/TerritoryFramework/...` — pick one copy per asset and delete the other.

Hierarchy: **City Definition → District Definition (`Places[]`) → Place Definition**. A Place is
`ETerritoryControlMode::Independent` (fixed by class); a City/District is aggregate-only.

## 2. The rule that governs everything

```text
ATerritoryVolume::BeginPlay
  -> ApplyTerritoryDefinition()            (TerritoryVolume.cpp:753)
  -> TerritoryDefinition->ApplyToTerritory(this)
  -> if it fails: "%s has no valid Territory Definition.
     Runtime activation is disabled; actor/Blueprint overrides are no longer an authoring source."
  -> UTerritoryRegistrySubsystem::RegisterTerritory(this)   (only if the definition applied)
```

**A Place with no valid definition does nothing in game** — it is not registered, not capturable, not
economically active. The actor's own defaults are not an authoring source any more.

## 3. Complete config — `UTerritoryPlaceDefinition`

All inherited fields below are EditAnywhere, BlueprintReadOnly, and grouped by the asset editor categories.

### 01 Identity (base)
| Field | Default | Meaning |
|---|---|---|
| `TerritoryTag` | — | stable `Territory.*` GameplayTag used by lookups, Narrative conditions, hierarchy |
| `DisplayName` | — | readable name only; tags/IDs still identify |
| `StableTerritoryGUID` | auto | **persistent identity** shared with the placed actor and saved records |
| `RelativeTransform` | identity | placement relative to the parent (City: level anchor) |
| `TerritoryActorClass` | — | `TSoftClassPtr<ATerritoryVolume>` — the BP used for bounds/presentation only |

### 02 Hierarchy
| `DerivedParentTerritoryTag` | auto | filled from City→District→Place; never typed by hand |

### 03 New Campaign
| Field | Default | Meaning |
|---|---|---|
| `InitialOwningFaction` | — | `Narrative.Factions` tag for a new campaign; saved ownership wins |
| `InitialAvailability` | Unlocked | `Locked` keeps the Territory silent until its Locked exit conditions pass. **New campaign only** — an existing save keeps its own value |
| `InitialState` | Automatic | derives from the initial owner |
| `ControlMode` | Independent | fixed by class (Place = Independent, City/District = Aggregate) |
| `MaxConcurrentAttackers` | 3 | strategic attacker slots (Narrative's per-target tokens stay separate) |

### 04 Economy
| Field | Default | Meaning |
|---|---|---|
| `PeriodicIncome` | 100 | base currency per economy cycle (before state/faction/upgrade/hierarchy modifiers) |
| `GuardUpkeepPerCycle` | 50 | currency per guard per cycle |
| `GuardRecruitmentCost` | 50 | base currency to recruit one guard |
| `bAttitudeAffectsPrices` | false | let the buyer's standing with the owner change recruitment prices |
| `FriendlyPriceMultiplier` | 0.85 | Allied / Trade Agreement |
| `NeutralPriceMultiplier` | 1.0 | Neutral / Non-Aggression / Ceasefire |
| `WarPriceMultiplier` | 1.5 | at War |

### 05 State Rules
| Field | Meaning |
|---|---|
| `StateConfigs` | `TMap<ETerritoryState, FTerritoryStateConfig>` — exactly four rows: Locked, Unclaimed, Contested, Claimed. Contested Entry Events run once per real entry, not per capture tick |
| `QuestRuntimeOverrides` | Quests that temporarily own the rules; normal rules resume after the quest |
| `DefaultStealthProfile` | pre-conflict stealth policy (empty = entering immediately starts Contested) |

### 06 Capture
| Field | Default | Meaning |
|---|---|---|
| `bStoryCaptureFromBounds` | false | story mode: the whole multi-floor volume contests. **Automatically turns off Capture Point progress** |
| `CapturePoint` | — | `FTerritoryCapturePointTemplate` — physical point/flag for domination; mutually exclusive with story bounds |

### 07 Guards
| Field | Default | Meaning |
|---|---|---|
| `DefaultGuardDefinition` | — | fallback `UNPCDefinition` when no faction/post override matches |
| `FactionGuardDefinitions` | — | per-owning-faction NPC definitions, so one Place supports different occupiers |
| `InitialGuardCount` | 3 | desired new-campaign garrison, bounded by available posts |
| `PostCaptureGarrisonPolicy` | PlayerChooses | how the new owner's garrison is filled after a verified capture |
| `GuardBehavior` | — | shared patrol / crowd avoidance / target priority |
| `GuardPosts` | — | `FTerritoryGuardPostTemplate` rows with stable `GuardPostID`, reserve settings, per-post patrol |
| `DefenderDiedEvents` | — | Narrative events on each registered defender death |
| `AllDefendersDefeatedEvents` | — | Narrative events when the last relevant defender falls |

### 07 Guards | Floor (Place only)
| Field | Meaning |
|---|---|
| `Floors` | `FTerritoryFloorTemplate` rows — authored floors for staging defenders and per-floor story beats. **Empty disables floor grouping.** Each post's `FloorIndex` must name a row |
| `DefaultFloorCombatPolicy` | separation rule for every floor that names none of its own; empty = floors restrict nothing |

Floors exist **only** on the Place: a City/District registers no defenders of its own.

### 08 Counter Attack
| Field | Default | Meaning |
|---|---|---|
| `CounterAttackProfile` | — | scheduling, finite force, Narrative attackers |
| `CounterAttackApproaches` | — | typed entry routes (vehicle entries need a valid road route) |
| `GuardQuality` | 1.0 | relative quality multiplier for the strategic defence estimate |
| `FortificationStrength` | 0 | authored defence strength |
| `NearbyAlliedSupport` | 0 | authored allied contribution |
| `StrategicValue` | 1.0 | target importance. Aggregated by **max**, not sum — so a District is as valuable as its best Place |

### 09 Presentation / Management
| Field | Default | Meaning |
|---|---|---|
| `bShowGameplayHUD` | true | the compact passive Territory card while inside this exact volume. Notifications, POIs, markers, menus stay available. Off for broad City/District volumes |
| `ManagementPoint` | — | optional Narrative interaction/UI helper for managing the Place or its District |

### 10 Place | Benefits / Production / Upgrades (Place only)
| Field | Default | Meaning |
|---|---|---|
| `PropertyRoleTag` | — | semantic role, e.g. `Territory.Property.Role.ArmsShop` |
| `GameplayBenefits` | — | `FTerritoryPropertyGameplayBenefit[]` — abilities, persistent effects, benefit tags, weapon catalog entries unlocked by owning/upgrading |
| `ProductionProfile` | — | `UTerritoryProductionProfile` — input/output recipes run by the economy + Narrative inventory. Null is a valid non-producing Place |
| `MaxUpgradeLevel` | 3 | **0 disables upgrades** |
| `UpgradeCostPerLevel` | 500 | base currency cost per upgrade purchase |
| `IncomeBonusPerLevel` | 25 | additional income per purchased level |

### 11 Place | Story
| Field | Meaning |
|---|---|
| `StoryOwner` | `FTerritoryStoryOwnerTemplate` — optional protected Narrative NPC/dialogue for explicit story handover |

### Definition API
`ApplyToTerritory(Territory)` (BlueprintCallable) · `GetGuardPostTemplate` / `FindGuardPost` ·
`GetFloorTemplate` / `FindFloor` / `GetFloorGuardPostCount` / `HasAuthoredFloors` ·
`RefreshHierarchyLinks` (**CallInEditor**) · `IsDefinitionCompatible` · `GetEffectiveFloorCombatPolicy` ·
`GetPrimaryAssetId`.

## 4. Complete config — the runtime actor (`ATerritoryProperty`)

| Member | Type | Notes |
|---|---|---|
| `UpgradeLevel` | int32 | **`SaveGame`** + `ReplicatedUsing=OnRep_UpgradeLevel`; `DOREPLIFETIME` |
| `MaxUpgradeLevel` | int32 | **Transient** — copied from the definition by `ApplyToTerritory` |
| `UpgradeCostPerLevel` | int32 | Transient, same |
| `IncomeBonusPerLevel` | int32 | Transient, same |
| `ProductionProfile` | `UTerritoryProductionProfile*` | Transient, same |

Blueprint API: `GetUpgradeLevel` · `GetProductionProfile` · `GetOwningDistrict` (exact authored District,
null rather than guessing from tag prefixes) · `CanUpgrade` · `GetUpgradeCost` · `GetEffectiveIncome` ·
**`TryUpgrade(Requester)`** (`BlueprintAuthorityOnly`).

Hooks: `OnPropertyCapturedDelegate` · `OnPropertyCaptured(NewOwner)` (BlueprintNativeEvent) ·
`OnUpgradeLevelChanged(NewLevel)` (BlueprintImplementableEvent) · overrides
`OnOwnershipChanged_Implementation` and `ReconcileOwnershipDependentSystems`.

The base `ATerritoryVolume` supplies the rest — its own config categories are
`Guards (32 fields) · Lock (12) · Ownership (10) · Counter Attack (6) · Defenders (3) · State Rules,
Economy, Hierarchy, Bounds, Command, Definition (2 each) · Stealth, Capture|Story, Visual, UI, Audio,
Save, Debug, Tales|Quest Override (1 each)`, with replicated/saved runtime state
`OwnershipData`, `GarrisonSnapshot`, `TerritoryGUID`.

## 5. The bridge — `ApplyToTerritory`, exactly what it copies

```text
guard: !Territory || !IsDefinitionCompatible(Territory)  -> false
guard: !TerritoryTag.IsValid()                           -> false   (a Place must have a tag)
TerritoryDefinition, TerritoryTag, TerritoryDisplayName, InitialOwningFaction,
InitialAvailability, InitialState, ParentTerritoryTag
StrategicValue                     (always — even for aggregates)
bPhysicalPlace gate: ControlMode, InitialMaxConcurrentAttackers, InitialPeriodicIncome,
  InitialGuardCost, InitialGuardRecruitmentCost, bStoryCaptureFromBounds, GuardNPCDefinition,
  FactionGuardDefinitions, GuardSpawnCount, PostCaptureGarrisonPolicy, CounterAttackProfile,
  CounterAttackApproaches, GuardQuality, FortificationStrength, NearbyAlliedSupport
TerritoryGUID = StableTerritoryGUID (when valid)
Place block:  ProductionProfile, MaxUpgradeLevel, UpgradeCostPerLevel, IncomeBonusPerLevel
District block: bIsCapital, CapitalIncomeMultiplier
editor: EnsureCounterAttackApproachIDs()
always: RebuildRuntimeNarrativeConfiguration(*this)
```

The `bPhysicalPlace` gate is deliberate: a City/District is an aggregate with no guards, so economic and
defence-strength fields are zeroed — but its `StrategicValue` survives, because a District is a legitimate
target in its own right.

## 6. Integration map

| System | Connection |
|---|---|
| Hierarchy | `GetOwningDistrict()`, `DerivedParentTerritoryTag`, `RefreshHierarchyLinks()`; the City→District→Place reducer owns derived state |
| Registry | `UTerritoryRegistrySubsystem::RegisterTerritory` during `BeginPlay`; spatial lookup by tag |
| Capture | `UTerritoryControlSubsystem` owns validation/progress; `CapturePoint` **or** `bStoryCaptureFromBounds`, never both |
| Economy | `PeriodicIncome`, `GuardUpkeepPerCycle`, `GuardRecruitmentCost`; `GetEffectiveIncome()` = `TerritoryCurrencyRates::Income(PeriodicIncome, IncomeBonusPerLevel, UpgradeLevel, capitalMultiplier, stateRules.bAllowPeriodicIncome)`; `TryUpgrade` debits through `UTerritoryEconomySubsystem::DebitCurrencyWithResult` (Narrative inventory is the money authority) |
| Guards | `DefaultGuardDefinition` / `FactionGuardDefinitions` / `InitialGuardCount` / `GuardPosts` / `Floors` / `GuardBehavior` / `PostCaptureGarrisonPolicy`; reserve and death handling by the guard systems |
| Counter-attack | `StrategicValue`, `GuardQuality`, `FortificationStrength`, `NearbyAlliedSupport`, `CounterAttackProfile`, `CounterAttackApproaches` — planning data only; ownership still changes only through the capture flow |
| Narrative | `RebuildRuntimeNarrativeConfiguration`, `DefenderDiedEvents`, `AllDefendersDefeatedEvents`, `PropertyRoleTag`, `GameplayBenefits`, `StoryOwner` |
| Save | `UpgradeLevel` (SaveGame) + `TerritoryGUID` = `StableTerritoryGUID`; owner/state/progress in Territory World State |
| UI | `bShowGameplayHUD` (passive card), `ManagementPoint` (management/journal) |
| Audio | base `ATerritoryVolume` Audio fields + `Music.Territory.State.*` tags |

## 7. In-game lifecycle

```text
level load -> ATerritoryVolume::BeginPlay
  -> ApplyTerritoryDefinition (invalid => silent, unregistered, dead Place)
  -> Registry->RegisterTerritory
  -> guards spawn on GuardPosts (floors group them, policies may separate them)
  -> player enters bounds: passive HUD card (if bShowGameplayHUD), POI/markers already registered
  -> contest: capture point progress OR story bounds; Contested entry events fire once
  -> verified capture -> CommitOwnership -> OnPropertyCaptured(NewOwner) -> garrison policy fills posts
  -> management point -> TryUpgrade(Requester)
        server-only, Claimed only, requester must be in the owning faction,
        cost checked with CanActorAfford, level staged silently, then
        DebitCurrencyWithResult (rollback if the debit is rejected)
  -> economy cycle: GetEffectiveIncome (capital multiplier + state rules)
  -> counter-attack planner reads StrategicValue / GuardQuality / Fortification / AlliedSupport
  -> save/load: UpgradeLevel + stable GUID + World State records
```

## 8. Authoring checklist

1. Duplicate an existing **Place Definition** (`DA_Place_Blacksmith`) rather than an empty asset.
2. Set `TerritoryTag`, `DisplayName`, keep/regenerate `StableTerritoryGUID`.
3. Economy: `PeriodicIncome`, `GuardUpkeepPerCycle`, `GuardRecruitmentCost`, attitude multipliers.
4. Capture: choose **one** — `CapturePoint` (domination) or `bStoryCaptureFromBounds` (story).
5. Guards: `DefaultGuardDefinition`/`FactionGuardDefinitions`, `InitialGuardCount`, `GuardPosts` with
   stable `GuardPostID`s, `Floors` (if multi-storey) and a `DefaultFloorCombatPolicy` when floors separate.
6. Benefits/Production/Upgrades: `PropertyRoleTag`, `GameplayBenefits`, `ProductionProfile`,
   `MaxUpgradeLevel` (>0 or upgrades are disabled), `UpgradeCostPerLevel`, `IncomeBonusPerLevel`.
7. Add the definition to its **District Definition's `Places[]`**, then **Refresh Hierarchy Links**.
8. Place a `BP_Property_*` actor, assign the Definition on the instance, size the bounds.
9. Validate the asset (data validation runs the floor/post/tag checks).

## 9. Pitfalls (all verified)

- **No definition = no Place.** `BeginPlay` refuses activation and the actor is never registered.
- **`InitialAvailability` / `InitialOwningFaction` only matter on a new campaign.** An existing save keeps its
  saved values, so editing them looks like "nothing happened" while testing on an old save.
- **`MaxUpgradeLevel = 0` disables upgrades** — the Place still captures and produces.
- **`bStoryCaptureFromBounds` turns off physical capture progress**; never author both.
- **Floors are Place-only.** Each guard post's `FloorIndex` must name a row in `Floors`.
- **`UpgradeLevel` is saved and replicated** — never author it in the BP; it belongs to gameplay.
- **Aggregates zero their economy/defence fields on purpose** — do not expect a District to have guards.
- **Duplicate copies** of `BP_Property_*` and `DA_Place_*` exist in two content roots; keep one.

## 10. State configuration — the C++ side, in full

### 10.1 The enums

| Enum | Values |
|---|---|
| `ETerritoryState` | **Unclaimed** (no owner) · **Claimed** (one faction securely owns; capture is the action, Claimed the result) · **Contested** (a real physical capture started, or an aggregate's children disagree) · **Locked** (*legacy serialized value only* — runtime locking lives in `Availability`) |
| `ETerritoryAvailability` | **Unlocked** · **Locked** — saved and replicated **independently of political control**, so a locked enemy Place still contributes political power |
| `ETerritoryInitialState` | **Automatic** (Claimed if an initial owner is set, else Unclaimed) · **Unclaimed** · **Claimed** (empty faction ⇒ Unclaimed) · Locked (hidden legacy) |
| `ETerritoryControlMode` | **Independent** (Place) · **AggregateOnly** (City/District) · Cascading (hidden legacy) |
| `ETerritoryHierarchyLevel` | City · District · Place |
| `ETerritoryUnlockScope` | AutomaticHierarchy · ExactOnly · ForceExact · ForceHierarchy |
| `ETerritoryStateCounterAttackPolicy` | CaptureTriggered · WhileAtWar · QuestOnly · Disabled |

Two free functions are the **single answer** for new-campaign values (four copies had drifted before):
`TerritoryResolveInitialAvailability(InitialState, InitialAvailability)` — the legacy `Locked` initial state wins
only when it is literally the old value — and `TerritoryResolveInitialPoliticalState(InitialState, bHasInitialOwningFaction)` —
`Unclaimed` always wins; otherwise "is an initial faction set?" decides. "Claimed with an empty faction" never exists.

### 10.2 `FTerritoryTransitionContext` — the explicit context

`Instigator` (actor) · `TargetPawn` · `PlayerController` (may be null on dedicated servers / AI captures) ·
`TalesComponent` · `RequestingFaction`. Every transition carries one; a world-level transition deliberately
passes an empty one. This is the AGENTS.md "never call `GetFirstPlayerController()`" rule in type form.

### 10.3 `FTerritoryStateGameplayRules` — what a state row actually is

| Field | Default | Meaning |
|---|---|---|
| `CounterAttackPolicy` | CaptureTriggered | `CaptureTriggered` (after capture / profile schedule) · `WhileAtWar` (automatic while at War) · `QuestOnly` (explicit narrative waves only) · `Disabled` (no new assaults; an ongoing battle continues, peace still cancels) |
| `AllowedAttackingFactions` | empty | empty = any otherwise eligible hostile faction; tags match **exactly** |
| `bAllowPeriodicIncome` | true | owner earns periodic currency in this state; guard upkeep stays payable |
| `bAllowResourceProduction` | true | owner may run Property production recipes; blocked cycles expire **without backpay** |
| `bAllowCapitalCaptureReward` | true | the authored capital capture bonus |
| `GrantedCommandCapabilities` | empty | `Territory.Capability.*` controls granted to the current owner *while this state is active* (e.g. `GuardStaffing`). Derived from live ownership — no save data, removed the moment the Place is lost |
| `EntryConditions[]` | — | `UNarrativeCondition` — all must pass before the state can begin |
| `ExitConditions[]` | — | all must pass before the state can end **and something must still request the change** (a gate, not a trigger — nothing polls) |
| `EntryEvents[]` | — | `UNarrativeEvent` — fired **once** after the state is committed; each event's own conditions must pass |
| `ExitEvents[]` | — | fired after the state ends |

`AllowsAssault(AttackingFaction, bExplicitNarrativeRequest)` = policy allows **and** the faction is valid **and**
(`AllowedAttackingFactions` empty or contains it exactly). Diplomacy and finite-force rules still apply on top.

### 10.4 `FTerritoryStateConfig : FTerritoryStateGameplayRules`

Adds, per state row:

| Field | Meaning |
|---|---|
| `Audio` (`FTerritoryStateAudioConfig`) | `bOverrideNarrativeMusic`, `MusicPriority` (higher wins within a tier; Places beat Districts beat Cities; ties → smaller bounds → stable identity), `MusicSetOverride` (`UTaggedMusicSet`), theme tag |
| `StealthProfileOverride` | per-state stealth policy; empty falls back to the Definition's `DefaultStealthProfile` |
| **`FactionOverrides : TMap<FGameplayTag, FTerritoryStateGameplayRules>`** | **exact owner-faction rows** — a matching row replaces common conditions, events, capabilities, income and counter-attack policy; unlisted factions use the common row. Entry uses the incoming owner, exit the outgoing owner. **Audio and stealth stay on the state row.** |

`ForFaction(OwnerFaction)` returns the override or the common row — that is the whole mechanism.

### 10.5 How the actor resolves a state row

```cpp
GetStateGameplayRules(State, OwnerFaction)  -> GetStateConfigs().Find(State)->ForFaction(OwnerFaction)
GetActiveStateGameplayRules()               -> rules for (IsLocked() ? Locked : GetTerritoryState(), OwningFaction)
GetStateConfigs()                           -> RuntimeStateConfigs      // runtime copies, applied from the Definition
GetActiveStealthProfile()                   -> AggregateOnly => null;  state override ?? Definition default
GetActiveTerritoryAudioConfig(Out)          -> row for (IsAvailableForGameplay() ? State : Locked)
ShouldShowGameplayHUD()                     -> !Definition || Definition->bShowGameplayHUD   // fails visible
```

Note the two deliberate substitutions: **availability beats state** for audio (a locked Place plays the Locked row
without reviving the legacy state), and a missing Definition still shows the HUD so the authoring error is visible.

### 10.6 `ATerritoryVolume` — what it actually is

`class ATerritoryVolume : public AActor, public INarrativeSavableActor, public ITerritoryOwnershipInterface,
public ITerritoryEventReceiverInterface` — the runtime Territory entity for all three hierarchy levels
(Place = `ATerritoryProperty`, District = `ATerritoryDistrict`, City = `ATerritoryCity`).

**Runtime state (saved/replicated):** `OwnershipData` (`FTerritoryOwnershipData`) · `GarrisonSnapshot`
(`FTerritoryGarrisonSnapshot`) · `TerritoryGUID`.

`FTerritoryOwnershipData` — `OwningFaction` · `FormerOwningFactions` (only verified commits write here) ·
`CapturedBy` (who physically took it) · `CapturedFor` (on whose behalf — differs when you capture for another
faction, which is what makes a betrayal beat authorable) · `State` · `Availability` · `ControlProgress` ·
`ContestingFaction` · `DefenderCount` · `MaxConcurrentAttackers` · `PeriodicIncome` · `GuardCost` ·
`GuardRecruitmentCost` · `DesiredGuardCount` (INDEX_NONE = untouched) · `LockReason`.

**Definition-driven transients:** tag/display name, initial owner/availability/state, parent tag, control mode,
max attackers, periodic income, guard cost/recruitment cost, story capture flag, guard NPC + faction map,
guard spawn count, garrison policy, counter-attack profile/approaches, guard quality, fortification, allied
support, strategic value, state configs, defender-died/all-defenders-defeated events, bounds, audio.

**API by category:** Ownership (`GetOwningFaction`, `GetTerritoryState`, `GetControlProgress`, `IsContested`,
`IsOwnedByFaction`, `GetMaxConcurrentAttackers`, `CommitOwnershipData`, `SetOwningFaction*`) · Lock
(`IsLocked`, `LockTerritoryWithContext`, `TryUnlock[WithContext]`, `CanUnlock`, `GetLockReason`,
`IsAvailableForGameplay`) · Guards (`GetDefenderCount`, `TrySpawnSingleGuard`, `GetSpawnedGuardCount`,
`GetConfiguredGuardCount`, `GetDesiredGuardCount`, `GetMaxGuardCount`, `GetPostCaptureGuardCount[ForOwner]`,
`GetGarrisonSnapshot`, `GetGuardRecruitmentCost`, `GetRegisteredDefenders`) · Economy (`GetPeriodicIncome`,
`GetGuardCost`) · Bounds (`GetTerritoryBounds`, `ContainsPoint`) · Hierarchy (`GetParentTerritoryTag`,
`GetControlMode`, `GetOwningDistrict`) · State Rules (`GetStateGameplayRules`,
`GetActiveStateGameplayRules`, `DoStateRulesAllowAssault`) · Stealth · Audio · Counter Attack
(`GetStrategicValue`, `GetGuardQuality`, `GetFortificationStrength`, `GetNearbyAlliedSupport`) · Save
(`GetActorGUID`, `PrepareForSave`, `Load`, `WasRestoredFromCampaignSave`) · UI (`ShouldShowGameplayHUD`,
`IsCommandCapabilityConfigured`).

**Delegates:** `FOnTerritoryControlChanged` · `FOnTerritoryStateChanged` · `FOnTerritoryAvailabilityChanged` ·
`FOnTerritoryRegistered` · `FOnEconomyTick` · `FOnCaptureAttempted` · `FOnTransactionRecorded` ·
`FOnCityCaptured/Lost` · `FOnDistrictCaptured` · `FOnPropertyCaptured` · `FOnAllGuardsDefeated` ·
`FOnTerritoryGarrisonChanged` · `FOnGuardKilled` · `FOnTerritoryFloorCleared` ·
`FOnTerritoryDefenderSpawned`.

**Blueprints override, never duplicate:** `OnOwnershipChanged` (fires on **every** ownership path — direct
capture, hierarchy cascade, quest event) and `ReconcileOwnershipDependentSystems` on `ATerritoryProperty`;
`OnPropertyCaptured` (native event, after a verified capture) and `OnUpgradeLevelChanged` (implementable event,
also on replication).

### 10.7 The atomic commit — `CommitOwnershipDataValidated`

```text
HasAuthority                                        else reject
NewData.State == Locked                             reject ("use LockTerritoryWithContext")   // legacy state
bPoliticalControlChanged && AggregateOnly           reject (parent is reduced from children)
IsGameplayMutationInProgress()                      reject  // reentrant commit during the event bundle
ValidationGuard                                     // conditions cannot commit again mid-validation
CommitLoadGeneration + IsGameplayCallbackStateCurrent  // a save restore invalidates an older operation
transition conditions (unless bypassed)             // Entry/Exit conditions of the state rows
bCapturedByDifferentOwner = Claimed -> Claimed with a different owner   // a real handover still runs
                                                                         // old owner's Exit + new owner's Entry
=> commit -> replicate/save snapshot -> Narrative events -> one verified broadcast
```

Nothing broadcasts success before the final invariant holds; a same-owner reset does not refire the state events,
and walking through a Place is not a contest unless story-bounds registration actually makes the player an attacker.

