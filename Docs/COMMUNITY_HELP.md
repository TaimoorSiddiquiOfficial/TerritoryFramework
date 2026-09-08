# Help in easy English

In Unreal, hold the mouse over a setting name or a Blueprint function. Its tooltip explains what the option does. Expand a group or an array item to see help for the settings inside it.

For a first setup, use [Quick Start](01_Quick_Start.md). For the full system, use the [Easy Complete Guide](00_Easy_Complete_Guide.md).

## Common words

| Word | Meaning |
|---|---|
| Place | A location that can be captured, such as a farm or shop. The C++ actor is called Territory Property. |
| District | A group of Places. Its control comes from those Places. |
| City | A group of Districts. Its control comes from those Districts. |
| Faction | A team defined in Narrative Pro. |
| Definition | A data asset that stores the setup for a Territory. |
| Profile | A reusable group of settings, such as counterattack rules. |
| State | Whether a Territory is Unclaimed, Contested, or Claimed. |
| Availability | Whether the story has locked or unlocked the Territory. A locked Place can still have an owner. |
| Garrison | Guards assigned to protect a Place. |
| Reserve | The limited number of replacement guards or attackers left. |
| Assault | A counterattack that tries to take a Place back. |
| Diplomacy | How factions treat each other, including war, peace, and alliances. |
| Server | The game instance that approves gameplay changes in multiplayer. |
| Replicated | Sent from the server to connected players. |
| Runtime | While the game is running. |
| Instigator | The actor that caused an action, such as the player who captured a Place. |

## Numbers and empty settings

- A **fraction** uses 0 to 1. For example, 0.5 means 50%. City control and capture progress use this range.
- A field that explicitly says **percent** or **0 to 100** uses that range instead. Read its tooltip.
- **Seconds** are a duration. An economy tick is one income and cost update; it does not mean one rendered frame.
- An empty setting does not always mean disabled. Some settings use a parent or a project default when empty. The tooltip tells you when this happens.
- Read-only fields show a calculated or saved result. Change the source settings instead.

## Simple examples

**Multiplayer flag capture:** use physical automatic capture for the Place and add its Capture Point. Eligible players or attackers fill capture progress under the existing combat and diplomacy rules.

**Story handover:** use story-bounds capture when a quest or owner conversation should approve the handover. A Place cannot run story-bounds capture and physical automatic capture together.

**Faction-only income:** use the Place's state rules and faction overrides to allow income for the chosen owner faction. Narrative inventory remains the money account.

**Retake conversation:** use a Situation Profile to check former ownership, diplomacy, power, and control. Use a Dialogue Recipe for the planning conversation or handover. See the [retake guide](CONDITIONAL_RETAKE_DIALOGUE_2026-09-07.md).

## Help coverage

This pass adds help to 991 previously empty property and function tooltips across the runtime and editor modules. It covers settings, nested data structures, read-only results, Blueprint functions, and events that declare an editor category. It also improves the Active Capture Mode row.

This count does not claim every enum choice, private C++ member, or custom project widget has been rewritten. Report unclear wording with the asset type and setting name so it can be improved.

The final Unreal Header Tool check found help on all 2,051 category-bearing properties and functions in these two modules, including the new content-import function.
