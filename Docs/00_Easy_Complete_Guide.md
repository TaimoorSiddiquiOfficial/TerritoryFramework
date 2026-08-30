# Territory Framework — Easy Complete Guide

This is the first document to read.

It explains the complete Territory Framework in simple English. It is written for Blueprint
developers, level designers, quest designers, UI designers, and C++ programmers. You do not
need to know the plugin's internal code before using it.

Territory Framework extends Narrative Pro. It does not replace Narrative Pro and does not edit
Narrative Pro source files.

## 1. The whole idea in 30 seconds

A world is organized like this:

```text
City
└── District
    └── Place
```

Example:

```text
Haven Reach City
├── Market Square District
│   └── Blacksmith Place
└── Castle Hill District
    └── Farm Place
```

The **Place** is where physical gameplay happens. A Place can have bounds, guards, patrols,
capture rules, a protected story owner, production, a map marker, and counterattack approaches.

The **District** summarizes its Places. The **City** summarizes its Districts. A District or City
does not need its own physical guards or capture point.

## 2. The two questions that must never be mixed

Every Territory answers two independent questions.

### Question A: Is this content available now?

This is **Availability**:

- `Locked`: the story has not made this Territory available.
- `Unlocked`: gameplay may use it, subject to its other rules.

### Question B: Who controls it?

This is **Political State**:

- `Unclaimed`: nobody securely owns it.
- `Contested`: control is being challenged or child control is mixed.
- `Claimed`: one faction securely owns it.

Availability always comes first in player UI.

| Availability | Political State | Player-facing status |
|---|---|---|
| Locked | Unclaimed | Locked |
| Locked | Contested | Locked |
| Locked | Claimed | Locked |
| Unlocked | Unclaimed | Unclaimed |
| Unlocked | Contested | Contested |
| Unlocked | Claimed | Claimed |

A locked Place may remember an owner or an old political state. That information is useful for
the story and save game, but it must still appear as **Locked** to the player.

## 3. The Castle Hill Farm example

The project contains this setup:

- Castle Hill Farm Definition: `Initial Availability = Locked`.
- Blacksmith Definition: `Initial Availability = Unlocked` and `Initial State = Claimed`.
- Blacksmith's Claimed entry events contain **Try Unlock Territory**.
- That event targets the exact Farm tag:
  `Territory.HavenReach.CastleHill.Farm`.

Expected new-campaign flow:

1. A new campaign starts.
2. Farm is Locked and silent.
3. A real Blacksmith ownership change enters the Claimed lifecycle.
4. The unlock event checks Farm's Locked exit conditions.
5. Farm becomes Unlocked only when those conditions pass.
6. Farm then appears in player-facing Territory lists and can use its gameplay rules.

### Why Farm can look Contested after its DataAsset says Initial Locked

`Initial Availability` is a **new-campaign seed**. It is not a command that rewrites an existing
save.

If an older save already contains Farm as Unlocked/Contested, the save is the runtime truth.
Changing the DataAsset to Initial Locked does not erase player progress on the next load.

Use this rule:

- Testing a fresh setup: start a new campaign or use a clean test save.
- Changing the current story: run a Territory Lock or Unlock Narrative Event.
- Inspecting a contradiction: use the Territory debugger and compare `Runtime` with
  `New-campaign seed`.

Do not delete player saves as a normal migration strategy.

## 4. The four sources of truth

| Source | Owns | Does not own |
|---|---|---|
| Territory Definition DataAsset | Authoring plan and new-campaign defaults | Current saved control |
| Territory runtime actor | Current server state, bounds, guards, capture | Long-term strategic directory by itself |
| Territory WorldState | Save/late-join/World Partition read data | Physical capture authority |
| Territory subsystems | Validated gameplay decisions | Designer-authored identity |

Easy example:

- The Farm Definition says how Farm begins in a new campaign.
- The Farm actor is the live server object during gameplay.
- WorldState remembers Farm while it is saved or streamed out.
- Control Subsystem decides whether a capture request is legal.

## 5. Definition DataAssets

A Territory Definition is the main setup asset. Do not split policy between many Blueprint
instances.

### City Definition

A City Definition contains:

- identity, display name, and complete Territory tag;
- an array of District Definitions;
- City-level rights, settings, and presentation;
- new-campaign availability;
- State Configs for City-level story effects.

City control is calculated from its complete District array.

