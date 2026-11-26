# 45 — Territory Skill Set & Upgrade Tiers (plan)

Status: **core implemented** — data model, faction-scoped tiers, perk gate, multi-purpose costs, scripted upgrades, capture events and deterministic AI balance exist and are tested. Perk assets and UI wiring are the remaining authoring layer.

---

## 1. Vendor system we reuse (verified)

| Piece | Reality |
|---|---|
`UTreeSkill : UObject` (`EditInlineNew`, Blueprintable) | `Perks : TArray<FPerkConfig>`; each config has `Perk`, `PerkCords`, `LinkedTo`. `SkillDisplayName`, `SkillDescription`, `SkillLevel`. |
`UTreePerk : UObject` | `MaxLevels`, `PerkLevel`, `LinkedPerks`/`LinkedPerkClasses`, display fields; `SetPerkLevel(int32)` is a **BlueprintNativeEvent** — the intended extension point. |
`USkillTreeComponent` on the player | `GiveSkillPoints`, `BuyPerk`, `CanBuyPerk`, `GetPerkLevel`, `GetPerk`, `HasPerk`; saves via `FSavedSkill` / `FSavedPerk`. |
`TreePerk_AttributeModifier` | Vendor's ready-made attribute perk (percent/flat modifiers). Useful for simple numeric perks, not for Place mutations. |
`Skill_Sneak` example | `Perks = [Perk_Sneak_FeatherQuiet @ (50,0)]`, max 5 levels; its GE is authored but never applied (see Docs/44 §8). |

## 2. Shape

```
Skill_Territory  (UTreeSkill)
  Perk_TerritoryUpgrade_I    "Upgrade I — Unlock"     LinkedTo: _
  Perk_TerritoryUpgrade_II   "Upgrade II"             LinkedTo: Perk I
  Perk_TerritoryUpgrade_III  "Upgrade III"            LinkedTo: Perk II
```

Two layers, deliberately separate:

| Layer | Authority | Meaning |
|---|---|---|
Skill tier | `USkillTreeComponent` + server validation | The **capability** to upgrade Places up to tier N. Player-wide, saved by the vendor component. |
Place upgrade | `ATerritoryVolume` / Place definition (`UpgradeLevel`, SaveGame + replicated) | The **actual** upgrade of one Place. Costs currency/items, takes time, applies effects. Never inferred from a perk. |

So `Upgrade I` does not silently level every Place; it **unlocks** tier 1 upgrades. Buying the tier on a
Place remains a separate, server-authoritative action.

## 3. Assets to author

| Asset | Type | Contents |
|---|---|---|
`Skill_Territory` | `UTreeSkill` child | three perk configs, coordinates, `LinkedTo` chain. |
`Perk_TerritoryUpgrade_I/II/III` | `UTerritoryUpgradePerk` (C++ child of `UTreePerk`) | `UnlockedTier` 1/2/3, `MaxLevels = 1`, description/cost display. |
`UTerritoryUpgradeDefinition` | `UPrimaryDataAsset` (new, Territory) | per-tier rows: cost (currency + items), build time, required perk tier, effects (income multiplier, guard capacity, fortification, command capabilities). |
`UTerritoryUpgradePerk` | C++ | `SetPerkLevel` → server-only `UTerritoryUpgradeSubsystem::SetUnlockedTier(Player, Tier)`. |

## 4. Flow

```
Player buys Perk_TerritoryUpgrade_II
  → USkillTreeComponent::BuyPerk (must be server-validated)
  → UTerritoryUpgradePerk::SetPerkLevel(1)
  → UTerritoryUpgradeSubsystem::SetUnlockedTier(Player, 2)      [authority]
  → HUD/journal and the Place panel read the unlocked tier

Player upgrades a Place to level 2
  → Place panel asks UTerritoryUpgradeSubsystem::CanUpgrade(Place, 2)
        requires: unlocked tier >= 2, cost, ownership, not contested-locked, build slot
  → ATerritoryVolume::ApplyUpgrade(2)                            [authority, atomic]
        validate → calculate final state → commit → reconcile guards/economy → replicate → save → events
```

## 5. Server-authority notes

- `USkillTreeComponent::BuyPerk` is `BlueprintCallable` and the vendor component can exist on a client
  pawn. The perk path must not trust the client: either route the purchase through a server RPC that
  calls `BuyPerk` on the authority, or have `UTerritoryUpgradePerk::SetPerkLevel` refuse to record a
  tier unless `HasAuthority()`. Skill points granted by quests are server-side already.
