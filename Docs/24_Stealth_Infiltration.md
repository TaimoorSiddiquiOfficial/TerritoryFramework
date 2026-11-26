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

### Where the stone actually flies

The camera supplies the aim **point**, never the spawn point. The spawn stays at the throwing hand
(`Launch Socket Name`, `hand_l` by default) or at the avatar origin plus `Fallback Spawn Height` when
no socket exists, and `Forward Spawn Offset` only applies to that fallback. A camera-to-hand offset in
a third-person view is what used to send stones over the shoulder.

The launch direction is solved, not copied from the crosshair. With the authored `Initial Speed` of
1200 cm/s and gravity scale 1.0, a stone leaving along the crosshair sags 0.9 m at 5 m and 3.4 m at
10 m, which reads as "it does not follow the crosshair". `bCompensateLaunchForGravity` (on by default)
bends the throw up by the low ballistic arc so the same speed and gravity land on the traced aim point,
capped by `Max Compensated Launch Pitch` (55°). Targets beyond the ballistic reach, a target directly
overhead, or zero gravity/speed keep the straight aim direction instead of inventing an arc.

A `ServerOnly` ability runs on the server's copy of a remote client, where the camera is only refreshed
by `ServerUpdateCamera`. Until that first camera update `APlayerController::GetPlayerViewPoint` falls
back to the view-target actor rotation, which is yaw only, so a remote client's throw silently lost its
pitch. The ability now reads the possessed pawn's replicated eye location plus the replicated
`ControlRotation` in that window, and keeps the synced camera once the camera cache is live.

The camera supplies the aim point in one of two ways. While the game hides the cursor (the TDA demo
player controller captures and locks it), the screen centre *is* the pointer, so the camera view ray is
already the pointer ray and `bAimWithVisibleMouseCursor` changes nothing. When a project shows the
cursor, the pointer pixel is not the screen centre any more: the ability deprojects the mouse position
with `APlayerController::DeprojectScreenPositionToWorld` and rebuilds the aim ray through that pixel,
so the stone follows the mouse instead of the crosshair. A pointer that resolves behind the camera keeps
the crosshair ray. Cursor state is not replicated, so a remote client's pointer can never be honoured on
the server — that throw keeps the replicated aim described above.

The spawn direction is only half of it: the projectile must not re-interpret the launch vector. The
throwing ability spawns the projectile deferred, writes a world-space velocity (hand socket → aim
point), and then calls `FinishSpawning`. That call runs
`UProjectileMovementComponent::InitializeComponent`, which rotates `Velocity` by the component rotation
whenever `bInitialVelocityInLocalSpace` is true — the engine default. The stone was therefore rotated a
second time by its own launch rotation: a throw aimed along -X left along +X. `ATerritoryDistractionProjectile`
now sets `bInitialVelocityInLocalSpace = false` because every throw this plugin produces supplies world
space. Any custom projectile Blueprint assigned to `Projectile Class` must do the same, otherwise it will
inherit the double rotation.

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

## Wielded weapon contest gate

A story-bounds Place must not start a confrontation just because a player walked in. The stealth
profile owns the rule, and one shared helper enforces it on **every** route that can register a
contester.

| Option (`profile` → `07 Contest Gate`) | Meaning |
|---|---|
| `Require Wielded Weapon For Contest` | Default on. While the player's Narrative weapon is holstered the Place stays Claimed and the nearest guard warns instead of contesting. |
| `Holstered Weapon Warning Tag` | Line played while the player stays holstered. Empty uses `Territory.TaggedDialogue.Warn.HolsteredWeapon`. |
| `Holstered Weapon Warning Cooldown` | Minimum seconds between warnings for the same player (default 20). |

Admission rule (`UTerritoryBlueprintLibrary::DoesPawnAdmitStoryContest`):

```text
profile flag off          -> contest (legacy behavior)
war with the Place owner  -> contest (hostility is already declared, no weapon check)
weapon in a wield slot    -> contest (weapon drawn)
otherwise                 -> no contest; warn, guards keep investigating
```

"Wielded" reads Narrative's own state: a slot in `ANarrativeCharacter::GetWeaponWieldState().WieldSlots`.
An equipped-but-holstered weapon stays in `EquipSlots` only, so it does not count.

Both call sites must stay on that helper:

- `ATerritoryVolume::ReconcileStoryBoundsContesters` - the bounds/overlap route (`StoryBounds`).
- `UTerritoryControlSubsystem::ReportStealthEvidence` - the exposure escalation (`Exposure`). Without
  this one a spotted player is still contested while holstered, because guards report suspicion and
  exposure through their own observer component independently of the volume.

