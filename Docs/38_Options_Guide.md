# 38 — Options Guide

**Plain English, with a worked example for each option, for the options you will actually touch.**

---

## What this file is, and what file 37 is

There are two option documents in this plugin because they answer two different questions.

| | **File 37 — Every Option Reference** | **File 38 — this file** |
|---|---|---|
| Answers | *"What is this option in the Details panel?"* | *"What will it do to my game?"* |
| Covers | **all 787 designer options** | the ones a developer touches first |
| Written by | **generated from the C++** | hand, from the code that consumes each option |
| Guarantee | cannot be incomplete or invent a name | a test proves it cites no option and no default that C++ disagrees with |

Read 37 when you are staring at a checkbox and want to know its type and default. Read this file when
you want to know what to set and why. This file does not repeat 37's tables — it points into them.

### The honest state of the plugin's own documentation

Of those 787 options, **398 (51%) have no description written in the source.** File 37 prints
`no description in source` for every one rather than inventing a sentence, and this file explains the
important ones by reading the code that uses them. So for the options below, the explanation here is
not a copy of a tooltip — it is derived from behaviour, and where a tooltip and the code disagree,
**this file states what the code does and says so.**

### How to read this file

Every option appears as a bullet:

> - OptionName — what it does, in one sentence. **Example:** what happens on Castle Hill Farm.

- Code in monospace is always a real name from the C++ — an option, a class, or a type. A test
  enforces this, so a mistyped option name cannot survive in this file.
- **Bold** is a *value*: an enum choice like **Claimed**, or an asset name like **DA_CounterAttack**.
- Section headings are the class the options live on, so every bullet is answerable to exactly one class.
  Four sections — **Stealth and disguise**, **Economy**, **Diplomacy, attitude and betrayal** and
  **Saving and persistence** — are *concept* sections that group the classes working together; their
  bullets still sit under a class heading inside, and a concept section on its own makes no option
  claims. Find that class in file 37 for the full row.

The running example is **Castle Hill Farm**: a single Place, owned by the player's faction
**Faction.Heroes** after they captured it, with one guard assigned to it. It is the smallest thing
that exercises every system here.

---

## Start here: the ten options that decide whether your first Place works

If you set nothing else, set these. Every one of them makes the difference between "it works" and
"nothing happens and I cannot see why".

