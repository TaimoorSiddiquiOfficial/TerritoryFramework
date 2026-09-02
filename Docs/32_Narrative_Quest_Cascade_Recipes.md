# Narrative Quest Cascade Recipes

## What this feature solves

A story pattern often repeats:

1. reach a Place;
2. defeat its defenders;
3. claim it;
4. speak with the owner;
5. succeed, fail, or open another route.

Building this graph by hand for every Place is slow and easy to wire incorrectly. A
`UTerritoryQuestCascadeRecipe` stores that reusable authoring pattern. Press one button and the
Territory editor creates a normal, editable Narrative Quest beside the recipe.

The recipe is not a second quest system. After generation:

- Narrative Pro owns quest state, task progress, replication, journal UI, navigation markers,
  events, and save/load;
- the generated asset is an ordinary `UQuestBlueprint`;
- runtime does not need to read the recipe;
- changing the recipe does not silently rewrite an existing quest.

The recipe now mirrors Narrative's complete reusable Quest setup: journal name/description,
initial tracked state, optional linked Quest Dialogue, Dialogue Play Params, resume-after-load,
state checkpoint policy,
Objective/Success/Failure states, route visibility, tasks, conditions, and events. Narrative's
custom Blueprint functions and parent-Quest `Inheritable States` remain hand-authored features:
the recipe cannot safely generate a function body that does not exist, and it deliberately
creates a standalone Quest rather than a child-Quest inheritance contract.

## The important task rule

Narrative activates every outgoing branch on the current state.

- **Tasks inside one branch mean AND.** Every task must complete before that branch advances.
- **Two branches mean alternative routes.** Whichever branch completes first selects its
  destination and deactivates the other route.
- A branch needs at least one required task or condition. A completely empty branch, or a branch
  made only from Optional tasks, would complete immediately, so validation rejects it.

Easy example: put `Kill Enemy (3)` and `Capture Territory` on one branch when both are required.
Create separate **Assault** and **Stealth** branches when either method may advance the story.

## Create a recipe

1. In the Content Browser, choose **Territory Framework > Story and Quests > Territory Quest
   Cascade Recipe**.
2. Rename the suggested `DA_QC_NewMission` asset to `DA_QC_LiberatePlace`.
4. Set `Quest Name`, `Quest Description`, initial `Tracked` state, and `Start State ID`.
5. Optionally assign a Narrative `Quest Dialogue`, configure its normal Play Params, and choose
   whether it resumes after loading.
6. Add Objective, Success, and optional Failure rows to `States`.
7. Add routes to each Objective state.
8. In each route's `Tasks` array, choose any Narrative task or Territory Narrative task and set
   its normal properties.
9. Add shared State Conditions, route-specific Branch Conditions, and Narrative Events where
   required.
10. Choose a checkpoint policy under **Quest > Saving** when death/loading should resume the
    latest reached objective.
11. Read the live **Mission Logic (Read Only)** panel and fix its Data Validation messages.
12. At the top of the recipe Details panel, press
   **Create New Narrative Quest From This Recipe**.

The editor creates a unique `NQ_` asset in the same folder, lays out its graph, copies every task
and Narrative Event, condition gate, and Quest setting, compiles it, and opens it. If
`NQ_LiberatePlace` already exists, the next
asset receives a unique name; no quest is overwritten.

If the recipe still has a generic filename such as `NewTerritoryQuestCascadeRecipe` or
`DA_QC_NewMission`, the generator uses the player-facing Quest Name instead. For example,
`Quest Name = Liberate the Blacksmith` creates `NQ_LiberateTheBlacksmith`, not
`NQ_NewTerritoryQuestCascadeRecipe`. The generated Quest's real class defaults receive the same
Quest Name and Quest Description, so the Narrative journal does not show a filename or the vendor
placeholder text.

## Checkpoints, death, and save/load

Narrative Pro already serializes:

- the current Quest State ID;
- every branch task's progress;
- every reached State ID;
- SaveGame properties on the Quest.

That data reaches disk only when Narrative's Save subsystem commits a save. The recipe provides
three `Checkpoint Mode` choices:

- **Disabled** — no automatic save is injected;
- **Every Objective State** — save after each playable state becomes current;
- **Every State Including Endings** — also commit Success and Failure immediately.

Easy three-state example:

```text
State 1: Reach Blacksmith
  -> passes
State 2: Defeat Guards       <- automatic checkpoint is written here
  -> player dies
Load latest campaign save
  -> State 2 resumes; State 1 is still recorded as reached
State 3: Capture Blacksmith
```

