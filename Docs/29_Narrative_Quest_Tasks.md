# Territory Narrative Quest Tasks

Territory Framework uses Narrative Pro's real `UNarrativeTask` class. It does not create a second
quest system. A Territory task belongs inside a normal Narrative Quest branch, reports progress
through Narrative, and completes through Narrative's server-authoritative task flow.

## Where the tasks appear

In the Narrative Quest graph, add a task and open:

```text
Tasks: Territory
```

Narrative discovers task **Blueprint assets**, not native C++ classes, from its configured Quest
Task Search Paths. When the Territory editor module starts, it safely adds
`/TerritoryFramework/Tales/Tasks/` to Narrative's in-memory setting. The host project does not
need a config edit, and Narrative Pro is not modified. Territory ships small Blueprint wrappers
with the category **Territory**. The wrappers contain no gameplay logic. Their C++ parent tasks
own behavior; the Blueprint metadata supplies the friendly picker name and explanation required
by Narrative's editor.

Every task also inherits Narrative's normal settings:

- **Required Quantity** — how much progress is needed;
- **Description Override** — optional player-facing text;
- **Optional** and **Hidden**;
- **Navigation Marker** — compass, minimap, world map, and screen-space objective marker.

## Territory tasks

| Narrative task | Use it when | Easy example |
|---|---|---|
| Capture or Lose Territory | One Territory must gain an owner, one exact faction must own it, or its starting owner must lose it | Capture Blacksmith for Heroes |
| Territory State / Garrison | A Territory must unlock, lock, enter a political state, lose all defenders, receive guards, or be entered/left | Unlock Castle Hill Farm after Blacksmith is claimed |
| Territory Counterattack / Chase | A durable counterattack or boss-chase record must reach an outcome | Repel the Bandit counterattack at Blacksmith |
| Territory Disguise Mission | A disguise, checkpoint, cover, or double-agent outcome must occur | Enter the Bandit camp with an accepted Bandit uniform |

Quest Cascade Recipes also generate an internal **Wait For Narrative Conditions** task whenever a
State or Branch contains conditions. This adapter exists because current Narrative Pro Quest nodes
do not evaluate their displayed Conditions array. Designers normally configure the recipe's
friendly Conditions lists instead of adding this hidden task themselves. It uses Narrative's own
condition evaluator and never creates another quest, condition, save, or multiplayer authority.

The dedicated **Tasks: Territory Story** wrapper exposes detailed boss and chase branches such as
boss defeated, target reaches the road exit, chase distance lost, final fight started, attackers
withdrawn, and assault cancelled. Community movement, GAS, combat, and AI tasks are documented in
[Community Narrative Quest Tasks](30_Community_Narrative_Tasks.md).

## State / Garrison objectives

### Unlock Territory

Completes only when the target and every authored City/District ancestor are unlocked.

```text
Objective: Unlock Territory
Target Territory: Territory.HavenReach.CastleHill.Farm
```

Player text becomes **Unlock Castle Hill Farm**. If Blacksmith's Claimed entry event runs
`Try Unlock Territory` for Farm and all Farm Locked exit conditions pass, the task completes.

### Lock Territory

Completes when the target's own availability becomes Locked. This is local availability, not a
second political state. The owner may still be Bandits or Heroes while the Place is unavailable.

### Unclaimed, Contested, and Claimed

These objectives observe the real `ATerritoryVolume` political state.

- **Unclaimed** means no stable owner controls the Territory.
- **Contested** completes once when a valid contest begins. Repeated capture-progress ticks do not
  create repeated task progress.
- **Claimed** accepts any stable owner. Use **Capture or Lose Territory** when an exact faction,
  such as Heroes, must own it.

### Defeat All Territory Defenders

Completes from the authoritative **On All Defenders Defeated** transition. A random unregistered
NPC death cannot complete it. Pending replacements must also be exhausted according to the normal
Territory defender flow.

Easy example: use this before a neutral Blacksmith owner begins the handover dialogue.

### Assign Guards to Territory

Set Narrative **Required Quantity** to the desired objective count.

```text
Objective: Assign Guards to Territory
Target Territory: Territory.HavenReach.MarketSquare.Blacksmith
Required Quantity: 3
```

The description becomes **Assign 3 guards to Blacksmith**. Progress follows the Territory's saved
Desired Guards value, so a UI click, Narrative garrison event, save restore, and replication all
read the same garrison authority.

### Enter or Leave Territory

These use the quest owner's real pawn and the Territory bounds. **Leave Territory** requires an
observed inside-to-outside transition; merely starting the task outside does not complete it.
World Partition streaming is not treated as leaving.

## Description rules

Territory tasks automatically prefer the live Territory Display Name. If the actor is streamed
out, they turn the last part of the tag into readable text. For example:

```text
Territory.HavenReach.CastleHill.Farm -> Farm
```

Use Narrative's **Description Override** only when the story needs special wording:

```text
Default:  Contest Blacksmith
Override: Create a distraction while Aisha reaches the forge
```

The override changes presentation only. It does not change the objective or its authority.

## Navigation and World Partition

Enable the inherited Narrative Navigation Marker if the task should show an objective. The task
attaches it to the live Territory actor. If that actor is streamed out, Narrative uses the authored
fallback location; when the actor registers again, the task rebinds and refreshes the marker.

## Authority and persistence

- Narrative owns the quest, task progress, task UI, tracking, and marker lifecycle.
- `ATerritoryVolume` owns owner, political state, availability, bounds, and garrison state.
- Territory subsystems own assault and disguise records.
- Tasks only observe those authorities and call Narrative `CompleteTask` or `SetProgress` on the
  authoritative quest component.
- Tasks never capture, unlock, spawn guards, change diplomacy, or award rewards by themselves.

This keeps save/load and multiplayer deterministic: the task follows the saved Territory state
instead of inventing another objective database.

## Common mistakes

- Do not select **Claimed** when the quest specifically requires Heroes. Use Capture Task with
  Required Capturing Faction = Heroes.
- Do not use Required Quantity for Contested. Contested is one state transition.
- Do not put Territory task Blueprints outside the configured task search path unless you also add
  that folder to Narrative Quest Task Search Paths.
- Do not edit task assets under `/NarrativePro`. Territory wrappers live under
  `/TerritoryFramework` and survive Narrative marketplace updates.
