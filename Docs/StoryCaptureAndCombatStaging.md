# Story Capture and Staged Territory Battles

This guide explains how to build story capture, multiplayer capture, reserve arrivals,
multi-floor fights, and adaptive enemy difficulty without creating a second authority
beside Narrative Pro.

## Choose one capture presentation

Both presentations finish through the same server-authoritative Territory mutation.
They only change how the player asks to capture.

### Multiplayer: physical capture flag

Place `ATerritoryCapturePoint` inside the property and set `Target Territory Tag`.
Assign any flag, radio, or control-panel Static Mesh to `Capture Marker Mesh`.
Players contest through the existing overlap flow. The mesh is hidden while the Place
is Locked or otherwise unavailable when `Hide Marker While Capture Unavailable` is on.

Easy example: two teams fight at Blacksmith. After its defenders die, a player enters
the flag radius. Territory validates faction, diplomacy, defenders, and state on the
server before capture progresses.

### Story mode: owner NPC surrender

Use one Narrative NPC as the property owner. Do not let Territory spawn a duplicate
copy of that NPC.

1. Put the Place in a State Config with the correct owner, defenders, and lock/unlock
   conditions.
2. In `On All Defenders Defeated Events`, reveal or enable the Narrative owner NPC and
   start its surrender dialogue.
3. On the handover dialogue option, add `Territory Capture Eligibility Condition`.
   Select `Narrative Target Faction` when the dialogue target is the player.
4. Add `Territory Capture Event` to the accepted option and use the same faction source.

Easy example: the Bandit guards at Blacksmith are defeated. The blacksmith owner steps
outside and says, "I surrender the forge." Accepting the dialogue captures the Place for
the player's current exact faction. If the story later changes the player from Heroes to
Rebels, the same dialogue asset captures for Rebels without a hard-coded Heroes tag.

`b Force Capture` is for an explicit story override. Leave it off for normal surrender;
the eligibility condition then applies the same defender, lock, hierarchy, and diplomacy
rules as physical capture.

#### Ready-to-use owner handover

Configure the **Story Owner** template on the Place Definition: enable it, select the project
Blueprint derived from `ATerritoryStoryOwnerSpawner`, select a normal Narrative Pro
`NPCDefinition`, and set its relative transform/dialogue options. The Definition synchronizer
creates or updates the placed Blueprint. Do not author `Owner Spawn > NPC To Spawn` separately;
the Data Asset is the source. The spawner remains inactive before handover and saves and
replicates `Handover Activated`.

Add `UTerritoryOwnerHandoverEvent` to the correct modular State Config event list:

- A defended, already-unlocked Place normally uses `On All Defenders Defeated Events`.
- A story-locked Place uses the Locked State Config's `Exit Events`, so its owner stays
  silent until the authored quest, ownership, or diplomacy conditions unlock it.

The reusable event stores only the stable Territory tag. At runtime it resolves the matching
Definition-backed spawner, which remains safe across World Partition and asset duplication. On
the server it spawns exactly one owner and can start that NPC's Narrative dialogue immediately.
It does not create a client duplicate.

Automatic dialogue uses only the pawn/controller/Tales component supplied by the Narrative
event. If an environment or scripted kill has no player participant, the owner still appears but
remains manually interactable. The framework never chooses the first world player, so one
client cannot accidentally receive another client's handover dialogue.

For a story-only Place, enable `Story Capture From Territory Bounds` on the Place. The
complete `Bounds Shape` becomes the combat area, including every floor covered by that
volume. An eligible hostile player anywhere inside starts and holds `Contested`, so the
normal `Contested + War` State Config starts the fight. It never adds capture progress.
Living defenders block the handover; after they die, only the owner dialogue, quest, or
explicit Territory event can transfer ownership.

Any Capture Point targeting that Place is automatically inactive and its marker is
hidden while story-bounds mode is enabled. You may keep the actor in the level: turning
story-bounds mode off later makes the same point available for multiplayer/domination,
where it fills the normal pressure meter after defenders are gone. The deprecated
`Contributes Automatic Capture Progress` Boolean is no longer a mode switch.