Automatic checkpoints are normal inline `Save Narrative Quest Checkpoint` events generated on
State Start. They run on server authority, wait until the current state transition finishes, use
Narrative's active campaign save name, and never refire while a save is loading. If no campaign is
active yet, the default fallback is `NarrativeSave0`. Set `Checkpoint Save Name Override` only
when the host game intentionally uses a different save contract.

The reusable event can also be added manually to one State. A manual checkpoint on that State
prevents the generator from adding a duplicate. Saving a checkpoint does not decide the game's
death screen behavior: the host game's death/respawn flow must load the latest Narrative save if
death is meant to rewind the world. If the game respawns without loading, the current in-memory
quest naturally remains where it was.

## Conditions that really work in quests

Narrative Pro exposes a Conditions array on Quest states and branches, but the current Narrative
Quest runtime does not evaluate those arrays. Dialogue nodes use Narrative's evaluator. Territory
Narrative Events explicitly evaluate their own inherited Conditions because the installed Quest
event dispatcher does not do so automatically. Copying Quest-node conditions alone would
therefore create a dangerous editor option that appears configured but never blocks progress.

Territory resolves this without modifying Narrative Pro:

- **State Conditions** are shared requirements for every route leaving that state. They do not
  prevent the Quest from entering the state.
- **Branch Conditions** apply only to that route.
- all listed State + Branch conditions use AND logic and must remain true until the other required
  tasks complete;
- the generator leaves Narrative's unsupported Quest State/Branch condition arrays empty and
  copies every requirement only into one hidden `Wait For Narrative Conditions` task;
- the hidden task calls Narrative's own evaluator, preserving inherited `Not`, character filters,
  and multiplayer party policies;
- a Success or Failure state has no route to leave, so conditions placed on it produce a warning.

Easy example:

```text
State: NegotiateHandover
  Shared condition: Territory defenders are defeated

Branch: OwnerAccepts -> PropertySecured
  Route condition: Player reputation with owner faction is at least 25
  Task: Finish the owner dialogue

Branch: OwnerRefuses -> EscapeFailure
  Route condition: NOT player reputation at least 25
  Task: Finish the refusal dialogue
```

Conditions inside a Territory Narrative Event are different: they decide whether that one event
fires. They do not gate the whole route, and each Territory event runs them through Narrative's
normal condition semantics (`Not`, filters, and party policy) before mutating gameplay.

## Mission Logic summary

The recipe Details panel contains a live read-only report like Territory Place/District/City
Story Outcome. It shows:

- objective, success, and failure state counts;
- every route and destination;
- player tasks, Required Quantity, Optional, Hidden, and Navigation Marker settings;
- shared and route-specific conditions;
- state/branch events and their totals;
- automatic checkpoint count and policy;
- unreachable states, unsafe instant routes, missing IDs, and other setup problems.

`BuildMissionLogicSummary()` exposes the same counts, flow lines, errors, and warnings to Blueprint
at runtime for developer tools. `BuildPlainTextPreview()` returns the complete copyable report.
Both are read-only; neither starts or changes a Quest.

### Inspect an existing Narrative Quest graph

A recipe can also audit a Quest that was built or edited directly in Narrative's graph editor.
This does not copy the Quest into the recipe and does not replace its runtime:

1. Assign **Narrative Quest Graph** at the top of the recipe Details panel.
2. Press **Refresh Runtime Quest Summary** after compiling or saving that Quest.
3. Read the expanded runtime report, or press **Copy Runtime Report**.

The report reads `UQuestBlueprintGeneratedClass::QuestTemplate`, which is the compiled template
Narrative duplicates when the Quest starts. It includes the real Quest name/description, tracked
state, Start State, every state and route, each task's class and editable settings, hidden
condition gates, event runtime/options, event conditions, markers, endings, and reachability.
It therefore also covers hand-authored options that do not exist in the recipe.

Important findings are explicit. An empty Event row is an error. A state not reachable from the
Start State is a warning. A hand-authored Quest State/Branch Condition without a hidden
`Wait For Narrative Conditions` task is also warned because this Narrative Pro version displays
those arrays but does not evaluate them in Quest progression.

For an older generated Quest, assign it to **Narrative Quest Graph** and press
**Remove Unsupported Quest-Node Conditions**. The migration preserves missing requirements in
hidden gate Tasks, clears the ineffective State/Branch arrays, recompiles the Quest, and marks it
dirty for review and Save. Event Conditions are already functional and are never moved.