- The Place mutation stays inside `ATerritoryVolume` + control subsystem per AGENTS.md §4.2/4.3. The
  perk only widens the allowed tier.
- Do not persist the unlocked tier twice: the skill tree saves the perk; the Place saves its own
  `UpgradeLevel`. The subsystem derives the tier from the component at read time.

## 6. Integration loops this unlocks

- **Economy**: upgrade cost in `UTerritoryUpgradeDefinition`, paid through Narrative inventory.
- **Guards**: capacity / quality per tier feeds `UTerritoryGuardPostDefinition` and the counterattack
  vulnerability inputs (more guards must monotonically reduce launch probability).
- **Counterattack**: fortification tier is a required input, not a probability roll.
- **Diplomacy**: some treaties or state events can require a minimum upgrade tier.
- **Tales**: conditions read the unlocked tier / Place upgrade level; upgrade completion fires a
  Narrative event.
- **UI**: the vendor skill-tree menu hosts `Skill_Territory`; the Place panel shows tier, cost and the
  next requirement. No duplicate screen stack.

## 7. Implementation steps when we start

1. `UTerritoryUpgradePerk` + `UTerritoryUpgradeSubsystem` (authority, replicated read).
2. `UTerritoryUpgradeDefinition` data asset + editor factory/menu registration.
3. `Skill_Territory` + three perk assets, wired `I → II → III`.
4. `ATerritoryVolume` upgrade mutation (validate, commit, reconcile, save, events) with tier gate.
5. Tests: perk gate blocks tier 2 without Upgrade II; upgrade cost/commit; save/load of tier + Place
   level; two-client replication; failure paths (contested lock, insufficient funds).
6. UI hooks: Place panel + skill-tree menu entry.

## 8. Open questions

- Upgrade cost: Narrative currency only, or currency + authored items?
- Is the unlocked tier **account-wide**, or per faction / per city hierarchy?
- Can a Place be downgraded or lose a tier on capture? (Today ownership changes reset contest state,
  not upgrade level.)
- Should Upgrade III require a story quest in addition to the perk?

---

## 9. Open questions — fact-based scenarios

The facts below are from the shipped code, not assumptions.

```
ATerritoryProperty::TryUpgrade(AActor* Requester)          Private/Core/TerritoryHierarchy.cpp:963
  requires: HasAuthority, Claimed state, no mutation in progress,
            Requester in the owning faction, CanUpgrade (UpgradeLevel < MaxUpgradeLevel)
  cost:     GetUpgradeCost() = UpgradeCostPerLevel × (UpgradeLevel + 1)   → 500 / 1000 / 1500
  payment:  UTerritoryEconomySubsystem::DebitCurrencyWithResult(Requester, ...)
            (per-actor Narrative account; the faction wallet API is deprecated)
  atomic:   ApplyUpgradeLevel(PurchasedLevel) → debit → on Rejected roll back to OldLevel
  commit:   PublishUpgradeLevelChange → replicated (OnRep_UpgradeLevel) + SaveGame
Capture:    ReconcileOwnershipDependentSystems → new owner && level > 0 → SetUpgradeLevel(0)
Skill tree: USkillTreeComponent lives on ANarrativePlayerState (per player, saved with the player state)
```

### Q1 — Currency only, or currency + items?

| Scenario | Walk-through | Consequence |
|---|---|---|
**A. Currency only (current)** | Player (Bandits) stands in a Claimed Bandit Place, level 0 → cost 500. Has 600 → `TryUpgrade` stages level 1, Narrative debit succeeds, level replicated, next economy tick `TerritoryCurrencyRates::Income(..., UpgradeLevel=1, ...)` pays the bonus. Save/load keeps level 1 and the reduced balance. | Zero new code. The whole atomic/rollback/replication path already exists. No item authority involved. |
**B. Currency + items** | Same, but tier 2 also needs 10 Iron. Player has 500/1000 coins + 9 Iron → rejected before any debit; nothing consumed. Need `UTerritoryUpgradeDefinition` per-tier item rows and a validate-both-then-commit order (items first validated, then currency, then level; rollback level + items if debit is rejected). | More authoring, but the transaction already has the rollback pattern to copy. **Fact that matters:** `TryUpgrade` requires a **player in the owning faction**; there is no faction wallet, so item costs do not create an NPC-upgrade path either. |
**C. Mixed by tier** | I & II currency, III currency + quest item. | Best of both without touching the early economy. |

