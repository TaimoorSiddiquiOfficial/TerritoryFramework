# Narrative Pro Integration

## How TerritoryFramework Extends Narrative Pro (Without Modifying It)

```
Narrative Pro
├── ANarrativeGameState         ← Authority for faction attitudes
├── Narrative Save System       ← Saves TerritoryVolume + WorldState actors
├── Narrative Tales             ← Territory tasks, conditions, events
├── Narrative Navigator         ← Territory markers, map drawing
├── Narrative NPC/GAS           ← Guard definitions, factions, death events, AI
└── Narrative Navigation        ← Territory map markers, ownership colors

TerritoryFramework
├── ATerritoryVolume            ← Extends AActor + INarrativeSavableActor
├── ATerritoryGuardCharacter    ← Extends ANarrativeNPCCharacter
├── UTerritoryCaptureTask       ← Extends UNarrativeTask
├── UTerritoryCaptureEvent      ← Extends UNarrativeEvent
├── UTerritoryOwnershipCondition ← Extends UNarrativeCondition
├── UTerritoryMapMarker         ← Extends UMapMarker
└── UTerritoryNavigationMarkerComponent ← Extends UNavigationMarkerComponent
```

## Faction System

TerritoryFramework uses Narrative Pro's faction tags (`Narrative.Factions.*`).

- Territory ownership is a single `FGameplayTag`
- `ANarrativeGameState::FactionAllianceMap` is the **sole authority** for AI attitudes
- `AttemptCapture` checks `GetFactionAttitudeTowardsFaction` before allowing capture
- Friendly factions cannot capture each other's territories

### Integration Points

| Narrative API | TerritoryFramework Usage |
|---|---|
| `GetFactionAttitudeTowardsFaction` | Pre-capture attitude check |
| `SetFactionAttitude` | Sync diplomacy changes |
| `OnFactionAttitudeChanged` | Reconcile treaty metadata |
| `FactionAllianceMap` | Load treaty state from save |

## GAS Integration

| GAS API | Usage |
|---|---|
| `UNarrativeAbilitySystemComponent::OnDied` | Guard death tracking → spawn point notification |
| `INarrativeTeamAgentInterface::AddFaction` | Assign guard faction from territory owner |
| Attack tokens | `UTerritoryCombatDirector` wraps attack permissions per territory |

## Tales Integration

### Quest Tasks

| Class | Parent | Trigger |
|---|---|---|
| `UTerritoryCaptureTask` | `UNarrativeTask` | Completes when territory is captured/lost |

### Quest Events

| Class | Parent | Trigger |
|---|---|---|
| `UTerritoryCaptureEvent` | `UNarrativeEvent` | Fired by quest/dialogue to capture territory |

### Dialogue Conditions

| Class | Parent | Check |
|---|---|---|
| `UTerritoryOwnershipCondition` | `UNarrativeCondition` | Gates dialogue by territory ownership |

## Save System

| Save Class | What It Saves |
|---|---|
| `ATerritoryVolume` | OwnershipData (owner, state, progress, income, guards) |
| `ATerritoryWorldState` | Economy, treaties, reputation, transactions |
| `ATerritorySavableData` | Legacy single-player save adapter |

## Navigation Integration

| Class | Parent | Purpose |
|---|---|---|
| `UTerritoryMapMarker` | `UMapMarker` | Dynamic ownership color, name, outline |
| `UTerritoryNavigationMarkerComponent` | `UNavigationMarkerComponent` | Auto-creates marker, binds to changes |

## What NOT to Duplicate

These Narrative Pro systems already exist — TerritoryFramework does **not** re-implement:

- ❌ Faction attitude storage (use `ANarrativeGameState`)
- ❌ NPC spawning framework (use `ANarrativeNPCCharacter` + `UNPCDefinition`)
- ❌ Behavior tree nodes (reuse `BTTask_MoveTo`, `BTTask_RotateToGoal_C`, etc.)
- ❌ Save serialization (use `INarrativeSavableActor`)
- ❌ Map widget rendering (use Narrative Navigator)
