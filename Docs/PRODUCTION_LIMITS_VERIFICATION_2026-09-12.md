# Production limits and notification verification — 2026-09-12

## Problem and change

Recurring production could keep adding ammo without a stock target. Each rule
also inherited the global notification policy, so a frequent refill could fill
the HUD and activity feed. Rules now expose inventory stop comparisons, output
stock caps and their own message switches and text. See the
[setup guide](PRODUCTION_LIMITS_AND_NOTIFICATIONS.md).

Free production adds only the missing amount. A recipe with inputs waits until
its complete output fits, consuming nothing when blocked. Stock-limited cycles
expire through the existing checkpoint; they cannot accumulate a refill burst
after save/load or catch-up. An exact-class transaction also rejects Native's
child-stack fallback when it would credit a different item class.

The TDA Blacksmith's existing `Territory.Property.Benefit.WeaponUpgrades` rule
now refills up to **1,000 rifle rounds**, checks **At Least 1,000**, and has
**Notifications > Enabled** turned off. Its existing output rate, upgrade and
state requirements remain. The separate Livestock rule is unchanged. This is
the project asset in `/Game/TerritoryFramework/Economy/Profiles`, not a change
to the plugin's sample profile. The selected amount is an adjustable default.

## Existing authorities and migration

- `UTerritoryEconomySubsystem` owns cycle eligibility, transactions and checkpoints.
- `UNarrativeInventoryComponent` owns actual stock. Checks use its live exact-class
  stacks with 64-bit totals; no shadow inventory or frame polling was added.
- Native player faction identity and the existing resource-account router select
  eligible accounts. Faction changes do not create another inventory.
- `ATerritoryWorldState` continues to publish existing resource/site read models.
- `UTerritoryPlayerManagementComponent` uses its existing HUD/feed. An owning-client
  RPC delivers allowed verified production results to remote faction members.

All stock mutations stay on the server. No campaign save field was introduced.
The appended `StockLimited` enum value preserves older numeric values. Blueprint
status switches may add a case for it. New limits default empty; ordinary success
and blocked notifications retain their existing defaults. Expected full-stock
messages default off. Native source was not changed.

## Verification

Evidence directory: `Saved/Verification/20260912_ProductionLimits` in TDA.

| Check | Result |
|---|---|
| UE 5.8 Editor, Development and Shipping | Passed |
| UE 5.7 compatibility host Editor, Development and Shipping | Passed |
| UHT | Passed on both engines |
| UE 5.8 Territory automation | 314 passed; 0 failed; 0 not run; 22 passed with warnings |
| UE 5.7 Territory automation | 314 passed; 0 failed; 0 not run; 24 passed with warnings |
| UE 5.8 asset validation | 245 checked; 147 Blueprints compiled; 0 errors; 8 existing warnings |
| HopDistrictTest host and two clients | All 19 production checks passed |
| Narrative source comparison | 741 files compared; 0 differences |
| UE 5.8 cook/package and 60-second startup | Passed; cook and game process exit 0 |

Two new native behavioral regressions exercise real Native item transactions:

- `TerritoryFramework.Production.Regression.StockLimitsAndRuleNotifications`
- `TerritoryFramework.Production.Regression.StockLimitCatchupRestoreAndCallbacks`

They cover exact refill quantities, multiple stacks, all six comparison operators,
disabled and malformed settings, input preservation, subclass routing, authority,
actual message text/switches, Blueprint contracts, inventory save/load, restored
detached production sites, catch-up expiry, missing/returning accounts, and stock
changes inside a real Native inventory callback. Callback compensation preserves
independent changes to items that were only being checked.

The live multiplayer fixture verifies one selected resource recipient, replicated
stock snapshots and owner inventory, silent production, partial refill text on all
three players, no growth or message spam at full stock, rejected client production,
optional stock-limit messages, and resumed silent production. It then changes
factions using the Territory Narrative faction event: unrelated players stop
receiving messages, the former faction cannot produce into the changed account,
and the new faction retains the same cap and receives the correct messages.

Production calls in that fixture run on a normal native gameplay timer. Unreal
forces actor RPCs to local execution during editor Python scripts, including
Python Slate callbacks. `UTerritoryAuditEventProbe::ScheduleProductionRecipeForPIE`
is editor-only test support which avoids that misleading test path; it neither
bypasses RPC rules nor changes production behavior.

## Limits of this evidence

The network proof uses a live listen host and two clients. Package startup uses
the Development Game executable in server mode, not a compiled `TDAServer` target.
The production test restores a detached site and account; it is not a new full
AlMalik World Partition streaming test.

The packaged map still reports its previously recorded missing optional
`CutscenePlayerActor` warning. Packaging also reports existing config-staging
allow-list warnings; neither is a production-limit validation result.

The previously recorded cold AlMalik appearance failure, restored-assault-wave
crash, returning-client streaming proof, finite reserve event adapters and
coordinated guard conversations remain open in the
[roadmap](ROADMAP_AND_REMAINING.md). This batch does not clear those release gates.