### District Definition

A District Definition contains:

- identity, display name, and complete Territory tag;
- an array of Place Definitions;
- District rights, command perks, and settings;
- new-campaign availability;
- State Configs for District-level story effects.

District control is calculated from its complete Place array.

### Place Definition

A Place Definition can contain:

- identity and parent link;
- initial owner, state, and availability;
- story capture or automatic capture-point setup;
- full multi-floor bounds behavior;
- guard NPC definitions;
- guard posts, patrol routes, and reserves;
- protected story-owner setup and handover behavior;
- diplomacy rules and faction-based dialogue profiles;
- economy and production profiles;
- counterattack profiles and typed approaches;
- stealth profile;
- map marker and discovery setup;
- State Config conditions, events, and command capabilities.

Use **Refresh Hierarchy Links** after editing child arrays. Use the City synchronization tool to
create or update the placed actors.

## 6. Tags and names

Use complete gameplay tags. Names in this project are examples, not hardcoded framework rules.

```text
Territory.HavenReach
Territory.HavenReach.CastleHill
Territory.HavenReach.CastleHill.Farm
```

Do not target `CastleHill` when an event should unlock only `Farm`.

The Definition arrays are the hierarchy authority. A similar tag prefix does not make an actor a
child. This prevents accidental control from unrelated actors.

## 7. How hierarchy control works

Only a Place is independently captured.

A District becomes:

- Claimed when every required Place is available, securely Claimed, and owned by the same faction;
- Contested when Place control is mixed, partial, locked, or contested;
- Unclaimed when no child has political control.

A City uses the same rule with its Districts.

Example:

- Heroes own 45% of a City's complete District set.
- Bandits own the other 55%.
- Heroes do not dominate the City.
- The City is Contested because no one owns every required District.

Capturing a District or City never pushes ownership down into its children.

## 8. Initial values, runtime values, and save values

### Initial values

Used once when no saved Territory record exists.

### Runtime values

Changed by capture, lock/unlock, diplomacy, quests, story handover, and server gameplay.

### Saved values

Restored when the campaign continues. They take priority over initial values.

This is deliberate. Otherwise a plugin update could erase a player's captured territory.

## 9. State Configs

Each Definition can configure lifecycle rows.

| Row | Entry means | Exit means |
|---|---|---|
| Unclaimed | Territory became politically unclaimed | It stopped being unclaimed |
| Contested | A real political contest began | The contest ended |
| Claimed | A faction secured it | That ownership tenure ended |
| Locked | Availability became Locked | Availability became Unlocked |

The Locked row is the authoring home for availability conditions and events. `Locked` is not a
normal political state anymore. The old enum value remains only for legacy save migration.

### Do entry events run continuously?

No. Entry events run once for a real transition.

Example:

- Claimed -> Contested: Contested Entry runs once.
- Remaining Contested for ten minutes: it does not run every tick.
- Contested -> Claimed: Contested Exit and Claimed Entry run.
- Faction A Claimed -> Faction B Claimed through a trusted handover: Claimed Exit and Claimed
  Entry run once because the ownership tenure changed.
- Rewriting the same owner and same state does not create a fake capture event.

Conditions are checked before their transition or event. If a condition fails, its configured
action does not run.

## 10. Capture modes

### Automatic capture point

Use for domination and multiplayer-style capture.

- The player enters the capture sphere.
- Server rules check faction, availability, diplomacy, defenders, and capture eligibility.
- Valid pressure fills progress.
- Ownership changes only through Control Subsystem.

An optional flag mesh can present the capture point.

### Story capture from full bounds

Use for quests and multi-floor locations.

- The complete Place volume covers the ground floor, stairs, and upper floors.
- An eligible player inside can hold the Place Contested.
- It does not automatically fill capture progress.
- Defenders must be handled by the story rules.
- A protected owner dialogue or Narrative capture event completes handover.
- Any automatic Capture Point for that Place is disabled and hidden.

This avoids a broken second-floor fight where the player leaves a small ground-floor capture
sphere.

### Recapture without the player

Counterattack forces attack the Place and its defenders without waiting for the player. If the
player is absent, the authored recapture countdown can resolve the outcome. If the player is
present, physical combat remains required. The finite assault record tracks planned, alive,
reserve, killed, and withdrawn attackers.

## 11. Defenders, guards, patrols, and the protected owner