Recommendation: ship **A** now (it is already implemented and tested by the replication/save path), add
`UTerritoryUpgradeDefinition` before choosing B; keep every tier in the same atomic order.

### Q2 — Account-wide, per faction, or per city?

| Scenario | Walk-through | Facts that decide it |
|---|---|---|
**A. Account-wide** | Bandit players A and B. A buys Upgrade II; B did not. A can upgrade a Bandit Place to 2, **B cannot** — same faction, same Place, different permission. | `USkillTreeComponent` is on `ANarrativePlayerState`, saved per player. This is the vendor's native scope and needs no new storage. Only coherent for a single protagonist. |
**B. Faction-wide** | A buys Upgrade II → a server-side bridge writes “Bandits unlocked tier 2”. Both A and B can then upgrade Bandit Places. | Needs a new durable record (Territory subsystem + SaveGame + replication) because no vendor faction skill tree exists. Perk purchase is still per player (skill points). |
**C. Per city** | Each City keeps its own tier; a District Place can only be upgraded by the city that owns it. | Most storage and UI; only worth it if “city specialization” is a design pillar. Hierarchy aggregation must then define how a City’s tier applies to its Places. |

Recommendation: **A** for the single-player campaign now; add the **B bridge** the moment two players
can share a faction, because `TryUpgrade` already asks “is the requester in the owning faction?” and a
per-player perk would contradict that faction-level question.

### Q3 — Keep, reset, or downgrade the Place level on capture?

| Scenario | Walk-through | Facts |
|---|---|---|
**A. Keep** | Bandits upgrade a Place to 3 (paid 500+1000+1500). Heroes capture it → Heroes immediately get level-3 income bonus and guard capacity without paying. | Requires **removing** the current reset in `ReconcileOwnershipDependentSystems`, and `OnPropertyCaptured` compatibility reset. Capture becomes a way to buy upgrades with blood instead of money. |
**B. Reset to 0 (current)** | Same capture → level 0; Heroes must pay 500/1000/1500 again. Investment is lost with the Place. | Already implemented and saved/replicated correctly. No code change. |
**C. Downgrade by one** | Level 3 → 2 on capture. New owner keeps some value, attacker loses some. | New rule; needs an explicit definition flag and save/migration handling; also `GetUpgradeCost` then charges 1500 again for level 3. |

Recommendation: keep **B** (current, zero risk). If “captured value” is wanted, add
`bPreserveUpgradeOnCapture` to the Place definition rather than changing the default.

### Q4 — Should Upgrade III require a story quest?

| Scenario | Walk-through | Facts |
|---|---|---|
**A. Perk only** | Buy Upgrade III, pay 1500, level 3. | `TryUpgrade` already only checks cost/faction/Claimed. Adding the perk gate is one condition. |
**B. Perk + Tales condition** | `UTerritoryUpgradeDefinition` tier 3 carries an inline `UNarrativeCondition` (quest completed / faction rep). `TryUpgrade` evaluates it server-side before staging the level, returns a reason text otherwise. | Tales conditions are `EditInlineNew` and already used by Place state rules; quest progress is saved by Tales. No new save data. |
**C. Perk + quest item** | Quest rewards a “City Charter” item; tier 3 upgrade consumes it together with the currency. | Items are Narrative inventory (authority), so it shares Q1-B’s transaction work; simpler than condition authoring for designers. |

Recommendation: **B** if the story must gate the tier, **C** if designers prefer a physical reward.
Either way the gate belongs in `TryUpgrade` before `ApplyUpgradeLevel`, so a rejected gate never
publishes a level or a debit.

---

## 10. Decisions (user answers) and the resulting best approach

These supersede the earlier recommendations where they differ.

| # | Decision |
|---|---|
**D1** | The **perk only unlocks the upgrade tier**. The cost itself is **multi-purpose**: currency, resources/items, quest condition, or **free** — composed from vendor conditions and events so a designer can author several upgrade routes, including a free tutorial upgrade. |
**D2** | The tier unlock is **faction-scoped**: in a team, **one** member buying the perk unlocks it for the whole faction; every member can then upgrade. In a single campaign the **AI factions auto-upgrade** to stay near the player's tier, otherwise the game becomes trivial. Multiplayer solo match behaves like the single campaign. |
**D3** | Capture **keeps the existing reset to level 0**, and additionally fires a **Narrative event** carrying the Place, its old level and the new owner, so a quest can drive upgrades whenever we want — including upgrading a Place we do not own. |
**D4** | **Upgrade III needs no story quest**: the perk is the only mandatory gate. A quest condition can still be used as one of D1's optional cost paths, never as a hard requirement. |

