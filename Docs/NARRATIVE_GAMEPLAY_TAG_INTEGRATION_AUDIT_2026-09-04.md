# Narrative Gameplay Tag Integration Audit — 2026-09-04

## Result

Territory Framework now uses Narrative Pro's Gameplay Ability, input, event, effect, weapon,
faction, state, damage, and UI contracts directly where those systems overlap. The distraction
projectile is owned by a Narrative Gameplay Ability, the rescue stealth profile has a real exposure
event, arms-shop Properties can grant upgrade-tier GAS benefits, and the Territory journal exposes
those benefits without requiring a binary Widget Blueprint rebuild.

This audit inspected Narrative Pro as a read-only vendor dependency. No `/NarrativePro` source or
content was modified.

## Inventory checked

`NarrativeGameplayTags.cpp` contains 210 executed native registrations across these roots:

| Root | Executed tags | Territory use |
|---|---:|---|
| `Narrative.*` | 146 | input, character/weapon/NPC states, equipment and groom slots, factions, settlements, POIs, tagged dialogue, animation, UI map layers, music |
| `Abilities.*` | 18 | ability identity, damage type and cancellation queries |
| `GameplayEvent.*` | 14 | combat, death, wield, reload, interaction and montage event delivery |
| `SetByCaller.*` | 13 | damage, attack scaling, healing, attributes, duration, XP and stealth magnitude |
| `Ability.*` | 8 | standardized ability-activation failure reasons |
| `Camera.*` | 8 | first/third-person and render-state behavior |
| `UI.*` | 3 | game, menu and modal Common UI layers |

Three `GameplayCue.*` examples are present but intentionally commented out because Narrative notes
that Gameplay Cues cannot use native-only tags. They must remain registered in project
`DefaultGameplayTags.ini`. The project currently contains 178 configured Gameplay Tag entries,
including the Narrative input mappings and cue tags.

## Contract map

| Narrative contract | Correct Territory integration |
|---|---|
| `Narrative.Input.*` | Ability input only. `GA_TerritoryDistraction` uses `Narrative.Input.Throw`; Narrative adds it to the Ability Spec's dynamic source tags and its ASC consumes the input. |
| `Abilities.*` | Ability classification/cancellation. The distraction uses `Abilities.Distraction.Throw`; exposure cancels `Abilities.Crouch` and authored stealth tags. |
| `Narrative.State.*` | Live ASC state and activation gating. The throw is blocked while dead, dialogue-controlled, or sequencer-controlled; stealth respects `InvisibleToEnemies`; cinematics use Narrative player/HUD state. |
| `GameplayEvent.*` | Transient action notification. Throw and impact use `GameplayEvent.Distraction.Thrown` and `.Impact`; Territory damage/death remains driven by Narrative ASC delegates and death flow. |
| `SetByCaller.*` | Effect magnitude. Counterattack scaling uses `SetByCaller.AttackDamage`; Narrative damage uses `SetByCaller.Damage`; permanent stealth progression uses Narrative's Sneak/Stealth attributes. |
| `Narrative.Equipment.*` | Equipment slots, wield slots, attachments, groom/mesh presentation. Property weapon rows point to `UWeaponItem` types but do not bypass Narrative inventory or wield authority. |
| `Narrative.Factions.*` | Faction identity used by ownership, diplomacy, hostility and player-faction resolution. Territory does not create a parallel team identity. |
| `Narrative.TaggedDialogue.*` | AI bark/investigation vocabulary. Territory hearing evidence feeds Narrative perception; Narrative remains the dialogue/bark owner. |
| `Narrative.Settlements`, `Narrative.POIs` | World/navigation projection from visible loaded Territory actors. |
| `Narrative.Music.Theme.*` | Territory state audio selects Narrative tagged music rather than implementing a second music state machine. |
| `UI.Layer.*` | Narrative/Common UI ownership. Territory journal content is hosted in the existing Narrative UI contract. |

## Distraction ability and stealth repair

`UTerritoryDistractionAbility` derives from `UNarrativeGameplayAbility`. It is Server Only, commits
through GAS, performs view/AI aiming, spawns the replicated Territory projectile with owner and
instigator, and emits the thrown event. The projectile ignores its thrower, reports one Narrative
hearing stimulus on first blocking impact, and sends the impact event back through the thrower's
Narrative ASC.

This separation is deliberate:

```text
Narrative input -> Gameplay Ability -> replicated projectile -> collision/hearing -> Narrative event
```

The Haven Reach rescue profile now serializes `Territory.Event.Stealth.Exposed`. Runtime also uses
that tag as a migration fallback when an older profile contains an empty serialized event, while
editor validation continues to report empty new authoring.

## Property, arms shop, and weapon upgrades