### Guards

- Guards belong to the current Territory owner.
- A newly captured player Place can start with zero assigned guards.
- The player can set a desired garrison target from the Command Center.
- Recruitment has a one-time cost; active assignments have upkeep.
- Guard definitions can change by owning faction.

### Guard posts and patrol routes

- One Guard Post row represents one active guard slot.
- Patrol points may be outside a small Place shape when explicitly authored.
- Navigation projection and separation help prevent guards colliding and becoming stuck.
- Combat Director limits concurrent attackers and gives close valid attackers priority.

### Protected story owner

The protected owner is not the owning faction's combat guard. The owner may be neutral and ask
the winner for protection.

Typical story flow:

1. Defenders are defeated.
2. Story owner becomes available for dialogue.
3. Player accepts responsibility for the Place.
4. Owner Handover Event requests validated capture for the player's current Narrative faction.

Do not place the raw C++ class directly when the project has a configured Story Owner Blueprint.
The Definition references the project class and interaction distance.

## 12. Diplomacy and faction-changing stories

Territory uses Narrative faction tags at runtime. It must not assume the player is always
`Heroes`.

Example story:

1. The player captures Blacksmith for the Regime.
2. The Regime betrays the player.
3. The player's active Narrative faction changes to Rebels.
4. Future capture, guards, diplomacy, dialogue, and ownership checks use Rebels.
5. A different faction can later recapture the same Place.

Only War permits ordinary on-sight Territory combat. Neutral, no-treaty, peace, and allied
relationships must not behave like War. Contested State Configs can declare War using the live
owner and live contesting faction instead of hardcoded tags.

Diplomacy dialogue profiles select appropriate Narrative dialogue for the current relationship,
so a newly allied Bandit does not continue using hostile dialogue.

## 13. Counterattacks and enemy waves

A normal strategic counterattack requires an attacking faction that still holds at least one
secure District. A faction with no secure District cannot launch normal recurring counters.

A Story Pursuit is different. A quest can explicitly schedule it even when the pursuing faction
has no District, similar to a boss chase.

Counterattack flow:

1. Grace period.
2. Deterministic evaluation.
3. Player warning.
4. Wait for physical activation range when configured.
5. Spawn finite waves from valid typed approaches.
6. Attack the Place and its guards with or without the player.
7. Resolve as succeeded, defeated, cancelled, or another explicit result.

Quest rules can block or allow an assault. Example: do not counter during a rescue quest, or
allow a special pursuit only while that quest is active.

## 14. Economy and production

Territory currency uses Narrative inventory/currency authority. It does not create a second
wallet.

A production rule can:

- require Narrative item inputs;
- create Narrative item outputs;
- scale by upgrade level;
- pause while the Place or an ancestor is Locked;
- stop when the faction loses the Place;
- report Missing Input, Storage Full, Produced, Inactive, and other durable statuses.

Example:

- Farm produces Grain each campaign cycle.
- Grain is a Narrative Item class.
- The owning faction must have a registered Narrative resource inventory.
- Losing or locking Farm stops future Grain.
- Unlocking does not pay missed cycles retroactively.

## 15. Stealth

Stealth uses Narrative abilities, Ability System tags, stealth rating, AI Perception, and a
Territory Stealth Profile.

Evidence can include:

- sight;
- firing while seen;
- damage;
- gunshot sound;
- bullet impact direction;
- a discovered body;
- a seen defender kill;
- a throwable distraction.

Evidence can move the player through Undetected, Suspicious, and Exposed. A Story Config may
delay War while stealth is valid. Point-blank sight, confirmed damage, or configured strong
evidence can expose the player. Guards can investigate without immediately knowing the player's
identity.

## 16. Map, POI, and waypoint rules

- Places own physical POIs.
- Districts are strategic groups, so tracking a District resolves to a useful visible Place.
- Locked Territories stay silent and do not reveal their names or markers.
- First discovery uses Narrative navigation discovery.
- A waypoint button tracks the selected unlocked Place.
- Captured markers can use the friendly/green presentation.
- The HUD and compass use the same selected marker truth.

## 17. Command Center and HUD

### HUD

The HUD is a compact, passive card. It shows immediate local information, not the full strategic
database. A locked Place never reveals its name: the HUD falls back to the nearest unlocked
parent District or City, or stays hidden when the complete branch is locked. Narrative Menu and
Modal layers hide it while full menus are open.

