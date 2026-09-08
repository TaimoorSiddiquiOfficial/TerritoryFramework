# Included content

The plugin now contains copies of all 108 assets from the project's Territory Framework folder. It also includes their Territory attack-goal Blueprint and the nine existing Tales tasks: **118 assets in total**.

In the Content Browser, enable **Show Plugin Content** and open **Territory Framework Content**. Asset paths start with `/TerritoryFramework/`.

| Folder | What you can use |
|---|---|
| Definitions and Blueprints | Haven Reach City, Market Square and Castle Hill Districts, Blacksmith and Farm Places, and actor templates. |
| AI | Guard and assault characters, NPC Definitions, activities, patrol and combat setup, and guard posts. |
| UI | Capture HUD, Command Center, District management, economy and production rows, buttons, and textures. |
| Dialogue/Retake and StoryCapture | Planning and handover dialogue, owner NPCs, situation checks, and dialogue recipes. |
| Economy | Farm and Blacksmith production profiles, grain and meat items. |
| Stealth | Distraction ability, throwable item, projectile, and a stealth profile. |
| Cinematics/Dialogue/Shots | Seven reusable dialogue camera sequences. |
| Framework | Example player character, player controller, Game Mode, and quest recipe. |
| PhysicalMaterial | A road physical material to assign to your own road meshes. |
| Tales/Tasks | Nine Narrative task Blueprints. |

## Changes made for the community copy

References between the copied assets point to the plugin. The original project assets are preserved.

The plugin registers its five example Territory tags from `Config/Tags/TerritoryExamples.ini`
through Unreal's Gameplay Tags manager. These are example identities, not a second
ownership or faction database. Your project's Narrative setup still supplies Narrative tags.

- The example Game Mode uses Narrative's default player Definition instead of TDA's custom player appearance.
- The example vehicle approach uses Narrative's sedan.
- The distraction item uses a small Engine sphere. Replace its visible mesh with a rock you own if you want a different shape.
- UI examples use standard Narrative Pro styles. The separate RPG UI theme is not required.
- Blacksmith uses the current Narrative ambient or combat theme. Its TDA music and entry sounds are not included. `DA_BlacksmithMusic` is an empty music-library template where you can add your own tracks.
- The copied Farm Definition omits an unfinished reward row that had no weapon selected. Add a complete gameplay benefit when you choose the reward for your game.
- Saving the Farm example applies the existing story-capture rule: Story Capture From Bounds disables automatic flag progress. The original project Definition is unchanged. For multiplayer flag capture, turn off Story Capture From Bounds and enable Automatic Capture on your own Place.

The shared content is saved in UE 5.7 format so both releases can read it. Compatible original assets were restored through Unreal's Asset Tools. Nine current data assets and both current retake conversations (52 nodes) were restored through the editor APIs and compared by their authored values, links, and node positions. The Farm capture normalization above is the only difference in that comparison. No asset version headers were patched.

## Use the examples in your own game

Duplicate an example into your project's Content folder before changing it. Give new Cities, Districts, and Places their own Territory tags and identities. Do not place two actors with the same Territory tag in one world.

Set up your map, Navigation Mesh, and road routes. Vehicle spawn and drop-off positions are examples from the test scene; move them to valid roads in your map. Add the Capture Point or story handover flow your Place uses.

Assign the supplied Territory HUD and menu through your Narrative player controller. The plugin does not replace your game's controller or force a global UI theme on installation. The [UI guide](18_Operations_UI.md) explains the setup.

The HDR Editor Utility is optional. Its sky setup needs Ultra Dynamic Sky and Narrative's UDS integration. Install those separately before using the UDS part of that tool.
An empty sky-class pin uses the standard Narrative UDS class. A class you select explicitly takes priority.

## Import helper for developers

`TerritoryDefinitionEditorLibrary.CopyProjectContentToPlugin` uses Unreal's Asset Tools to copy an explicit map of project package paths into the Territory plugin. It updates references between those copies and saves them. It rejects play mode, missing sources, vendor source paths, duplicate destinations, and existing destination packages.

It does not automatically copy every external dependency. Inspect the copied assets with Reference Viewer and Data Validation, then test them in a clean project. A failed save can leave partial copies, so review the reported destination before retrying.
