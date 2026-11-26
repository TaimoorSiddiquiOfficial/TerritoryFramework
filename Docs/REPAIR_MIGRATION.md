# Save and API migration for the October repair

Both TDA and a separate licensed Narrative Pro host are acceptance targets. The
verification ledger records actual results; this guide is not release certification.

## Before opening a campaign

Back up the matching plugin code/content, project Definitions and Native save slots
outside the release ZIP. Verify Narrative Pro 2.4.2 with
`Tools/Verify-NarrativeCompatibility.py`; see [the dependency guide](NARRATIVE_COMPATIBILITY.md)
for the explicit, hash-checked patch and rollback. Other Native versions fail the
compatibility check until separately reviewed. Never patch unrelated Engine installs.

Player-only loads now keep their save object local. The campaign save pointer,
actor RecordMap and campaign slot identity do not change during a join. Missing,
corrupt or incompatible player slots fail without importing a campaign.

## Existing saves

- Place, guard-post and assault IDs retain their meanings. New fields are additive;
  serialized enum values are not renumbered.
- Guards restore Claimed and Contested survivors by their saved post occupancy.
  Explicit zero survivors/reserves stay zero. Total-count migration applies only
  when legacy per-post occupancy is absent. Owner/post load order may differ.
- New reserve saves store remaining delay. Old saves without that field resume
  the configured delay after their owner and post are ready. Restore cleanup does
  not discard a saved queue or replenish an exhausted reserve.
- Existing production-site rows also store versioned currency-rate inputs and
  desired garrison. Narrative inventory still owns money. Loaded and unloaded
  owned Places use the same rate calculation and existing payout cadence.
- Legacy currency rows recover saved actor values through Native's existing
  RecordMap and their Definition. Missing actor data or unknown parent/capital
  policy is an explicit `CurrencyMigrationIssue`; the system withholds the unknown
  contribution instead of inventing ownership, staffing or a payout. Restore the
  matching data/Definition and reconcile the row before accepting the migration.
- Unload retains a site's contribution. The existing directory retirement policy
  explicitly removes a retired identity. Reload does not trigger an extra payout.
- Farm's Locked setting is a new-campaign seed. Its existing saved owner and
  unlocked availability remain authoritative. Blacksmith's existing prerequisite
  releases the gate; one unlock event reveals its owner without starting dialogue.

Never run an old binary against a newly written save on the assumption that all
new fields are backward compatible. Roll back code/content **and** the matching
pre-migration slots together. Trial migration on a copied campaign first.

## Capture read and mutation contract

`TryResetCapture` returns the existing structured mutation response. The original
`ResetCapture` node remains a compatible void wrapper. Rejected terminal recovery
keeps its tracking and retries; cleanup follows a verified commit. A restore or
destruction callback invalidates the old operation's success result.

Use the supported capture getters for loaded replicated actor state. The
Blueprint library's `TryGetTerritoryCaptureState` uses the existing WorldState
directory for unloaded Places. A directory snapshot is not an exact participant list.

For exact living count/pressure, call `TryGetCaptureParticipants` and check its
boolean result. Client detail is currently unavailable, represented by false and
sentinel values, **not an authoritative zero**. Display "unknown" in that case.
`GetActiveAttackers` and `HasAttackBudget` remain for existing Blueprints with
deprecation metadata; migrate callers to the readiness-aware query. No node is
removed by this repair. Debug and operations UI follow the supported read model.

## Authoring and generated references

A Place Definition identifies one durable Place; use shared profiles for reusable
configuration. Follow [Duplicate a Place safely](21_Definition_Assets.md#duplicate-a-place-safely)
for a genuinely new location. A duplicate level actor using the same Definition
does not receive a new identity.

Run `Extract-Options.py` before the option generators and verifiers. Verifiers now
compare their cached input with fresh source and fail on stale inventory. Nested
type closure restores the containing class when assigning property ownership.

## Rollout gate

Production profiles/definitions and assault definitions/classes, triggers and
vehicle classes are retained by the existing owning subsystem after asynchronous
preload. Pending production reports `LoadingAssets` and leaves checkpoints/items
unchanged. The existing observation timer retries that cycle. An accepted story
assault can wait in its existing evaluation state for assets; loading consumes no
physical force or vehicle credit and cannot reroll its saved decision. Missing
assets fail configuration checks. Restore/world shutdown cancels the old handles;
there are no asynchronous gameplay mutation callbacks to resurrect old state.

Treasury, transaction history and production replication now update separate
client caches. A complete campaign import still restores all three read models.
These changes do not alter Narrative's inventory authority or convert arrays to
FastArray replication.

Use the [repair ledger](Verification/2026-10-05-repair-ledger.md), full automation,
content validation, two-client cold-restart/streaming scenario and build/cook
receipts. A green focused unit suite alone does not authorize release. Public
push/upload remains a separate explicit decision.

## Streamed ancestor readiness

A missing City/District actor and missing durable ancestor row mean readiness is
pending, not that a designer locked the Place. Physical guard deployment waits;
existing living-slot records, finite reserves and the remaining timer delay survive.
Ancestor registration reconciles loaded posts without re-reading older save records
over recent casualties. A fresh post receives its initial occupant once; a later
readiness refresh is never a free casualty replacement. A confirmed authored lock
retains the existing lock policy.

The composed owner/post regression includes City/District arriving after the Place
and its posts. This initially reproduced zero guards where two were required;
after repair all 56 guard tests and the 526-test TDA suite passed. Physical World
Partition and release-host certification are separate gates.

## WorldState residency and currency read refresh

WorldState must be non-spatially loaded in World Partition. `bAlwaysRelevant` is
a replication setting; it does not keep the actor or its campaign directory
resident. New WorldState actors default to non-spatial residency. Existing actors
and Blueprint defaults migrate that editor-only flag on load; open and resave
existing WorldState Blueprints and their maps/external actors before cooking so
World Partition actor descriptors receive the corrected value. Preserve the
WorldState save GUID and campaign records. Do not place it in an unloading runtime
data layer or streamed level; use the persistent world.

Dirty currency rate views refresh through the existing production observation
timer, including when the production cycle has not changed. This is rate-only:
it does not move Native currency, advance item checkpoints or alter the existing
payout interval/account selection.


## Physical vehicle staging and AI teardown (integration correction)

Vehicle staging keeps `VehicleStagingBlockedSince` as a campaign timestamp for
existing diagnostics. New saves additionally retain `VehicleStagingElapsedSeconds`.
The 120-second physical grace now advances with world time, matching vehicle
movement/retirement; Native campaign time acceleration must not spend this grace.
The runtime world-clock anchor is never saved or replicated, and re-anchors after
restore without charging offline time. Existing saves lacking the new field receive
one full bounded physical grace; no force, deployment budget or decision is rerolled.
A permanently blocked entrance still reaches the existing configured retry/count
failure path. Matching server/client builds remain required.

Territory's activity adapter cancels owned goal generators' pending latent actions
and object-bound timers on EndPlay, including removed generators retained by delayed
Blueprint work. This prevents a streamed-out NPC's EQS Delay loop from dereferencing
its retired activity. Other NPCs' latent work is preserved. Narrative source is not
modified for either correction.

## Combat tree callback lifetime

The plugin-owned melee tree disables the Native attack service's search-start
tick. Normal scheduled ticks, Native goal/activity selection and the configured
interval remain in use. No campaign migration or durable-ID replacement is
needed. Custom Territory combat trees using Native `BTS_Attack` must also leave
**Call Tick on Search Start** disabled. Use matching content on server and clients.
See [the cause and regression](Verification/2026-10-06-combat-search-lifetime.md).
