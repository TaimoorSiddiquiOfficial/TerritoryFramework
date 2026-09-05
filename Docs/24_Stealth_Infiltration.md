# Stealth Infiltration

Territory stealth lets a player enter an enemy Place for a rescue, espionage, sabotage, or
handover quest without turning the whole Place into `Contested` simply because they crossed a
volume. Narrative Pro remains responsible for character stealth rating, sight, hearing, damage,
NPC goals, activities, factions, dialogue, and the Ability System. Territory Framework converts
that evidence into a per-player infiltration state and decides when Territory conflict may begin.

## The simple rule

```text
Player inside bounds
    -> Undetected: no Territory conflict
    -> Suspicious: closest guards investigate evidence
    -> Exposed: register this player as a contester
    -> Contested state events decide diplomacy and War
```

Presence is not proof. An unheard player on another floor must not begin a war. A confirmed player
may begin conflict, but only through the active stealth profile and the conditions on the
`Contested` state events.

If no `Territory Stealth Profile` is assigned, the existing immediate story-bounds contest flow is
kept. This makes the feature opt-in and keeps old Territory Definition assets compatible.

## Create a reusable profile

1. Create a Data Asset whose class is `Territory Stealth Profile`.
2. Name it for the design, not for a single level. Example: `DA_Stealth_RescueMission`.
3. Set `Allow Stealth Infiltration` to true.
4. Choose an escalation scope:
   - `Local Alarm Only` makes guards investigate without contesting the Territory.
   - `Territory Conflict` exposes the player and starts contesting; state events decide diplomacy.
   - `Faction War Through State Event` is the clearest choice when the `Contested` event declares War.
5. Assign the profile to the Place Definition's `Default Stealth Profile`.
6. Use `Stealth Profile Override` in one state config only when that state needs different rules.

The active state override wins. The definition default is used when the state has no override.
This keeps stealth policy inside the same modular state-config system as capture, diplomacy, and
quest rules.

## Recommended Blacksmith rescue setup

For a Blacksmith owned by Bandits, where the player must rescue a neutral owner:

1. Enable `Story Capture From Bounds` on `DA_Place_Blacksmith`.
2. Assign `DA_Stealth_RescueMission` as its default stealth profile.
3. Keep the Bandits/Heroes attitude authored by the diplomacy system. Do not hardcode Heroes in a
   guard Blueprint.
4. On the `Contested` state, add `Territory Exposure Condition` with
   `Exposed Or Stealth Is Disabled` to the event that changes diplomacy to War.
5. Add normal quest conditions to the same event when required. Conditions are ANDed, so War may
   require both confirmed exposure and the appropriate quest stage.
6. Let defender deaths create corpse evidence. The story owner still appears through the normal
   all-defenders-defeated and handover flow; stealth does not replace capture criteria.

Example condition composition:

```text
Contested -> Set Diplomacy To War
Conditions:
  Territory Exposure = Exposed Or Stealth Is Disabled
  Quest State = Rescue mission is active
  Faction Attitude = not already at War
```

This supports betrayal stories: the same Place may later be recaptured for another player faction,
because the player and owning factions are resolved at runtime rather than assumed to be Heroes and
Bandits.

## Evidence behavior

| Evidence | Default result | Why |
|---|---|---|
| Weak/partial sight | Suspicion rises | A brief glimpse should not identify the player immediately. |
| Strong sight | Exposed | The guard has confirmed the player. |
| Fire while seen | Exposed | Identity and hostile action are both known. |
| Gunshot while unseen | Investigation | Guards know a sound location, not the shooter identity. |
| Unseen bullet impact | Investigation | Guards investigate the impact and estimated shot direction. |
| Damage | Exposed | The attacked guard has direct hostile evidence. |
| Defender kill witnessed | Exposed | A surviving observer can identify the killer. |
| Unseen defender death/corpse | Investigation | Nearby guards search the body location without magical knowledge. |
| Throwable distraction | Investigation | Only the closest configured number of guards are assigned. |
| Scripted reveal | Exposed | A quest or cinematic explicitly confirms the infiltrator. |

The effective sight value uses Narrative's sight stimulus, Narrative character `Stealth Rating`,
the profile detection multiplier, and Narrative's `InvisibleToEnemies` state tag. It does not
replace or copy Narrative perception configuration.

`Point Blank Sight Always Exposes` prevents a high Stealth Rating from making the player invisible
while standing directly in front of a guard. `Point Blank Sight Exposure Distance` defaults to
300 cm. Narrative AI Perception must still report valid sight, so walls and floors remain respected.
The explicit Narrative `InvisibleToEnemies` tag is also still respected. At longer distances,
Stealth Rating continues to reduce effective sight and slow suspicion growth.

## Multi-floor Places

Use one Territory bounds volume that covers the complete Place, including every floor. Guards use
their assigned Narrative controller perception and navigation path length when choosing who should
investigate. A sound on the second floor therefore does not expose a player on the ground floor;
guards receive an evidence location and must navigate to it.

Keep navigation connected with stairs, lifts, or nav links. If no valid navigation route exists,
the guard cannot stage a natural investigation even when the evidence is valid.

## Throwable distractions

Create a Narrative `UEquippableItem` for the throwable and grant `GA_TerritoryDistraction` from
that item's `Equipment Abilities`. The supplied **Throwable Rock** is the easy example. It uses
Narrative's `Narrative.Equipment.Slot.Mesh.Throwable` slot, so equipping and unequipping the item
grant and remove one ability spec with the rock as its source object.

