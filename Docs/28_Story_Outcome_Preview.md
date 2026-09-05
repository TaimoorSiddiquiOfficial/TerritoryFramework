# Story Outcome Preview

The preview consolidates Locked availability into one lifecycle row. If several analysis paths
produce the exact same rendered outcome, the duplicate is removed and its source references are
merged. Different success/failure branches remain separate even when they concern the same state.

The **Story Outcome (Read Only)** panel explains what a City, District, or Place Definition can
cause during play. It appears at the top of the Data Asset Details panel and updates after relevant
settings change.

The preview is an explanation, not another gameplay system:

- it does not save text into the Definition;
- it does not execute Narrative conditions or events;
- it does not roll counterattack probability;
- it does not change a faction, owner, quest, inventory, guard, music theme, or level actor;
- the Definition remains authoring truth, and saved/replicated runtime state remains campaign truth.

Use **Copy Report** when sharing a setup with another designer or including it in a review.

## How to read a scenario

Every expandable scenario uses the same simple language:

| Label | Meaning |
|---|---|
| When | What starts this branch |
| Only if | Conditions or runtime facts that must pass |
| Then | The configured result |
| If not | The safe failure or alternative branch |
| Also affects | Follow-on systems such as hierarchy, guards, music, or economy |
| Based on | The Definition category that owns the settings |

The label on the right explains certainty:

- **Configured Result** — directly known from the asset;
- **Runtime Condition** — depends on the live quest, player, faction, diplomacy, world or inventory;
- **Chance-Based** — a deterministic campaign decision still includes a probability roll;
- **Custom Blueprint** — Narrative provides a readable name, but custom Blueprint behavior cannot
  be predicted safely;
- **Setup Warning** — the current settings are missing, contradictory, or commonly misunderstood.

The collapsed **Setup Health** section uses the existing Territory Definition validator. Fix errors
before testing the story. Warnings describe an authoring risk but may be intentional.

The **Presentation > Passive gameplay HUD card** row explains the Definition's exact HUD policy.
It also states what remains available, so hiding a broad City card cannot be mistaken for disabling
notifications, POIs, the map, Command Center intelligence, or management.

## Blacksmith unlocks Castle Hill Farm

Example setup:

```text
Blacksmith
  Initial Availability: Unlocked
  Claimed Entry Event: Unlock Castle Hill Farm
  All Defenders Defeated Event: Activate Story Owner

Castle Hill Farm
  Initial Availability: Locked
  Locked Exit Condition: Blacksmith is owned by the live capturing faction
```

The preview explains these separate branches:

1. Defeating the Blacksmith guards may activate its Story Owner.
2. Guard defeat does not transfer ownership.
3. The dialogue/handover must complete a real faction change.
4. A real handover runs the old Claimed Exit events and new Claimed Entry events.
5. The Farm unlock event opens its ancestor path first.
6. Farm still opens only when its own Locked Exit Condition passes.
7. If the condition fails, Farm stays Locked and silent; its ownership is not rewritten.

This is why the panel uses **Runtime Condition** instead of promising that Farm will always unlock.

## Place scenarios

A Place preview includes the configured campaign seed, availability, stealth and exposure,
capture adapter, defender events, Story Owner handover, post-capture guards, income, production,
counterattacks, unattended recapture, management interaction, state events, command capabilities,
and Narrative music/state effects.

Examples:

- A production rule may say “produce Grain,” but it is conditional on claimed state, upgrade level,
  inputs, storage and the Narrative inventory account.
- An unlimited counterattack schedule means unlimited separate opportunities for finite battles.
  It never means infinite attackers inside one battle.
- A Story Owner appearing means the owner is ready to talk. It does not mean the Place is captured.

## District and City scenarios

District and City control is always read from the complete authored child arrays:

```text
all children loaded + unlocked + Claimed by one faction -> parent Claimed
mixed / partial / locked / contested child control      -> parent not securely Claimed
no political child control                              -> parent Unclaimed
```

A parent never pushes ownership down into its children.

Political Entry/Exit Conditions on District or City `Unclaimed`, `Contested`, and `Claimed` rows
cannot block this hierarchy reduction. The panel reports them as a setup warning. Parent state
events still run after the derived state is committed. Put an availability story gate in the
parent **Locked Exit Conditions**, or put a physical capture requirement on the relevant Place.

## Why outcomes are branches

The Content Browser has no live player, quest, diplomacy record, inventory, World Partition state,
Narrative difficulty, or counterattack decision seed. Evaluating those systems while editing would
be unsafe and misleading. The preview therefore describes both the success and failure path.

For the actual running campaign, use the Territory debug report and PIE tests. The Story Outcome
panel answers “what can these settings cause?” The runtime debugger answers “what is happening now?”
