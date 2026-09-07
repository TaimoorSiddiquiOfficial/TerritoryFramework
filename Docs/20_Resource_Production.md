# Resource Production

## Authority

`UTerritoryEconomySubsystem` owns production rates, campaign-day scheduling, ordering, and settlement. `ATerritoryProperty` owns the capturable site state and optional `ProductionProfile`. `UNarrativeInventoryComponent` owns every real resource quantity and its save data. `ATerritoryWorldState` owns production checkpoints plus replicated late-join read models.

The system does not create a second inventory, wallet, faction database, or capture path.

## Property modes

Currency and item production are independent:

| Property setup | Result |
|---|---|
| `InitialPeriodicIncome > 0`, no profile | Currency income only |
| `InitialPeriodicIncome = 0`, valid profile | Item resources only |
| Income plus valid profile | Currency and item resources |
| No income, no profile | Capturable strategic Property with no economic output |

For example, a Farm can use `InitialPeriodicIncome=50` and a production profile that debits Feed or Grain and outputs Meat each campaign day.

## Profile authoring

Create a `UTerritoryProductionProfile` data asset and add one or more `FTerritoryProductionRule` entries. Each rule requires:

- a unique stable `RuleTag`;
- at least one exact `UNarrativeItem` output class;
- non-negative rates with a positive base or upgrade contribution;
- no duplicate class in one input/output list;
- no exact class used as both input and output in the same atomic rule.

Lower `Priority` runs first. Equal priorities use `RuleTag` lexical order. This ordering is stable across save/load and lets one rule produce an item that a later rule consumes. Catch-up replays one campaign day at a time rather than multiplying each rule independently.

`QuantityPerCycle + QuantityPerUpgradeLevel * UpgradeLevel` is calculated with `int64` intermediates and rejected if the final item quantity does not fit `int32`.

## Account registration

Add `UTerritoryFactionResourceAccountComponent` to a server-owned actor that resolves to a `UNarrativeInventoryComponent` and exact Narrative faction membership. A PlayerController is supported: startup registration retries are bounded while possession is pending, and the controller resolves its current pawn after respawn. The component unregisters on EndPlay. `RegistrationRetryInterval` and `MaxRegistrationAttempts` control the startup window.

Registration is runtime routing only. It is intentionally not persisted as an actor pointer. The account must register again after respawn or World Partition streaming.

For a normal single-player game, explicit setup is optional. When
`Use Sole Online Faction Player Inventory` is enabled and exactly one online Narrative player
belongs to the Property's owning faction, every produced `UNarrativeItem` is added to that
player's Narrative inventory. An explicitly registered account always wins.

Multiplayer remains deliberate and safe. If two or more online players belong to the owner
faction, automatic item routing stops instead of choosing a player or duplicating the reward.
Add `UTerritoryFactionResourceAccountComponent` to the faction's shared depot, leader, or other
chosen Narrative inventory actor.

Example: the Heroes player captures a Farm that produces Grain. The next completed production
cycle adds Grain to the player's existing Narrative inventory. If Bandits recapture the Farm,
Heroes keep Grain already earned but receive no more Grain. The Bandit owner begins earning on
the next cycle; capture never creates a catch-up payment for either faction.

## Daily settlement

The default cycle length is `2400` Narrative accumulated-time units, matching one Narrative day. A new site, a changed profile, or a changed owner checkpoints at the current cycle and begins on the next cycle; capture never grants retroactive resources.

The subsystem observes Narrative's accumulated campaign clock every
`ProductionCycleObservationIntervalSeconds` (default: 1 real second). The observer does not
create its own time or pay partial days; it only notices that one or more complete Narrative
cycles have passed and asks the existing production authority to settle them. Currency's
`EconomyTickInterval` is separate. Designers therefore do not need to wait for the default
five-minute currency tick before a completed item-production cycle reaches inventory.

Ownership loss is an immediate production boundary. The authoritative control transaction
refreshes and republishes the site record in the same transition, so the old faction cannot earn
another cycle and clients do not continue showing the old owner until the economy timer runs.

For each pending day:

1. Sort sites by stable Territory GUID.
2. Sort site rules by Priority, then RuleTag.
3. Validate owner, claimed/contested policy, upgrade level, profile, and account.
4. Simulate exact input removal, output stacks, inventory slots, and weight.
5. Hold the recipe mutation guard, debit inputs, credit outputs, and verify the complete quantities.
6. Update the per-rule checkpoint and outcome.
7. Publish WorldState site and stockpile projections.

Missing input consumes that day and produces nothing. Storage unavailable or full remains pending, but only the latest `MaxProductionCatchupCycles` days can be recovered. Disabled, under-level, unclaimed, or contested rules consume the day as inactive. This prevents unlimited stockpiling of missed production.

