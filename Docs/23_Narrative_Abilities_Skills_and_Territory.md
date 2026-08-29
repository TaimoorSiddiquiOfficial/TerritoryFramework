# Narrative Abilities, Skills, and Territory Integration

This audit describes the Narrative Pro systems currently available to Territory Framework. It
separates three ideas that are easy to mix together:

- a **Gameplay Ability** performs an action, such as crouch, sprint, reload, or attack;
- a **Gameplay Effect** changes a value or state, such as +50 Stealth Rating while crouched;
- a **Skill or Perk** is long-term progression purchased by the player.

The asset snapshot was checked against the project on 2026-08-30.

## Current player loadout in Haven Reach

The live `BP_TerritoryPlayerCharacter` Ability System starts with these nine abilities:

| Ability | Capability |
|---|---|
| `GA_Melee_Punch_Unarmed` | Basic unarmed melee attack. |
| `GA_Melee_Block_Unarmed` | Blocks while no weapon supplies a different block ability. |
| `GA_Crouch` | Toggles crouch and applies/removes the configured crouch stealth effect. |
| `GA_Sprint` | Uses stamina-driven faster movement. |
| `GA_Jump` | Performs the Narrative jump action. |
| `GA_Weapon_Wield` | Draws or holsters equipped weapons. |
| `GA_Weapon_Reload` | Reloads the active weapon when its rules allow it. |
| `GA_Death` | Owns the death ability flow. |
| `GA_Cover` | Enters and controls the Narrative cover flow. |

Equipment and interactable actors can grant more abilities at runtime. Therefore, this list is the
starting loadout, not a limit on the character.

The verified baseline values were 100 Health, 100 Stamina, 2 Stamina regeneration, 122 Attack
Rating, about 58.6 Armor, and 40 Stealth Rating. Narrative crouch applies `GE_CrouchStealth`, which
temporarily raises Stealth Rating from 40 to 90 in this setup.

## All Narrative Pro Gameplay Ability assets

Narrative Pro contains 39 `GA_` assets under its core Gameplay Abilities folder. Some are complete
player actions, while others are parent/base abilities intended for a child asset.

### Combat bases and melee

- `GA_CombatAbilityBase` — shared combat rules.
- `GA_Attack_ComboBase` — shared combo sequencing.
- `GA_Attack_Combo_Melee` — main-hand melee combo.
- `GA_Attack_Combo_Melee_Offhand` — off-hand melee combo.
- `GA_Melee_Punch_Unarmed` — unarmed attack.
- `GA_Weapon_Bash` — weapon bash attack.

### Bow, firearm, and throwable attacks

- `GA_Attack_Bow` — bow draw/fire flow.
- `GA_Attack_Firearm_Base` — common firearm behavior.
- `GA_Attack_Firearm_Projectile` — projectile firearm base.
- `GA_Attack_Firearm_Trace` — hit-scan/trace firearm base.
- `GA_Attack_Firearm_Proj_Launcher` — launcher projectile behavior.
- `GA_Firearm_Pistol` — pistol attack.
- `GA_Firearm_Pistol_R` — pistol variant.
- `GA_Firearm_Rifle` — rifle attack.
- `GA_Attack_ThrowGrenade` — throwable/grenade attack.

### Magic and special attacks

- `GA_Attack_Magic_Beam` and `GA_Attack_Magic_Beam_Offhand` — beam attacks.
- `GA_Attack_Magic_Proj` and `GA_Attack_Magic_Proj_Offhand` — magic projectiles.
- `GA_Attack_Magic_VoltBringer` — VoltBringer example attack.
- `GA_Attack_Magic_Fall` — fall-related example effect/attack.
- `GA_Attack_Magic_NPCControl` — NPC-control example ability.
- `GA_Attack_Magic_Ragdoller` — ragdoll example ability.

### Movement and survival

- `GA_Cover` — cover.
- `GA_Crouch` — crouch and temporary stealth effect.
- `GA_Dodge` — dodge.
- `GA_Jump` — jump.
- `GA_LockOn` — target lock-on.
- `GA_Sprint` — sprint.
- `GA_Death` — death flow.

### Weapon utility

- `GA_Melee_Block` — weapon block.
- `GA_Melee_Block_Unarmed` — unarmed block.
- `GA_Weapon_Aim` — weapon aiming.
- `GA_Weapon_Reload` — reload.
- `GA_Weapon_Wield` — wield/holster.

### Interaction

- `GA_Interact_AnimatedInteractable` — animated object interaction.
- `GA_Interact_Mount` — mount interaction.
- `GA_Mount_Vehicle` — vehicle mounting.
- `GA_Interact_Sit` — sitting interaction.

Most action abilities use Local Predicted execution. Interaction examples are Server Initiated, and
death is Server Only. This is important when extending them for multiplayer.

## Existing Narrative skills and perks

`USkillTreeComponent` is present on `BP_NarrativePlayerState`. Skill levels begin at 1. Purchasing a
perk consumes one skill point, increases the owning skill level, and saves skill/perk levels.
Narrative's source currently warns that perks are not fully multiplayer-supported even though the
component itself replicates, so perk purchases need an authority/replication pass before co-op use.