The warning line is delivered by the **nearest** registered defender via
`UTerritoryDiplomacyDialogueComponent::RequestTaggedDialogue`. That component owns the whole chain
natively for guards, assault characters and story NPCs: it resolves the row from the NPC definition's
`TaggedDialogueSet`, applies the NPC's `RequiredTags`/`BlockedTags`, calls Narrative's own
`PlayTaggedDialogue` for its per-instance cooldown bookkeeping, then plays the line through the
listener's Tales component (`UTalesComponent::BeginDialogue`, `Speakers[0]` = the NPC) - the same
call Narrative's own `NPCInteractable` makes.

Why the native chain is required: `ANarrativeNPCCharacter::ExecutePlayTaggedDialogue` is a
`BlueprintImplementableEvent`, and **nothing in this project implements it** - neither the vendor
`BP_NarrativeNPC`, nor `BP_TerritoryGuard`, nor `BP_TerritoryStoryNPC`. Without the C++ playback the
row would only be stamped (`LastPlayTime`) and never actually heard, which is exactly what "the
dialogue does not play" looked like.

Two routes now share one playback rule, `UTerritoryBlueprintLibrary::PlayTaggedDialogueLine(Row,
Speaker, DialogueInstigator)`:

| Route | Trigger | Path |
|---|---|---|
| Territory's own | the contest gate warns a holstered player | `RequestTaggedDialogue` → Narrative gate + `PlayTaggedDialogue` → helper (or the hook, if it already started one) |
| Narrative's own | the vendor BT/activity calls `PlayTaggedDialogue` (Taunt, BeginAttacking, Investigate.*, Flee) | `PlayTaggedDialogue` → the guard Blueprint's `ExecutePlayTaggedDialogue` **event** → helper |