Multi-floor example: Blacksmith has guards on the ground and second floors. Scale the
Place `Bounds Shape` to cover both floors. The player may enter either floor and start
the contest; no Capture Point is required on the stairs or ground floor.

For the Contested row, set diplomacy from `Current Owning Faction` to `Contesting
Faction` as War. For the Claimed row, return `Current Owning Faction` and `Opposing
Faction From Transition` to the desired peace/neutral state. The opposing source means
the old owner after capture, or the departing attacker after an abandoned fight. It does
not assume the player is Heroes.

Add `Territory Ownership Changed During Transition` to Claimed-state rewards or enemy
wave events that should run only after a real capture. Without it, leaving a contest can
re-enter Claimed for the same owner and incorrectly replay that event. Narrative's
inherited `Not` option gives the inverse rule when needed.

Current example in `/Game/HopDistrictTest`:

- Blacksmith has three guards. Their final defeat fires the owner handover event, which
  activates `StoryOwner_Blacksmith` and opens `DBP_BlacksmithHandover`.
- Farm Hill keeps its existing Locked State Config conditions. Exiting Locked activates
  `StoryOwner_Farm` and opens `DBP_FarmHandover`.
- Both dialogue choices use `TerritoryCaptureEligibilityCondition` followed by
  `TerritoryCaptureEvent`, with `Narrative Target Faction`. The exact player faction at
  the time of the choice becomes the new owner; Heroes is not hard-coded.
- Both Places use full-bounds story capture. Their existing physical points keep valid
  target tags for future multiplayer use but automatically remain inactive in story mode.

Set `Owner Interaction Distance` on the owner spawner, not on a native actor placed in
the level. Start with 300 cm. The owner Blueprint applies it to Narrative's interactable
component after spawning; Narrative's player interaction trace must also reach at least
that far.

## Counterattack and recapture outcome

A counterattack activates and attacks registered territory guards even when no player is
nearby. After the attackers defeat the complete defence front:

- A living defending player inside the Place or its parent District means the fight must
  continue. Ownership cannot change through the unattended timer.
- If no defending player is there, `Allow Unattended Recapture Countdown` starts a saved
  countdown. The default is 30 game-time seconds. Returning alive cancels that countdown
  and resumes the fight.
- If `Concede When Defending Player Dies` is enabled and the defending player dies inside
  the battle area, the recapturing faction receives the Place immediately, before respawn.
- If the player stays away until the countdown ends, the recapturing faction receives the
  Place. Territory emits its normal ownership event and the Command Center receives the
  loss report.

Easy example: Heroes own Blacksmith and assign one guard. Bandits attack while the player
is at Farm Hill. When the guard dies, a 30-second recapture warning begins. If the player
returns to Market Square, the timer stops and Bandits must defeat the player. If the player
does not return, Blacksmith is handed back to Bandits when the timer ends.

## Unlock for a reason

Locked Places should remain silent: they do not appear as capture choices and the
capture marker is hidden. Configure the reason in the Locked State Config's exit
conditions.

Easy examples:

- Finish quest `Find the Missing Informant` to unlock Blacksmith.
- Reach War with the Bandits to unlock their checkpoint.
- Read an intelligence document to reveal and unlock a hidden tunnel property.
- Complete a dialogue choice to make the property owner willing to surrender.

Do not use a second "start locked" Boolean. Author the initial availability and use the
Locked State Config for its conditions/events.

Use `Territory Unlock Event` for an explicit quest or dialogue unlock. Its target must be
the complete tag of the locked actor. `Unlock Scope` controls the transaction:

- `Automatic Hierarchy` on a Place unlocks its City/District path and that Place, but never a sibling.
- `Automatic Hierarchy` on a District or City attempts its authored descendants too.
- `Exact Only` attempts only the target.
- `Force Exact` and `Force Hierarchy` bypass local lock conditions for an intentional admin,
  migration, or cinematic override.