| Option | On | Why it matters | Section |
|---|---|---|---|
| `TerritoryTag` | the stable ID | Leave it empty and the Place is invisible to every lookup, quest and condition | [Territory](#uterritorydefinition--the-place-itself) |
| `InitialOwningFaction` | who starts owning it | Empty means the Place starts unowned and capture is the only way in | [Territory](#uterritorydefinition--the-place-itself) |
| `FactionForces` | one row per attacking faction | **Empty means no counterattack can ever be scheduled.** The single most common "nothing happens" | [Counterattacks](#fterritoryfactionassaultconfig--who-attacks-and-with-what) |
| `CounterAttackProfile` | a profile asset | Null means no scheduled counterattacks at all | [Counterattacks](#uterritorycounterattackprofile--how-the-enemy-decides) |
| `DefaultGuardDefinition` | an NPC definition | Empty and with no faction row, no guard spawns — so the Place has nothing to defend it | [Guards](#fterritoryfactionguarddefinition--which-npc-guards-which-owner) |
| `FactionGuardDefinitions` | a row for *the player's* faction | Guards are resolved from the **current owner**. A player-faction row is what makes a captured Place staffable | [Guards](#fterritoryfactionguarddefinition--which-npc-guards-which-owner) |
| `bGarrisonTriggersActivation` | on, if you also require proximity | Off, a counterattack force waits for a *player* and ignores the garrison. This is the reported bug | [Counterattacks](#uterritorycounterattackprofile--how-the-enemy-decides) |
| `bEngageAtWarInClaimedTerritory` | on, to defend fresh captures | Off (default), a guard will not fight a faction it is at war with while the Place is merely **Claimed** | [Guards](#fterritoryguardbehaviortemplate--how-a-guard-thinks) |
| `StateConfigs` | four rows, authored | This is where income, events, capability grants and stealth per state live | [States](#fterritorystategameplayrules--what-each-state-does) |
| `EconomyTickIntervalSeconds` | your cycle length | Real seconds between income ticks. Read **once** at startup, so changing it needs a new world | [Economy](#economy) |

---

## `UTerritoryDefinition` — the Place itself

The data asset that describes one Place. 36 designer options. The editor can spawn and sync the
matching actors in the level from this asset.

### Identity and ownership

- `TerritoryTag` — the stable ID every lookup uses; the registry indexes territories by it and
  rejects an invalid one. **Example:** set **Territory.Place.Farm** and quest conditions, map markers
  and `GetTerritoryByTag` can all find Castle Hill Farm; leave it empty and none of them can.
- `DisplayName` — the name shown to the player. Cosmetic only; identity stays with the tag.
  **Example:** "Castle Hill Farm" here, while the tag stays machine-readable.
- `InitialOwningFaction` — who owns the Place in a **brand-new campaign only**; a loaded save
  overrides it. **Example:** set **Faction.Bandits** so the farm starts hostile, and the player has
  something to capture; this has no effect on a save you already made.
- `InitialAvailability` — whether the Place can be captured at all in a new campaign. **Locked**
  makes capture return a Locked result, empties its command capabilities and skips its income.
  **Example:** **Locked** on a late-game island so it cannot be taken early.
- `InitialState` — the starting political state. **Automatic**, **Locked** and **Claimed** all resolve
  to **Claimed** when a faction is set, and **Unclaimed** when it is not. **Locked** additionally
  forces availability to Locked. **Example:** leave it on **Automatic** and let
  `InitialOwningFaction` decide.
- `RelativeTransform` — editor-sync placement only; no runtime code reads it. **Example:** it decides
  where the sync command puts the Place actor, and moving that actor afterwards does not write back.
- `TerritoryActorClass` — the Blueprint the editor sync spawns for this Place. **Example:** point it
  at your farm Blueprint or the sync command reports "no compatible Blueprint class" and creates
  nothing.

### Guarding

- `DefaultGuardDefinition` — the fallback guard NPC, used when no faction row matches the owner.
  **Example:** set it to your **BP_FarmGuard** so a Place captured by a faction you never planned for
  still gets a guard.
- `FactionGuardDefinitions` — per-owner-faction guard NPCs, checked before the fallback. Guards are
  resolved from the **current owner**, so a Place you capture uses the *player's* row.
  **Example:** **Faction.Bandits** gets ragged bandits and **Faction.Heroes** gets soldiers — the same
  farm, different garrison depending on who holds it.
- `InitialGuardCount` — how many guards a new campaign staffs, clamped between 0 and the maximum your
  guard posts allow. **Example:** 3 on Castle Hill Farm, with 4 guard posts authored so you can raise
  it later.
- `PostCaptureGarrisonPolicy` — how many defenders the **new** owner gets the moment a capture
  succeeds. **AlwaysUnstaffed** gives none, **ConfiguredForEveryOwner** gives the configured count,
  and **PlayerChooses** gives the configured count but zero when a live player already owns the new
  faction. **Example:** **PlayerChooses** means a player who just captured the farm is offered the
  garrison rather than handed it.
- `GuardBehavior` — the shared behaviour every guard from this Place gets as it spawns. Read straight
  off this asset per guard, so **changing it affects guards spawned afterwards, not the ones already
  alive**. **Example:** see the Guard Behavior section below.
- `GuardPosts` — the authored physical posts. Each row becomes one guard slot, counted even while its
  actor is unloaded. **Example:** 4 rows on the farm = a hard ceiling of 4 guards.
- `GuardQuality` — a relative quality multiplier in the defence estimate. **Example:** 1.5 for elite
  guards makes the farm look harder to the attacker's maths and lowers the chance a counterattack is
  launched.
- `FortificationStrength` — a flat bonus to defence power, which also lowers the attack chance.
  **Example:** 20 on a walled farm.
- `NearbyAlliedSupport` — a flat defence bonus for allied help. **Example:** 10 while an ally holds the
  neighbouring Place.
- `StrategicValue` — how valuable a target this is. Aggregated across a defence front by **maximum**,
  not sum, so it describes importance rather than counting Places. **Example:** 3.0 on Castle Hill Farm
  if the story needs it defended, so the AI prioritises attacking it.

### Capture mechanics

- `CapturePoint` — the optional physical flag or hold-zone the player stands in. **Example:** enable it
  with a radius for a farm the player must stand inside to take.
- `bStoryCaptureFromBounds` — story capture over the whole volume: any player inside the bounds counts
  as a contester. **Example:** on for a farm you take simply by walking its fields, with no flag. It is
  only allowed on an independent Place, and enabling it clears the capture point's own automatic
  capture.
- `MaxConcurrentAttackers` — how many attackers may hold a slot at once; a Place further capped by
  Narrative's per-difficulty attack tokens. Ignored on a City or District, which normalise to 1.
  **Example:** 3 lets a small farm be attacked by three at a time while six attackers wait their turn.

### Money

- `PeriodicIncome` — base currency each economy cycle. **Only Places earn.** **Example:** 250 for a rich
  farm, 40 for a poor one.
- `GuardUpkeepPerCycle` — currency per guard per cycle, so the total is this times the guard count.
  **Example:** 50 with 3 guards costs 150 a cycle, which is why an over-staffed poor farm loses money.
- `GuardRecruitmentCost` — base cost per guard recruited, charged once when you raise the target.
  **Example:** 50 means raising a garrison from 1 to 4 costs 150 plus any attitude pricing.
- `bAttitudeAffectsPrices` — off, everyone pays the base cost. On, the price is scaled by the buyer's
  diplomacy relation with the owner. **Example:** on, so a faction you are at war with pays more to
  recruit in your Place — if they somehow can.
- `FriendlyPriceMultiplier` — applied when the relation is **Alliance** or **TradeAgreement**.
  **Example:** 0.85 for a 15% discount to allies.
- `NeutralPriceMultiplier` — the default branch, covering **Neutral**, **NonAggression** and
  **Ceasefire**. **Example:** leave it at 1.0 so only friendship and war move the price.
- `WarPriceMultiplier` — applied when the relation is **War**. **Example:** 1.5 so enemies pay half as
  much again.

### Story hooks

- `StateConfigs` — the per-state rule table (see the States section). Guaranteed to have four rows:
  **Locked**, **Unclaimed**, **Contested** and **Claimed**. **Example:** give the **Claimed** row
  income and the **Contested** row none, so a farm under attack stops paying.
- `QuestRuntimeOverrides` — while a listed quest is active for any player, it suspends one primary
  runtime rule (automatic capture, counterattacks, state rules, or defender combat). **Example:** a
  tutorial quest that switches off counterattacks so the player is not ambushed while learning. Nothing
  is replayed when the quest ends.
- `DefenderDiedEvents` — Narrative events run when a registered defender dies. **Example:** a line of
  dialogue the first time a farm guard falls.
- `AllDefendersDefeatedEvents` — run once when the **last** defender is defeated. **Example:** reveal
  the story owner of the farm at the moment its garrison breaks.
- `DefaultStealthProfile` — the pre-conflict stealth policy used when the state row sets no override.
  **Empty means infiltration is disabled**, so entering the bounds starts a **Contested** state
  immediately. **Example:** a rescue-mission profile lets the player cross a **Claimed** enemy farm
  while undetected.
- `ManagementPoint` — the optional management interaction and its widget. **Example:** enabled on a
  district so the player opens the Command Center from the town square.
- `bShowGameplayHUD` — off, the passive capture card collapses while the player is inside this exact
  Place. Nothing else is hidden. City definitions default to off because they are broad ambient
  regions. **Example:** off on a huge countryside region so the card is not permanently on screen.

---

## `FTerritoryStateGameplayRules` — what each state does

The four rows of `StateConfigs`, one per state. This is where most "why did nothing happen when the
state changed" answers live.

- `CounterAttackPolicy` — the gate that decides whether a counterattack may be scheduled at all.
  **CaptureTriggered** and **WhileAtWar** allow automatic scheduling, **QuestOnly** allows only an
  explicit narrative request, and **Disabled** rejects both. **Example:** **Disabled** on a
  tutorial farm; the log then reads "Check Counterattack Policy and Allowed Attacking Factions".
- `AllowedAttackingFactions` — an exact-tag allow-list. Empty permits any eligible hostile faction.
  Matching is exact, so a parent tag does not admit its children. **Example:** list only
  **Faction.Bandits** so no other enemy may counterattack Castle Hill Farm.
- `bAllowPeriodicIncome` — off, this Place contributes no income for its owner. Guard upkeep is **not**
  gated by it, so a blocked Place can still cost money. **Example:** off on the **Contested** row, so
  being attacked stops the income but not the wage bill.
- `bAllowResourceProduction` — off, production rules report Inactive and the cycle is advanced anyway,
  so blocked cycles are never paid later. **Example:** off while a farm is contested, and the player
  loses that output rather than banking it.
- `bAllowCapitalCaptureReward` — off, the authored capital capture reward is skipped for both the City
  and its District. Only meaningful on a capital. **Example:** off during a story chapter where taking
  the capital must not pay out yet.
- `GrantedCommandCapabilities` — capabilities this state grants its owner, read live rather than saved,
  so they vanish the moment the state is lost. **Example:** add
  **Territory.Capability.GuardStaffing** to a district's **Claimed** row — without it the player cannot
  raise a garrison target at all.
- `EntryConditions` — every condition must pass before a transition **into** this state commits; empty
  passes. **Example:** require a quest flag before a farm may become **Claimed**.
- `ExitConditions` — every condition must pass before the current state may end. **Example:** keep a
  Place **Locked** until a story beat releases it.
- `EntryEvents` — narrative events run once on entering the state. **Example:** declare war, or grant
  reputation, at the moment the farm flips.
- `ExitEvents` — run after the state ends, and they select the **outgoing** owner's row.
  **Example:** clean up the previous owner's alliance when you take their farm.

---

## `FTerritoryGuardBehaviorTemplate` — how a guard thinks

Lives on the Place definition as `GuardBehavior`. A guard copies most of it at spawn, but **the combat
flags are re-read live from the Place's definition on every target query**, so they always reflect
whoever owns the Place right now.

- `PatrolGoalClass` — the Narrative goal created for a guard that has a route of two or more nodes.
  **Empty means no goal and no patrolling.** **Example:** leave the default; only change it on a farm
  whose guards need custom patrol behaviour.
- `bEnablePatrolCrowdAvoidance` — turns server-side steering around other agents on or off.
  **Example:** a crowded town gate where three guards jam — leave it on and lower the weight below.
- `PatrolAvoidanceConsiderationRadius` — how far away a guard starts steering around others.
  **Example:** 500 for a farm field; raise it in a tight courtyard so guards move aside earlier.
- `PatrolAvoidanceWeight` — how much of the avoiding *this* guard does, 0 to 1. **Example:** 0.3 on a
  guard who should hold his line and let others walk around him.
- `bPrioritizeClosestHostilePlayer` — enables the closest-player scoring pass, and it **only runs while
  the Place is Contested**. Outside a contest it changes nothing. **Example:** on, so a guard in a
  contested square spreads its goals onto the nearest fightable player.
- `bAllowPersonalRetaliation` — on, a guard may attack a target Narrative marks personally hostile to
  it, such as whoever just shot it, with **no war and no contest needed**. **Example:** on, so a guard
  who is sniped from a hedgerow fights back.
- `bDefendAgainstExposedEnemies` — only used when stealth infiltration is on and the target is a
  player: on, a guard may attack a player who is exposed and at war. **Example:** on, so being caught
  sneaking into the farm is dangerous.
- `bEngageAtWarInClaimedTerritory` — **the option behind a reported bug.** Off (the default), a guard
  will not fight a faction it is at war with while the Place is only **Claimed** and not contested —
  the guard's answer is "The Place is not contested and there is no confirmed local threat".
  **Example:** the player captures Castle Hill Farm and assigns a guard; a bandit counterattack force
  arrives and walks past that guard. Turn this **on** so the assigned guard actually meets them.
- `CombatTargetFactions` — an exact-faction allow-list. Non-empty, a target must match exactly or the
  guard refuses before any other rule runs. Empty allows every faction that passes diplomacy, stealth
  and quest rules. It tests the **perceived** faction, so an accepted disguise changes the answer.
  **Example:** empty on a farm that must fight any invader; set to one tag on a post that must ignore
  other wars.
- `ClosestHostilePlayerGoalScoreBonus` — score added to the existing attack goal that targets the
  closest engageable player. It only re-prioritises goals that already exist; it never creates one and
  never makes a friendly player hostile. **Example:** 0.75 keeps guards focused on the player in front
  of them.
- `DialogueProfile` — the fallback relationship dialogue for the guard. **Example:** a generic
  "move along" set used by every guard in the region.
- `FactionDialogueProfiles` — per-owning-faction dialogue, checked before the fallback. **Example:** one
  guard Blueprint reused by two towns, each faction getting its own surrender lines.

**A trap worth knowing:** a guard's faction decides who it can fight, and it refuses targets sharing
its own faction. Both `FTerritoryGuardPostTemplate.FactionOverride` and
`UTerritoryGuardPostDefinition.FactionOverride` **pin a guard's faction** instead of following the
current owner. Leave a stale override naming the *previous* owner on a Place the player captured and
you get a guard that cannot be at war with the attacker — and that the attackers do not count as a
defender, so they ignore it. **Example:** after the player's faction takes the farm, clear that
override on the post so the guard it assigns belongs to the new owner.

---

## `FTerritoryGuardPostTemplate` — where guards stand

One row per guard post, in the Place's `GuardPosts` array. The editor sync copies every field onto the
placed actor, and **at runtime the actor is what is read, never this row**.

- `GuardPostID` — the key that ties a placed actor to this row. **Renaming it detaches the placed post**,
  which then keeps whatever it last copied. **Example:** **FarmGate_01**; rename it and the actor in the
  level no longer belongs to any row.
- `ActorClass` — the Blueprint spawned when the editor creates this post. Never read at runtime.
  **Example:** your **BP_TerritoryGuardSpawnPoint**.
- `RelativeTransform` — where the post sits relative to the Place actor; editor-only. **Example:** the
  transform that places the post at the farm gate.
- `GuardPostDefinition` — a reusable data asset supplying NPC, activity, trigger sets, patrol route and
  reserve values. **Every "effective" value prefers this asset over the row's own field.**
  **Example:** one **DA_FarmGuardPost** shared by every farm, with per-post overrides where a farm
  differs.
- `NPCDefinitionOverride` — this post's NPC type, above the definition asset and above the territory's
  faction-resolved NPC. **Example:** a single officer post on the farm that spawns a tougher NPC.
- `ActivityConfigurationOverride` — this post's Narrative activity configuration, above the definition
  asset. **Example:** idle-at-post behaviour overridden for a gate sentry.
- `TriggerSetOverrides` — Narrative trigger sets applied to guards from this post. **Example:** a
  "gate guard" trigger set that adds a challenge line when the player approaches.
- `FactionOverride` — pins the spawned guard's faction instead of the owner's. See the trap above:
  this is the field most likely to leave a captured Place's guard unable to fight. **Example:** clear
  it so the guard belongs to whoever currently owns the Place.
- `Priority` — fill order. Guards deploy from the highest-priority free post downwards, ties broken by
  having a patrol route. **Example:** 100 on the front gate, 10 on the back field, so the gate is always
  manned first.
- `ReserveSlots` — fallback reserve count; the definition asset wins when set. Reserves are consumed
  one per successful replacement after a guard is killed. **Example:** 1 means each post can replace its
  guard once.
- `bAutoSpawnReserves` — whether this post schedules its own reserve deployment. Off leaves a queued
  reserve pending until something else calls spawn. **Example:** off on a post whose relief should
  arrive only as part of a story beat.
- `ReserveSpawnDelay` — seconds before a queued reserve tries to deploy; a high-influence owner can
  shorten it while the Place is contested. **Example:** 3 is a brisk relief; 15 makes losses bite.
- `ReserveSpawnRetryInterval` — seconds between failed placement attempts. **Example:** 2 retries
  quickly when the spawn point is briefly blocked.
- `ReserveSpawnRadius` — search radius for a reserve spawn point, and it is **only used when the
  deployment must be out of sight of player cameras**. **Example:** 600 keeps reliefs appearing behind
  the barn rather than in front of the player.
- `ReserveMinimumPlayerDistance` — minimum distance between a reserve and every player camera, on the
  same camera-avoidance path. **Example:** 500 so reinforcements never pop in beside the player.
- `ReserveSpawnCandidateCount` — navigation candidates sampled per attempt. **Example:** 12; raise it
  on cluttered geometry where good spots are hard to find.
- `ReserveCameraAvoidanceRetryLimit` — failures allowed while still requiring concealment; after this
  many, the next attempt **drops the concealment requirement** and uses the authored post location.
  **Example:** 3, so a cornered garrison still gets its relief rather than failing forever.
- `ReserveTotalRetryLimit` — failed attempts before the deployment is abandoned and the defender defeat
  completes **without** consuming reserves. **Example:** 10.
- `ReserveOwnershipPolicy` — what happens to reserves on an ownership change. **PersistWithPost** keeps
  the remaining count, **ResetToDefinitionOnOwnerChange** resets to the definition asset's value, and
  **RefillOnOwnerChange** resets to this row's own value. All three clear pending spawns.
  **Example:** **RefillOnOwnerChange** so a freshly captured farm's new owner gets full reserves.
- `PatrolRoute` — this post's route. Node transforms are **relative** to the post actor.
  **Example:** four stops around the farm's perimeter.
- `bLoopPatrol` — the loop flag for this post's route. **Example:** on for a perimeter circuit; off for
  a route that should be walked once.

---

## `UTerritoryGuardPostDefinition` — reusable post values

A data asset holding the same values so many posts can share them. Each field **beats the Place row's
own value when its guard condition is met** — and the conditions differ per field, which is a real
source of confusion. The pattern to remember: for the reserve numbers the asset wins when the value is
**greater than zero**, except `ReserveMinimumPlayerDistance`, where **zero does override**, because it
means "let reserves spawn right next to a camera".

- `DisplayName` — no runtime effect; it exists for the data validator, which warns when it is empty.
  **Example:** fill it in for tidiness, not for the game.
- `FactionOverride` — same pinning behaviour and same trap as the row field.
  **Example:** leave it empty so guards follow the owner.
- `NPCDefinition` — used when the post has no inline override. **Example:** a shared **BP_Guard** for
  every ordinary post.
- `ActivityConfiguration`, `TriggerSetOverrides` — used when the post has none of its own.
  **Example:** a shared "on duty" trigger set.
- `PatrolRoute` — used when the Place row has none. These nodes are **world-space**, unlike the row's
  relative nodes, so a shared route cannot be reused at a second post without re-authoring it.
  **Example:** a route authored for one specific gate; do not share it.
- `bLoopPatrol` — the loop flag for this asset's route, and it applies only when this route is the one
  in use. **Example:** on for a circuit.
- `ReserveSlots`, `ReserveSpawnDelay`, `ReserveSpawnRetryInterval`, `ReserveSpawnRadius`,
  `ReserveMinimumPlayerDistance`, `ReserveSpawnCandidateCount` — the shared reserve values, each
  overriding the row under the rule above. **Example:** `ReserveSlots` of 0 on a "static post"
  definition removes refills from every post using it, including after a capture.

---

## `FTerritoryFactionGuardDefinition` — which NPC guards which owner

One row per faction in a Place's `FactionGuardDefinitions`. Guards are resolved from the **current
owner**, so this is what makes a Place you captured staffable by *your* people.

- `Faction` — the exact faction this row matches; the array is walked in order and the first match
  wins. **Example:** a **Faction.Heroes** row so the player's faction gets soldiers on a farm it took.
- `NPCDefinition` — the NPC for that faction. A matching row with a null definition falls through to
  `DefaultGuardDefinition`, **and if that is also empty no guard spawns at all** — which means the
  Place has no defender registered, and a counterattack force finds nothing to fight there.
  **Example:** a player-faction row pointing at **BP_HeroGuard**, so the farm the player captured is
  actually defended.

---

## `UTerritoryCounterAttackProfile` — how the enemy decides

The profile that governs scheduled counterattacks against a Place: whether one happens, when, with
what, and whether it waits for you. `CounterAttackProfile` on the Place points at one of these, and
**43 designer options** live here.

### Should a counterattack happen at all?

The launch chance is a score, not a single number. Every term below is **added** (or subtracted), and
the result is clamped. There is no need for the weights to sum to anything.

- `BaseLaunchProbability` — the starting odds for every eligible target, before every other term.
  **Example:** 0.15 means roughly one in seven eligible evaluations rolls a counterattack.
- `MinimumLaunchProbability` — the floor. Raising it makes even a discouraged attack more likely,
  because a heavily penalised score can no longer fall below it. **Example:** 0.01 keeps hopeless
  attacks rare but not impossible.
- `MaximumLaunchProbability` — the ceiling. At 1.0 an attack is certain once every hard gate passes.
  **Example:** 0.95 leaves a sliver of luck; 1.0 means "if they can, they will".
- `UnguardedLaunchProbability` — **replaces the entire formula when the Place has no living guards**
  (reserve guards alone do not count). **Example:** 1.0 means an undefended farm is attacked the moment
  it is eligible — a strong incentive to keep a guard posted.
- `AttackerPowerWeight` — multiplies the attacker's normalised military power. **Example:** 0.30 makes
  strong factions attack more often than weak ones.
- `DefenceDeterrenceWeight` — **subtracted**, and the only term that lowers the score. It mixes how
  well-manned the Place is with its raw defence power. **Example:** 0.45 is why a well-garrisoned farm
  is much less likely to be attacked. **This is the dial to turn if you want defence to feel safe.**
- `StrategicValueWeight` — multiplies the target's importance. **Example:** 0.15 favours important
  Places over trivial ones.
- `ReadinessWeight` — multiplies the average of economy and supply readiness. **Example:** 0.10 makes a
  well-supplied faction attack more than a starving one of equal strength.
- `InfluenceWeight` — multiplies the faction's territorial influence. **Example:** 0.20 favours
  factions already established in the area.
- `FactionForces` — **the array of factions that may attack.** Empty, or with no row matching a given
  faction, means **no counterattack can ever be scheduled for that faction**. There are no attacker
  fields in the profile itself — it is all here. **Example:** one row for **Faction.Bandits**. A
  profile with no rows is silent, not aggressive.
- `QuestRules` — extra gates evaluated only for strategic counterattacks; empty passes. Each rule scans
  online players in scope, and **all** rules must pass. A rule with no quest class set **fails the
  whole gate**. **Example:** block counterattacks on a Place while the player is mid-quest there.

### When does it happen?

- `GracePeriodGameTime` — campaign-time delay between scheduling and evaluation. **This is the warning
  window.** **Example:** 300 gives the player five campaign-minutes of notice.
- `MinimumInfluenceTimingScale` — with high influence, how much shorter that window gets: the delay is
  the base value interpolated toward this fraction. **Example:** 0.25 turns a 300 grace into 75 for a
  faction at full influence.
**Everything else about *when* is not on this profile.** The time-of-day policy, the repeat cooldown,
and whether the series is finite all live on each row of `FactionForces` — see "When may this faction
attack?" under [`FTerritoryFactionAssaultConfig`](#fterritoryfactionassaultconfig--who-attacks-and-with-what).
The split looks arbitrary until you see the rule behind it: **this profile says *how* a counterattack
behaves; a `FactionForces` row says *who* attacks and *how often*.** One profile, many factions, each
with its own schedule.

### Does it wait for the player? — the reported bug

Two flags decide this, and they only make sense as a pair.

- `bRequirePlayerProximityForActivation` — off (the default), the force activates on its own when it
  arrives. On, it holds in a **WaitingForPlayerProximity** state until a relevant player is inside
  `ActivationRadius`.
- `bGarrisonTriggersActivation` — **only consulted when the option above is on.** On, a garrison
  standing in the Place is itself a reason to attack. Off (the default), a garrison does **not** count,
  and the force idles until a real player arrives.

Read together:

| Proximity required | Garrison triggers | What the arriving force does |
|---|---|---|
| off (default) | ignored | Activates on arrival |
| on | on | Activates for a nearby player **or** a garrison defending the Place |
| **on** | **off** | **Idles until a real player is inside the radius, then attacks — and the player is the only target it has** |

The third row is the reported bug: *"the enemy wait … when Player Character Enter District it React and
move to chase player to attack ignoring the one Guard Assign by Player Character."* The guard is not
mis-targeted; it is simply not a trigger. **Fix:** turn `bGarrisonTriggersActivation` on, and the
assigned guard makes the force attack on arrival.

- `ActivationRadius` — how close a player must be to count, used both for activation and for whether
  reserve waves may deploy. **Example:** 5000 cm, about 50 m.
- `bNotifyDefendingFactionOnly` — on, only the defending faction is warned and counted for proximity and
  power scaling; off, every player counts. **Example:** off in co-op so all players get the alert.
- `NotificationRadius` — how far warnings and state broadcasts reach. **Example:** 12000 so allies
  across the district hear about it.
- `bContinueFiniteWavesAfterActivation` — on, pending reserve waves deploy with nobody nearby; off,
  reserves pause unless a relevant player is within `ActivationRadius`. **Example:** off means a farm
  you walk away from stops being reinforced.
- `bUsePlayerRelativeReserveStaging` — on, and with a relevant player in range, the authored approach
  list is **re-sorted** so attackers arrive from a cinematic distance. Approaches are never invented,
  only reordered. **Example:** on, so bandits arrive from off-screen rather than on top of the player.

### Can the Place fall while you are away?

- `bUseUnattendedRecaptureHandover` — with attackers inside, no defenders and no defending player
  present, on starts a countdown to hand the Place over; off returns to fighting, so **the Place never
  falls while the player is away**. **Example:** off for a story Place that must not be lost silently.
- `UnattendedRecaptureDelayGameTime` — the length of that countdown. Any defender, a living defending
  player, or a missing attacker cancels it. **Example:** 30 gives the player half a campaign-minute to
  get back.
- `bConcedeWhenDefendingPlayerDies` — on, a defending player dying inside the Place hands ownership over
  immediately instead of waiting for the countdown. **Example:** on, so dying while defending a farm
  loses it at once.

### What shows up?

- `WaveStrategy` — **Legacy** refills to the wave size and waits for zero before the next wave,
  **BackToBack** sends the next once vehicles have cleared, **AfterDefeated** waits for the current wave
  to be dead, and **Simultaneous** sends everything at once. **Example:** **AfterDefeated** gives the
  player a breather between waves; **Simultaneous** does not. Note the wave **size** is per-faction
  (`WaveSize` on the row below); only the *sequencing* is here.
- `ParticipantSpacing` — centre-to-centre gap in the attackers' formation. **Example:** 220 keeps a
  squad from overlapping as it walks in.
- `SpawnPlacementAttemptsPerParticipant` — attempts per attacker to find a legal, separated spawn slot
  before the wave is deferred. **Example:** 4; raise it in cramped Places.
- `ReserveMinimumPlayerDistance`, `ReservePreferredPlayerDistance`, `ReserveMaximumPlayerDistance` —
  the arrival distance the staging score aims for: too close is penalised hard, the preferred distance
  scores best, and beyond the maximum is still usable if it is the only route. **Example:** 1200 /
  2600 / 5500 gives arrivals that are visible but not in the player's face.
- `PreferredCameraEdgeDot` — targets the camera's edge rather than its centre. **Example:** 0.55 so the
  force appears at the side of the screen.
- `SameFloorHeightTolerance` — how much height difference is still "the same floor" for the arrival
  score. Multi-floor Places need authored approaches and nav links. **Example:** 500 on flat ground.
- `MaximumApproaches` — ceiling on how many authored routes one assault uses; the number actually used
  scales with the attacker's power ratio. **Example:** 3, so a weak force uses one road and a strong one
  uses up to three.
- `bUseNavigationAwareObjectives` — on, only complete paths are accepted; off falls back to straight-line
  distance. **Example:** on, so attackers do not aim through a wall.
- `bDistributeParticipantsAcrossObjectives` — on, each attacker starts its objective scan at a different
  slot, so they spread out instead of all picking the nearest. **Example:** on, so a squad does not pile
  onto one gate.
- `bCapConcurrentAttackersToNarrativeDifficulty` — on, the concurrent limit becomes the lower of your
  Territory maximum and Narrative's attack tokens for the current difficulty. **Example:** on, so Easy
  difficulty sends one attacker at a time regardless of what you authored.
- `StalledMovementRetryInterval` / `MaxStalledMovementRetries` — how long to wait before retrying a
  movement that stopped short, and how many failures before the attacker withdraws and is destroyed.
  **Example:** 1.5 seconds, 8 retries — after that the attacker gives up and its slot is spent.

### Who do they fight when they arrive?

These four options are about **target priority inside the fight**, not how strong the attackers are.
Strength — level, power, the gameplay effect that scales them — is per-faction, on the row below.

- `DamagingEnemyMemorySeconds` — how long a real damage event keeps the shooter on an attacker's threat
  list. **Example:** 20 seconds means shooting a bandit makes you its problem for twenty seconds even if
  you break line of sight.
- `bDamageRetaliationOverridesDefenderPriority` — off (the default), a living registered guard is always
  fought before any non-guard, **including the player who just shot the attacker**. **Example:** leave it
  off if you want the garrison to matter; turn it on if you want enemies to hold grudges.
- `bPrioritizeTerritoryTakeover` — on for a capture assault, the engagement list becomes registered
  defenders plus defending players inside the Place bounds expanded by the padding below.
  **Example:** on, so attackers fight the garrison and the player.
- `DefendingPlayerEngagementPadding` — how far outside the Place's bounds a defending player still counts
  as part of the fight. **Example:** 800 lets attackers chase a player who has stepped just outside.

### What the code does that no option controls

Two terms in the launch formula are **hardcoded** and cannot be tuned from this profile:
a **+0.20** bonus proportional to how far a Place's garrison falls short of its target, and a **+0.05**
momentum term. So an under-staffed Place is more attractive to attackers than the visible weights
suggest, and you cannot switch that off. Worth knowing before you spend an afternoon tuning the weights
and wonder why a half-empty farm still gets hit.

---

## `FTerritoryFactionAssaultConfig` — who attacks, and with what

One row of `FactionForces`, per attacking faction. **A faction with no row here can never
counterattack**, so an empty array is the most common cause of "the enemy never comes back".

The rule that orders this whole section: **the profile above says *how* a counterattack behaves; a row
here says *who* attacks and *how often*.** That is why scheduling and level scaling are here and launch
probability is not.

- `Faction` — which faction this force is. A force is skipped if the tag is invalid or if it equals the
  target Place's owner. **Example:** **Faction.Bandits**.
- `AttackerDefinition` — the NPC definition spawned for every attacker. **A null definition disqualifies
  the row entirely.** **Example:** **BP_BanditRaider**.
- `MilitaryPower` — strategic strength, feeding the attack power ratio and the launch chance. It never
  captures by itself. **Example:** 100 for a matched fight; 300 for a faction that should usually win.
- `EconomyReadiness` / `SupplyReadiness` — averaged into the readiness term, which only affects the
  launch chance, not troop count or combat strength. **Example:** 0.5 each for a faction that is
  struggling to fund its war.
- `RecentMomentum` — recent wins and losses, contributing at most ±0.05. **Example:** leave it at 0 and
  drive the drama from readiness and influence instead.
- `TerritorialInfluence` — how established this faction is locally. It raises the launch chance **and**
  shortens the grace period, so an entrenched faction attacks sooner and more often. **Example:** 0.8 for
  bandits who have held the valley for years.
- `PlannedForce` — total attackers for one assault, including those already lost. Zero or negative
  disqualifies the row. **Example:** 6 means six bandits in total, and killing them ends the assault.
- `WaveSize` — how many arrive at once; zero disqualifies the row too. **Example:** 3, so six attackers
  arrive as two waves of three. How those waves are *sequenced* is `WaveStrategy` on the profile above.

### When may this faction attack?

- `TimePolicy` / `TimeWindowStart` / `TimeWindowEnd` — **AnyTime** makes the grace period the only timing
  gate; **NarrativeTimeWindow** additionally holds the attack until Narrative's time of day is inside the
  window. **Example:** **NarrativeTimeWindow** from 1800 to 0500 gives you night attacks that wrap across
  midnight — and note the window is per-faction, so bandits can be nocturnal while a militia is not.
- `RecurringCounterCooldownGameTime` — the gap that must pass after a resolved assault before this faction
  may try the same Place again, so a defeated wave does not re-roll every tick. **Example:** 900 stops a
  farm being hammered continuously.
- `ScheduleMode` / `MaximumScheduledAssaults` — **SingleAssault** never repeats, **FiniteSeries** repeats
  up to the maximum, **UnlimitedSeries** always allows another attempt once the cooldown has passed and
  every other rule is satisfied. **Example:** **FiniteSeries** with 3 gives the first battle plus two more,
  then peace — good for a story beat, bad for an endless war.

### Scaling to the player's level

- `bScaleLevelToRelevantPlayerPower` — off, spawned attackers keep their authored level; on, the strongest
  relevant player in range sets the level. **Example:** on for a co-op game so the counterattack is not
  trivial for a high-level player.
- `EnemyLevelOffset`, `MinimumScaledEnemyLevel`, `MaximumScaledEnemyLevel` — the offset added to that
  player's power and the clamps around the result. **Example:** offset 1, clamped 1 to 100.
- `PlayerPowerTiers` — optional tag-to-level map, so a player's **power tier tag** contributes a level
  rather than their Narrative character level. **Example:** a "veteran" tag worth level 8.
- `PowerScalingEffect` / `PowerScalingMagnitudePerEnemyLevel` — the gameplay effect applied to a scaled
  attacker and the magnitude per level above one. **Example:** 1.5 gives a level 6 enemy 7.5 extra attack
  damage. Narrative's own damage and friendly-fire rules stay authoritative.

### Staging and vehicles

- `StagingRequirement` — **OwnsSecureDistrict** requires the attacker to hold at least one loaded,
  unlocked District with its whole Place set secure; **None** removes the gate. **Example:** **None** for
  a guerrilla faction that should be able to attack from nowhere.
- `bAllowStoryPursuitWithoutStagingDistrict` — a dual opt-in that only matters for story pursuits.
  **Example:** leave it off unless a quest specifically needs a staging-free assault.
- `SignatureVehicleClass` — one vehicle class that identifies this faction across every road, winning
  over the approach's own vehicle. **Example:** a distinctive technical so the player recognises the
  bandits before they arrive.
- `bScaleVehicleCountByNarrativeDifficulty` and `VehicleCountsByDifficulty` — how many cars come, either
  from Narrative's difficulty or from your own per-difficulty table. **Example:** Easy sends one car,
  Hard sends two; a maximum of 0 makes the faction arrive on foot.
- `ReserveWaveAlertDialogueTag`, `TakeoverStartedDialogueTag`, `FinalFightDialogueTag` — dialogue played
  when a wave arrives, when an attacker first starts taking the Place, and when a chase target abandons a
  damaged car. **Example:** a shouted warning on arrival, then "we've got the farm!" on takeover.
- `ActivityConfigurationOverride` and `TriggerSetOverrides` — passed straight into Narrative's spawn call
  for each attacker. Their meaning lives in Narrative Pro rather than this plugin. **Example:** an
  activity configuration that makes attackers loot rather than stand idle.

---

## `FTerritoryAssaultApproach` — the roads they come down

One row per authored entry route, in the Place's `CounterAttackApproaches` array. A row that fails
validation is dropped with a logged reason, and an unnamed row is **never** used.

- `ApproachID` — the stable route identity, and **blank IDs are auto-filled in the editor** from the type
  plus an index. An unnamed approach is never selected or spawned. **Example:** **Road_North**.
- `Type` — **only used once, to name a blank ID** such as **Road_01**. It does not affect routing,
  spawning or validation at runtime — a genuine surprise when you expect it to matter. **Example:** leave
  it accurate anyway, since it names your IDs.
- `EntryType` — **OnFoot** spawns at the transform and walks in; **NarrativeVehicle** requires a
  resolvable vehicle and a complete road route, and dismounts before walking in. **Example:** a vehicle
  approach down the farm track, and an on-foot approach over the back field.
- `RelativeSpawnTransform` — where they appear, multiplied by the Place's transform. **Example:** the
  lay-by at the end of the lane.
- `VehicleClass` — the fallback car for this road; if it is missing, a valid faction signature vehicle
  must exist or the approach fails as unusable. **Example:** your **BP_BanditTruck**.
- `RelativeVehicleDropOffTransform` — where the car parks and they dismount, used when no road guide
  resolves. A walkable route from here into the Place must exist. **Example:** the farm's turning circle.
- `RoadGuideID` — the placed road guide to use; blank falls back to the approach ID. A found guide
  replaces the spawn and drop-off transforms with its spline ends. **Example:** **FarmLane_Guide**.
- `RoadLaneSide` — which lane they drive on, for both the assault and a reversed chase route.
  **Example:** **Right**, matching the road's own traffic.
- `MaximumVehicleDeployments` — how many cars this one route may create in a single assault; later
  attackers arrive on foot. **Example:** 1 car, so the rest of the wave walks.
- `VehicleOccupantCapacity` — seats requested per car, combined with the wave size and the vehicle's real
  seat count. **Requesting more seats than the car exposes destroys the car and spawns nobody** — a
  silent-looking failure with a real cause. **Example:** 4 for a four-seat truck.
- `VehicleMaximumDriveSpeed` — desired AI road speed; zero uses the plugin's safe default of 1400 cm/s.
  **Example:** 2000 for a fast approach across open ground.
- `VehicleIngressTimeoutSeconds` — how long mounting, driving, parking and dismounting may take before
  the ingress withdraws safely. **Example:** 120.
- `MaxWaveSize` — the largest part of one wave that may use this route; it never creates extra reserves.
  **Example:** 4 per approach.
- `bEnabled` — disabled approaches are excluded from selection and from the failure log entirely.
  **Example:** switch a route off while decorating the level, without losing its authoring.
- `VehicleAwareness` and `VehicleRetirement` — per-approach probe sizes for the driven car, and the
  cleanup policy for the temporary vehicle. The behaviour lives in the road and vehicle components rather
  than here. **Example:** retire the car a while after the assault resolves, unless a player has taken it.

---

## Guards and counterattacks: putting it together

The reported bug in one paragraph, for reference. A player captured Castle Hill Farm and assigned one
guard. The bandit counterattack force arrived and **waited**; when the player entered the district it
woke up, **ignored the assigned guard**, and chased the player. Three separate defaults produce this,
and each is a designer choice rather than broken code:

1. **The force waits for a player.** `bRequirePlayerProximityForActivation` on with
   `bGarrisonTriggersActivation` off means a garrison is not a reason to attack — "the force idles until a
   player walks in, and then has a player to chase", as the code's own comment puts it.
   **Fix:** turn `bGarrisonTriggersActivation` on.
2. **The guard will not fight a mere war.** `bEngageAtWarInClaimedTerritory` off means a **Claimed**
   Place's guard ignores a faction it is at war with, unless the arriving actor is an assault character
   whose defence front includes this exact Place. **Fix:** turn it on if you want fresh captures defended.
3. **The guard may be the wrong faction.** A stale `FactionOverride` on the post leaves the guard unable
   to be at war with the attacker, and outside the attacker's own defender list. **Fix:** clear it after a
   capture, or drive the faction from the owner rather than an override.

Also worth knowing: the one branch that would make a guard engage an arriving assault only fires when the
assault's target is an **Independent Place** or a same-owner sibling of it. A counterattack scheduled
against a **District** produces an empty defence front, so **no** guard qualifies through that branch. If
you schedule counterattacks against districts, guard reactions for them depend entirely on
`bEngageAtWarInClaimedTerritory`.

---

---

## Stealth and disguise

Infiltration is what happens when the player enters a Place's bounds **without** being meant to be
there. It is a separate policy asset from the Place itself, so one profile can be shared by many Places:
`DefaultStealthProfile` on the Territory Definition is the pre-conflict policy, and any state row may
replace it with `StealthProfileOverride`. **An empty profile means infiltration is disabled**, so the
bounds register a contest the moment the player walks in.

Two profiles do the work: one for how guards detect the player, one for the uniform that makes them not.

### `UTerritoryStealthProfile`

**Detection.**

- `bAllowStealthInfiltration` — the master switch. On, entering the bounds registers an **undetected
  infiltrator** instead of an attacker. Off, the Place reacts as if the player had attacked it.
  **Example:** on for a military base the player is meant to sneak through; off for a Place that should
  always be a straight fight.
- `EscalationScope` — how far one exposure spreads. **Local Alarm Only** keeps it local: guards
  investigate but the Territory is **not** registered as **Contested**. The other two register the player
  as a contester as well. **Example:** **Local Alarm Only** on a tutorial Place so a mistake costs a
  chase, not a war.
- `ImmediateSightExposureThreshold` — effective Narrative sight at or above this confirms the player at
  once, skipping suspicion accumulation. **Example:** 0.8 means being clearly seen in the open is not
  survivable, while a glimpse in cover still builds slowly.
- `MinimumSightEvidence` — sight below this is ignored entirely. **Example:** leave it at 0.2, which is
  the floor Narrative's own attack goal uses, unless you want guards reacting to things they cannot see.
- `GuardDetectionMultiplier` — multiplies Narrative's sight strength before the player's Stealth Rating is
  applied. **Example:** 1.25 on an elite base so the same crouch-walk that works in a village fails there.
- `SightSuspicionGainPerSecond` / `SuspicionDecayPerSecond` — how fast partial sight builds suspicion and
  how fast it fades once every guard has lost sight. **Example:** 2.0 and 0.2 means a two-second glimpse
  costs ten times as long to forget, which makes a cautious player's patience pay off.
- `MaximumStealthRating` — the scale Narrative's Stealth Rating is read on. **Example:** leave it at 100
  and a rating of 50 reads as half-hidden; change it and every rating in your project changes meaning.

**The things that always give you away.** Each of these is a separate, deliberate "no" to stealth:

- `bPointBlankSightAlwaysExposes` with `PointBlankSightExposureDistance` — a guard with sight of the
  player exposes them if within this distance, whatever the cover says. **Example:** on at 300 so walking
  into a guard's face cannot be talked away by a high rating.
- `bFireWhileSeenExposes` — shooting while visible confirms the player. **Example:** leave it on; a
  silenced shot from cover is a different setting, `bFireWhileUnseenStartsInvestigation`.
- `bFireWhileUnseenStartsInvestigation` — an unseen shot sends guards to look rather than confirming you.
  **Example:** on, so a sniper who never shows themself is hunted but not identified.
- `bDamageImmediatelyExposes` — dealing damage confirms the player. **Example:** on for a Place where
  wounding a guard should be as loud as firing.
- `bSeenDefenderKillExposes` / `bUnseenDefenderDeathStartsInvestigation` — the same split for a killing
  blow on a defender. **Example:** both on, so a silent kill buys time but a witnessed one ends stealth.
- `bAnonymousEvidenceCanExpose` — off (the default), evidence that cannot be attributed to this player
  does **not** expose them. **Example:** leave it off so a bandit dying to wildlife does not burn the
  player's infiltration.

**Noise and suspicion values.** `GunshotSuspicion`, `BulletImpactSuspicion`, `CorpseSuspicion` and
`ThrowableDistractionSuspicion` are the suspicion each event adds. **Example:** a corpse at 0.5 is worth
two gunshots at 0.35 apiece, so bodies are what actually get players caught. `ShotCorrelationWindow` ties
a muzzle report and its impact together as one event inside that window, so a single shot is not counted
twice. `bRespondToOutsideThreats` lets guards react to threats that are not the player at all.
`bRespectNarrativeInvisibleTag` keeps Narrative's own invisibility tag authoritative.

**Investigations.** When suspicion is high enough without confirmation, guards investigate instead of
attacking: `InvestigationRadius` (how far from the evidence they search), `InvestigationDuration` (how
long), `InvestigationAcceptanceRadius` (how close counts as arriving), and `MaximumInvestigators` (how
many go at once). `InvestigationActivityClass` is the Narrative activity they run while searching.
**Example:** 2 investigators over 12 seconds within 5000 units — a local search, not a base-wide sweep.

**Ending stealth cleanly.** `bSendBreakStealthGameplayEvent` with `BreakStealthGameplayEventTag` tells the
exposed player's Ability System that stealth is over; `bCancelActiveStealthAbilitiesOnExposure` with
`StealthAbilityTagsToCancel` cancels their crouch-style abilities, and
`bRemoveActiveStealthEffectsOnExposure` with `StealthGameplayEffectsToRemove` clears the temporary
Gameplay Effects. **Example:** leave all three on, or a player can keep a stealth buff running while every
guard is shooting at them.

### `UTerritoryDisguiseProfile`

A disguise is a uniform the guards read instead of the player's real faction. The profile describes one
uniform; the stealth profile decides whether uniforms are accepted here at all.

- `PerceivedFaction` — the faction guards believe they are looking at while the disguise holds.
  **Example:** a Bandit uniform inside a Bandit-held farm.
- `Quality` — how convincing it is, on a 0-to-1 scale. **Example:** 1.0 is a perfect uniform; 0.6 holds up
  in public but fails at a checkpoint that requires more.
- `ClearanceTags` — access tags the uniform carries, checked against the Place's `RequiredDisguiseClearanceTags`.
  **Example:** an officer's clearance tag, so only command areas open to you.
- `bCompromiseWhenFiringWhileSeen`, `bCompromiseWhenDealingDamage`, `bCompromiseWhenDefenderKillIsSeen` —
  the three ways violence burns the uniform, each independent. **Example:** turn off
  `bCompromiseWhenDealingDamage` for a double agent who is expected to shoot and still be trusted.
- `bCompromiseFromScriptedReveal` — a scripted reveal or a failed identity check burns it. **Example:** on,
  so a story beat can unmask the player deliberately.
- The six event tags — `ActivatedEventTag`, `RemovedEventTag`, `CompromisedEventTag`, `RestoredEventTag`,
  `IdentityCheckPassedEventTag` and `IdentityCheckFailedEventTag` — are what your abilities and quests
  listen to. **Example:** bind a HUD hint to `CompromisedEventTag` so the player learns the uniform failed.

**On the stealth profile side, three options decide whether uniforms work here at all.**
`bAllowFactionDisguises` turns them on for the Place, `bRequireOwningFactionDisguise` insists the uniform
match the current owner rather than merely being non-hostile, `MinimumDisguiseQuality` rejects any uniform
below a threshold, and `bCompromiseDisguiseOnFailedIdentityCheck` decides whether a failed scripted check
burns it. **Example:** a public street accepts any faction at quality 0.5; an enemy headquarters requires
the owning faction's uniform at 0.9.

**A note on `EscalationScope` having three values and two behaviours.** In the shipped code every
comparison is against **Local Alarm Only**; **Territory Conflict** and **Faction War Through State Event**
take the same path. The difference is intent, not behaviour: both register the player as a contester, and
**Faction War** documents that your Contested state event is what declares the war. Pick **Faction War** if
a state event handles diplomacy; pick **Territory Conflict** if something else does. Whichever you choose,
set the state event up, because nothing else will declare it for you.

---

## Economy

Three systems pay a faction, and the option that tunes one does nothing for the others. Keeping them
apart answers most "why is my income wrong" questions.

| System | What it pays | What sets its rhythm |
|---|---|---|
| Currency | `PeriodicIncome` per Place, less `GuardUpkeepPerCycle` per guard | `EconomyTickIntervalSeconds` |
| Property resources | the items in a Property's production recipes | Narrative's campaign clock, watched by `ProductionCycleObservationIntervalSeconds` |
| One-off rewards | the authored capital capture bonus, and quest rewards | the state row's `bAllowCapitalCaptureReward`, and the quest itself |

**The currency tick.** `EconomyTickIntervalSeconds` (default **300**) is real seconds between income and
upkeep. It is read once, in `UTerritoryEconomySubsystem::Initialize`, where the repeating timer is armed —
so changing it takes effect on a new world rather than mid-session. The timer is server-only: economy
state is server-authoritative.

**The state row decides whether a Place actually pays.** Setting `PeriodicIncome` is not enough; the Place's
current state row must also allow it. Two traps live here, both stated in the tooltips and both easy to miss:

- Blocking income does **not** block upkeep. `bAllowPeriodicIncome` off stops the earnings while
  `GuardUpkeepPerCycle` stays payable — so a Place held in **Contested** with a garrison bleeds money for
  as long as the fight lasts.
- Blocked production is lost, not banked. With `bAllowResourceProduction` off the cycle simply expires;
  there is no backpay when the state ends.

**Production follows Narrative's clock, not the economy tick.** `ProductionCycleObservationIntervalSeconds`
(default **1**) only controls how often the server *checks* that clock — it does **not** set the production
cycle length, which belongs to Narrative. The setting exists for latency: when sleeping advances the story
by a day, a value of 1 delivers the farm's items in about a second instead of waiting for the five-minute
currency tick.

**Two settings that look like income and are not.** `DefaultTerritoryIncome` and `DefaultGuardCost` are
still in the config file and are explicitly marked deprecated: gameplay never reads them. Set
`PeriodicIncome` and `GuardUpkeepPerCycle` on the Territory Definition instead. If you inherit a project
that tuned these and nothing changed, this is why.

**For a custom economy screen**, `UTerritoryEconomyWidget` exposes `ResourceRowClass` and
`ProductionSiteRowClass`, so your own widgets are used for each resource row and each production-site row.

---

## `UTerritoryDeveloperSettings` — timings, limits, the look of the UI, and debugging

Project-wide settings, in **Project Settings → Game → Territory Framework**. Everything here applies to
every Place at once, which is what separates it from the per-Place options above. 78 options live here;
these are the ones that change how the game feels.

### Capture timing

- `CaptureProgressPerSecond` — default progress gained per second, from 0 to 1, before any capture
  modifiers. **Example:** 0.1 means ten seconds of standing in the bounds to capture a Place.
- `CaptureProgressDecayPerSecond` — progress removed per second when the capture rules call for decay.
  **Example:** 0.05, half the gain rate, so a capture you were forced off slips away twice as slowly as it
  built.
- `CaptureTickInterval` — how often capture progression is evaluated. **Example:** leave it at 0.1; raising
  it saves a little CPU and makes the progress bar advance in visible steps instead of smoothly.
- `TreatyExpirationCheckInterval` — how often treaties are checked for expiry. **Example:** 10 means a
  lapsed non-aggression pact stops protecting a Place within ten seconds of running out.

### How many counterattacks can be running at once

These four caps are what stop a large map from emptying itself of NPCs. The first three are independent
limits and the lowest one wins.

- `MaxConcurrentScheduledAssaults` — world-wide ceiling on simultaneous scheduled or active assaults.
  **Example:** 8 across the whole map, so six Places under threat do not spawn six armies at once.
- `MaxConcurrentAssaultsPerFaction` — ceiling charged to one attacking faction. **Example:** 2 means the
  bandits can threaten two Places but cannot open five fronts.
- `MaxLiveCounterAttackNPCs` — global budget of living counterattack NPCs. Reserve troops do **not** bypass
  it. **Example:** 24 means three assaults of eight cannot all be on the field — the next wave waits.
- `MaxRetainedAssaultRecords` — how many completed assault records are kept for history and snapshots.
  **Example:** 100 keeps a long campaign's history without growing the save forever.

### The assault scheduler

- `CounterAttackUpdateInterval` — real seconds between server updates of the strategic assault scheduler.
  **Example:** raise it to 5 on a very large map to cut CPU; lower it and a launched attack reacts sooner.
- `CounterAttackCampaignSeed` — the seed for repeatable planning: the same seed and the same campaign
  inputs produce the same decision, and a decision loaded from a save is not rolled again. **Example:**
  leave it at 1337 for a fair test run; change it to make a campaign play out differently without touching
  a single balance number.
- `SpatialCellSize` — cell size of the spatial index used for proximity queries. **Example:** 2000 suits a
  town; raise it for a large open map, lower it for a dense city, and always with the knowledge that this
  is a performance knob, not a gameplay one.

### Reputation, diplomacy and the player's faction

- `bReputationDrivesDiplomacy` — when enabled, Territory campaign reputation can change treaties it
  created. First choose an explicit campaign faction using Set Reputation Subject Faction on the server.
  **Example:** choose Heroes, then lower Bandit reputation below the hostile threshold to declare war
  between Bandits and Heroes. With no subject selected, scores change but treaties do not.
- `HostileReputationThreshold` / `AlliedReputationThreshold` — where those crossings sit. **Example:** -50
  so robbing a faction three times turns it hostile; 50 so a long chain of helpful quests earns an
  alliance without a scripted reward.
- `DefaultPlayerFaction` — fallback for a player whose Narrative faction is not set. **Example:** choose
  your project's player faction so a misconfigured character still counts as somebody.
- `Notifications` — the notification settings used for Territory messages. **Example:** route capture and
  counterattack messages through your own notification style instead of the default.

### The look of the HUD and the Command Center

This is the group that answers "the Territory widgets look wrong". Every colour and texture the HUD card
and the Command Center draw is here, so a project can restyle both without touching a widget.

- `TerritoryHUDCardSize` — the size of the passive capture card. **Example:** 328 x 108 fits the default
  card; enlarge it for larger text or a longer Place name.
- `TerritoryHUDCardFillColor` — the card's background colour. **Example:** a mostly transparent value keeps
  the card readable without covering the world.
- `bTerritoryHUDCardUsePanelTexture` with `TerritoryPanelTexture` — draw a texture instead of a flat fill.
  **Example:** on with a parchment texture for a period setting.
- `TerritoryHUDCardAlertExtraHeight` — extra height the card grows by when it shows an alert line.
  **Example:** 38 adds one line without resizing the card permanently.
- `TerritoryCommandScreenFillColor`, `bTerritoryCommandScreenUseBackgroundTexture` with
  `TerritoryScreenBackgroundTexture` — the same choice for the full-screen Command Center.
- `TerritoryCommandPanelFillColor` / `TerritoryCommandPanelOutlineColor` — the panel fill and border of the
  Command Center. **Example:** an opaque panel colour with a faint outline is what makes the screen look
  deliberate instead of unfinished.
- `TerritoryProgressFrameTexture` / `TerritoryProgressFillTexture` — the capture bar's frame and fill.
- `TerritoryInterfaceFont` and `TerritoryTextScale` — one font and one global scale for every Territory
  widget. **Example:** raise the scale for a couch-friendly UI rather than editing each widget.
- `DefaultTerritoryTextStyle`, `TerritoryTitleTextStyle`, `TerritoryHeadingTextStyle`,
  `TerritoryMutedTextStyle` — body, title, heading and muted text, so every screen draws from one place.
- `DefaultTerritoryButtonStyle`, `TerritoryTabButtonStyle`, `TerritoryActionButtonStyle` and
  `DefaultNarrativeButtonClass` — the buttons. **A trap worth knowing:** these must point at styles whose
  own parent style resolves. A style asset whose parent path is broken still shows as a valid object
  reference, and the tab silently renders unstyled rather than reporting an error. If a Territory button
  looks plain while its neighbours do not, check the parent chain of the style it points at first.

### Territory inside Narrative cinematics

- `bHideTerritoryHUDDuringNarrativeDialogue` — hide the Territory HUD while a Narrative conversation plays.
  **Example:** on, so the capture card does not sit over a dialogue cutscene.
- `bUseCinematicParticipantLODDuringDialogue` — temporarily raise participant LOD quality during dialogue.
  **Example:** on, so a speaker is not a low-poly stand-in in a close-up.
- `bIncludeAttachedActorsInCinematicLOD` — include attached visual actors in that temporary LOD change.
  **Example:** on for a character whose hat or backpack would otherwise stay low detail.

### Debugging

- `bEnableDebug` — the master switch. Off, nothing below does anything. **Example:** on in a development
  build, off in a shipping one.
- `DebugVerbosityLevel` — how much detail is printed. **Example:** 5 for full detail while chasing a bug;
  lower it once the log is too noisy to read.
- Per-system toggles, each independent: `bDebugCounterAttacks`, `bDebugGuardSpawning`, `bDebugStealth`,
  `bDebugDiplomacy`, `bDebugEconomyTicks`, `bDebugSaveLoad`. **Example:** turn on only
  `bDebugCounterAttacks` while investigating why an assault did not launch, so the log stays readable.
- On-screen drawing: `bDrawTerritoryBounds`, `bDrawOwnershipOverlay`, `bDrawCaptureProgress`,
  `bDrawGuardSpawnPoints`, `bDrawSpatialGrid`. **Example:** `bDrawTerritoryBounds` answers "is the Place's
  volume actually where I think it is" faster than any amount of log reading.

Roughly two dozen further `bDebug*` switches exist for narrower systems — registry, transactions, tales,
behaviour trees, map markers and the rest. They all behave the same way: on for detail, off for quiet.

**One honest limitation while you debug guards.** When a guard decides *not* to fight, the plugin computes
a full human-readable reason — thirteen of them, from "that target is my own faction" to "the Place is not
contested and there is no confirmed local threat" — and then does not log it. So a guard standing still has
to be diagnosed by reading the options above. Until that changes, check
`bEngageAtWarInClaimedTerritory` and the post's `FactionOverride` first: those two account for most of it.

---

## Diplomacy, attitude and betrayal

Diplomacy decides whether a fight is allowed at all, and it is fed by two mechanisms that are **not** the
same thing. Keeping them apart is the difference between "my treaty works" and "my treaty seems to do
nothing".

- **Treaties** are this plugin's rich relations, listed below. They are replicated: applied on the server
  and delivered to clients.
- **Attitude** is Narrative Pro's per-faction friendliness number. This plugin reads it — for example in
  `CanFactionCaptureTerritory` and through `GetFactionAttitudeTowardsFaction` — but does not own it, and
  Narrative replicates only `AccumulatedTime`. So an attitude-driven decision is a **server-side**
  decision. Every caller of the capture check is server-side today, so that is a boundary rather than a
  bug; it would only bite a future client-side preview of "can I capture this?".

| Relation | What it does |
|---|---|
| Neutral / No Treaty | Nothing. Capture and counterattacks are allowed when every other rule passes |
| War | The opposite of a block: physical capture and counterattacks are allowed |
| Non-Aggression Pact | Blocks Territory capture and scheduled assaults |
| Ceasefire | Blocks new assaults and cancels assaults already waiting |
| Alliance | Blocks capture and counterattacks |
| Trade Agreement | Friendly, with an optional expiry; trade rules come from Territory metadata, AI friendliness from Narrative attitude |

Three assets read and write this state.

### `UTerritorySetDiplomacyEvent`

The Narrative event that changes a relation — this is how your Contested state row declares war, and how a
peace deal ends one.

- `NewState` — the relation to apply. **Example:** **War** from a Contested state event, so being caught
  inside the bounds is what actually starts the war.
- `FactionASource` / `FactionBSource` — where each side comes from. **AnyTime**-style explicit tags, or the
  dynamic sources: for a Contested state row, **Current Owning Faction** on side A and **Contesting
  Faction** on side B. **Example:** use the dynamic source so one state row works for every Place instead
  of one row per Place.
- `FactionA` / `FactionB` — the explicit tags, and the migration fallback for older assets.
  **Example:** **Narrative.Factions.Heroes** and **Narrative.Factions.Bandits**.
- `bFallbackToExplicitFactionWhenContextMissing` — when a dynamic source resolves to nothing, use the
  explicit tag instead of failing. **Example:** leave it on so an event fired outside a Territory still
  does something sensible.
- `bRequireContainingTerritoryOwner` — inside a state config, require the post-transition owner to match.
  **Example:** on, so the war is declared by whoever ends up owning the Place rather than by whoever
  happened to trigger the event.
- `bPreserveOtherActiveTerritoryWars` — before applying a peace, check whether another loaded Territory
  still has these two factions at war. **Example:** on, so peace in one valley does not silently end a war
  being fought in another.
- `TradeDurationGameTime` — campaign-time length of a Trade Agreement; zero or negative means it never
  expires. **Example:** 1440 for a one-day agreement that lapses on its own.
- `bApplyWhenStateStartsActive` — on a fresh world only, apply the relation without waiting for a
  transition. **Example:** on for a state policy so a campaign's opening relations are already correct.

### `UTerritoryDiplomacyCondition`

The question "are these two factions currently at war / allied / at peace?" for a quest or a branch.

- `FactionA` / `FactionB` — the two Narrative factions to compare. **Example:** **Narrative.Factions.Heroes**
  and **Narrative.Factions.Bandits**.
- `RequiredState` — the relation that must be true right now. **Example:** **War**, so a quest step only
  becomes available once the war has actually been declared.

### `UTerritoryDiplomacyDialogueProfile`

What an NPC says, chosen by the relation between the NPC's faction and the speaker's. One asset covers all
six cases:

- `SameFactionDialogue` — the interactor shares an exact faction tag with the NPC.
- `NeutralDialogue` — no treaty exists. **Example:** cautious but not hateful.
- `CeasefireDialogue`, `NonAggressionDialogue`, `TradeAgreementDialogue`, `AllianceDialogue` — one line for
  each relation, so a peace, a pact and an alliance all sound different.
- `WarDialogue` — spoken while the two factions are at War. **Example:** the threat before the fight.

### Making a betrayal authorable

The plugin records **who** took a Place and **who it was taken for**, as two separate tags on the Place's
ownership data. They differ exactly when someone captures a Place on another faction's behalf — and that
difference is what makes a mid-game betrayal a data condition rather than a scripted special case.

The source's own example, which is also the intended authoring pattern: the Regime sends the player to take
a Bandit outpost. `CapturedBy` is **Faction.Heroes** — whoever did the work. `CapturedFor` is
**Faction.Regime**. Later the Regime turns on the player, and a quest can ask *"which Places did I win for
the faction that betrayed me?"* by testing `CapturedFor` against the Regime and `CapturedBy` against
Heroes. `FormerOwningFactions` keeps the earlier owners as well, so "who has held this Place" is answerable
too.

A betrayal story can lower the Regime's campaign reputation. With an explicit subject and
`bReputationDrivesDiplomacy` enabled, reaching `HostileReputationThreshold` can declare War.
Reputation scores and Narrative combat attitude are different values. This does not automatically
transfer any Place. Use the capture history to choose the relevant Places, then author the intended
capture or ownership events. A player changing faction also does not silently change the shared
campaign reputation subject.

---

## Saving and persistence

Territory state is saved through Narrative Pro's save system rather than a file of its own. Two actors
carry the identity the save needs, and both are usually already in your level if you used the editor
setup.

### `ATerritoryWorldState`

- `WorldStateGUID` — the world's stable identity across saves. **Example:** it is how a loaded save finds
  *this* campaign's world state instead of creating a second one.
- `CampaignCities` — the cities in this campaign. **Example:** the City definitions the world state tracks,
  so a City is not re-registered on every load.

### `ATerritorySavableData`

- `SavableDataGUID` — a stable save identity for the actors that hold Territory data. **Example:** keep it
  set and unchanged for the level's lifetime; regenerating it makes an existing save treat the data as a
  different object.

**What comes back, and what does not.** Ownership, current state, treaties, garrison counts, assault
history, economy accounts and the capture history above all persist. The options that describe a *new*
campaign — `InitialOwningFaction`, `InitialState`, `InitialAvailability` and `InitialGuardCount` — do not
re-apply to a loaded save, which is deliberate: your captured farm stays yours. That is also why a
tutorial change to those options appears to do nothing until you start a fresh campaign, and
`bDebugSaveLoad` is the switch that shows the save path in the log while you confirm it.

---

## Appendix — the defaults used in this file

**Generated from the C++ inventory, not typed by hand.** A test fails if any value here disagrees
with the initialiser in the source, so this table cannot drift. `—` means the author wrote no
initialiser, so the type's zero or empty value applies (0, false, empty array, null pointer).

| Class | Option | Default in C++ |
|---|---|---|
| `UTerritoryDefinition` | `TerritoryTag` | `—` |
| `UTerritoryDefinition` | `DisplayName` | `—` |
| `UTerritoryDefinition` | `InitialOwningFaction` | `—` |
| `UTerritoryDefinition` | `InitialAvailability` | `ETerritoryAvailability::Unlocked` |
| `UTerritoryDefinition` | `InitialState` | `ETerritoryInitialState::Automatic` |
| `UTerritoryDefinition` | `RelativeTransform` | `FTransform::Identity` |
| `UTerritoryDefinition` | `TerritoryActorClass` | `—` |
| `UTerritoryDefinition` | `DefaultGuardDefinition` | `—` |
| `UTerritoryDefinition` | `FactionGuardDefinitions` | `—` |
| `UTerritoryDefinition` | `InitialGuardCount` | `3` |
| `UTerritoryDefinition` | `PostCaptureGarrisonPolicy` | `ETerritoryPostCaptureGarrisonPolicy::PlayerChooses` |
| `UTerritoryDefinition` | `GuardBehavior` | `—` |
| `UTerritoryDefinition` | `GuardPosts` | `—` |
| `UTerritoryDefinition` | `GuardQuality` | `1.f` |
| `UTerritoryDefinition` | `FortificationStrength` | `0.f` |
| `UTerritoryDefinition` | `NearbyAlliedSupport` | `0.f` |
| `UTerritoryDefinition` | `StrategicValue` | `1.f` |
| `UTerritoryDefinition` | `CapturePoint` | `—` |
| `UTerritoryDefinition` | `bStoryCaptureFromBounds` | `false` |
| `UTerritoryDefinition` | `MaxConcurrentAttackers` | `3` |
| `UTerritoryDefinition` | `PeriodicIncome` | `100` |
| `UTerritoryDefinition` | `GuardUpkeepPerCycle` | `50` |
| `UTerritoryDefinition` | `GuardRecruitmentCost` | `50` |
| `UTerritoryDefinition` | `bAttitudeAffectsPrices` | `false` |
| `UTerritoryDefinition` | `FriendlyPriceMultiplier` | `0.85f` |
| `UTerritoryDefinition` | `NeutralPriceMultiplier` | `1.f` |
| `UTerritoryDefinition` | `WarPriceMultiplier` | `1.5f` |
| `UTerritoryDefinition` | `StateConfigs` | `—` |
| `UTerritoryDefinition` | `QuestRuntimeOverrides` | `—` |
| `UTerritoryDefinition` | `DefenderDiedEvents` | `—` |
| `UTerritoryDefinition` | `AllDefendersDefeatedEvents` | `—` |
| `UTerritoryDefinition` | `DefaultStealthProfile` | `—` |
| `UTerritoryDefinition` | `ManagementPoint` | `—` |
| `UTerritoryDefinition` | `bShowGameplayHUD` | `true` |
| `FTerritoryStateGameplayRules` | `CounterAttackPolicy` | `ETerritoryStateCounterAttackPolicy::CaptureTriggered` |
| `FTerritoryStateGameplayRules` | `AllowedAttackingFactions` | `—` |
| `FTerritoryStateGameplayRules` | `bAllowPeriodicIncome` | `true` |
| `FTerritoryStateGameplayRules` | `bAllowResourceProduction` | `true` |
| `FTerritoryStateGameplayRules` | `bAllowCapitalCaptureReward` | `true` |
| `FTerritoryStateGameplayRules` | `GrantedCommandCapabilities` | `—` |
| `FTerritoryStateGameplayRules` | `EntryConditions` | `—` |
| `FTerritoryStateGameplayRules` | `ExitConditions` | `—` |
| `FTerritoryStateGameplayRules` | `EntryEvents` | `—` |
| `FTerritoryStateGameplayRules` | `ExitEvents` | `—` |
| `FTerritoryGuardBehaviorTemplate` | `PatrolGoalClass` | `—` |
| `FTerritoryGuardBehaviorTemplate` | `bEnablePatrolCrowdAvoidance` | `true` |
| `FTerritoryGuardBehaviorTemplate` | `PatrolAvoidanceConsiderationRadius` | `500.f` |
| `FTerritoryGuardBehaviorTemplate` | `PatrolAvoidanceWeight` | `0.5f` |
| `FTerritoryGuardBehaviorTemplate` | `bPrioritizeClosestHostilePlayer` | `true` |
| `FTerritoryGuardBehaviorTemplate` | `bAllowPersonalRetaliation` | `true` |
| `FTerritoryGuardBehaviorTemplate` | `bDefendAgainstExposedEnemies` | `true` |
| `FTerritoryGuardBehaviorTemplate` | `bEngageAtWarInClaimedTerritory` | `false` |
| `FTerritoryGuardBehaviorTemplate` | `CombatTargetFactions` | `—` |
| `FTerritoryGuardBehaviorTemplate` | `ClosestHostilePlayerGoalScoreBonus` | `0.75f` |
| `FTerritoryGuardBehaviorTemplate` | `DialogueProfile` | `—` |
| `FTerritoryGuardBehaviorTemplate` | `FactionDialogueProfiles` | `—` |
| `FTerritoryGuardPostTemplate` | `GuardPostID` | `—` |
| `FTerritoryGuardPostTemplate` | `ActorClass` | `—` |
| `FTerritoryGuardPostTemplate` | `RelativeTransform` | `FTransform::Identity` |
| `FTerritoryGuardPostTemplate` | `GuardPostDefinition` | `—` |
| `FTerritoryGuardPostTemplate` | `NPCDefinitionOverride` | `—` |
| `FTerritoryGuardPostTemplate` | `ActivityConfigurationOverride` | `—` |
| `FTerritoryGuardPostTemplate` | `TriggerSetOverrides` | `—` |
| `FTerritoryGuardPostTemplate` | `FactionOverride` | `—` |
| `FTerritoryGuardPostTemplate` | `Priority` | `50` |
| `FTerritoryGuardPostTemplate` | `ReserveSlots` | `1` |
| `FTerritoryGuardPostTemplate` | `bAutoSpawnReserves` | `true` |
| `FTerritoryGuardPostTemplate` | `ReserveSpawnDelay` | `3.f` |
| `FTerritoryGuardPostTemplate` | `ReserveSpawnRetryInterval` | `2.f` |
| `FTerritoryGuardPostTemplate` | `ReserveSpawnRadius` | `600.f` |
| `FTerritoryGuardPostTemplate` | `ReserveMinimumPlayerDistance` | `500.f` |
| `FTerritoryGuardPostTemplate` | `ReserveSpawnCandidateCount` | `12` |
| `FTerritoryGuardPostTemplate` | `ReserveCameraAvoidanceRetryLimit` | `3` |
| `FTerritoryGuardPostTemplate` | `ReserveTotalRetryLimit` | `10` |
| `FTerritoryGuardPostTemplate` | `ReserveOwnershipPolicy` | `EReserveOwnershipPolicy::RefillOnOwnerChange` |
| `FTerritoryGuardPostTemplate` | `PatrolRoute` | `—` |
| `FTerritoryGuardPostTemplate` | `bLoopPatrol` | `true` |
| `UTerritoryGuardPostDefinition` | `DisplayName` | `—` |
| `UTerritoryGuardPostDefinition` | `FactionOverride` | `—` |
| `UTerritoryGuardPostDefinition` | `NPCDefinition` | `—` |
| `UTerritoryGuardPostDefinition` | `ActivityConfiguration` | `—` |
| `UTerritoryGuardPostDefinition` | `TriggerSetOverrides` | `—` |
| `UTerritoryGuardPostDefinition` | `PatrolRoute` | `—` |
| `UTerritoryGuardPostDefinition` | `bLoopPatrol` | `true` |
| `UTerritoryGuardPostDefinition` | `ReserveSlots` | `1` |
| `UTerritoryGuardPostDefinition` | `ReserveSpawnDelay` | `3.f` |
| `UTerritoryGuardPostDefinition` | `ReserveSpawnRetryInterval` | `2.f` |
| `UTerritoryGuardPostDefinition` | `ReserveSpawnRadius` | `600.f` |
| `FTerritoryFactionGuardDefinition` | `Faction` | `—` |
| `FTerritoryFactionGuardDefinition` | `NPCDefinition` | `—` |
| `UTerritoryCounterAttackProfile` | `BaseLaunchProbability` | `0.15f` |
| `UTerritoryCounterAttackProfile` | `MinimumLaunchProbability` | `0.01f` |
| `UTerritoryCounterAttackProfile` | `MaximumLaunchProbability` | `0.95f` |
| `UTerritoryCounterAttackProfile` | `UnguardedLaunchProbability` | `1.f` |
| `UTerritoryCounterAttackProfile` | `AttackerPowerWeight` | `0.30f` |
| `UTerritoryCounterAttackProfile` | `DefenceDeterrenceWeight` | `0.45f` |
| `UTerritoryCounterAttackProfile` | `StrategicValueWeight` | `0.15f` |
| `UTerritoryCounterAttackProfile` | `ReadinessWeight` | `0.10f` |
| `UTerritoryCounterAttackProfile` | `InfluenceWeight` | `0.20f` |
| `UTerritoryCounterAttackProfile` | `FactionForces` | `—` |
| `UTerritoryCounterAttackProfile` | `QuestRules` | `—` |
| `UTerritoryCounterAttackProfile` | `GracePeriodGameTime` | `300.f` |
| `UTerritoryCounterAttackProfile` | `MinimumInfluenceTimingScale` | `0.25f` |
| `UTerritoryCounterAttackProfile` | `bRequirePlayerProximityForActivation` | `false` |
| `UTerritoryCounterAttackProfile` | `bGarrisonTriggersActivation` | `false` |
| `UTerritoryCounterAttackProfile` | `ActivationRadius` | `5000.f` |
| `UTerritoryCounterAttackProfile` | `bNotifyDefendingFactionOnly` | `true` |
| `UTerritoryCounterAttackProfile` | `NotificationRadius` | `12000.f` |
| `UTerritoryCounterAttackProfile` | `bContinueFiniteWavesAfterActivation` | `true` |
| `UTerritoryCounterAttackProfile` | `bUsePlayerRelativeReserveStaging` | `true` |
| `UTerritoryCounterAttackProfile` | `bUseUnattendedRecaptureHandover` | `true` |
| `UTerritoryCounterAttackProfile` | `UnattendedRecaptureDelayGameTime` | `30.f` |
| `UTerritoryCounterAttackProfile` | `bConcedeWhenDefendingPlayerDies` | `true` |
| `UTerritoryCounterAttackProfile` | `WaveStrategy` | `ETerritoryAssaultWaveStrategy::Legacy` |
| `UTerritoryCounterAttackProfile` | `ParticipantSpacing` | `220.f` |
| `UTerritoryCounterAttackProfile` | `SpawnPlacementAttemptsPerParticipant` | `4` |
| `UTerritoryCounterAttackProfile` | `ReserveMinimumPlayerDistance` | `1200.f` |
| `UTerritoryCounterAttackProfile` | `ReservePreferredPlayerDistance` | `2600.f` |
| `UTerritoryCounterAttackProfile` | `ReserveMaximumPlayerDistance` | `5500.f` |
| `UTerritoryCounterAttackProfile` | `PreferredCameraEdgeDot` | `0.55f` |
| `UTerritoryCounterAttackProfile` | `SameFloorHeightTolerance` | `500.f` |
| `UTerritoryCounterAttackProfile` | `MaximumApproaches` | `3` |
| `UTerritoryCounterAttackProfile` | `bUseNavigationAwareObjectives` | `true` |
| `UTerritoryCounterAttackProfile` | `bDistributeParticipantsAcrossObjectives` | `true` |
| `UTerritoryCounterAttackProfile` | `bCapConcurrentAttackersToNarrativeDifficulty` | `true` |
| `UTerritoryCounterAttackProfile` | `StalledMovementRetryInterval` | `1.5f` |
| `UTerritoryCounterAttackProfile` | `MaxStalledMovementRetries` | `8` |
| `UTerritoryCounterAttackProfile` | `DamagingEnemyMemorySeconds` | `20.f` |
| `UTerritoryCounterAttackProfile` | `bDamageRetaliationOverridesDefenderPriority` | `false` |
| `UTerritoryCounterAttackProfile` | `bPrioritizeTerritoryTakeover` | `true` |
| `UTerritoryCounterAttackProfile` | `DefendingPlayerEngagementPadding` | `800.f` |
| `FTerritoryFactionAssaultConfig` | `Faction` | `—` |
| `FTerritoryFactionAssaultConfig` | `AttackerDefinition` | `nullptr` |
| `FTerritoryFactionAssaultConfig` | `MilitaryPower` | `100.f` |
| `FTerritoryFactionAssaultConfig` | `EconomyReadiness` | `1.f` |
| `FTerritoryFactionAssaultConfig` | `SupplyReadiness` | `1.f` |
| `FTerritoryFactionAssaultConfig` | `RecentMomentum` | `0.f` |
| `FTerritoryFactionAssaultConfig` | `TerritorialInfluence` | `0.5f` |
| `FTerritoryFactionAssaultConfig` | `PlannedForce` | `6` |
| `FTerritoryFactionAssaultConfig` | `WaveSize` | `3` |
| `FTerritoryFactionAssaultConfig` | `TimePolicy` | `ETerritoryCounterTimePolicy::AnyTime` |
| `FTerritoryFactionAssaultConfig` | `TimeWindowStart` | `1800.f` |
| `FTerritoryFactionAssaultConfig` | `TimeWindowEnd` | `500.f` |
| `FTerritoryFactionAssaultConfig` | `RecurringCounterCooldownGameTime` | `900.f` |
| `FTerritoryFactionAssaultConfig` | `ScheduleMode` | `ETerritoryCounterScheduleMode::UnlimitedSeries` |
| `FTerritoryFactionAssaultConfig` | `MaximumScheduledAssaults` | `3` |
| `FTerritoryFactionAssaultConfig` | `bScaleLevelToRelevantPlayerPower` | `false` |
| `FTerritoryFactionAssaultConfig` | `EnemyLevelOffset` | `1` |
| `FTerritoryFactionAssaultConfig` | `MinimumScaledEnemyLevel` | `1` |
| `FTerritoryFactionAssaultConfig` | `MaximumScaledEnemyLevel` | `100` |
| `FTerritoryFactionAssaultConfig` | `PlayerPowerTiers` | `—` |
| `FTerritoryFactionAssaultConfig` | `PowerScalingEffect` | `—` |
| `FTerritoryFactionAssaultConfig` | `PowerScalingMagnitudePerEnemyLevel` | `0.f` |
| `FTerritoryFactionAssaultConfig` | `StagingRequirement` | `ETerritoryAssaultStagingRequirement::OwnsSecureDistrict` |
| `FTerritoryFactionAssaultConfig` | `bAllowStoryPursuitWithoutStagingDistrict` | `false` |
| `FTerritoryFactionAssaultConfig` | `SignatureVehicleClass` | `—` |
| `FTerritoryFactionAssaultConfig` | `bScaleVehicleCountByNarrativeDifficulty` | `true` |
| `FTerritoryFactionAssaultConfig` | `VehicleCountsByDifficulty` | `—` |
| `FTerritoryFactionAssaultConfig` | `ReserveWaveAlertDialogueTag` | `—` |
| `FTerritoryFactionAssaultConfig` | `TakeoverStartedDialogueTag` | `—` |
| `FTerritoryFactionAssaultConfig` | `FinalFightDialogueTag` | `—` |
| `FTerritoryFactionAssaultConfig` | `ActivityConfigurationOverride` | `nullptr` |
| `FTerritoryFactionAssaultConfig` | `TriggerSetOverrides` | `—` |
| `FTerritoryAssaultApproach` | `ApproachID` | `—` |
| `FTerritoryAssaultApproach` | `Type` | `ETerritoryAttackApproachType::Road` |
| `FTerritoryAssaultApproach` | `EntryType` | `ETerritoryAssaultEntryType::OnFoot` |
| `FTerritoryAssaultApproach` | `RelativeSpawnTransform` | `—` |
| `FTerritoryAssaultApproach` | `VehicleClass` | `—` |
| `FTerritoryAssaultApproach` | `RelativeVehicleDropOffTransform` | `—` |
| `FTerritoryAssaultApproach` | `RoadGuideID` | `—` |
| `FTerritoryAssaultApproach` | `RoadLaneSide` | `ETerritoryRoadLaneSide::Right` |
| `FTerritoryAssaultApproach` | `MaximumVehicleDeployments` | `1` |
| `FTerritoryAssaultApproach` | `VehicleOccupantCapacity` | `4` |
| `FTerritoryAssaultApproach` | `VehicleMaximumDriveSpeed` | `0.f` |
| `FTerritoryAssaultApproach` | `VehicleIngressTimeoutSeconds` | `120.f` |
| `FTerritoryAssaultApproach` | `MaxWaveSize` | `4` |
| `FTerritoryAssaultApproach` | `bEnabled` | `true` |
| `FTerritoryAssaultApproach` | `VehicleAwareness` | `—` |
| `FTerritoryAssaultApproach` | `VehicleRetirement` | `—` |
| `UTerritoryStealthProfile` | `bAllowStealthInfiltration` | `true` |
| `UTerritoryStealthProfile` | `EscalationScope` | `ETerritoryStealthEscalationScope::FactionWar` |
| `UTerritoryStealthProfile` | `ImmediateSightExposureThreshold` | `0.8f` |
| `UTerritoryStealthProfile` | `MinimumSightEvidence` | `0.2f` |
| `UTerritoryStealthProfile` | `GuardDetectionMultiplier` | `1.f` |
| `UTerritoryStealthProfile` | `SightSuspicionGainPerSecond` | `2.f` |
| `UTerritoryStealthProfile` | `SuspicionDecayPerSecond` | `0.2f` |
| `UTerritoryStealthProfile` | `MaximumStealthRating` | `100.f` |
| `UTerritoryStealthProfile` | `bPointBlankSightAlwaysExposes` | `true` |
| `UTerritoryStealthProfile` | `bFireWhileSeenExposes` | `true` |
| `UTerritoryStealthProfile` | `bFireWhileUnseenStartsInvestigation` | `true` |
| `UTerritoryStealthProfile` | `bDamageImmediatelyExposes` | `true` |
| `UTerritoryStealthProfile` | `bSeenDefenderKillExposes` | `true` |
| `UTerritoryStealthProfile` | `bUnseenDefenderDeathStartsInvestigation` | `true` |
| `UTerritoryStealthProfile` | `bAnonymousEvidenceCanExpose` | `false` |
| `UTerritoryDisguiseProfile` | `PerceivedFaction` | `—` |
| `UTerritoryDisguiseProfile` | `Quality` | `1.f` |
| `UTerritoryDisguiseProfile` | `ClearanceTags` | `—` |
| `UTerritoryDisguiseProfile` | `bCompromiseWhenFiringWhileSeen` | `true` |
| `UTerritoryDisguiseProfile` | `bCompromiseWhenDealingDamage` | `true` |
| `UTerritoryDisguiseProfile` | `bCompromiseWhenDefenderKillIsSeen` | `true` |
| `UTerritoryDisguiseProfile` | `bCompromiseFromScriptedReveal` | `true` |
| `UTerritoryDeveloperSettings` | `CaptureProgressPerSecond` | `0.1f` |
| `UTerritoryDeveloperSettings` | `CaptureProgressDecayPerSecond` | `0.05f` |
| `UTerritoryDeveloperSettings` | `CaptureTickInterval` | `0.1f` |
| `UTerritoryDeveloperSettings` | `TreatyExpirationCheckInterval` | `10.f` |
| `UTerritoryDeveloperSettings` | `MaxConcurrentScheduledAssaults` | `8` |
| `UTerritoryDeveloperSettings` | `MaxConcurrentAssaultsPerFaction` | `2` |
| `UTerritoryDeveloperSettings` | `MaxLiveCounterAttackNPCs` | `24` |
| `UTerritoryDeveloperSettings` | `MaxRetainedAssaultRecords` | `100` |
| `UTerritoryDeveloperSettings` | `CounterAttackUpdateInterval` | `2.f` |
| `UTerritoryDeveloperSettings` | `CounterAttackCampaignSeed` | `1337` |
| `UTerritoryDeveloperSettings` | `SpatialCellSize` | `2000.f` |
| `UTerritoryDeveloperSettings` | `bReputationDrivesDiplomacy` | `false` |
| `UTerritoryDeveloperSettings` | `HostileReputationThreshold` | `-50` |
| `UTerritoryDeveloperSettings` | `AlliedReputationThreshold` | `50` |
| `UTerritoryDeveloperSettings` | `DefaultPlayerFaction` | `—` |
| `UTerritoryDeveloperSettings` | `Notifications` | `—` |
| `UTerritoryDeveloperSettings` | `TerritoryHUDCardSize` | `FVector2D(328.f, 108.f)` |
| `UTerritoryDeveloperSettings` | `TerritoryHUDCardFillColor` | `FLinearColor(0.04f, 0.07f, 0.09f, 0.62f)` |
| `UTerritoryDeveloperSettings` | `bTerritoryHUDCardUsePanelTexture` | `false` |
| `UTerritoryDeveloperSettings` | `TerritoryHUDCardAlertExtraHeight` | `38.f` |
| `UTerritoryDeveloperSettings` | `TerritoryCommandScreenFillColor` | `FLinearColor(0.f, 0.f, 0.f, 0.f)` |
| `UTerritoryDeveloperSettings` | `bTerritoryCommandScreenUseBackgroundTexture` | `false` |
| `UTerritoryDeveloperSettings` | `TerritoryCommandPanelFillColor` | `FLinearColor(0.04f, 0.06f, 0.08f, 0.92f)` |
| `UTerritoryDeveloperSettings` | `TerritoryCommandPanelOutlineColor` | `FLinearColor(0.18f, 0.52f, 0.48f, 0.42f)` |
| `UTerritoryDeveloperSettings` | `TerritoryProgressFrameTexture` | `—` |
| `UTerritoryDeveloperSettings` | `TerritoryProgressFillTexture` | `—` |
| `UTerritoryDeveloperSettings` | `TerritoryInterfaceFont` | `—` |
| `UTerritoryDeveloperSettings` | `TerritoryTextScale` | `1.f` |
| `UTerritoryDeveloperSettings` | `DefaultTerritoryTextStyle` | `—` |
| `UTerritoryDeveloperSettings` | `TerritoryTitleTextStyle` | `—` |
| `UTerritoryDeveloperSettings` | `TerritoryHeadingTextStyle` | `—` |
| `UTerritoryDeveloperSettings` | `DefaultTerritoryButtonStyle` | `—` |
| `UTerritoryDeveloperSettings` | `TerritoryTabButtonStyle` | `—` |
| `UTerritoryDeveloperSettings` | `TerritoryActionButtonStyle` | `—` |
| `UTerritoryDeveloperSettings` | `bHideTerritoryHUDDuringNarrativeDialogue` | `true` |
| `UTerritoryDeveloperSettings` | `bUseCinematicParticipantLODDuringDialogue` | `true` |
| `UTerritoryDeveloperSettings` | `bIncludeAttachedActorsInCinematicLOD` | `true` |
| `UTerritoryDeveloperSettings` | `bEnableDebug` | `false` |
| `UTerritoryDeveloperSettings` | `DebugVerbosityLevel` | `5` |
| `UTerritorySetDiplomacyEvent` | `NewState` | `EDiplomacyState::War` |
| `UTerritorySetDiplomacyEvent` | `FactionASource` | `ETerritoryDiplomacyFactionSource::ExplicitTag` |
| `UTerritorySetDiplomacyEvent` | `FactionBSource` | `ETerritoryDiplomacyFactionSource::ExplicitTag` |
| `UTerritorySetDiplomacyEvent` | `FactionA` | `—` |
| `UTerritorySetDiplomacyEvent` | `FactionB` | `—` |
| `UTerritorySetDiplomacyEvent` | `bFallbackToExplicitFactionWhenContextMissing` | `true` |
| `UTerritorySetDiplomacyEvent` | `bRequireContainingTerritoryOwner` | `true` |
| `UTerritorySetDiplomacyEvent` | `bPreserveOtherActiveTerritoryWars` | `true` |
| `UTerritorySetDiplomacyEvent` | `TradeDurationGameTime` | `-1.f` |
| `UTerritorySetDiplomacyEvent` | `bApplyWhenStateStartsActive` | `true` |
| `UTerritoryDiplomacyCondition` | `FactionA` | `—` |
| `UTerritoryDiplomacyCondition` | `FactionB` | `—` |
| `UTerritoryDiplomacyCondition` | `RequiredState` | `EDiplomacyState::War` |
| `UTerritoryDiplomacyDialogueProfile` | `SameFactionDialogue` | `—` |
| `UTerritoryDiplomacyDialogueProfile` | `NeutralDialogue` | `—` |
| `UTerritoryDiplomacyDialogueProfile` | `CeasefireDialogue` | `—` |
| `UTerritoryDiplomacyDialogueProfile` | `NonAggressionDialogue` | `—` |
| `UTerritoryDiplomacyDialogueProfile` | `TradeAgreementDialogue` | `—` |
| `UTerritoryDiplomacyDialogueProfile` | `AllianceDialogue` | `—` |
| `UTerritoryDiplomacyDialogueProfile` | `WarDialogue` | `—` |
| `ATerritoryWorldState` | `WorldStateGUID` | `—` |
| `ATerritoryWorldState` | `CampaignCities` | `—` |
| `ATerritorySavableData` | `SavableDataGUID` | `—` |