`BP_TerritoryGuard` carries that event in its EventGraph (`Event ExecutePlayTaggedDialogue` →
`Play Tagged Dialogue Line`, `Speaker` = Self, `Row` = the event's `Dialogue` pin). It must be an
**Event node**: an `override_parent_function` function graph of the same name clashes with it and
leaves the Blueprint in an error state that `compile_blueprint` still reports as `UpToDate`.

The listener is chosen from: the explicit instigator (unless it is the speaker itself, which is what
Narrative's own callers pass), then the nearest player pawn inside the row's `MaxDistance`, then the
remaining player pawns. A candidate only wins if it actually has a `UTalesComponent`, so an NPC
instigator never swallows the line, and an instigator already inside a dialogue is skipped rather
than blocking the bark.

`ATerritoryVolume` keeps its own warning cooldown, and it only stamps that cooldown when the line
actually started - a listener already inside a dialogue is retried on the next evaluation instead of
being silenced for the whole cooldown.

The authored row lives in `/TerritoryFramework/AI/Dialogue/TaggedDialogues_TerritoryGuard`:

| Tag | Dialogue | Cooldown | MaxDistance | RequiredTags |
|---|---|---|---|---|
| `Territory.TaggedDialogue.Warn.HolsteredWeapon` | `/Game/TerritoryFramework/AI/Dialogue/DBP_TerritoryGuard_WarnHolsteredWeapon` (a project-owned copy so the line can be edited without touching vendor content; give it the real VO) | 20 | 5000 | none (plays while the guard is merely wary) — Blocked: `Narrative.State.IsDead`, `Narrative.State.NPC.IsBusy` so a fighting or dead guard does not warn |

While a warning is being delivered the warning defender carries `Territory.DialogueState.Wary`, and every
defender of a Place whose owner faction is at war with a visitor carries `Territory.DialogueState.Hostile`
- both written only by that defender's own `UTerritoryDiplomacyDialogueComponent`, driven from
`ATerritoryVolume::SyncDefenderDialogueStates` (the same evaluation that reconciles the contesters). No
state machine is involved; see `NARRATIVE_TAGGED_DIALOGUE_ATTITUDE_STATES.md` for that decision.

That set is a **superset of the vendor bandit set** (Taunt, BeginAttacking, the six `Investigate.*`
lines, plus the warning row), so replacing the guard definition's set does not remove the
investigation dialogue.

### Where the contest-gate options are visible in the editor

| Editor location | What you author there |
|---|---|
| Profile asset → `07 Contest Gate` | `Require Wielded Weapon For Contest`, `Ignore War For Weapon Requirement`, `Holstered Weapon Warning Tag` (tag picker, `Territory.TaggedDialogue`), `Holstered Weapon Warning Cooldown` |
| Same asset, Blueprint | `Get Effective Holstered Weapon Warning Tag` / `... Cooldown` (`BlueprintPure`) return the authored value or the native fallback, so a graph never needs its own fallback branch |
| `Project Settings → Territory → Debug → Tagged dialogue` | The `[TaggedDialogue]` log category (row, tag, dialogue class and listener of every started line) |
| `Project Settings → Territory → Debug → Enable Debug System` | Master switch for every Territory debug category |
| Guard dialogue asset (`TaggedDialogues_TerritoryGuard`) | The rows: tag, dialogue class, cooldown, max distance, required/blocked tags |

Guard definitions exist twice in this project, and the live NPCs use the **project** copies:

| Asset | Live? |
|---|---|
| `/Game/TerritoryFramework/AI/NPC_TerritoryBandit` (Hero / RangedGuard / BanditAssault) | yes — spawned guards report these paths |
| `/TerritoryFramework/AI/NPC_Territory*` (plugin content) | no, but keep it in sync |

Both copies must carry `TaggedDialogueSet = TaggedDialogues_TerritoryGuard` and a **null**
`Dialogue` field: an *empty string* soft class path there makes `ApplyDialogue(nullptr)` break
`ANarrativeNPCCharacter::OnCharacterDefinitionDataLoaded`, which then never populates
`TaggedDialogues`. Monolith's `save_packages` crashed the editor on that invalid state
(`FMonolithEditorActions::HandleSavePackages`), so save these assets with
`unreal.EditorAssetLibrary.save_loaded_asset` instead.

Debug: with `Project Settings → Territory → Debug → Enable Debug System` and the `Stealth`, `Capture`
or `Tagged dialogue` category enabled, the gate logs one line per delivered warning and one line when
a contest starts, including `wielded`, `relationship`, `aggressor` and the profile's own flags, plus
one `[TaggedDialogue]` line per line that actually started playing.

### The guard engagement decision

`ATerritoryGuardCharacter` calls the same rule before it fights an exposed player, so an unarmed
visitor who has not attacked anyone keeps **investigation and the warning line** instead of combat.
A guard still fights:

- a player who is wielding a weapon (that is the wielded-weapon test), or
- a player whose last stealth evidence is aggressive - `Damage`, `Gunshot`, `BulletImpact`,
  `FireSeen`, `DefenderKilledSeen`, or a story `Scripted` exposure.

`Ignore War For Weapon Requirement` therefore covers both the contest and the engagement. With it
off, an authored war admits a weaponless contest again (legacy behaviour).

### War is derived from Narrative attitude

The starting hostility in this project is authored, not random. Measured at PIE start:

```text
GameState          BP_NarrativeGameState_C
FactionAllianceMap Narrative.Factions.Bandits -> Narrative.Factions.Heroes : TeamAttitude.HOSTILE
Territory treaty   Narrative.Factions.Bandits <-> Narrative.Factions.Heroes : War
```

`UTerritoryDiplomacySubsystem::RefreshFromGameState` reads
`ANarrativeGameState::FactionAllianceMap[...].AttitudeMap` and derives treaties through
`AttitudeToDiplomacyState`; the reverse direction is `SyncNarrativeAttitudeForTreaty` /
`DiplomacyStateToAttitude`. The mapping is lossless for the supported states:

| Narrative attitude | Territory treaty | Narrative attitude written back |
|---|---|---|
| Friendly | Alliance / Trade / Non-Aggression | Friendly |
| Hostile | **War** | Hostile |
| Neutral | None (treaty row removed) | Neutral |

`Friendly` is never written back as `Neutral`. To change the starting relationship, author the
project's own GameState (or a project GameMode) rather than the vendor `BP_NarrativeGameState`, or
declare peace at runtime through `UTerritoryDiplomacySubsystem`, which syncs back to Narrative
attitudes and therefore to the dialogue-state tags.

Debug: with `Project Settings → Territory → Debug → Enable Debug System` and the `Stealth`, `Capture`
or `Tagged dialogue` category enabled, the gate logs one line per delivered warning and one line when
a contest starts, including `wielded`, `relationship`, `aggressor` and the profile's own flags, plus
one `[TaggedDialogue]` line per line that actually started playing.

> District and City actors are `AGGREGATE_ONLY`: they show `Contested` whenever their children are not
> uniformly owned and secured (`TerritoryHierarchyPolicy::ReduceControl`). That state is derived and
> is not caused by a player standing inside a child Place.

## Verification checklist

- Enter the Place unseen: state remains unchanged and the player is Undetected.
- Cross weak sight briefly: suspicion rises, then decays when every guard loses sight.
- Fire unseen: the closest guards investigate; the Territory does not expose the player yet.
- Let a guard confirm sight: the player becomes Exposed and the configured escalation runs.
- Kill a defender unseen: surviving guards investigate the corpse, not the hidden player actor.
- Throw a distraction on another floor: only nearby path-reachable guards respond.
- Join with two players: one may be exposed while the other remains independently tracked.
- Remove the profile: legacy immediate contest behavior still works.
- Enter a story-bounds Place with the weapon holstered: the Place stays Claimed and the nearest guard plays the warning line.
- Draw the weapon while inside: the next evaluation registers the contest and the Territory becomes Contested.
- Be at war with the Place owner: the weapon is never checked and the contest starts immediately.
- Leave and re-enter inside the warning cooldown: the line is not repeated.
