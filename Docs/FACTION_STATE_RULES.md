# Faction state rules and diplomacy-driven counterattacks

Open a Territory Definition → **State Configs** → the desired state. Common gameplay
fields apply to owners without an override. **Faction Overrides** maps an exact
Narrative faction tag to a replacement gameplay rule set. A matching override replaces
the common conditions, events, capabilities, economy permissions and counterattack
policy; it does not append them. Music and stealth stay on the common state row.

The selector is the territory's owner, not the nearest player or the attacking faction.
During an ownership handover, entry checks/events use the incoming owner and exit
checks/events use the outgoing owner. This also applies to a Claimed → Claimed
handover. A same-owner no-op and loading a save do not replay these events. A
world-level transition deliberately retains an empty Tales/player context.

## Attack modes

| Counterattack Policy | Behavior |
|---|---|
| After Capture (Profile Schedule) | Existing ownership-change response and profile-controlled recurring schedules; explicit Narrative Waves remain possible. Default for old assets. |
| Automatic While At War | Also permits the first schedule against an already-owned Place without needing a new ownership change. The scheduler checks eligible factions on its existing server timer. |
| Quest / Explicit Waves Only | Rejects automatic ownership-change and recurring schedules. Use the existing Wave of Enemies / Story Pursuit Narrative events. |
| No New Assaults | Rejects automatic and explicit new assaults. |

**Allowed Attacking Factions** is an exact-tag allowlist for both automatic and explicit
waves. Empty permits any otherwise eligible hostile force. These tags describe attackers;
the Faction Overrides map key describes the defending owner.

Every mode still requires War in Territory diplomacy, a valid physical target, Narrative
NPC configuration, finite forces, valid deployment routes, and available budgets. Peace,
alliance, trade, non-aggression and ceasefire remain hard blocks. The existing strategic
calculator, defence monotonicity, campaign seed, grace, clock window, warnings, profile
quest requirements, reinforcement/staging requirements, and wave strategy are reused.

An explicit immediate story Wave retains its existing deliberate behavior: it does not
roll whether to obey that immediate event, and may bypass automatic quest/staging/perk
gates. It still obeys state policy, the attacker allowlist, diplomacy, budgets, routes,
finite casualties and physical capture. Author the event's Narrative Conditions for its
quest requirements. Automatic While At War does not force the probability roll to pass.

While-at-war initiation starts one schedule for a territory/attacker that has no saved
evaluation history. Subsequent attempts follow the force profile's Single Assault,
Finite Series or Unlimited Series settings and cooldown. The saved cycle high-water mark
prevents trimmed history or reload from generating a new initial roll. A fresh ownership
change retains its existing ability to start a new response series.

Pending grace/warning assaults recheck the current owner/state policy and cancel with
`StateRuleBlocked` when it no longer permits them. An already physical battle is not
erased merely because entering Contested selects another state row; diplomacy still
cancels physical assaults. Existing Quest Runtime Overrides keep their pause behavior.

These controls do not disable unrelated capture mechanics. For a quest that must own
capture as well, configure the existing Quest Runtime Override's Automatic Capture
pause or the Place's authored capture availability.

## Example: only Heroes receive this Place's rewards and earnings

1. In Claimed common rules, leave Entry Events empty; disable **Allow Periodic Income**,
   **Allow Resource Production**, and **Allow Capital Capture Reward**.
2. Add `Narrative.Factions.Heroes` under Faction Overrides. Enable the desired economy
   permissions. Add Narrative reward/quest events to this override's Entry Events.
3. Set the override's counterattack mode and capabilities explicitly. It is a full
   replacement row, so copy any common conditions that must also apply to Heroes.
4. Configure the other relevant states too. For example, keep earnings disabled while
   Contested if that is the intended story policy.

This permits a Bandit-owned Place to exist without paying Heroes' reward or earning
income. When Heroes really take ownership, their entry bundle executes with the capture
context, and future earnings go through the existing Narrative account policy. A reward
event requiring a player does not invent a beneficiary during an AI/world transition.
Use an explicit faction-account Narrative event when that is the intended reward.

Income permission controls a Place's effective periodic currency, including upgrades
and capital multipliers. It does not waive guard upkeep. Resource permission controls
the existing daily Property production scheduler, not unrelated manual crafting calls.
Blocked production cycles expire, so re-enabling production does not pay those days later.

City/District Definitions now expose **Capital Capture Reward**, defaulting to their
previous 1000/500 amounts. Set it to zero to move the entire reward into authored
Narrative Entry Events. The selected owner's **Allow Capital Capture Reward** gates this
bonus. Generic currency operations outside Territory state events are unchanged.

## Persistence, networking and migration

- Volume remains the owner/state authority; CounterAttack remains the schedule authority;
  Economy owns rates and recipe scheduling; Narrative inventory owns actual currency/items.
- Common serialized property names remain available through `FTerritoryStateConfig`'s
  `FTerritoryStateGameplayRules` base. Existing assets default to previous behavior and
  empty faction overrides. No new faction database, actor GUID or capture authority exists.
- Runtime Narrative conditions/events are duplicated per Territory, including each override.
  Assets are authoring templates, not mutable shared event instances.
- Clients select the same authored rules using replicated owner/state. Mutations and reward
  execution remain on the server. `Get Active State Gameplay Rules` is a Blueprint read API.
  Remote map/directory views must use the existing always-relevant WorldState summaries;
  a local Territory actor outside its network relevancy radius can retain an older value
  until the player returns. This does not require making every physical city actor always relevant.
- Production records add a saved/replicated soft Definition reference and StateRulesVersion.
  This lets an unloaded Property resolve its current faction policy. A legacy version-zero
  site waits for its physical Place to register and supply the reference; missed blocked
  cycles are not paid retroactively. Missing referenced Definitions fail closed.
- Existing assault enums retain their numeric values; `StateRuleBlocked` is appended.
  Decision seeds, finite forces, wave strategy and cycle high-water records stay authoritative.
- No project story ownership mappings are assigned automatically. Apply these rules to the
  intended Definition assets; AlMalik still needs its actual story Territories authored.

Validation evidence for this batch is recorded in `COMPLETE_REAUDIT_2026-09-05.md`.