### 10.1 Data model (vendor conditions/events, instanced inline)

```cpp
UENUM(BlueprintType)
enum class ETerritoryUpgradePayment : uint8
{
    Currency,        // Narrative account debit (the current path)
    Item,            // Narrative inventory consume
    ConditionOnly,   // a UNarrativeCondition is the "payment" (quest, reputation, flag)
    Free             // tutorial / story grant, no cost
};

USTRUCT(BlueprintType)
struct FTerritoryUpgradeCostEntry
{
    /** Optional grouping: one satisfied path per tag satisfies the tier. */
    FGameplayTag PathTag;                 // Territory.Upgrade.Path.Currency / Resource / Quest / Free

    UPROPERTY(EditAnywhere) ETerritoryUpgradePayment Payment = ETerritoryUpgradePayment::Currency;
    UPROPERTY(EditAnywhere) int32 CurrencyAmount = 500;
    UPROPERTY(EditAnywhere) TSoftClassPtr<class UNarrativeItem> ItemClass;
    UPROPERTY(EditAnywhere) int32 ItemQuantity = 1;

    /** True = one satisfied optional entry is enough; false = every entry must pass. */
    UPROPERTY(EditAnywhere) bool bOptional = false;

    /** Vendor conditions gating this specific path. */
    UPROPERTY(EditAnywhere, Instanced)
    TArray<TObjectPtr<class UNarrativeCondition>> Conditions;
};

USTRUCT(BlueprintType)
struct FTerritoryUpgradeTier
{
    UPROPERTY(EditAnywhere) int32 Level = 1;
    UPROPERTY(EditAnywhere) int32 RequiredFactionTier = 1;     // perk tier I/II/III
    UPROPERTY(EditAnywhere, Instanced)
    TArray<TObjectPtr<class UNarrativeCondition>> Requirements; // hard gates (state, ownership, story flags)
    UPROPERTY(EditAnywhere) TArray<FTerritoryUpgradeCostEntry> Costs;
    UPROPERTY(EditAnywhere, Instanced)
    TArray<TObjectPtr<class UNarrativeEvent>> OnUpgraded;
    // effects: income bonus, guard capacity, fortification, capabilities
};
```

This mirrors the existing Territory pattern (`UTerritoryDefinition::Events`,
`FTerritoryStateConfig::EntryConditions/ExitConditions/EntryEvents/ExitEvents` are already instanced
vendor conditions/events), so designers use the same objects in quests, dialogue and interactions.

**Multi-way rule:** every `bOptional == false` entry must pass, and **at least one** `bOptional == true`
entry must pass (if any exist). That produces the three requested routes without new enums:

| Route | Authored as |
|---|---|
Currency | `Currency` entry, not optional |
Resource | `Item` entries (iron/wood), not optional |
Quest | `ConditionOnly` entry with the quest condition, optional |
Free / tutorial | `Free` entry, optional — combined with the hint menu below |

**Atomic order** (must match the existing `TryUpgrade` discipline):
validate requirements → validate **all** costs (affordability, item presence, conditions) → stage the
level → debit/consume → on any rejection roll back the staged level and consumed items → publish →
`OnUpgraded` events. No partial charge may survive a rejected upgrade.

### 10.2 Faction tier and team sharing (D2)

- New `UTerritoryUpgradeSubsystem`:
  - `TMap<FGameplayTag Faction, int32> FactionTiers` — SaveGame + replicated read model.
  - `GetFactionTier(Faction)` (pure), `SetFactionTier(Faction, Tier)` (authority only).
  - `TryUpgradePlace(Place, Requester)` — checks `GetFactionTier(Place->GetOwningFaction()) >= tier`,
    then the normal faction/owners/state checks.
- `UTerritoryUpgradePerk::SetPerkLevel` → server RPC → `SetFactionTier(PerkOwnerFaction, Tier)`.
  One member buying the perk unlocks the tier for the team; every member can then pay for upgrades.
