# Community Narrative Quest Tasks

Territory Framework adds reusable tasks to Narrative Pro's normal Quest graph. These are not a
second quest system. Narrative still owns branches, task progress, save/load, multiplayer
authority, quest UI, and navigation markers.

The new Blueprint wrappers appear in five clear picker categories:

```text
Tasks: Territory Story
Tasks: Community - Movement
Tasks: Community - GAS
Tasks: Community - Combat
Tasks: Community - AI
```

Each task observes an existing gameplay authority. It never moves a Character, damages an Actor,
adds a Gameplay Effect, commands an AI, changes Territory ownership, or starts a boss encounter.
Use a Narrative Event, ability, activity, dialogue, or Territory event to perform the action. Use
the task to wait for the real result.

## Choose the right task

Narrative Pro already ships good tasks. Reuse them when they fit.

| Desired objective | Correct task |
|---|---|
| Walk or run a total distance | Narrative **Move** |
| Kill ordinary enemies | Narrative **Kill Enemy** |
| Wait for a project-specific headshot, parry, reload, or takedown event | Narrative **Wait Gameplay Event** |
| Wait for one tag to be added | Narrative **Wait Gameplay Tag Added** |
| Reach a player destination | Narrative **Go To Location** |
| Escort/follow an NPC to a destination | Narrative **Follow NPC To Location** |
| Jump, crouch, sprint, swim, climb, cover, hurdle, mantle, or vault | Community **Character Movement Action** |
| Wait for several tags, tag removal, or an attribute threshold | Community **Gameplay Tag or Attribute State** |
| Count damage, healing, hits, death, or revive | Community **Combat Progress** |
| Observe AI goals, activities, perception, vehicles, or attack tokens | Community **AI Story Observation** |
| Observe a Territory boss fight, road chase, warning, withdrawal, or cancellation | **Territory Boss Fight or Chase Outcome** |

## Territory story: boss fight and chase

First run **Start Territory Boss Chase** from the Narrative branch. Then add one or more Territory
story tasks with the same Territory, attacking faction, and Scenario ID. The event creates the
physical encounter; the task only observes its durable assault record.

### One underboss

```text
Start Territory Boss Chase
  Territory: Territory.HavenReach.MarketSquare.Blacksmith
  Attacking Faction: Narrative.Factions.Bandits
  Scenario ID: Blacksmith_Underboss
  Planned Force Override: 1

Territory Boss Fight or Chase Outcome
  Objective: Defeat Story Boss
  same Territory, Faction, and Scenario ID
  Required Quantity: 1
```

The boss objective completes only for an explicit finite **Story Pursuit** that ends with all
attackers removed. An unrelated strategic counterattack cannot complete it.

### Branch a vehicle chase cleanly

Use separate Narrative branches for the outcomes you care about:

- **Chase Target Starts Final Fight** — the damaged or blocked target abandons the vehicle.
- **Chase Target Reaches Exit** — the target reaches the authored Road Guide exit.
- **Player Loses Chase Distance** — every participating player stays outside the configured range
  for the full grace period.
- **Boss Fight or Chase Resolved** — any terminal Story Pursuit result; useful for cleanup.

The exit and distance-loss objectives are intentionally different. This lets dialogue explain
what really happened instead of showing the same generic “escaped” result.

Other story objectives include warning issued, assault activated, recapture countdown started,
attackers withdrawn, Territory taken, attack repelled, and assault cancelled. All are read from
the saved `FTerritoryAssaultRecord`.

## Movement and traversal

**Character Movement Action** has 19 objectives:

- jump, jump apex, land;
- crouch and stand;
- start/stop sprint and slow walk;
- start/stop swimming, falling, and climbing;
- enter/exit cover;
- hurdle, mantle, and vault.

Easy tutorial:

```text
Task 1: Character Movement Action / Crouch
Task 2: Character Movement Action / Enter Cover
Task 3: Character Movement Action / Vault
```