The checked project uses the Blacksmith's `Territory.Property.Benefit.WeaponUpgrades` tag as an
optional capability gate through the ability's built-in `Activation Required Tags`. The
Blacksmith grants only that revocable tag; it does **not** grant a second copy of the ability.
This distinction prevents one `Narrative.Input.Throw` press from launching two projectiles. Losing
the Blacksmith removes the capability, while the rock remains a normal Narrative inventory item.

The ability is server-authoritative: it validates the equipped source item and capability, commits
cost/cooldown, traces from the player's view (or AI aim), and spawns
`BP_TerritoryDistractionProjectile` on the server. Exactly one rock is consumed only after the
projectile finishes spawning. Missing capability, empty inventory, failed aim, failed commit, or
failed spawn consumes nothing.

The projectile deliberately remains an Actor rather than pretending to be a Gameplay Ability. It
is the replicated physical payload and provides swept sphere collision, projectile movement,
bounce, owner/instigator collision filtering, and the `Territory Distraction` component. Duplicate
the Blueprint or change `Projectile Class` on a child ability to create a stone, bottle, or other
prop. The supplied rock uses the same authored mesh for its Narrative pickup and projectile
instead of the Engine sphere placeholder.

For an existing Narrative projectile child that already has a valid swept collision root, add
`Territory Distraction` directly. The component reports one tagged Narrative hearing stimulus on
its first blocking hit. Configure:

- `Loudness` for the strength of the Narrative hearing stimulus;
- `Maximum Range` for its useful radius;
- the profile's `Maximum Investigators`, `Investigation Radius`, and `Investigation Duration`.

For a quest-controlled distraction, call `Report Distraction At Location` on the server or use the
`Report Territory Distraction` Narrative Event. A distraction raises suspicion but does not expose
the player by itself.

The ability sends `GameplayEvent.Distraction.Thrown` to its Narrative Ability System after a
successful spawn. The projectile sends `GameplayEvent.Distraction.Impact` to its instigator after
the first blocking hit, with loudness as the event magnitude. Those hooks let Narrative quests,
effects, and conditions react without duplicating the physical throw or hearing logic.

## Quest conditions and events

Conditions available to Narrative quest/state configuration:

- `Territory Stealth Policy Condition`: is infiltration currently enabled?
- `Territory Exposure Condition`: Undetected, Suspicious, Exposed, or migration-safe Exposed/Disabled.
- `Territory Stealth Evidence Condition`: was the latest evidence sight, gunshot, corpse, and so on?
- `Territory Suspicion Condition`: has suspicion crossed a chosen percentage?

Events available to Narrative quests and Territory state configs:

- `Set Territory Stealth Infiltration Override`: enable, disable, or clear a quest override.
- `Reveal Territory Infiltrator`: force confirmed exposure for a scripted reveal.
- `Clear Territory Exposure`: reset an escape, disguise, cease-search, or mission-failure step.
- `Report Territory Distraction`: create anonymous evidence at a story-selected location.

Useful rescue example:

```text
Quest begins       -> Enable stealth override
Player distracts  -> Report distraction
Guard finds body  -> Evidence condition advances an optional objective
Player is seen    -> Exposure condition permits the Contested War event
Friend rescued    -> Clear exposure or disable the override for the escape battle
```

Clearing exposure does not automatically undo an already-authoritative Territory capture or
diplomacy change. Use the normal Territory and diplomacy events when the story requires a ceasefire
or a contest cancellation.

## Ability integration

On exposure, the control subsystem sends the profile's `Break Stealth Gameplay Event Tag` to the
player's Ability System. The default tag is `Territory.Event.Stealth.Exposed`. It also cancels active
abilities matching `Stealth Ability Tags To Cancel`. The defaults are `Abilities.Crouch` for
Narrative Pro and `Territory.Ability.Stealth` for a dedicated project ability.

Narrative Pro's `GA_Crouch` is an instant toggle, so the ability may already be inactive while its
infinite `GE_CrouchStealth` effect is still applied. Add that effect to `Stealth Gameplay Effects To
Remove` and leave `Remove Active Stealth Effects On Exposure` enabled. This removes the temporary
+50 crouch bonus on detection. Do not add permanent Sneak perks or equipment effects: those are
player capabilities and must survive detection.

Easy setup:

```text
Stealth Ability Tags To Cancel
  - Abilities.Crouch
  - Territory.Ability.Stealth

Stealth Gameplay Effects To Remove
  - GE_CrouchStealth
```

## Multiplayer and persistence

Infiltration is server-authoritative and tracked separately for every player. One exposed player
does not fabricate sight evidence for a hidden co-op partner. Suspicion and current observer lists
are transient encounter data; ownership, capture, diplomacy, and quest state remain in their
existing save authorities.

Use explicit quest state for long mission checkpoints. Do not expect a mid-search save to restore
individual guards' short-lived sight memories.

## Verification checklist

- Enter the Place unseen: state remains unchanged and the player is Undetected.
- Cross weak sight briefly: suspicion rises, then decays when every guard loses sight.
- Fire unseen: the closest guards investigate; the Territory does not expose the player yet.
- Let a guard confirm sight: the player becomes Exposed and the configured escalation runs.
- Kill a defender unseen: surviving guards investigate the corpse, not the hidden player actor.
- Throw a distraction on another floor: only nearby path-reachable guards respond.
- Join with two players: one may be exposed while the other remains independently tracked.
- Remove the profile: legacy immediate contest behavior still works.