| Skill | Existing perks | Current capability and audit result |
|---|---|---|
| Melee / Combat | Godlike Strength, Weapon Bash, Critical Chance, Long Arms, Example Melee Perk | Godlike Strength applies an infinite Attack Rating effect at 5/10/20/30/40. Weapon Bash, Critical Chance, and Long Arms are mostly marker/demo assets and need gameplay consumers. Example Melee Perk is a placeholder. The skill asset also contains one empty perk entry. |
| Sneak | Feather Quiet | Five levels: 5/10/20/30/40. Its Gameplay Effect increases Narrative Stealth Rating through `SetByCaller.Sneak`. Territory sight reads the same attribute, so this perk already improves infiltration at normal distance. |
| Speech | Attuned Speaker, Gift of the Gab | Attuned Speaker is intended to reveal secret dialogue but has no self-contained effect. Gift of the Gab has five price levels but no Gameplay Effect class and uses the Sneak set-by-caller tag, so the sample is incomplete/misconfigured. |
| Fortitude | Juggernaut | Five levels: +5/+10/+20/+30/+40 Max Health through an instant Gameplay Effect. |

Perk prerequisites are links in the skill tree. The checked source accepts a purchase when at least
one linked parent prerequisite is owned. Do not assume all linked parents are required.

## Territory stealth as an ability integration

Territory Framework owns detection policy, not the player's movement implementation. The current
project uses Narrative's `GA_Crouch` as the stealth action and `GE_CrouchStealth` as its temporary
capability bonus.

Confirmed exposure now performs all of these steps:

1. cancel active abilities tagged `Abilities.Crouch`, `Territory.Ability.Stealth`, or any custom tag
   added to the profile;
2. remove only the temporary Gameplay Effect classes listed in `Stealth Gameplay Effects To Remove`;
3. send `Territory.Event.Stealth.Exposed` to the player's Ability System;
4. register the player as a contester when the profile escalation scope allows it;
5. let the Contested State Config decide diplomacy and War.

The Haven Reach rescue profile lists Narrative's `GE_CrouchStealth`. Permanent Feather Quiet or
equipment effects are intentionally not listed, so detection removes the temporary crouch bonus but
does not erase earned character progression.

At normal distance, higher Stealth Rating reduces effective guard sight. At point-blank distance,
valid Narrative sight forces exposure (300 cm by default), even at very high Stealth Rating. The
explicit Narrative `InvisibleToEnemies` tag remains respected.

For a future dedicated stealth Gameplay Ability:

```text
Ability Tags
  Territory.Ability.Stealth

Activation
  Apply your temporary stealth Gameplay Effect

End / Cancel
  Remove that temporary effect

Territory Stealth Profile
  Add the ability tag to Stealth Ability Tags To Cancel
  Add the temporary effect to Stealth Gameplay Effects To Remove
```

## Capture and fight requirement

Ordinary capture is fight-gated. A Place cannot change to a new Claimed owner while it still has a
registered living defender. The same guard rule now protects automatic progress, final completion,
direct ownership setters, and normal Narrative Capture Events.

Explicit `Force Capture` or a force story override may bypass defenders. That bypass is for a quest,
cinematic, migration, or administrator action and must be chosen deliberately.

A state-only `Contested -> Claimed` recovery is still allowed when the original defenders repel an
attack. It does not mean the Place changed owner. For that reason, capture-only rewards, waves, and
unlock events must use `Territory Ownership Changed During Transition`.

The Blacksmith Farm unlock now follows this safe flow:

```text
Blacksmith is captured by a different faction
  -> defenders are zero
  -> owner changes
  -> Blacksmith enters Claimed
  -> Ownership Changed condition passes
  -> Try Unlock Farm runs without Force
  -> Farm Locked exit condition checks Blacksmith is Claimed
  -> Farm unlocks
```

If Blacksmith merely returns to Claimed for the same owner, the Farm stays Locked and capture-only
events do not replay.

## Known Narrative sample-content findings

- `GA_LockOn` uses the `Abilities.Cover` tag in the inspected vendor asset.
- Several interaction abilities share `Abilities.Interact.Sit`.
- The off-hand melee base inspected without an ability tag.
- Gift of the Gab has no Gameplay Effect and points at a Sneak set-by-caller tag.
- Several one-level perks describe behavior but contain no direct graph/effect implementation.
- Narrative's own header notes that perks are not currently multiplayer-supported.

These are vendor/sample-content findings. Territory Framework does not modify Narrative Pro assets,
which keeps Narrative updates safer. Project children or Territory-owned assets should provide any
required correction.

## Recommended progression direction

Use Narrative skills to improve a capability, and let Territory read the resulting authoritative
attribute or tag. Good future examples are:

- Sneak reduces long-range sight gain and investigation duration, but never defeats point-blank sight;
- Command increases guard capacity and reinforcement speed;
- Intelligence improves espionage success and report detail;
- Logistics increases production storage or delivery reliability;
- Diplomacy unlocks more peaceful handover choices without hardcoding a player faction.

Keep every permanent perk separate from temporary stealth effects. That makes detection, save/load,
multiplayer authority, and character progression predictable.