An output-only rule is valid. For example, a test Blacksmith rule with no inputs and
`BP_Item_Grain x4` in Outputs adds four Grain to the resolved owner inventory after the next
complete campaign cycle. If it does not, inspect the published production-site status:

- `StorageUnavailable` means no explicit account and not exactly one online player in the owner faction;
- `StorageFull` means Narrative inventory slots or weight rejected the output and any applied quantities were restored;
- `Inactive` means owner/state/upgrade/profile policy did not permit that cycle;
- `Produced` with the expected quantity confirms that Narrative inventory accepted the item.
- `RollbackIncomplete` means a callback prevented full compensation; the UI shows **Inventory needs attention**. That scheduled cycle is consumed so it cannot replay the same partial conversion.
- `SettlementChanged` means a callback changed stock and the cancelled recipe's affected quantities were restored.

## Crafting bridge

`ExecuteResourceRecipe` exposes the same server-authoritative settlement for crafting or scripted conversions. The requester must be an authoritative actor with the exact Narrative faction and inventory. The call returns `FTerritoryProductionResult` with status, stable batch ID, source, actual item amounts, and failure reason. Preflight rejection and fully compensated failure return empty consumed/produced arrays. An incomplete compensation returns the input/output amounts that its inverse operations could not restore/remove.

Item and settlement callbacks cannot execute a nested recipe or recursively run the daily scheduler.
Rejected nested calls return `SettlementInProgress` without publishing another settlement event.
To deliberately chain recipes from `OnProductionSettled`, defer the next request until that callback
has returned. A campaign or Native inventory load supersedes an in-flight recipe: its remaining
outputs, old compensation and stale publication are discarded.

Compensation uses Narrative's existing item API and verifies both operation returns and affected
class totals. It is bounded by the request and current stock; it cannot remove pre-existing output
or refund input beyond its starting quantity. Arbitrary callbacks can still change Native capacity,
removal permissions or stock directly. Such interference is reported instead of claiming atomic
restoration. This API does not promise atomic saves from every intermediate Native item callback,
nor restoration of custom per-instance item metadata when a consumed stack must be recreated.

New status values are appended to the enum; existing save/Blueprint values retain their numbers.
WorldState continues to persist and replicate the same site/rule records and stock projections.

Do not call inventory debit and output nodes separately from Blueprint. That can lose inputs when output storage changes.

## Save, streaming, and replication

WorldState saves:

- `FTerritoryProductionCheckpoint` by stable Territory GUID and RuleTag;
- `FTerritoryProductionSiteRecord`, including per-rule outcome and soft profile reference;
- read-only `FTerritoryFactionResourceSnapshot` values.

World Partition-unloaded Properties continue from their durable site record. No live Property, account, inventory, or item pointer is campaign state. Actual items restore only through Narrative inventory.

Clients receive `ReplicatedProductionSites` and `ReplicatedResourceSnapshots`. They never read the server-only resource-account map. `bStorageAvailable` distinguishes a known empty inventory from unavailable account routing.

## Modular UI

`FTerritoryDistrictOperationsView` exposes child production sites and aggregated resource flows. `FTerritoryEconomyOperationsView` exposes stockpile, storage status, and all faction sites.

Use:

- `UTerritoryResourceRowWidget` for one item's stored/input/output/net values;
- `UTerritoryProductionSiteRowWidget` for one Property and its composed resource rows;
- `OnEconomyOperationsUpdated` when a project Blueprint owns a custom CommonUI layout;
- `Producing`, `ProductionBlocked`, `MissingInputs`, and `StorageFull` filters in District lists.

These widgets are read-only. Gameplay mutations stay in EconomySubsystem and Narrative inventory.

`BuildProductionSiteOperationsView` exposes one site from the server subsystem or replicated WorldState projection, so compact territory HUDs and project-specific modules do not need to query server-only maps.

The supplied `BP_Property_Farm` uses `DA_Production_Farm`; `BP_Item_Grain` and `BP_Item_Meat` provide the example Narrative item classes. A placed Farm still requires its own unique editor-authored Territory tag and GUID.

## Farm example

```text
Property: Farm
InitialPeriodicIncome: 50
ProductionProfile: DA_Production_Farm

RuleTag: Territory.Production.Farm.Livestock
Priority: 10
Input:  BP_Item_Grain x2 per day
Output: BP_Item_Meat  x3 per day
MinimumUpgradeLevel: 0
RequiresClaimedState: true
PauseWhileContested: true
```

With no Grain, the Farm earns its configured currency but produces no Meat for that day. With resource storage unavailable or full, the UI reports the exact block and the bounded recovery window remains pending.

## Validation

The profile overrides `IsDataValid`, and `UTerritoryDataValidator` also validates standalone profiles and assigned Property profiles. Release validation must include native automation, save/load, dedicated server, two-client late join, World Partition load order, Blueprint compile, and data validation.