### Command Center

The Command Center is the detailed Territory journal. It provides:

- Live Notifications and archived intelligence;
- Active/unlocked Territories;
- Captured Territories;
- City -> District -> visible Place hierarchy;
- hidden Place count without leaking locked names;
- Overview, Places, Garrison, Economy, Production, Threats, Diplomacy, and Command details;
- waypoint, espionage, guard staffing, reinforcement, and perk information;
- durable reports for income, attack, loss, capture, perk gain, and perk loss.

UI read models never own gameplay state. Buttons send validated requests to server authority.

## 18. Save, World Partition, and multiplayer

- Server authority owns mutations.
- Runtime state replicates to clients.
- WorldState publishes stable read summaries for late join and streamed-out actors.
- Definitions provide identities and expected child counts.
- Locked child names remain hidden; an anonymous count may still show completion requirements.
- Save restore does not replay one-shot capture rewards or state events.
- A saved contest restores its leading faction and progress for decay, not transient attacker
  actor identities.

## 19. Narrative Events supplied by the framework

Friendly purpose first; the C++ class name is included for search.

| Event | Use it for |
|---|---|
| Capture Territory (`UTerritoryCaptureEvent`) | Validated scripted capture or explicit force override |
| Owner Handover (`UTerritoryOwnerHandoverEvent`) | Capture after protected-owner dialogue |
| Lock Territory (`UTerritoryLockEvent`) | Make content unavailable now |
| Try Unlock Territory (`UTerritoryUnlockEvent`) | Unlock exact or hierarchical targets after conditions |
| Set Diplomacy (`UTerritorySetDiplomacyEvent`) | War, peace, alliance, trade, or neutral story change |
| Modify Reputation (`UTerritoryModifyReputationEvent`) | Raise or lower faction reputation |
| Schedule Enemy Wave (`UTerritoryScheduleEnemyWaveEvent`) | Finite strategic or story assault |
| Cancel Enemy Waves (`UTerritoryCancelEnemyWavesEvent`) | Cancel matching non-terminal assaults |
| Hierarchy Story Override (`UTerritoryHierarchyStoryOverrideEvent`) | Explicit story control across a hierarchy |
| Set Garrison Target (`UTerritorySetGarrisonTargetEvent`) | Script desired guard staffing |
| Upgrade Property (`UTerritoryUpgradePropertyEvent`) | Upgrade a producing Place |
| Execute Resource Recipe (`UTerritoryExecuteResourceRecipeEvent`) | Consume/produce Narrative items immediately |
| Set Stealth Override (`UTerritorySetStealthOverrideEvent`) | Temporarily allow or block infiltration rules |
| Reveal Infiltrator (`UTerritoryRevealInfiltratorEvent`) | Force confirmed exposure |
| Clear Exposure (`UTerritoryClearExposureEvent`) | Clear exposure and optionally suspicion |
| Report Distraction (`UTerritoryReportDistractionEvent`) | Send authored investigation evidence |

`UTerritoryCaptureTask` is the Narrative quest task that follows Territory capture ownership.

## 20. Narrative Conditions supplied by the framework

| Condition | Easy example |
|---|---|
| Ownership | Farm is owned by Heroes |
| Capture Eligibility | The player's current faction may capture Farm now |
| Diplomacy | Heroes and Bandits are at War |
| Garrison | Blacksmith has at least two active guards |
| Quest State | Rescue Friend is In Progress |
| Event Context | A real player pawn, controller, Tales component, or ASC exists |
| Ownership Transition | This Claimed entry came from a real owner change |
| Territory State | Blacksmith is Contested |
| Control Progress | Capture pressure is at least 75% |
| Reputation | Bandit reputation is at least 50 |
| District Holdings | Bandits still hold at least one secure District |
| Assault | Latest wave has at least three killed attackers |
| Presence | Narrative target is inside Castle Hill or a child Place |
| Production Status | Farm is Missing Input |
| Faction Resource | Heroes have at least ten Medicine items |
| Stealth Policy | This Place currently allows infiltration |
| Exposure | Player is Suspicious but not Exposed |
| Stealth Evidence | Last evidence was a Bullet Impact |
| Suspicion | Suspicion is below a quest threshold |

All events inherit Narrative Pro's event-condition support. The defender-died and
all-defenders-defeated arrays can therefore run an event only when its attached conditions pass.

