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

## Unlock for a reason

Locked Places should remain silent: they do not appear as capture choices and the
capture marker is hidden. Configure the reason in the Locked State Config's exit
conditions.

Easy examples:

- Finish quest `Find the Missing Informant` to unlock Blacksmith.
- Reach War with the Bandits to unlock their checkpoint.
- Read an intelligence document to reveal and unlock a hidden tunnel property.
- Complete a dialogue choice to make the property owner willing to surrender.

Do not use a second "start locked" Boolean. The State Config is the one source of truth.

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

## Concurrent attackers and target priority

Narrative Pro remains the tactical authority. Its difficulty attack-token count limits
how many NPCs actively attack one defender. Territory's combat director only adds the
strategic upper bound.

Territory guards may temporarily prioritize the closest hostile player while their Place
is Contested. This priority requires exact War diplomacy. Neutral / No Treaty never
becomes hostile merely because the player is visible. Registered defending guards remain
the preferred targets of assault participants, so a counterattack can attack the District
without the player being present.

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

For a SetByCaller modifier, set `Power Scaling Magnitude Tag` and
`Power Scaling Magnitude Per Enemy Level`. Example: the included project profile uses
`Territory.SetByCaller.PowerAttackDamage` and 1.5, so level 6 adds 7.5 Attack Damage.

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