Leave **Subject Provider** empty to watch the quest player. Use Narrative **Find NPC** or another
Actor Provider to watch an escort. **Count Initial State** is off by default, so a “Crouch” task
normally requires a new standing-to-crouched transition. Stop/exit objectives always require a
real earlier active state; merely starting while standing cannot satisfy “Stop Sprinting.” Jump
Apex requires the Character Movement Component's **Notify Apex** setting.

## Gameplay Tags and attributes

**Gameplay Tag or Attribute State** supports:

- all required tags present;
- any required tag present;
- all required tags absent;
- attribute at least a threshold;
- attribute at most a threshold.

Easy rescue example:

```text
Objective: Gameplay Tags Are Removed
Subject Provider: Find NPC / Prisoner
Required Tags: State.Restrained
```

Easy survival example:

```text
Objective: Gameplay Attribute Reaches Minimum
Attribute: Health
Threshold: 50
```

The task binds to the subject's real Ability System Component. It never adds/removes a tag or
changes an attribute. Turn off **Complete If Already Satisfied** when a new transition must happen
after the task begins.

## Combat progress

**Combat Progress** supports deal damage, take damage, receive healing, landed-hit count,
survived-hit count, death, and revive.

Examples:

```text
Deal 500 fire damage to the boss
  Subject: quest player
  Counterparty: Find NPC / Boss
  Objective: Deal Damage
  Required Quantity: 500
  Required Effect Tags: Damage.Fire

Heal the prisoner for 100
  Subject: Find NPC / Prisoner
  Counterparty: quest player
  Objective: Receive Healing
  Required Quantity: 100
```

Damage and healing come from Narrative ASC delegates. Positive magnitudes are rounded to integer
Narrative progress; a small positive event still counts as one. Effect tags and the counterparty
are optional filters. Use Narrative's built-in Kill Enemy or Wait Gameplay Event for ordinary
kills, weapon reloads, headshots, parries, blocks, and project-specific combat verbs.

## AI story observation

**AI Story Observation** supports:

- Actor available, NPC alive, NPC dead;
- AI reaches the quest player, another Actor, or a location;
- Narrative AI receives a Goal class or runs an Activity class;
- AI Perception detects the quest player or loses them after first detecting them;
- Narrative NPC enters/leaves a vehicle;
- Narrative AI claims/releases an attack token against the quest player.

Easy stealth escape:

```text
Target Provider: Find NPC / Jail Guard
Objective: AI Loses Quest Player
```

This cannot complete merely because the guard starts unaware or streams out. The task must first
observe a successful stimulus, then observe every stimulus become unsuccessful. It checks for a
valid controller and perception component before reading either, avoiding pending-kill Blueprint
runtime errors.

Easy chase staging:

```text
1. AI Story Observation / AI Enters Vehicle
2. Territory Boss Fight or Chase Outcome / Boss Fight or Chase Started
3. AI Story Observation / AI Leaves Vehicle
4. Territory Boss Fight or Chase Outcome / Defeat Story Boss
```

Vehicle exit, lost sight, and token release are transition objectives. They must observe the
positive state first, so “not in a vehicle” or “no token” at quest start cannot complete them.

## Actor Providers and World Partition

Use Narrative Actor Providers instead of saving a live Actor pointer. **Find NPC** is the normal
choice for named story characters. Tasks re-resolve providers when the actor becomes available.
An invalid or streamed-out actor is never treated as dead, escaped, out of a vehicle, or no longer
perceived.

## Multiplayer and save/load

- Narrative tasks evaluate on the server.
- GAS, AI, Territory, and assault systems remain their own authorities.
- Narrative saves task progress. Territory saves durable assault outcomes.
- Live delegates and provider bindings are rebuilt when the task becomes active after load.
- Tasks contain no saved live UObject pointers and do not replicate a second state database.

This is why the same quest graph works for story mode and multiplayer without letting a client
award its own combat, movement, AI, or Territory progress.