## 21. Debug system

Open:

`Edit -> Project Settings -> Territory Framework -> Debug`

1. Enable **Enable Debug System (Master Gate)**.
2. Enable only the categories needed for the current problem.
3. Set verbosity.
4. Filter Output Log by `LogTerritory`.

Useful categories include Availability + Hierarchy, Registry, Capture, Ownership, Guards,
Diplomacy, Counterattacks, Economy, Production, Stealth, Save/Load, WorldState, Spatial, Markers,
UI, Interaction, Narrative/Tales, Behavior Trees, and Combat.

The master switch is a gate. It does not automatically select every category.

### Exact Farm diagnosis

Use the Blueprint-pure debugger function:

```text
Build Territory Debug Summary By Tag
Tag = Territory.HavenReach.CastleHill.Farm
```

The report shows:

- exact actor and Definition paths;
- runtime source: Definition seed, campaign save, or server replication;
- runtime Availability and Political State;
- the status the UI must display;
- new-campaign initial Availability and State;
- hierarchy path and every local lock;
- effective hierarchy availability;
- owner, contesting faction, progress, guards, and assault records;
- a note when runtime differs from the Definition seed.

`Build Territory System Debug Report` gives a sorted line for every loaded Territory.

### What intentionally stays outside the debug gate

Real Error and Warning diagnostics remain on:

- invalid or missing Definitions;
- duplicate tags or GUIDs;
- invalid hierarchy and cyclic parents;
- client attempts to mutate server state;
- save corruption or missing persistent identity;
- invalid guard/assault spawn classes;
- configuration that would silently break gameplay.

These are not optional debug noise. Hiding them could make broken community content fail silently.

## 22. Fast troubleshooting

| Problem | Check |
|---|---|
| DataAsset says Locked but HUD says Contested | Exact bound tag, runtime source, saved availability, UI status helper |
| Unlock event did nothing | Exact Place tag, target loaded/registered, Locked exit conditions, event context, inherited Not flag |
| Event ran twice | Confirm it is not authored in two arrays; inspect ownership-transition condition |
| Claimed entry did not run | Same owner/same state is not a capture; inspect transition source |
| Guard attacks a neutral player | Diplomacy must be War, correct live faction tags, Territory guard controller/definition |
| Counterattack never starts | Attacker holds a secure District, War permits it, profile/force/approach valid, quest rules pass |
| Story pursuit never starts | Use Story Pursuit launch mode or explicit quest event, not only normal recurring eligibility |
| Production creates no item | Registered Narrative resource inventory, valid Narrative Item class, cycle advanced, owner valid, Place unlocked |
| POI does not appear | Track a visible Place, confirm discovery and marker component, check Narrative navigation setup |
| Upper-floor fight loses contest | Use Story Capture From Bounds and make the volume cover every floor |
| Owner does not appear | Place Definition story-owner class/template, defender events, spawn transform, interaction distance |
| Player gets XP runtime errors | Add Territory Event Context Condition requiring a valid player ASC |

## 23. Safe setup checklist

1. Create complete gameplay tags.
2. Create Place Definitions.
3. Create District Definitions and add their Places.
4. Create a City Definition and add its Districts.
5. Refresh hierarchy links.
6. Assign project Blueprint actor classes.
7. Synchronize the City into the level.
8. Resize Place bounds, including every floor.
9. Place one Territory WorldState actor.
10. Configure Narrative factions, NPC Definitions, inventories, and UI layers.
11. Run Data Validation.
12. Start a clean test campaign for initial-state tests.
13. Use explicit Narrative Events for changes to an existing campaign.
14. Test server authority, save/load, UI, and PIE before shipping.

## 24. Where to read next

- [Quick Start](01_Quick_Start.md)
- [Definition Assets](21_Definition_Assets.md)
- [Hierarchy, Availability, and Unlocks](Hierarchy_Availability_and_Unlocks.md)
- [Debug System](11_Debug_System.md)
- [Narrative Integration](06_Narrative_Integration.md)
- [Counterattacks](17_Counterattack_System.md)
- [Production](20_Resource_Production.md)
- [Stealth](22_Stealth_Infiltration.md)
- [Operations UI](18_Operations_UI.md)

The advanced documents explain exact APIs. This guide remains the easiest source of truth for
the framework's gameplay model.