- **Never** let the client call `USkillTreeComponent::BuyPerk` directly: wrap it in a server RPC that
  validates skill points and ownership first (the component's `BuyPerk` has no authority check).
- **AI auto-balance** (single and multiplayer-solo):
  - `UTerritoryBalanceProfile` data asset: `bAutoUpgradeAIFactions`, `TierOffset` (default −1),
    `MaxAutoTier`, `EvaluationInterval`, `bDeterministicSeed`.
  - Evaluated on faction-tier change, capture and a coarse timer; **deterministic** for the same
    campaign state and save (no per-tick rolls). Results are saved, so a reload never re-rolls.
  - AI upgrades go through the same mutation with a `Free` balance path and fire the same events.
- Team match: the faction tier is the shared authority; there is no per-player second copy.
- Solo match: identical to the single campaign because the faction has one member.

### 10.3 Capture and event-driven upgrades (D3)

Existing behaviour stays: `ATerritoryProperty::ReconcileOwnershipDependentSystems` resets
`UpgradeLevel` to 0 when the owner changes. Add events around it:

- `UTerritoryPlaceCapturedEvent : UNarrativeEvent` — fields: `PlaceTag` (optional filter),
  `NewOwnerFilter`, `MinOldLevel`; the Territory fires it with the captured Place, previous owner,
  new owner and old upgrade level. Quests, dialogue and interactions react through Tales.
- `UTerritoryUpgradePlaceEvent : UNarrativeEvent` — fields: `PlaceTag`, `TargetLevel`,
  `bAllowForeignOwnership`. Calls `UTerritoryUpgradeSubsystem::ApplyScriptedUpgrade(...)` on the
  authority. It uses the same validate → commit → replicate → save → events path; when
  `bAllowForeignOwnership` is true a quest may upgrade a Place its faction does not own.
- Reentrancy guard: a scripted upgrade marks the Place as event-driven for that frame, so an
  `OnUpgraded` quest cannot recursively upgrade the same Place in the same transaction (logged instead
  of looping).

### 10.4 Upgrade III (D4)

The tier's only mandatory gate is `RequiredFactionTier = 3` (perk III). No quest condition is
hard-wired. A designer may still add a `ConditionOnly` optional cost path for story flavour; that is a
cost choice, never a requirement.

### 10.5 Hints / teaching the upgrade (free path)

`NProTutorialHints` is **content-only** (no source module), and the project controller already exposes
`HintMenuClass` / `HintMenu`. So the tutorial path needs no new plugin code: author a `Free` (or
`ConditionOnly` "first visit") upgrade tier and have the hint menu/quest open the Place panel. The hint
system stays the presentation layer; the upgrade logic remains server-authoritative.

### 10.6 Vendor compatibility check for this model

- Conditions/events are exactly the vendor objects already used by Territory state configs, so a
  designer can copy them between a quest, a dialogue, an interaction and an upgrade tier.
- Dialogue and interaction never call the upgrade API directly; they open UI / fire the quest event,
  and the quest uses `UTerritoryUpgradePlaceEvent`, keeping one mutation authority.
- No new quest, dialogue or interaction system is introduced; the skill tree stays the vendor
  `USkillTreeComponent` on the player state.

### 10.7 Revised implementation order

1. `UTerritoryUpgradeDefinition` + tier/cost structs with instanced vendor conditions/events.
2. `UTerritoryUpgradeSubsystem` (faction tiers, scripted upgrade, save/replication).
3. `UTerritoryUpgradePerk` + server perk-purchase RPC.
4. Refactor `ATerritoryProperty::TryUpgrade` to consume the definition; keep `UpgradeCostPerLevel` as
   the fallback when no definition is assigned (migration safety).
5. Capture/upgrade Narrative events + reentrancy guard.
6. Balance profile for AI factions.
7. Tests: perk gate per faction, team share, multi-path costs (currency/item/quest/free), atomic
   rollback, capture reset + event payload, scripted foreign upgrade, AI determinism, save/load,
   two-client replication, failure paths.
8. UI: Place panel routes + skill tree menu entry; hint menu for the free tutorial path.

---

## 11. Implemented surface (2026-10-18)

### Code

| Piece | File | Notes |
|---|---|---|
`UTerritoryUpgradeDefinition`, `UTerritoryUpgradeBalanceProfile`, tier/cost structs | `Public/Core/TerritoryUpgradeDefinition.h/.cpp` | instanced vendor conditions/events; `TerritoryUpgrade::BuildPaymentPlan/ExecutePaymentPlan/RefundPaymentPlan` |
Faction tier authority | `Public/Subsystems/TerritoryUpgradeSubsystem.h/.cpp` | `GetFactionTier`, `SetFactionTier`, `TryUpgradePlace`, `ApplyScriptedUpgrade`, `RequestBuyPerk`, `EvaluateAIBalance`, broker maintenance |
Perk + skill chain | `Public/Skills/TerritoryUpgradePerk.h/.cpp`, `Public/Skills/TerritoryUpgradeSkill.h/.cpp` | `Perk_I/II/III` (tier 1/2/3, explicit `PrerequisitePerk`), runtime-injected `TerritoryUpgradeSkill` |
Server purchase broker | `Public/Interaction/TerritoryUpgradeBroker.h/.cpp` | replicated, controller-owned; `RequestUpgradeTier(1..3)`, `GetOwnerFaction`, `GetOwnerFactionTier`; vendor protected `BuyPerk`/`CanBuyPerk` via `ProcessEvent` |
Place mutation | `Public/Core/TerritoryHierarchy.h/.cpp` | `TryUpgradeToLevel`, `ApplyScriptedUpgrade`, tier gate, requirements, payment plan, rollback, OnUpgraded events, reentrancy guard |
Capture/quest events | `Public/Tales/TerritoryUpgradeEvents.h/.cpp` | `UTerritoryPlaceCapturedEvent` payload (Place/old owner/new owner/old level), `UTerritoryUpgradePlaceEvent` |
Persistence | `Public/Core/TerritoryWorldState.h/.cpp` | `FReplicatedFactionUpgradeTier`, SaveGame + Replicated + `OnRep_UpgradeTiers` |
Authoring | editor factories/menu + `TerritoryDataValidator` | “Upgrades & Progression” menu; tier/cost/interval validation |

### Assets (project)

| Asset | Class | State |
|---|---|---|
`/Game/TerritoryFramework/Upgrades/DA_Upgrade_Territory` | `UTerritoryUpgradeDefinition` | tiers 1/2/3, required faction tier 1/2/3, currency 500/1000/1500, **Require Faction Perk Tier = false** (migration window) |
`/Game/TerritoryFramework/Upgrades/DA_Upgrade_Balance` | `UTerritoryUpgradeBalanceProfile` | defaults: auto-AI on, offset −1, max tier 3, 30 s, AI Place upgrades on |
`DA_Place_Farm`, `DA_Place_Blacksmith` | `UTerritoryPlaceDefinition` | `UpgradeDefinition` assigned |
`BP_TerritoryWorldState` (HopDistrictTest) | `ATerritoryWorldState` | `UpgradeBalanceProfile` assigned |

### Verified

- `TerritoryFramework.Upgrade` — 5/5 (definition, payment routes, tier gate + scripted foreign, world-state roundtrip, perk/skill/capture contract).
- `TerritoryFramework.Editor.Authoring` — 9/9 (factories, menus, validation coverage, numeric contracts).
- PIE (HopDistrictTest, player faction `Heroes`, Blacksmith owner `Bandits`):
  - tier 0 → `TryUpgradeToLevel` rejected: “needs upgrade tier 1 (current 0)”;
  - buy Upgrade I → faction tier 1, level 1;
  - Upgrade II → tier 2, level 2;
  - Upgrade III → tier 3, level 3;
  - broker `GetOwnerFaction` resolves correctly only after the pawn-less-controller filter fix.

### Known limits / remaining

- **Perk-tier migration switch:** `UTerritoryUpgradeDefinition::Require Faction PerkTier` defaults to **true** (designed progression). The project asset is currently **false** because the skill-tree UI, starting skill points and quest grants are not authored yet; an existing territory therefore keeps upgrading with its normal currency cost. Flip it to true once the skill menu/points are live.
- **Vendor skill tree is not replicated.** Server-side perks and faction tiers are correct; a remote client UI must read `UTerritoryUpgradeSubsystem::GetFactionTier` / WorldState / broker getters, not `USkillTreeComponent::HasPerk`. The runtime-injected `TerritoryUpgradeSkill` exists on the server component.
- **AI balance PIE** not yet exercised end-to-end; the algorithm is deterministic and unit-covered only for defaults/roundtrip.
- **UI panels/hint menu** not authored; broker `RequestUpgradeTier` is the wiring point for a Place panel or skill menu button.
- **Capture event** payload is now covered by the native test; no authored capture event asset exists yet.
