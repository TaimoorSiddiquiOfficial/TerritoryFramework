# Disguises and Double-Agent Missions

## The simple idea

A disguise changes **who guards think the player is**. It does not change who the player
really is.

Example:

- true faction: Heroes;
- equipped uniform: Bandits;
- perceived faction: Bandits;
- capture credit, reputation, diplomacy and quest allegiance: still Heroes.

This separation is important. Adding Bandits to the Narrative PlayerState would make the player
a real Bandit and could accidentally change friendly fire, global diplomacy, Territory ownership,
dialogue, and later story choices.

## Narrative Pro integration

`UTerritoryDisguiseClothingItem` derives from Narrative Pro's
`UEquippableItem_Clothing`. It keeps the normal Narrative workflow:

1. Narrative inventory owns and saves the item.
2. Narrative equipment places it in an equipment slot.
3. Narrative changes the modular clothing mesh.
4. Narrative applies the equipment Gameplay Effect.
5. The Territory item activates its temporary perceived identity.
6. Unequipping removes that identity.

New disguise clothing defaults to **100 Stealth Rating**. Narrative's existing equipment
Gameplay Effect applies it through `SetByCaller.StealthRating`. Armor, Attack Rating, other
SetByCaller values, and granted abilities still work normally. Lower the inherited Stealth
Rating when the uniform should help identity but not make physical sneaking perfect.

Do not replace the equipment Gameplay Effect merely to add a disguise. Put the disguise rules in
`UTerritoryDisguiseProfile` and keep Narrative responsible for the equipment attributes.

## Create a Bandit uniform

1. Create a **Territory Disguise Profile** named `DA_Disguise_Bandit`.
2. Set Perceived Faction to `Narrative.Factions.Bandits`.
3. Set Quality to `1.0` for a perfect normal uniform.
4. Add clearance tags only when needed.
5. Create a Blueprint child of **Territory Disguise Clothing Item**.
6. Assign the clothing mesh, normal Narrative equipment slot, equipment values, and the profile.
7. Give the item through Narrative inventory, loot, dialogue, or a quest reward.

The item is saved and equipped through Narrative. Territory never edits the player's real
faction container.

## Territory security rules

Each active `Territory Stealth Profile` controls local security:

- **Allow Faction Disguises**: master switch for this Place/state;
- **Require Owning Faction Disguise**: a Bandit Place expects a Bandit uniform;
- **Minimum Disguise Quality**: public street may use `0.5`; headquarters may use `0.9`;
- **Required Disguise Clearance Tags**: officer floors can require
  `Territory.Disguise.Clearance.Officer`;
- **Compromise On Failed Identity Check**: failed checkpoints can burn the cover or only warn.

These rules may be selected by a Territory State Config. A quest can therefore make the lobby
accept common uniforms while a locked upper floor requires an officer badge.

## What guards do

When the uniform passes local security:

- guards can physically see the player but accept the perceived faction;
- ordinary sight and point-blank sight do not expose the player;
- diplomacy dialogue resolves against the perceived faction;
- Territory and reinforcement guards do not attack only because the true factions are at War.

The following evidence can burn the identity for the observing faction:

- firing while seen;
- dealing confirmed damage;
- killing a defender while seen;
- a scripted reveal;
- a failed identity or clearance check.

An unseen gunshot, bullet impact, corpse, or throwable still starts the existing investigation.
It does not magically identify the player. Once Bandits expose the player, the disguise can
remain valid for another faction until the story burns it globally.

## Gameplay tags and Gameplay Events

Runtime state tags:

- `Territory.State.Disguise.Active`
- `Territory.State.Disguise.Compromised`

Gameplay Events sent to the wearer's Ability System:

- `Territory.Event.Disguise.Activated`
- `Territory.Event.Disguise.Removed`
- `Territory.Event.Disguise.Compromised`
- `Territory.Event.Disguise.Restored`
- `Territory.Event.Disguise.IdentityCheckPassed`
- `Territory.Event.Disguise.IdentityCheckFailed`

Abilities can listen for these tags to play a heartbeat, warning effect, cover-blown montage,
temporary movement restriction, or UI message. The event payload contains the player, the
Disguise Profile, quality as Event Magnitude, and Territory context when available.

## Narrative events and conditions

Events:

- **Activate Territory Disguise**: useful for a cutscene; normal clothing activates itself;
- **Remove Territory Disguise**;
- **Set Territory Disguise Cover State**: compromise or restore one faction/global cover;
- **Perform Territory Disguise Identity Check**.

Condition:

- **Territory Disguise Condition** checks active, perceived faction, true faction,
  compromised faction, or acceptance by local Territory security.

Use Narrative's existing **Is Item Equipped** condition when a dialogue needs one exact clothing
Blueprint. Use the Territory condition when the story cares about identity rather than item class.

## Quest task

**Territory Disguise Mission Task** provides these objectives:

- Equip a Disguise;
- Enter Territory With Accepted Cover;
- Pass an Identity Check;
- Have Cover Compromised;
- Restore Cover Identity;
- Remove the Disguise;
- Leave Territory With Cover Intact.

The task is event-driven except for the two boundary objectives, which check at a small bounded
interval. It supports Territory and faction filters and uses the target Territory for its marker.

## Double-agent mission example

Player is truly Heroes. Heroes ask the player to discover why Bandits are receiving weapons.

1. Quest gives `BP_Item_BanditUniform`.
2. Task: **Equip a Disguise**, perceived faction Bandits.
3. Task: **Enter Territory With Accepted Cover**, Bandit warehouse.
4. A gate guard runs **Perform Territory Disguise Identity Check**.
5. Normal uniform passes the yard but cannot enter the officer floor.
6. Player steals an officer badge or finds an officer uniform.
7. Dialogue uses **Territory Disguise Condition: Accepted By Territory Security**.
8. Player may distract a guard, secretly rescue a prisoner, or plant evidence.
9. Firing while seen sends **Disguise Compromised**, starts Territory exposure, and may move the
   mission to an escape branch.
10. Task: **Leave Territory With Cover Intact** rewards a silent outcome. A separate branch can
    reward surviving after the cover is blown.

At every step the player remains Heroes. Bandit diplomacy is not rewritten simply because a coat
was equipped.

## Multiplayer and authority

Equip, unequip, identity checks, cover changes, suspicion, and Gameplay Events are decided by the
server. Narrative continues to replicate equipment and the Territory GAS state tags replicate
through the player's Ability System. Never let a client directly declare that its disguise passed.

## Important limits

- The integration changes perception for Territory guards and Territory assault/reinforcement
  characters. An unrelated custom Narrative NPC class must call the Territory perceived-faction
  API or derive from a Territory-aware guard class.
- A perfect disguise is not supernatural invisibility. Cameras, scripted biometric checks, quest
  reveals, and restricted areas can deliberately reject it.
- Disguise compromise is temporary mission knowledge, not global diplomacy and not Territory
  ownership.
