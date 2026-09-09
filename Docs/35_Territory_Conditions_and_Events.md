# Territory conditions and events

Conditions answer a question. Events perform an action. Both use the pawn, controller and Tales component supplied by Narrative Pro. They do not choose the first player in the world.

## Check whether a place is locked

Add **Territory State Condition** and select the complete Place tag.

| Query | Meaning |
|---|---|
| Political State | Claimed, Contested or Unclaimed. A locked place can still be Claimed. |
| Local Lock State | The selected place itself is Locked or Unlocked. |
| Territory State Is Known | The condition can read this territory's data. |
| Territory Actor Is Loaded | The real actor is loaded and registered in this world. |

For a new lock check, choose **Local Lock State → Locked**. For capture readiness through City and District locks, use **Territory Situation → Target Available** or **Territory Capture Eligibility**.

Old assets that select **Political State → Locked (Legacy)** are supported. They now read the separate runtime lock field. They previously compared against a legacy political enum value, so a correctly locked Claimed place failed the condition.

**Allow Unloaded Territory** reads the existing campaign directory when World Partition unloads the actor. It does not load any cells. It is off by default to preserve existing loaded-only rules. Unknown tags and missing directory entries fail. A directory row never satisfies the Actor Is Loaded query.

Narrative applies **Not** after the raw result. This means `Not Claimed` also passes when a loaded-only check cannot find its actor. When missing data must block a branch, require **State Is Known AND Not Claimed**, with the same tag and unloaded-data option on both rows.

## Mix requirements with All and Any

Use **Territory Condition Group (All / Any)**. Each row can contain any existing Narrative condition, including another group.

Example for the Blacksmith offer:

1. Add an **All** group to the dialogue node.
2. Add **Territory State → Local Lock State → Locked**.
3. Add **Narrative Quest State → Not Started** for the capture quest.
4. Add an **Any** group containing the desired reputation condition and an alternative faction-power condition.

Every All row must pass. One Any row is enough. A child's Not checkbox reverses that child's result. The group's Not checkbox reverses the complete group. Empty groups, empty rows and circular groups fail their raw check; do not invert an invalid group to make it pass. The validator reports invalid group structure.

Group configuration is stored in the quest, dialogue or Definition asset. Live state remains in Narrative or the existing Territory authority. Conditions are evaluated again when Narrative checks the node; this is not a timer or an automatic dialogue refresh.

## Faction changes and reputation

**Territory Faction Reputation Condition** and **Modify Territory Faction Reputation** share these sources:

| Source | Reads |
|---|---|
| Explicit Faction | The fixed authored tag. Existing assets keep this behavior. |
| Narrative Target / Player Faction | The exact target's current primary Narrative faction. |
| Player Controller Pawn Faction | The supplied controller's currently possessed pawn. |

If the player leaves Heroes and joins another faction, the two dynamic sources follow the new faction. They do not fall back to a stale explicit faction when character context is missing. Reputation is campaign metadata for that faction; it is not a personal relationship between two characters and does not automatically change AI attitude.

For rich relationships use **Territory Diplomacy Condition** and **Set Territory Diplomacy**. For identity use **Set Narrative Player Factions**. For perceived identity use the disguise events. For faction Place counts, history, dominance and relationships with an owner, reuse **Territory Situation Profile** and its optional exact faction override.

## Match the correct enemy wave

Give a story wave a stable **Scenario ID**, for example `BlacksmithRetake`. Use that same ID on **Enemy Wave / Assault Condition** and **Cancel Territory Enemy Waves**. Faction and Scenario ID filters must both match when both are set. Empty filters retain the existing broad selection.

Cancellation normally affects only preparing, warning or waiting forces. **Include Physically Active Assaults** also withdraws living attackers through the existing counterattack subsystem. Completed records are not cancelled again. This filter does not change launch rules, finite force budgets, diplomacy, routes, casualties or capture authority.

An assault query with no matching record fails, even for `Remaining Attackers = 0`. For victory, prefer **Latest Resolution = Attacking Force Defeated**, with the exact story ID. Latest queries prioritize an unfinished matching record over an old completed result. A warning alone is not a physically active battle.

## Guard and capture requirements

- **Active Guards** counts Territory guard pawns.
- **Living Defenders** counts all registered living defenders, including project actors.
- **Pending Reserve Deployments** counts replacements already committed but not spawned.
- **Remaining Reserve** counts the finite undeployed reserve.