`UTerritoryPlaceDefinition` now contains a `Territory.Property.Role.*` role and Gameplay Benefit
tiers. Each tier declares its required Property upgrade level, one
`Territory.Property.Benefit.*` identity, Narrative ability/effect grants, and Narrative weapon
unlock rows.

The authoritative Player Management Component aggregates every active tier from loaded, Claimed
Properties owned by the player's current Narrative faction. It grants unique abilities through
`ANarrativeCharacter::AddAbility`, applies persistent effects through the Narrative ASC, adds
aggregated benefit tags, and removes its own handles when the benefit is no longer valid. It never
removes handles owned by Narrative equipment or another system.

The journal dynamically adds **BENEFITS** to its existing detail tabs. For the selected Property it
shows active/locked state, required upgrade, benefit tag, abilities, effects, and weapon items. One
upgrade control per Property shows the next level/cost and sends a validated server request to the
existing `ATerritoryProperty::TryUpgrade` economy transaction. The Blacksmith is authored as an
Arms Shop and its first tier grants the revocable Weapon Upgrades capability tag. The equipped
Throwable Rock owns the Territory distraction ability spec and uses that tag as a GAS activation
requirement, so Property benefits never duplicate an equipment-owned ability.

Weapon-item rows are unlock/catalog information. Narrative inventory/shop code must perform the
actual purchase or item grant. Weapon-specific attack/block/aim abilities must continue to live in
the weapon's `WeaponAbilities`, `MainhandWeaponAbilities`, or `OffhandWeaponAbilities`, so
Narrative can grant/remove them with the wielded item and use that item as the Ability source.

## Damage alignment confirmation

The new work does not introduce a second damage path. Narrative Gameplay Effects remain the only
Health/damage authority. Territory observes `OnDamagedBy`, `OnDealtDamage`, and Narrative death;
adaptive power uses `SetByCaller.AttackDamage`; invulnerability, armor, Attack Rating, Health and
death stay inside Narrative. See `NARRATIVE_DAMAGE_REAUDIT_2026-09-03.md` for the verified flow.

## Vendor defect found

Narrative Pro's `NarrativeGameplayTags.cpp` lines 44–45 register both the Melee Attack and Magic
Attack tag strings into `Ability_MeleeAttack`:

```cpp
AddTag(Ability_MeleeAttack, "Abilities.Attacks.MeleeAttack", ...);
AddTag(Ability_MeleeAttack, "Abilities.Attacks.MagicAttack", ...);
```

`Ability_MagicAttack` is declared in the vendor header but is never assigned here. Runtime code
that reads the fields therefore sees `Ability_MeleeAttack` overwritten with the Magic tag and an
invalid/uninitialized `Ability_MagicAttack`. Territory does not patch vendor source. The durable
fix belongs in an upstream Narrative update; project/Territory code should request the explicit tag
string it owns until that update is applied.

## Remaining work, prioritized

1. **P1 — World Partition durable Property benefits.** Current reconciliation can aggregate only
   registered, loaded Property actors because upgrade level is actor-owned. Add a World State
   snapshot containing stable Property identity, owner faction, upgrade level, and active benefit
   tier so an unloaded owned arms shop cannot temporarily revoke access.
2. **P1 — Shop transaction/UI.** The BENEFITS tab describes `UWeaponItem` unlocks, but it does not
   create a second inventory system. Connect those filtered rows to the existing Narrative store
   purchase transaction and currency authority.
3. **P1 — Gameplay Cue configuration gate.** Keep every required cue, including damage/fire weapon
   variants, in project config and validate it during cook; Narrative explicitly cannot provide
   those three cues as native-only tags.
4. **P2 — Property benefit event hooks.** Add Narrative quest/event conditions for “benefit tier is
   active” and “weapon unlock is available” when story graphs need those checks without querying UI.
5. **P2 — Broader Property authoring.** Classify the remaining economic Properties (clinic,
   vehicle shop, intelligence office, barracks, etc.) with role/benefit tags and project-owned
   effects/abilities after their design values are chosen.
6. **External — Narrative Magic tag typo.** Track the vendor correction and remove any temporary
   explicit-string workaround only after upgrading and re-running this audit.

## Verification evidence

- UE 5.7 `TDA Win64 Development` and `TDAEditor Win64 Development`: clean non-hot-reload builds
  with UHT warnings treated as errors.
- Full automation: 206 passed (200 clean and 6 intentional warning fixtures), 0 failed or skipped.
- Focused automation: 9 passed, 0 failed or skipped across Stealth, Property Benefits and editor
  asset creation.
- Cold asset commandlet: `GA_TerritoryDistraction`, projectile class, input tag, rescue exposure
  tag, Blacksmith role and benefit grant all loaded and matched expected values.
- Territory validation rejects wrong tag roots, duplicate/null tiers and grants, out-of-range
  levels, and Instant benefit effects that cannot be revoked.