`BuildSelectedRuntimeQuestSummary()` returns the structured result and
`BuildSelectedRuntimeQuestReport()` returns the complete text. Both are read-only; pressing
Refresh never starts, recompiles, saves, or edits the selected Quest.

## Simple kill-and-capture recipe

This version uses several quest states so the journal changes as the story moves forward.

| State ID | Type | Branch ID | Tasks on that branch | Destination |
|---|---|---|---|---|
| `ApproachBlacksmith` | Objective / Start | `ReachOuterYard` | Narrative Move or AI Observation task | `ClearDefenders` |
| `ClearDefenders` | Objective | `GuardsDefeated` | Narrative `BPT_KillEnemy`, Required Quantity `3` | `ClaimPlace` |
| `ClaimPlace` | Objective | `BlacksmithClaimed` | `BPT_TerritoryCapture`, target Blacksmith, objective Claimed by Player Faction | `ReportToOwner` |
| `ReportToOwner` | Objective | `HandoverAccepted` | Narrative Interact or Finish Dialogue task | `PropertySecured` |
| `PropertySecured` | Success | — | — | — |

This is a cascade: each destination becomes the next current state, so Narrative closes the old
tasks and activates the new tasks.

For one combined battle objective, use one branch on `AssaultBlacksmith` containing both:

- `BPT_KillEnemy`, Required Quantity `3`;
- `BPT_TerritoryCapture`, target Blacksmith.

Both must complete. This is useful when the Territory capture task observes the final claim while
the Narrative kill task independently counts named quest enemies.

## Alternative story routes

One state may contain multiple branches:

| Current state | Route | Tasks | Destination |
|---|---|---|---|
| `EnterBanditCamp` | `OpenAssault` | Kill 5 Bandits + Capture Place | `MeetProtester` |
| `EnterBanditCamp` | `RemainDisguised` | Disguise remains accepted + reach the leader | `LearnFalseNarrative` |
| `EnterBanditCamp` | `CoverBlown` | Exposure becomes Burned | `EscapeFailure` |

These are alternatives. The stealth route can let the later friend explain that the Bandits are
protesters trapped by a false narrative. The assault route can still move the main story forward,
but with different reputation and relationship events.

## State and branch events

Recipe states and branches accept inline Narrative Events. They are duplicated onto the generated
Narrative nodes.

- State Start event: unlock the next Place after the Blacksmith is secured.
- Branch Start event: declare a scoped Territory war when open assault begins.
- Branch End event: restore diplomacy after the handover route completes.
- Success State event: grant resources, reputation, or a new faction relationship.

Use each event's normal `Event Runtime` (`Start`, `End`, or `Both`). Do not use events as hidden
task progress. A task observes progress; an event performs a story action.

## Validation rules

The recipe reports errors for:

- missing or duplicate state/branch IDs;
- one ID reused by both a state and a branch;
- a missing Start State;
- a terminal Start State;
- a branch with no destination;
- a destination that does not exist;
- a branch with no task or an empty task row;
- Required Quantity below one;
- branches leaving a Success or Failure state;
- no Success state.

It warns about an empty quest title or description, an Objective with no route forward, and states unreachable
from the Start State. It also reports empty condition/event rows, all-Optional instant routes,
terminal-state departure conditions, marker tasks without a Marker Class, and dialogue resume
enabled without a linked Quest Dialogue.

## Stable ID and save-game rule

State and Branch IDs become real Narrative node IDs. Treat them like save-data keys. Renaming an
ID after players have shipped saves can stop an old saved quest from finding that node. Descriptions,
task quantities, and layout may change more safely; IDs should remain stable.

## C++ and Editor Utility access

- Runtime recipe: `UTerritoryQuestCascadeRecipe`
- Validation: `ValidateRecipe()`
- Structured recipe report: `BuildMissionLogicSummary()`
- Read-only plan export: `BuildPlainTextPreview()`
- Selected compiled Quest report: `BuildSelectedRuntimeQuestSummary()` and
  `BuildSelectedRuntimeQuestReport()`
- Create beside recipe: `CreateQuestBesideRecipe()`
- Create in a selected content path: `CreateQuestFromRecipe()`
- Populate an empty Narrative Quest: `BuildEmptyQuestFromRecipe()`
- Friendly generated filename: `GetSuggestedQuestAssetName()`
- Manual checkpoint event: `UTerritoryNarrativeCheckpointEvent`

`BuildEmptyQuestFromRecipe` rejects any Narrative Quest that already contains authored nodes. This
is intentional protection for community developers' hand-written quest graphs.