Use Living Defenders and Pending Reserve Deployments when checking whether physical defence is cleared. For final handover also use Capture Eligibility, which retains normal capture and state rules. Client reserve conditions read the existing replicated garrison snapshot; they do not sum server-only guard posts.

Control Progress is expressed as a percentage. A Claimed place can have full control without an ongoing battle. Add a Contested state check if the line is only about an active capture.

## Where conditions run

| Family | Data source and important boundary |
|---|---|
| State, ownership, progress, garrison | Registered Territory actor and its replicated state. State can optionally use the campaign directory. |
| Presence | Loaded territory bounds at the explicit target position. |
| Quest | The supplied Narrative Tales component. Does not start a quest. |
| Wait Time | Saved Narrative elapsed campaign time or current world time. Does not sleep after entering a node. |
| Event Context | Independent checks for a target, player control, Ability System, controller and Tales component. |
| Ownership Transition | Only the containing Territory's synchronous owner-change callback. Ordinary dialogue outside that callback fails. |
| Diplomacy and reputation | Existing diplomacy subsystem, restored from the replicated world view on clients. |
| District holdings | Campaign directory. Only complete, unlocked Claimed Districts count. |
| Situation | Campaign directory for Place history/holdings; current defence power requires relevant loaded server actors. |
| Assault | Durable finite records from the counterattack subsystem and its replicated view. |
| Production and resources | Existing economy production records and faction storage snapshots. Missing records/accounts fail. |
| Stealth policy, exposure, evidence, suspicion | Server infiltration state. These are not general client-side security queries. |
| Disguise | Existing disguise system. Place security checks require a loaded place and server rules. |
| Condition Group | Evaluates existing child conditions; owns no gameplay state. |

## Event behavior

Territory events evaluate their inherited Conditions before their action. Mutations remain server-authoritative. Their default **Refire On Load** is off to avoid replaying rewards, purchases or waves. **Event Runtime = Both** can execute an action twice, once at node start and once at node end; use Start or End deliberately.

| Event family | Existing authority used |
|---|---|
| Capture and hierarchy owner overrides | Territory Control atomic mutation for each Place; parent owners come from the hierarchy reducer. A whole-city override is not one atomic transaction. Only loaded descendants are changed. |
| Lock / unlock | Territory availability and Control's unlock cascade. Definition defaults are not edited. |
| Owner handover | Existing loaded owner spawner. Revealing the owner does not itself transfer ownership. |
| Diplomacy / reputation | Territory Diplomacy and the Narrative GameState attitude bridge. |
| Player faction identity | Narrative PlayerState public faction API. |
| Wave / boss pursuit / cancellation | Counterattack subsystem and finite durable assault records. |
| Guard target | Existing guard assignment and Narrative currency transaction. |
| Property upgrade | Existing Property upgrade admission and payment. |
| Resource recipe | Existing economy transaction and Narrative inventory. |
| Checkpoint | Native save subsystem on the next tick, after the current transition settles. |
| Stealth and disguise | Existing infiltration/disguise authority. No parallel faction or combat system. |

Narrative Pro's generic event dispatcher does not enforce every native event's inherited Conditions array. For native events such as Begin Quest, put the gate on the dialogue/quest node unless that event implements its own condition check. Territory events have their own adapter for this. No Narrative Pro source was changed.

Narrative's current party node evaluator can return after a party condition and skip later rows. A Territory All group with its default party policy of None evaluates its child requirements individually through the existing adapter. The adapter also handles Any Player Passes with no passing members and an absent party leader. Avoid assuming these fixes change unrelated native dialogue nodes outside the adapter.

## Migration and validation

No saved gameplay records, territory GUIDs, replication fields or existing enum values were renamed. Old State/Locked selections now behave as their author intended. New state-query fields default to the previous loaded-only political check. Reputation defaults remain Explicit. The cancellation Scenario ID defaults to empty.

The audit covers every current Territory condition/event class. Picker tooltips explain usage and boundaries; field help is checked by automation. Runtime tests cover lock/unlock, Native node evaluation and Not, saved lock data, streamed actor absence, client reserve snapshots, nested groups, callback mutation, current faction reputation and scenario cancellation filters. Live multiplayer evidence and build results are recorded in the audit verification folder, not inferred from reflection tests alone.
