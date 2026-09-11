# Defender response and exposure cleanup

This batch separates a guard's combat decision from the rules that let a player
capture a Place. Narrative Pro source and assets are unchanged.

## Why the guards could stand still

Ordinary defenders required both Contested and War before Territory let Narrative
create an attack goal. A quest could pause automatic capture or the State Event
that declares War. Even a damaged guard then treated its attacker as neutral.

The guard now uses Native's `Hostiles`, `ShouldBeAggressiveTowardsTarget`,
`bAggressiveOnTakeDamage` and actual ASC `OnDamagedBy` notification for personal
defence. The existing Native controller still supplies damage perception and its
attack generator, activities and attack tokens still own fighting. No hit declares
War, unlocks a Place or transfers ownership.

The live test also caught an early damage hit producing an attack goal without
an exposure record. The existing ASC damage callback now records the
identified player's evidence directly, respecting the active profile's damage and
outside-evidence options. It does not need another sight event. Lethal/unseen-kill
handling retains the existing death-evidence policy.

## Authoring

In the Territory Definition, open **Guard Behavior > Combat**:

- **Allow Personal Retaliation** lets a guard fight a personally hostile actor.
  Enabled by default. Turn it off for guards that must not retaliate.
- **Defend Against Exposed Enemies** lets guards fight an exposed enemy faction at
  War even while a quest keeps the Place Claimed. Enabled by default.
- **Combat Target Factions** is optional. Empty accepts every otherwise eligible
  faction. Entries match exact perceived Narrative faction tags.

In **Quest Runtime Overrides**, **Pause Defender Combat** is a separate option.
It defaults to off. Pausing capture, counterattacks or State Events does not imply
pausing defender combat. The existing shared-world quest rule applies: one matching
online player's Tales quest can activate the authored override. Include Child
Territories retains its existing meaning. Explicit assault waves keep their own
combat policy.

The guard's **Evaluate Territory Target** query returns the decision and a reason.
Use it on the server; clients do not own guard perception or personal hostility.
The original **Can Engage Territory Target** node uses that same evaluation.

## Decision order

1. Require a living guard, a valid target, its owning Place and server authority.
2. Respect an explicit quest pause for defender combat.
3. Resolve Narrative identity and the existing disguise adapter.
4. Reject same perceived faction and protective treaties. Across multiple factions,
   a protective relationship takes priority over a different War pair.
5. Apply the optional exact combat-faction filter.
6. Allow authored personal retaliation through Narrative's personal hostility.
7. Otherwise require War. Active physical assaults retain their existing exception
   for the defended Place/front before Contested.
8. With stealth enabled, each player must be exposed separately. One player
   starting a fight never exposes another player. Local Alarm alone permits
   investigation; personal hostility is needed to fight.
9. Exposed-enemy defence may work while Claimed. Other ordinary faction combat
   retains the Contested requirement.

Peace, alliance, trade, non-aggression and ceasefire do not silently break when a
guard is hit. A story must explicitly change diplomacy to allow that combat.

## Outside shooters and exposure clearing

The Stealth Profile has **Accept Outside Threat Evidence**, enabled by default. Real
damage and heard shots/impacts may come from outside the Place. Ordinary outside
sight does not identify a passer-by. Once a shooter is exposed, actual Native
sight can continue tracking them outside. Anonymous sounds remain anonymous.
Seeing a projectile or thrown object never counts as seeing its hidden owner.
With outside evidence enabled, crossing the Place boundary keeps confirmed
exposure until it is explicitly cleared. Changing a quest capture lock also keeps
awareness; the lock controls capture participation, not what the guards know.

Outside evidence and personal retaliation are separate options. Disabling outside
evidence does not forgive an attacker already recorded in Native's Hostiles list.

Control now records why a physical actor participates: explicit capture, explicit
story contest, story bounds, or exposure. Duplicate reasons still count as one
actor. Only the capture reason produces automatic capture pressure.

**Clear Exposure** removes the exposure and bounds reasons, along with awareness.
It keeps explicit story and flag-capture participation. Leaving story bounds also
releases only the bounds reason. An empty zero-pressure story contest recovers on
the next authoritative control tick, even if a quest currently pauses capture.
Positive explicit progress retains the existing decay/save rules.
A visible or attacking player can be exposed again by the next real evidence report.

Clearing exposure is not forgiveness: Native personal hostility and a compromised
disguise are retained. Use their existing Narrative/disguise authoring APIs when
the story also intends forgiveness or a new disguise.

## Authority, migration and validation

`ATerritoryVolume` still owns political state. `UTerritoryControlSubsystem` owns
capture membership, progress and awareness. Native owns faction identity and local
hostility; Territory diplomacy owns treaties. Guard decisions derive from those
authorities. No new faction database, AI controller, capture subsystem, save format
or replicated property is added.

New asset options have safe defaults and old Blueprint signatures remain usable.
To retain the old clear-on-bounds-exit behavior, disable **Accept Outside Threat
Evidence** in the active Stealth Profile. With it enabled, confirmed outside
threats remain known until explicitly cleared or removed.
Defender Combat is appended to the quest override enum, preserving older values.
Participation reasons are transient and reconstructed by their physical callers;
they are not saved as live actor pointers. Existing political save records and
replicated Territory snapshots are unchanged.

Native's Hostiles list is also transient, not a new durable hostility record.
Freshly loaded guards use current quest/diplomacy policy and new perceived evidence.
The three existing Territory combat activities recheck this policy during Native
activity selection. A blocked activity stops on the next rescore; the Native goal
can remain for a later resume. Non-combat investigation and patrol remain eligible.

Evidence is recorded in `Saved/Verification/20260912_GuardResponse/`:

- UE 5.8 and the isolated UE 5.7 host each pass all 312 automation tests, with no
  failures or skipped tests. The two new regressions have no warnings. Existing
  fixtures produce warnings in 22 UE 5.8 tests and 24 UE 5.7 tests.
- Editor, Development and Shipping builds pass on both engines, including UHT.
- Validation checks 245 assets and compiles 147 Blueprints with zero errors and
  eight existing warnings. All 741 Native source files match the installed vendor.
- A HopDistrictTest server and two clients pass 24 checks: real outside GAS
  damage and exposure, a Native attack goal and melee
  activity, quest combat pause/resume, ceasefire interruption, a second player
  staying hidden, matching ownership and Claimed state, zero capture progress,
  rejected client evidence mutations, capture-lock changes, crossing the Place
  boundary and explicit outside exposure clearing. All three player pawns must
  finish spawning before the fixture starts. Temporary quest overrides were restored.
- Native regression coverage includes exposure before observer binding, retained
  flag capture pressure, explicit story contesting, clear-exposure cleanup,
  capture-lock and boundary transitions, a profile change during a callback,
  save/load without fabricated participants, owning-Place loss/re-resolution,
  optional faction filtering, treaty rejection and client authority rejection.
- UE 5.8 cook/package passes. The final boundary-only code changes were rebuilt
  and restaged with the same cooked content; the 60-second Development Game
  server-mode startup exits zero. The existing missing cutscene-player warning
  remains. The first cook's editor tool-server port conflict was resolved by
  closing the editor; no plugin server code was changed.

This is not a compiled dedicated-server or returning-client World Partition proof.
The previously recorded cold-appearance, restored-wave crash and returning-client
failures remain release blockers; this batch does not claim to repair them.

Reserve-event adapters, coordinated overheard conversations, patrol activity hooks
and the previously recorded World Partition/release blockers remain separate work.
