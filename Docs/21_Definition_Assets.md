# Territory Definition Assets

Territory Definition assets put the settings for one City, District, or Place in one easy-to-copy asset. They reduce level-actor setup without creating a second runtime territory system.

## The simple idea

For a Blacksmith, create one `Territory Place Definition` such as `DA_Place_Blacksmith`.
It can describe:

- the Property Blueprint and its parent District;
- new-campaign owner, state, capture mode, and state conditions/events;
- capture-point Blueprint, position, radius, and automatic capture policy;
- story-owner Blueprint, Narrative NPC Definition, dialogue, and interaction distance;
- management-point Blueprint, Narrative UI layer, widget, and interaction distance;
- every guard-post Blueprint, position, stable save ID, patrol route, Narrative activity,
  TriggerSets, reserve settings, and faction override;
- production, upgrades, economy, defender events, and counterattack configuration.

The level still contains physical actors because their bounds and positions are part of the world.
Those actors now read their authoring policy from the one definition asset.

```text
DA_City_HavenReach
├── DA_District_MarketSquare
│   └── DA_Place_Blacksmith
└── DA_District_CastleHill
    └── DA_Place_Farm
```

The City array creates the District parent links. Each District array creates its Place parent
links. Do not type the same parent tag on several actors.

## What owns what

| Information | Authority |
|---|---|
| Reusable setup | Territory Definition asset |
| Current owner, state, progress | placed Territory actor |
| Capture validation/progress | Territory Control Subsystem |
| Factions, NPCs, activities, dialogue, inventory, POI | Narrative Pro |
| Persistent snapshot and late join | Territory World State |

Example: the asset says a Blacksmith starts owned by Bandits. After Heroes capture it, the
placed Property actor and save data say Heroes own it. Reapplying the asset does **not** erase
that campaign result.

## Create a new hierarchy

1. In the Content Browser, create a Data Asset and choose `Territory City Definition`.
2. Create one `Territory District Definition` for each District.
3. Create one `Territory Place Definition` for each capturable Place.
4. Add the Place assets to each District's `Places` array.
5. Add the District assets to the City's `Districts` array.
6. Use `Refresh Hierarchy Links` on the City.
7. Assign Blueprint classes, never raw C++ classes, for the Territory and enabled helper templates.
8. Run `Synchronize Territory City In Current Level` from the Territory Definition editor library.

The synchronizer updates matching actors by definition or stable Territory tag. With `Create
Missing Actors` enabled, it creates missing actors from the Blueprint classes stored in the
assets. Existing actors are not moved unless `Move Existing Actors To Definition Transforms`
is enabled.

## Duplicate a Place safely

To make a new Bakery from the Blacksmith:

1. Duplicate `DA_Place_Blacksmith` as `DA_Place_Bakery`.
2. Give it a new Territory gameplay tag and display name.
3. Change its production, owner NPC/dialogue, guard posts, and event rules.
4. Add it to the correct District's `Places` array.
5. Refresh the City hierarchy and synchronize the level.

Normal asset duplication creates new Territory and guard-post GUIDs. This prevents the Bakery
from loading the Blacksmith's saved state. Never copy a stable GUID by hand.

## Story and multiplayer capture

- `Story Capture From Bounds = true`: the complete multi-floor Place bounds can start contesting.
  Defeating all defenders may activate the configured story owner. A Narrative dialogue/event
  performs the handover. Automatic capture-point progress is disabled.
- `Capture Point > Enabled = true`: the configured Blueprint provides physical progress for
  domination or multiplayer. Its target tag always comes from the Place asset.

These settings cannot create a second owner or progress value. They only configure adapters to
the existing server-authoritative capture flow.

## Conditions and events

`State Configs` is the modular table for `Locked`, `Unclaimed`, `Contested`, and `Claimed`.
Each row supports inherited Narrative conditions plus entry and exit events.

Easy examples:

- Locked exit condition: the quest `Meet the Informant` is complete.
- Contested entry event: set the current owner and contesting faction to War.
- Claimed entry event: give XP, unlock the next Place, or schedule a finite enemy wave.
- Claimed exit event: remove a perk when the Place is lost.

Events and conditions are authored as reusable templates inside the definition asset. At runtime,
the framework creates a private copy for each loaded Territory actor. This is important: a
Narrative event needs the live actor's World and transition context. Executing the template object
from the Content Browser would make diplomacy, handover, spawning, and other world actions silently
fail.

Direct references from a reusable event to one level actor are not supported. Use stable Territory
tags instead, so the correct live actor can be found after World Partition loads it.

## Completed migration and strict authority

The project migration is complete. There is no actor-side fallback and no copy/migrate button at
runtime or in the Blueprint API.

- Every placed City, District, and Place must have a matching Definition asset.
- Every Capture Point, Management Point, Story Owner, and Guard Post must link to the appropriate
  Definition (plus a Guard Post row ID where required).
- A placed Territory actor stores only its Definition link, physical bounds/presentation, stable
  identity, and saved/replicated campaign state.
- State rules, defender events, initial ownership, guards, economy, production, assault policy,
  and helper-actor settings come from the Definition.
- Missing or incompatible Definitions fail validation and disable runtime activation instead of
  falling back to stale Blueprint values.

Existing projects upgrading from an older plugin version must migrate in the older migration
release first, save the converted assets/maps, and only then upgrade to this strict version. That
ordering preserves stable Territory and Guard Post GUIDs and prevents old saved campaigns from
binding to a different Place.

## Validation messages

Data Validation catches missing/wrong Blueprint classes, stale hierarchy links, duplicate tags
or save GUIDs, invalid helper transforms/distances, missing story-owner NPC definitions, duplicate
guard-post IDs/GUIDs, invalid patrol nodes, and unsafe reserve limits.

Warnings such as “definition is not connected to a parent” are useful while authoring a loose
District or Place. They disappear after the asset is added to its parent and hierarchy links are
refreshed.