Every normal cascade still checks each actor's own Locked exit conditions. If Farm Hill has an
unfinished quest condition, unlocking Castle Hill reports Farm Hill as blocked instead of
silently opening it. A streamed-out authored child is reported as missing; it is never assumed
to be unlocked.

The event changes saved and replicated availability from Locked to Unlocked. It does not change
the political control state or owner, and it does not rewrite **Initial Availability** in the
Definition. A brand-new campaign therefore still begins behind the intended gate.

## Reserve waves that feel staged

Author real `Counter Attack Approaches` on the Territory. The runtime never invents a
spawn position and never bypasses navigation validation.

With `Use Player Relative Reserve Staging` enabled, valid routes are reordered so the
best available route is:

- near `Reserve Preferred Player Distance`;
- outside unfair pop-in distance;
- on the same floor when possible;
- near the left or right edge of the player's view;
- not directly behind the player and not in the center of the camera.

Easy multi-floor example: author `Street_West`, `SecondFloor_Stairs`, and
`Roof_NavLink` approaches. If the player reaches floor two, `SecondFloor_Stairs` is
preferred only when it has a complete NavMesh path. Reserves never materialize on the
floor just because its Z value is close.

Set `Reserve Wave Alert Dialogue Tag` on the faction force to let the first successfully
spawned attacker warn the player. Example line: "More of them on the stairs!" The normal
Territory notification still provides the strategic warning. The plugin includes the
example tag `Territory.Dialogue.ReservesArriving`; the dialogue asset supplies the line.

Guard-post reserves also use bounded retries. `Camera Safe Retry Limit` first relaxes only
the camera-avoidance preference. `Total Reserve Retry Limit` then abandons an impossible
deployment without consuming the reserve, allowing the owner handover instead of leaving
the Place permanently stuck because of missing NavMesh or collision.

## Concurrent attackers and target priority

Narrative Pro remains the tactical authority. Its difficulty attack-token count limits
how many NPCs actively attack one defender. Territory's combat director only adds the
strategic upper bound.

Territory guards may temporarily prioritize the closest hostile player while their Place
is Contested. This priority requires exact War diplomacy. Neutral / No Treaty never
becomes hostile merely because the player is visible. Registered defending guards remain
the preferred targets of assault participants, so a counterattack can attack an unlocked
Place and its garrison without the player being present.

## Adaptive difficulty after skill unlocks

`Scale Level To Relevant Player Power` is off by default. Turn it on per faction force
only when the game wants adaptive enemies.

The system reads the strongest relevant player's Narrative level and optional replicated
power tags. Narrative perk Gameplay Effects should grant tags such as
`Territory.Power.Tier.2`. Map each tag in `Player Power Tiers`, then use
`Enemy Level Offset = 1` for enemies one level above that power.

NPC level does not change constant attributes. If an NPC ability configuration has fixed
health, attack damage, and armour, assign `Power Scaling Effect` with scalable modifiers.
If the Narrative configuration already uses level curves, leave the effect empty.

For a SetByCaller modifier, set only `Power Scaling Magnitude Per Enemy Level`. Territory
always uses Narrative Pro's existing `SetByCaller.AttackDamage` tag. Example: 1.5 means
level 6 supplies +7.5 to the additive Narrative Attack Damage effect. The actual hit still
runs Narrative's normal AttackDamage, AttackRating, Armor and friendly-fire execution.

Keep Narrative difficulty and adaptive power separate:

- Narrative difficulty controls attack tokens and attack frequency.
- Territory adaptive power controls only newly spawned NPC level and an optional scaling
  Gameplay Effect.
- Weapon abilities control their base hit damage.

The Narrative demo one-handed sword ability used by the test Bandits has a base attack
damage of 55 and its heavy combo multiplier is 1.5. That can produce 82.5 base damage
against a 100-health character before other effects. For a real game, duplicate the
ability into the Territory/game content and tune it there; never edit the Narrative Pro
demo asset because a plugin update can replace it.
