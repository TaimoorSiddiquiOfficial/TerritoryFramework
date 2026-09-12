# Production limits and messages

Open a **Territory Production Profile**, expand **Rules**, and select a recipe.
Each recipe now has its own **Notifications**, **Inventory Stop Conditions** and
**Output Stock Caps**. These settings use the existing Economy subsystem and
Narrative inventory. They do not add another item account.

## Refill ammo without unlimited stock

For a free ammo refill (a rule with no Inputs):

1. Keep the ammo item in **Outputs**. Its quantity is the most the rule can add
   in one cycle.
2. Add an **Output Stock Cap** for that same ammo item.
3. Set **Maximum Quantity** to your desired total inventory stock.
4. Turn off **Notifications > Enabled** if this refill should be silent.

Example: output 1,000 rifle rounds, maximum stock 300. At 295 rounds, the rule
adds 5. At 300 or more, it adds nothing. After spending one round, the next
production cycle can add one. A cap does not remove items already above it.
Zero maximum stops that output.

Caps match the exact Narrative item class across all inventory stacks. They do
not count loaded magazines, pickups on the ground or other inventories. If a
rule has several free outputs, each capped output fills its own remaining space;
uncapped outputs still run. Add a stop condition to pause the whole rule instead.

Recipes that consume Inputs must fit their **complete** output batch below every
cap. They wait without consuming ingredients if the batch would exceed a cap.
They do not charge a full recipe price for a partial output.

## Pause a rule using an inventory comparison

Add an **Inventory Stop Condition**, choose an item, then choose **Stop When** and
an amount. Production pauses if **any enabled condition** matches current stock.
Disabled conditions are ignored. An empty list adds no checks.

| Stop When | Meaning with amount 300 |
|---|---|
| Equal To | Stop at exactly 300 |
| Not Equal To | Stop at every count except 300 |
| At Least | Stop at 300 or more (`>=`) |
| At Most | Stop at 300 or less (`<=`) |
| Greater Than | Stop above 300 (`>`) |
| Less Than | Stop below 300 (`<`) |

These comparisons check stock **before** the recipe runs. Equal To alone can miss
a count that jumps from 295 to 305. Use an Output Stock Cap for a strict maximum.
A condition may check a different item, such as stopping ammo production while
a required supply item is missing. Checks do not consume the item by themselves.

When a check or cap stops the whole recipe, the status is **Stock limit reached**
(`StockLimited`). No recipe items are changed. That cycle expires, and the next
cycle checks again. Sleeping, catch-up and loading a save cannot bank those
stopped cycles for a later production burst. Missing or conflicting accounts
still use the existing storage-unavailable behavior; they are never counted as
an empty inventory.

## Choose messages for each rule

Under **Notifications**:

- **Enabled** is the master switch for this rule's HUD messages and feed entries.
- **Notify On Success** allows a message after verified production.
- **Notify When Blocked** allows messages for missing inputs, storage and other
  blocking requirements.
- **Notify At Stock Limit** separately allows messages for inventory limits.
  It starts off because reaching a refill target is normal.
- **Record In Feed** keeps allowed messages in the activity feed.

Global Territory notification settings still apply. A rule cannot force messages
when the corresponding global HUD or recording setting is off. Muting a rule
does not mute another recipe. It does not stop production, inventory replication,
`OnProductionSettled`, quest listeners or the production-status view.

Write **Success Title**, **Success Message**, **Blocked Title** and **Blocked
Message** to customize text. Empty fields keep the existing default text.
Supported placeholders are:

| Placeholder | Value |
|---|---|
| `{Rule}` | Rule Display Name, or its friendly tag name |
| `{Resources}` | Actual produced items and quantities |
| `{Quantity}` | Total item units actually produced |
| `{Inputs}` | Total item units actually consumed |
| `{Cycle}` | Campaign cycle index; direct recipe calls can have no cycle (`-1`) |
| `{Territory}` | Friendly source Territory tag name |
| `{Reason}` | Why production did not complete |

Example title: `{Rule}: +{Quantity}`. Example message: `Supplies received: {Resources}.`
The HUD displays the title; the feed also keeps the message. Messages go only to
players in the result's faction, through their existing management component.
Remote clients receive the verified server result. Loading history does not
replay old HUD messages.

## Account, authority and compatibility

The stock being checked is the inventory that will receive production: the
selected faction resource account, or the sole eligible online faction player's
inventory when that fallback is enabled. Multiple eligible players need a
deliberately selected account. Limits are not copied to every player's inventory.
Faction changes continue through Native identity and the existing account router.

Only the server settles recipes. Counts are read from Native stacks with 64-bit
totals. Checks run during production, not every frame. Item callbacks that change
the relevant stock cancel the transaction through the existing compensation
path. Independent callback changes to check-only items are not rolled back.

Existing assets keep production and normal messages until new limits or switches
are configured. Existing enum values are unchanged; `StockLimited` is appended.
Add a case for it in project Blueprint status switches if needed. The existing
saved checkpoints/site status and replicated read models carry the new outcome.
There is no new campaign save field or stock authority. Native source is unchanged.

The Rule Tag is the recipe's stable identity. A tag such as
`Territory.Property.Benefit.WeaponUpgrades` does not, by itself, test whether a
Gameplay Effect benefit is active. The existing rule's upgrade/state settings
and the Place's faction-aware production policy still decide whether it is eligible.

See the [verification report](PRODUCTION_LIMITS_VERIFICATION_2026-09-12.md) for
builds, behavioral tests and multiplayer checks. Other items in
ROADMAP_AND_REMAINING.md remain open.
