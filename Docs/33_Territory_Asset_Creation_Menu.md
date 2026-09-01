# Territory Framework Asset Creation Menu

Territory Framework has its own **Add/New** category in the Unreal Content Browser. It follows
the same idea as Narrative Pro's Narrative Items menu: choose the exact asset you need instead of
creating a generic Data Asset or Blueprint and searching through a long class list.

## Open the menu

In the Content Browser:

1. Open the folder where the new asset should live.
2. Press **Add** or right-click empty space.
3. Open **Territory Framework**.
4. Open one of the small groups below.
5. Choose the exact asset.

The new asset is created in the selected folder and opens for editing.

## Definition and profile assets

| Menu group | Asset | Use it for |
|---|---|---|
| World & Hierarchy | Territory Place Definition | One capturable location, such as Blacksmith or Farm Hill |
| World & Hierarchy | Territory District Definition | A list of Place Definitions, such as Castle Hill |
| World & Hierarchy | Territory City Definition | A list of District Definitions, such as Haven Reach |
| Combat & Defence | Territory Counter-Attack Profile | Enemy forces, vehicles, schedules, difficulty, and recapture |
| Combat & Defence | Territory Guard Post Definition | Reusable guard capacity, spawn, patrol, and garrison rules |
| Economy | Territory Production Profile | Money or Narrative Item production and required inputs |
| Stealth & Disguise | Territory Stealth Profile | Sight, evidence, gunfire, investigation, and exposure rules |
| Stealth & Disguise | Territory Disguise Profile | Faction disguise identity and suspicion rules |
| AI & Diplomacy | Territory Diplomacy Dialogue Profile | Friendly, neutral, suspicious, and hostile dialogue selection |
| Story & Quests | Territory Quest Cascade Recipe | Reusable states, branches, tasks, and events that generate a Narrative Quest |

These are the authoritative reusable settings. Duplicate them when you need a new configured
Place, District, City, force, economy, or story recipe.

## Blueprint templates

The same Territory Framework menu also creates Blueprints with the correct native parent already
selected. There is no parent-class picker.

| Menu group | Blueprint | Use it for |
|---|---|---|
| World Actors | Territory Place Actor Blueprint | Level representation of one Place Definition |
| World Actors | Territory District Actor Blueprint | Level representation of one District Definition |
| World Actors | Territory City Actor Blueprint | Level representation of one City Definition |
| Interaction & Navigation Actors | Territory Capture Point Blueprint | Optional physical multiplayer/hold-zone capture |
| Interaction & Navigation Actors | Territory Story Owner Spawner Blueprint | Narrative NPC dialogue handover after combat |
| Interaction & Navigation Actors | Territory Management Point Blueprint | In-world POI for district commands and upgrades |
| Interaction & Navigation Actors | Territory Road Guide Blueprint | Reinforcement, traffic, chase, and vehicle route spline |
| Guard & Combat Actors | Territory Guard Spawn Point Blueprint | Reusable defender/patrol/counterattack staging point |
| Guard & Combat Actors | Territory Guard Character Blueprint | Narrative NPC defender with Territory behavior |
| Guard & Combat Actors | Territory Assault Character Blueprint | Narrative NPC attacker that can fight and take over a Place |

Blueprint templates are visual/level helpers. They do not replace Definition assets and they do
not become a second source of gameplay configuration.

## Easy Blacksmith example

Create these assets:

```text
DA_Blacksmith_Place             Territory Place Definition
DA_Blacksmith_Production       Territory Production Profile
DA_Bandit_Response             Territory Counter-Attack Profile
BP_Blacksmith_Place            Territory Place Actor Blueprint
BP_Territory_GuardSpawnPoint   Territory Guard Spawn Point Blueprint
BP_Blacksmith_StoryOwner       Territory Story Owner Spawner Blueprint (optional)
```

Then:

1. Assign `DA_Blacksmith_Production` and `DA_Bandit_Response` inside
   `DA_Blacksmith_Place`.
2. Assign `DA_Blacksmith_Place` to `BP_Blacksmith_Place`.
3. Place the Blacksmith actor and reusable spawn-point actors in the level.
4. Use a Story Owner only when capture needs dialogue handover.
5. Use a Capture Point only when this location uses physical hold-zone capture.

The Definition remains the source of truth. The level actors bind the world to that Definition.

## Narrative Pro boundary

This menu does not modify Narrative Pro. Narrative still owns quests, dialogue, factions, items,
NPC definitions, AI, inventory, navigation, save, and replication. Territory assets reference or
adapt those public systems.

For example, a Territory Production Profile selects a Narrative Item; it does not create a second
item type. A Territory Quest Cascade Recipe creates a normal Narrative Quest; it does not run a
separate quest system.

## If the category is missing

- Confirm the Territory Framework plugin and its editor module are enabled.
- Restart the editor after compiling a new plugin build.
- Open the Content Browser **Add** menu, not the Place Actors panel.
- Search for `Territory` if a narrow Content Browser window hides category names.

The creation menu is editor-only. Packaged games contain the created assets, not the factories or
Content Browser integration.
