# Complete TerritoryFramework re-audit — 2026-09-05

Status: **in progress**. Findings below distinguish confirmed defects from candidates.

Latest focused update: [batch 41 — passenger recovery after driver loss](CASUALTY_DRIVER_LOSS_2026-09-07.md).
UE 5.8.2 Editor/runtime/UHT and Game builds pass; **275 automation tests pass**.
Mounted driver death now lets surviving passengers leave through Narrative's mount
system without forfeiting their finite force slots. A rendered server and two clients
agree on all eight casualties, zero withdrawal and finite defeat; a real save/reload
after driver death reaches the same result. Batch 40's weapon execution, vehicle
departure and UDS work remains covered by the full suite. The earlier behavior-tree
crash after attacker casualties, city/interior visual review and broader audit gates
remain open. The previously untracked `DA_QC_NewMission` draft is no longer present;
the current 123-asset validation has zero errors and four existing warnings.
The batch 36 figures below are historical; they are not the latest build receipt.

Historical code verification: batch 36, **265 passing automation tests** (252 clean,
13 with fixture warnings; zero failed/skipped), Editor/runtime/UHT and
Development Game builds. Repeated live save/load preserved survivor GUIDs, saved
health, exhausted car budgets and four pending reserves; both clients received the
same final force counts and physical NPCs on return. Batch 36 validated 75 Blueprints
and 123 assets (zero errors/invalid, four presentation warnings). Its fresh package
verification is recorded below. Earlier rendered server plus two clients verified physical
counterattack combat, finite simultaneous vehicle squads, autonomous recapture with
players 250 meters away and ownership refresh on returning clients. Batch 35 added
Native ambient traffic to AlMalik and observed moving cars. The fresh HopDistrictTest
plus AlMalik cook/stage/package succeeded (zero errors, 591 asset warnings). Both
packaged smoke processes exited zero; counterattack squads completed ingress, and
the city retained its Native graph/spawner and registered driver bundle. The city
still reports the documented empty-vehicle Chaos ensure; zero exit is not a clean
runtime release gate.

These checks are not proof of complete framework release readiness. Remaining physical
spawn callback/malformed-save cases, the intermittent rendered Manny ensure, remaining
purchase/recipe and AI/UI/Tales review, dedicated-server testing and the documented
AlMalik decorative/empty-vehicle errors remain open. The installed Epic UE build
rejects TDAServer; this gate needs a server-capable engine. No overall completion claim
is made.

Baseline: host `d3db8b7`, plugin `79af71e`, UE 5.7. Work is isolated on
`hoptrendy/territory-complete-audit` in both repositories. The prior 213-test build,
Blueprint validation and cook are baseline evidence, not proof of the new changes.

## Coverage ledger

| Area | Current coverage |
|---|---|
| Baseline inventory | 255 C++ headers/implementations, 84,644 lines including tests; subsequent added regressions are recorded below |
| Stub/unsafe context sweep | No production TODO/FIXME/stub markers or gameplay first-player lookup found |
| Management requests | Nine server RPC paths inspected; count/sequence/cooldown checks traced into mutations |
| Economy/production | Currency routing, finite arithmetic, recipe transactions, checkpoints and callback boundaries under review |
| Property benefits | Definition payload, runtime GAS grant/revoke and existing tests inspected; regressions being added |
| Capture and hierarchy | Authoritative state transitions, availability and upgrade paths under review |
| Guards and assault | Counterattack lifecycle traced; finite guard reserve, warning/activation and callback fixes tested; physical spawn internals remain under review |
| Save/replication/streaming | WorldState cache/identity and all plugin-owned save interfaces inspected; real Narrative record/default reload tests pass; live streaming/replication gates pending |
| AI/stealth/Tales/navigation | Stealth and Tales callback/party fixes tested; remaining AI/navigation review pending |
| UI/editor/CI/packaging | Driving player identity corrected in UI read models/widgets; remaining presentation/editor review pending; 72 Blueprints and 113 assets revalidated in batch 25 |

## Findings register

| ID | Finding | Status |
|---|---|---|
| ECON-01 | Production multiplies a valid int64 per-cycle amount by an int32 batch count before checking the result; overflow can wrap to an accepted small quantity. | Fixed; red/green behavioral regression |
| ECON-02 | A zero-cost Property upgrade passes affordability but is rejected by the positive-only currency debit API. | Fixed; red/green behavioral regression |
| ECON-03 | Property income performs upgrade multiplication and final addition in int32, and converts an unchecked capital multiplier to int32. | Fixed; red/green behavioral regression |
| GAS-01 | Property benefits retain ability/effect/tag handles after external removal and do not reconcile them while ownership stays unchanged. | Fixed; real Narrative GAS removal/regrant test |
| GAS-02 | Runtime Property benefits apply Instant effects despite authoring validation rejecting them, so invalid content can repeat permanent modifiers on every refresh. | Fixed; red build applied twice, green applies zero times |
| GAS-03 | Vehicle possession replaces the management pawn even though Narrative retains the player character and its GAS/inventory identity. | Fixed; retained-character/PlayerState fixture passes |
| CAP-01 | Capture registration, progress updates and ticking retain map references/iterators across state callbacks. | Fixed; state callback cancellation and full suite pass |
| CAP-02 | Stealth evidence callbacks can remove an infiltrator, after which stale runtime state still exposes/registers it; decay callbacks also mutate an iterated map. | Fixed; red build emitted stale exposure and restored a contester; green withdrawal/decay tests pass |
| DIP-01 | Invalid/self diplomacy requests and superseded transitions can record successful events despite a rejected/finally different state. | Fixed; red/green callback and invalid-pair tests |
| DIP-02 | Reputation increments can overflow int32. | Fixed; both int32 boundaries tested |
| DIP-03 | Timed trade agreements created without the Narrative clock never receive a usable expiry. | Fixed; reject missing clock/nonfinite timing |
| DIP-04 | Duplicate reversed treaty pairs survive restore and make the chosen state depend on array order. | Fixed in subsystem; newest signed row and stable tie-breaks, expiry preserved; WorldState cache review ongoing |
| AUTH-01 | Upgrade and garrison staffing eligibility do not consistently enforce availability or world identity. | Garrison/upgrade admission fixed; locked and cross-world requests tested; callback transaction audit remains open |
| ATOMIC-01 | Upgrade, garrison purchase and production hold mutable state across Narrative inventory or Territory delegate callbacks. | Recursive payouts confirmed and fixed; purchase staging/refund and restore-during-callback paths remain open |
| ECON-04 | Member income split can lose a remainder when later accounts are full although earlier accounts have room; ceiling division can overflow at MAX_int32. | Fixed; full/partial/reversed capacity and maximum payout tests use real Narrative inventories |
| ECON-05 | Controller currency requests lose the retained player identity during vehicle possession; debit can claim success when the inventory's owner lacks authority. | Fixed; vehicle, role rejection and Narrative inventory load tests |
| SAVE-01 | WorldState district counting filters duplicate tags but misses duplicate valid GUIDs when both rows also have tags. | Fixed; duplicate-tag/GUID and GUID-only behavioral tests |
| SAVE-02 | Negative history limits can cause invalid removals in Economy restore/tick and WorldState transactions; WorldState RecordTransaction dereferences a missing world. | Fixed; detached world, negative/configured limits and client callback tests |
| SAVE-03 | Negative retained-assault limits can remove the final row and then index the empty array. | Fixed; real reflected WorldState event retains active records and safely removes the last terminal row |
| SAVE-04 | Treaty IDs use process-local FName hashes; normalized restored treaties are not copied back to the server late-join cache. | Fixed; stable faction-name IDs and authoritative cache migration/round-trip tests |
| SPACE-01 | Spatial insertion/query can enumerate an unbounded 3D grid; rejected actors can enter through bounds updates. | Fixed; bounded enumeration with oversized-volume fallback, registered-actor admission tests |
| SPACE-02 | Repeated registration emits duplicate events, and client local spatial caches do not reindex moved volumes. | Fixed; idempotent registration tested; live client movement gate remains pending |
| ASSAULT-01 | Blocking diplomacy does not cancel a pending assault while its target is streamed out. | Fixed; all five non-War states cancel immediately and remain cancelled through reload |
| ASSAULT-02 | Assault callbacks can supersede records while state events and calling functions continue through retained map references. | Terminal record committed before cleanup; stable event values and verified cancellation return fixed; activation/spawn callback review continues |
| ASSAULT-03 | Vehicle-only force planner sums valid seat capacities in int32 and can wrap. | Fixed; int64 bounded by requested force, MAX_int32 tests |
| ASSAULT-04 | Restored `Evaluating` records have no switch case and can remain stuck indefinitely. | Fixed; resumed evaluation reaches route validation using the same seed and creates no unroutable force |
| PROP-01 | Detached Property District lookup dereferences a missing world; negative upgrade caps write negative saved levels. | Fixed; query and level-boundary tests |
| TALES-01 | A condition can mutate the iterated event/gate requirement list or end the owning task, invalidating the shared probe. | Fixed; both direct/Narrative-node AND paths, task-end and recursive-gate tests |
| TALES-02 | Narrative's AnyPlayerPasses node path returns true even if every member fails, and its PartyLeaderPasses path dereferences an absent leader. | Fixed in Territory's adapter; member/leader/Not/AND behavior tested through public Narrative APIs |
| ECON-06 | Synchronous currency/item callbacks can reenter a scheduler and award a timer/campaign cycle twice. | Fixed; real Narrative inventory red/green regressions, later cycles and checkpoint reload tested |
| GUARD-01 | Reconciliation refills exhausted reserves, bypasses the preserve policy and ignores zero reserve definitions. | Fixed; one-time authoritative initialization and explicit ownership refills tested |
| SAVE-05 | Default delta fields do not replace populated live state on Narrative reload: zero reserve/upgrade/cost/sequence and empty treaty/assault/history arrays remain stale. | Fixed; shared plugin serialization adapter, full and legacy Narrative actor/component records tested |
| SAVE-06 | Deprecated persistence actor dereferences a missing world, mutates subsystem/save state on client actors and invents runtime persistence GUIDs. | World/authority guards and missing-GUID rejection added; empty legacy save, client and detached actor tests pass |
| GUARD-02 | Native activity deactivation can reload and replace a garrison while cleanup iterates it. | Fixed in batch 27; original runtime crashed, replacement/save/authority regression passes |
| GUARD-03 | Native spawn callbacks can change owner or remove the post before a stale guard is admitted. | Fixed in batch 28; ownership/post/death/client/save regressions pass |
| GUARD-04 | Reserve aggregation wraps negative, erases effective defence and increases attack priority after loading more reserves. | Fixed in batch 30; real post records, Volume snapshot, CounterAttack deterrence and district UI tested |
| ASSAULT-05 | Extreme power reduces selected approaches; authored vehicle counts expand before the existing eight-car budget applies. | Fixed in batch 29; bounded conversion/allocation, monotonicity and equivalence regressions pass |
| ASSAULT-06 | Reload reconstruction consumes another vehicle deployment slot, preventing remaining road-only reserves from deploying. | Fixed in batch 36; native save/vehicle regression and repeated server/two-client reload pass |
| ASSAULT-07 | One blocked vehicle approach repeats the identical spawn attempt for every NPC placement slot; restoring a blocked saved car repeats per occupant. | Fixed in batch 36; bounded road/physical obstruction tests and live second-wave deployment pass |
| ASSAULT-08 | Cached Native appearance loading can restore saved health before Native BeginPlay resets default attributes. | Fixed in batch 36; live survivor retains exactly 82 health after repeated reload |
| ASSAULT-09 | A retired driver remains a valid corpse reference, so surviving passengers wait until timeout and withdraw; missing/failed drivers also incorrectly fail the passengers. | Fixed in batch 41; Native exit/failure regression, server plus two clients, and real save/reload reach finite defeat with eight killed and zero withdrawn |
| VISUAL-01 | Rendered clients reproduce a SKM_Manny bone-visibility/component-space-transform ensure during the intro. | Open; batch 23 proves this is not limited to NullRHI |

## Architecture constraints

Territory actors remain state authority; Control owns capture; Economy owns rates and settlement;
Narrative inventory owns actual balances/items; Narrative GAS owns grants/effects; WorldState owns
durable global snapshots. No vendor source/assets will be modified. Each fixed batch must record
its behavioral tests, save/replication effects, migration requirements and remaining runtime gates.

## Verified batches

- **Batch 1:** Editor build/UHT passed. Full suite: 215 tests, 209 clean, six expected-warning
  tests, zero failures/skips. The pre-fix build reproduced a wrapped 16-item production order,
  a rejected free upgrade, and income of -2. Evidence: `Saved/Verification/20260905_DeepAudit/Batch1_*`.
- **Batches 2–3:** Editor build/UHT passed. Full suite: 217 tests, 211 clean, six expected-warning
  tests, zero failures/skips. Added real GAS grant/revoke, external removal, nested refresh,
  vehicle identity, load reconciliation, client-authority rejection, state callback withdrawal,
  stealth withdrawal and decay callback tests. Evidence: `Batch3_RedTests` and `Batch3_Tests`.
  Two preliminary vehicle fixtures hit Narrative's intentionally unimplemented native stable-GUID
  contract; corrected to unspawned native identity fixtures. No Narrative changes were made.
- **Effects:** No save schema, replicated property or Blueprint signature changes. GAS still owns
  grant replication; handles remain transient and are reconciled against restored state. Capture
  remains Control-owned and Volume snapshots remain the replicated/persistent authority. Invalid
  cross-world capture targets are rejected. Game build, package/cook, live multiplayer and streaming
  release gates have not yet been rerun for this audit.
- **Batches 4–5:** Editor build/UHT passed. Full suite: 220 tests, 214 clean, six expected-warning
  tests, zero failures/skips. Red/green evidence covers diplomacy event truthfulness, signed
  reputation bounds, duplicate treaty restoration, timed-treaty startup rejection, member payout
  capacity/overflow, vehicle currency and inventory authority. Narrative inventory prepare/load
  preserves the settled balance. Evidence: `Batch4_*`, `Batch5_*` in the same verification folder.
  Treaty migration has no schema change: valid rows are sorted by canonical faction pair, newest
  signed time, then state/permanence/expiry tie-breaks; invalid/self/None/nonfinite rows are ignored.
  Development Game build also passed (`Batch5_GameBuild.log`, 38 seconds).
- **Batches 6–7:** Editor build/UHT passed. Full suite: 225 tests, 219 clean, six expected-warning
  tests, zero failures/skips. Spatial tests cover oversized bounds, enormous queries, invalid
  inputs, relocation/removal, rejected duplicate registration and idempotence. Persistence tests
  cover district identity deduplication, canonical treaty migration, save/reload, and bounded
  transaction history. The treaty/district regressions failed against the pre-fix implementation.
  The transaction event fixture explicitly permits native actor callbacks in its unstarted world.
  No save schema or Blueprint signature changes. Old treaty IDs are recomputed on authoritative
  load; consumers should resolve by faction pair and refresh cached IDs. Client spatial reindexing
  changes only a local derived cache, with no new replicated authority. Live streaming/client and
  package gates remain pending. Evidence: `Batch6_*`, `Batch7_*`.
- **Batch 8:** Editor build/UHT passed. Full suite: 228 tests, 222 clean, six expected-warning
  tests, zero failures/skips. Pre-fix tests reproduced cancellation success after callback restore
  and persistent unloaded-target warnings despite all five blocking diplomacy states. Added
  seat-overflow and last-row replicated-history tests. Terminal force is committed before cleanup;
  delegate payloads are value snapshots. No save schema, Blueprint signature or new authority.
  Live callback-heavy spawning and multiplayer gates remain pending. Evidence: `Batch8_*`.
  Development Game build also passed (`Batch8_GameBuild.log`, 32 seconds).
- **Batch 9:** Editor/UHT passed. Full suite: 230 tests, 224 clean, six expected-warning tests,
  zero failures/skips. The old warning dispatcher sent a second player a stale warning after the
  first recipient cancelled. Cancellation and same-ID reload now stop the old dispatch. Transient
  access checks protect grace/warning/activation/casualty continuations from replaced or relocated
  map entries; nested ticks are suppressed. Interrupted evaluation resumes with its original seed.
  Physical spawn internals and transaction callbacks remain under review. Evidence: `Batch9_*`.
- **Batch 10:** Editor/UHT passed. Full suite: 231 tests, 225 clean, six expected-warning tests,
  zero failures/skips. Locked/cross-world garrison requests are rejected while valid reductions
  with missing posts remain allowed. An older detached garrison policy fixture now uses a real
  shared world. Detached District queries and negative upgrade caps are safe. No save schema,
  replicated property or Blueprint signature changes. Evidence: `Batch10_*`.
- **Batch 11:** Editor/UHT and Game Development builds passed. Full suite: 232 tests, 226 clean,
  six expected-warning tests, zero failures/skips. Narrative condition integration tests prove
  original AND requirements survive callback edits, ending a task stops remaining checks, and
  recursive gate queries terminate safely. No vendor, save schema, replication or Blueprint
  signature changes. Evidence: `Batch11_*`.

### Batch 12: Narrative party condition admission

Source inspection and a failing native regression proved that the supported Narrative node's
`AnyPlayerPasses` branch returns true with no passing members. Its leader branch also dereferences
an absent leader. Territory now composes the existing Narrative party and condition APIs for these
two policies; ordinary node/character-target policies remain delegated to Narrative. Each authored
event/task condition still participates in the complete AND gate. Narrative remains the authority
for party membership and condition execution; no vendor source or assets were changed.

Editor/UHT passed. Full suite: 232 tests, 226 clean, six expected-warning tests, no failures/skips.
The regression covers both event/task rejection, a later passing member, inherited Not, member-to-party
resolution, later AND requirements, native AllPlayers behavior, and an empty party/leader. The party
fixture exercises the multiplayer policy in a listen-mode world without sockets; it is not a live
two-client test. No save schema, replicated property, or Blueprint signature changes. Evidence:
`Batch12_RedTests` (two pre-fix assertion failures) and `Batch12_*` (passing build/full suite).

Existing Territory event/task assets using these policies now require an actual eligible member;
quests that accidentally relied on the old unconditional pass need their authored conditions corrected.

### Batch 13: recursive economy and production settlement

Real Narrative `OnCurrencyChanged` and `OnItemAdded` callbacks reproduced duplicate currency and
resource awards: one timer tick credited twice, and one production cycle generated two outputs.
Transient scheduler guards now reject recursive settlement while allowing later timer/campaign
cycles. The private economy tick also rejects client worlds explicitly. Narrative retains currency,
item and inventory-save authority; Economy retains rates, recipe scheduling and cycle checkpoints.
No schema or Blueprint signature changes. This does not claim atomic purchase/refund behavior or
protection against a callback restoring/replacing production maps; those remain under review.

Editor/UHT and all 233 automation tests passed: 227 clean, six expected-warning tests, zero failures
or skips. The regression uses real Narrative inventories, checks subsequent independent cycles,
Narrative currency save/load and production checkpoint restore. Evidence: `Batch13_RedTests`
(six assertions caused by the two duplicate awards), `Batch13_Build.log`, `Batch13_Tests`.

### Batch 14: finite guard reserves and zero-value reload

The post remains the authority for finite reserve counts. Reconciliation previously refilled an
exhausted post, bypassed PersistWithPost after ownership change, ignored zero-valued reusable reserve
definitions, and provisioned reserves on client actors. Initialization now provisions once; explicit
ownership policies still own refills. A guard registered before post initialization no longer suppresses
initial reserve provisioning. Counts loaded from Narrative remain initialized, including zero.

The real Narrative actor-record regression also proved that its default delta archive can omit a
saved zero and leave an already initialized post's current count untouched. The plugin post now writes
complete SaveGame values and resets its count defaults before reading legacy delta records. No vendor
change or new save field/schema is required. Old zero-valued records now restore zero in either load
order. Actor GUIDs remain authored and are preserved by Narrative's record API.

Editor/UHT and all 234 tests passed (228 clean, six expected-warning, zero failures/skips).
Tests exercise Narrative CreateActorRecord/LoadActorFromRecord, legacy delta bytes, initialization
before/after load, client rejection, preserve/refill policies and zero-capacity definitions. The seven
initial failures and the isolated remaining zero-reload failure are retained in `Batch14_RedTests`
and `Batch14_Tests`. Passing evidence: `Batch14_SaveBuild.log`, `Batch14_SaveTests`. Physical reserve
deployment and real World Partition streaming remain release gates; the load-order fixture does not
claim to replace them. Other plugin actor types are being checked for the same delta-archive boundary.

### Batch 15: complete and legacy Narrative save records

Sixteen pre-fix assertions reproduced stale wars, assault history, upgrade levels, nested guard costs,
player intelligence events and espionage sequence after a supposedly empty/zero-valued reload.
`FTerritorySaveSerializationScope` now adapts Narrative's tagged archive for Volume (including
Property), WorldState, GuardSpawnPoint, PlayerManagementComponent and the deprecated SavableData actor.
It restores SaveGame fields to the archive's archetype baseline before reading legacy deltas and
disables delta omission for new saves. Asset serialization and network replication are unchanged;
all effects are confined to `ArIsSaveGame`. No additional durable authority or vendor edit is added.

Native actor/component save interfaces and `CreateActorRecord`/`LoadActorFromRecord` are reused.
Existing scalar, nested-struct, array and map field formats remain compatible; no field names or
Blueprint signatures change. Old omitted values use the current archetype defaults, as Unreal's
delta format requires; an old save cannot reconstruct a historical default that was never stored.
New records explicitly store defaults and avoid that ambiguity. The deprecated actor still defers to
WorldState, rejects client/detached mutations and no longer invents a new identity at runtime.

Editor/UHT and Game Development builds passed. All 235 automation tests passed: 229 clean, six
expected-warning tests, zero failures/skips. Coverage includes new and legacy actor/component bytes,
empty authoritative diplomacy and late-join read models, zero upgrade/cost/sequence values, empty
player history, deprecated save maps, client rejection and detached legacy calls. Evidence:
`Batch15_RedTests`, `Batch15_FinalBuild.log`, `Batch15_FinalTests`, `Batch15_GameBuild.log`.
Physical multiplayer, asset/cook and streaming gates remain pending for this audit.

### Batch 16: state validation and callback-list consistency

Seven pre-fix assertions demonstrated nested ownership commits during entry validation and callbacks
replacing later requirements or events in an active list. Volume now rejects competing commits while
conditions are running, rejects a target destroyed by a condition, guards recursive evaluation of the
same state/phase, and evaluates copied entry, exit and defender-death callback lists. Validation remains
separate from the actual transition flag so existing previous-owner semantics are preserved.

Editor/UHT and all 236 tests passed: 229 clean, seven expected-warning tests, zero failures/skips.
The native Narrative condition/event fixture covers mutation, recursive queries, failure recovery,
destroyed targets, client rejection and real Narrative actor-record restore. Editor tests now declare
their direct NarrativeSaveSystem dependency. No SaveGame fields, replicated fields or Blueprint
signatures changed. Evidence: `Batch16_RedTests`, `Batch16_FinalBuild.log`, `Batch16_FinalTests`.
The separate exit and entry lists are captured when each phase starts; cross-phase authoring changes
remain under review. This batch does not claim transaction safety for unrelated purchase paths.

### Batch 17: upgrade purchase callback boundary

The real Narrative currency callback reproduced an old-price nested upgrade and a save containing a
paid wallet but the previous upgrade level. Property now applies its level and production read model
without callbacks immediately before Narrative validates/writes the debit. A rejected debit restores
the unobserved staged level; a successful debit exposes the matching level to currency/save observers.
Blueprint upgrade notification follows settlement. A transient purchase guard excludes competing
upgrades, state commits and garrison commands until publication completes. Narrative remains the wallet
authority; Property remains the level authority. Internal reset/restore calls retain the same writer.

Editor/UHT and all 237 tests passed (230 clean, seven expected-warning, zero failures/skips).
The regression covers exact price/level, callback-time real Narrative actor and inventory save/load,
competing mutations, subsequent higher-price upgrades, insufficient funds, free upgrades and clients.
No save schema or Blueprint signature changes; currency observers now see the committed level.
Evidence: `Batch17_RedTests` (ten failed assertions), `Batch17_Build.log`, `Batch17_Tests`.
This does not claim full garrison placement/refund or multi-item recipe atomicity. A campaign reload
or destroyed actor that supersedes a purchase prevents stale upgrade-success publication.

### Batch 18: production callback lifetime and cycle bounds

Source inspection confirmed that production retained pointers/references into the site, checkpoint and
profile arrays across Narrative inventory and settlement callbacks. Evaluations now copy site/rule data
and check transient revisions after callbacks. Campaign restore supersedes the old calculation, even
when the same GUID is restored into the same map slot. Actor refresh preserves the completed checkpoint
for items already awarded so the next tick cannot duplicate that cycle; it does not overwrite refreshed
ownership/upgrade inputs. Settlement observers retain access to the current per-rule read model.

Malformed negative/nonfinite clocks, nonfinite/overflowing cycle durations and negative saved cycle
indexes are rejected before conversion/subtraction. Runtime catch-up honors the existing authored
365-cycle ceiling. Narrative remains inventory/time authority; Economy owns scheduling/checkpoints and
WorldState publishes their read models. No durable field or Blueprint signature changes. Old malformed
negative checkpoints no longer produce catch-up; valid saved cycles keep their existing behavior.

Editor/UHT and all 238 tests passed (231 clean, seven expected-warning, zero failures/skips).
Real Narrative callbacks cover same-ID restore, full clearing from a settlement callback, profile-array
replacement, actor refresh without duplicate production, finite clock conversion and extreme catch-up.
Evidence: `Batch18_FinalBuild.log`, `Batch18_FinalTests`. Full multi-item inventory transaction rollback
and saves taken halfway through a multi-item recipe remain separate, unresolved audit items.

The dedicated TDAServer target was attempted after batch 17. This installed UE distribution rejects it
with "Server targets are not currently supported from this engine distribution." Evidence:
`DedicatedServerBuild_Batch17.log`. A Game build running with `-server` can exercise networking but is
not claimed as a compiled dedicated-server target. That release gate requires a server-capable engine.

### Batch 19: District income and recipe world admission

District's derived income summed individually valid int32 Property rates in int32, allowing two large
rates to wrap negative. The reducer now accumulates in int64 and saturates at the public int32 limit.
A detached District's City lookup also handles the missing world. Public resource recipes now reject
a requester or resolved Narrative inventory belonging to another campaign world before any item change.
These reuse the existing hierarchy registry and Narrative inventory authorities; no fields, replication
formats, Blueprint signatures or save migration are introduced.

Editor/UHT and all 239 tests passed (232 clean, seven expected-warning, zero failures/skips).
Coverage includes two authored Places, real Narrative actor-record reload of the maximum rate, child
unregistration, detached lookup and a foreign-world same-faction inventory receiving zero items.
Evidence: `Batch19_Build.log`, `Batch19_Tests`. The Game build also passed through batch 18
(`Batch18_GameBuild.log`); batch 19 and subsequent changes still require a final Game refresh.

### Batch 20: physical spawn callback lifetime

Wave, individual Narrative NPC and Narrative vehicle deployment now verify the authoritative assault
identity/generation after synchronous spawn, controller, GAS, dialogue and traffic callbacks. Force
configuration is copied before callbacks can replace the profile array. Unadmitted NPCs/vehicles and
staged occupants are retired through Narrative cleanup instead of escaping into the replacement
campaign. Nested physical wave spawning is excluded while the current wave is being built. A vehicle
deployment count reference is reacquired after callbacks; failed-wave notification cannot continue
through a replaced assault record. Autonomous and explicit immediate story activation remain supported.

Editor/UHT and all 240 tests passed (232 clean, eight warning-bearing fixtures, zero failures/skips).
The new behavioral fixture calls Narrative's actual SpawnNPC API and binds its Blueprint-assignable
OnNPCSpawned event, exercising empty and same-ID restores, cleanup, authority rejection and a later
successful spawn. It also spawns the real Narrative BPV_Sedan and restores from the actor-spawn callback,
proving zero occupants and no orphan vehicle. The synthetic NPC definition intentionally produces
asset-manager warnings; it is not an authored-content validation result. Evidence: `Batch20_VehicleBuild.log`,
`Batch20_VehicleTests`. The earlier `Batch20_FinalTests` failed because the initial vehicle fixture used
Narrative's abstract base; the final fixture uses the existing concrete sedan asset.

No Narrative files, durable fields or Blueprint signatures changed. This batch verifies callback
lifetime, not complete in-spawn casualty accounting or full physical road ingress. Force counts still
commit in the enclosing wave after participant construction; that callback timing and restoration
during retirement cleanup remain under review.

Additional batch-20 release evidence: Development Game build succeeded; HopDistrictTest cooked 6,888
packages with zero errors and five pre-existing vendor/config warnings; Windows staging, pak and archive
succeeded. The packaged game in server mode accepted two clients, including the later join. Its immediate
assault gate deployed four real Narrative occupants. Native save/reload preserved exactly one live
assault ID, but physical reconstruction repeatedly failed collision at the old sedan's spawn pad. The
record-only gate therefore did not prove physical restoration. Evidence: `Batch20_GameBuild.log`,
`Cook_Batch20.log`, `Package_Batch20.log`, `Server_Batch20.log`, `Client1_Batch20.log`, `Client2_Batch20.log`.

### Batch 21: campaign reload vehicle retirement

The packaged failure was caused by applying the ordinary 20-second vehicle retirement delay during
campaign restoration. Superseded empty assault cars now lose collision and visibility immediately on
the server, with a 0.75-second actor lifetime for Narrative latent mount/dismount cleanup. Previously
retired terminal-assault cars are cleared too. Ordinary assault resolution retains its authored delay.
Player drivers and passengers are preserved using Narrative's actual mount `SlotStatuses`; Narrative's
player interaction component belongs to the player controller. The same check prevents ordinary hard
retirement from destroying a passenger's car. Vehicle/guide tracking is detached before cleanup callbacks.

CounterAttack remains the transient mission-vehicle cleanup authority; Narrative owns occupancy. No
durable fields, Blueprint signatures or vendor files changed. Actor visibility/destruction is replicated
through the existing actor lifecycle. Native restored survivor/casualty accounting remains unchanged.

Editor/UHT and all 241 automation tests passed (233 clean, eight warning-bearing fixtures, zero failures
or skips). The new regression uses the real Narrative sedan and checks immediate collision removal,
latent lifetime, terminal-car cleanup, finite survivor preservation, ordinary retirement policy,
player driver/passenger protection, and non-authoritative actor rejection. The first test run exposed
an incorrect test assumption that player interaction lived on the character; both the adapter and
fixture were corrected to use the native controller component. Evidence: `Batch21_FinalBuild.log`,
`Batch21_FinalTests`. Game build and Windows staging/pak also succeeded, reusing batch-20's unchanged
cooked assets. The packaged server-mode run deployed four attackers, passed Native disk save/reload
with the same live ID, then reconstructed four attackers in a replacement sedan at the same pad; the
driver claimed the vehicle and began the ten-waypoint road route. No collision failure recurred.
Two NullRHI clients joined, but each emitted the same SKM_Manny bone-visibility ensure during the intro.
A third client with actual offscreen rendering completed the intro and reached the gameplay HUD without
the ensure. This distinguishes a headless presentation failure from the fixed physical reconstruction;
it does not certify the full two-client gameplay or visual release gates. Evidence: `Batch21_GameBuild.log`,
`Package_Batch21.log`, `Server_Batch21.log`, `Client1_Batch21.log`, `Client2_Batch21.log`,
`ClientRendered_Batch21.log`. The installed-engine TDAServer target limitation and missing physical
World Partition fixture remain unresolved.

### Authored asset validation after batch 15

Validation passed for 113 assets and compilation passed for 72 included Blueprints: zero errors or
invalid assets. Four presentation warnings remain: the two story NPCs use prototype Narrative Manny
appearances, and FarmHandover has no authored camera shot and a zero blend-out. Blacksmith's Claimed
background is verified as Unity in the Ashes and its entry sound as Horns of War. No assets were saved
by this validation. Evidence: `AssetValidation_Batch15.json`, `AssetValidation_Batch15.log` and
`CapitalDefinitions_Batch15.json`. This does not replace cook or physical multiplayer gates.

### Batch 22: nested restore during retirement callbacks

A regression bound the real Narrative activity component's deactivation delegate and loaded a
replacement assault with the same ID during old-NPC cleanup. The old implementation failed nine
assertions: it overwrote replacement phase/seed/casualties, erased replacement NPC/vehicle tracking,
and scheduled the replacement vehicle for removal. The same test also covers cleanup from terminal
assault resolution. Participant sets are now detached before Narrative callbacks, restore iterates
stable ID/value snapshots, and restore/resolution abandon stale continuation when a newer restore
generation is observed. Scoped guards preserve nested suppression and release correctly. Scheduling
rejects requests while campaign restoration or assault cleanup is in progress; ordinary autonomous
and immediate story scheduling remains supported after the transition.

CounterAttack retains the authoritative record and transient tracking. No save schema, Blueprint
signature or Narrative source changed. The new test verifies actual cleanup callbacks, preservation
of the replacement state and finite casualty counts, live actors, guard release and absence of stale
events. Editor/UHT and all 242 tests passed (234 clean, eight warning-bearing fixtures, zero failures
or skips). Evidence: `Batch22_RedBuild.log`, `Batch22_RedTests` (nine failing assertions),
`Batch22_Build.log`, `Batch22_Tests`. The Development Game build also passed
(`Batch22_GameBuild.log`). Physical package evidence remains batch 21 until refreshed.

### Batch 23: finite force during Native spawn and death callbacks

The real Narrative SpawnNPC callback regression initially failed 22 assertions: living/reserve counts
lagged admission, callbacks could remove an earlier NPC without recording a casualty, a retired NPC
could be admitted after its own callback, and a client actor could poison its retirement flag. Admission
now commits living/reserve counts with live tracking. A scoped construction token matches the exact
spawn GUID and assault generation so a death or withdrawal before SpawnNPC returns consumes one pending
unit exactly once. Failed placement/controller validation disables that path during cleanup, preserving
unspent reserve; unrelated NPCs cannot consume another construction's force. Wave dialogue selects a
living speaker, and the enclosing wave no longer repeats the admission count update.

Retirement removes capture pressure and records finite loss before Narrative activity cleanup callbacks.
The character's existing Native HandleDeath delegate is bound in SetNPCDefinition, before asynchronous
attribute initialization would normally bind it. AddUniqueDynamic preserves the later Native binding.
The character death handler retires the participant before cleanup, including deaths before its readiness
timer binds a separate listener. Both paths converge on the same exact-once accounting. Narrative owns
ASC death/ragdoll and NPC construction; CounterAttack owns finite force; Control owns capture pressure.

Editor/UHT and all 243 tests passed (234 clean, nine warning-bearing fixtures, zero failures/skips).
The regression checks an earlier attacker dying in a later spawn callback, own-construction death and
withdrawal, Native ASC death delegation, duplicate reports, reserve exhaustion, persisted casualties,
unrelated-NPC rejection, invalid-placement rollback and server authority. The synthetic definition uses
a minimal ability configuration; its missing ragdoll asset diagnostic is explicitly expected. During
test development a delegate API mismatch and the discovery that Native attribute initialization had
not bound the character handler were corrected; only `Batch23_EarlyDeathBuild.log` and
`Batch23_EarlyDeathTests` are final Editor evidence. `Batch23_RedTests` preserves the initial failures.
No durable schema, Blueprint signature or vendor source changed. Full Native archive/default, client,
load-order and contract suites remain green. The Development Game build and Windows stage/pak passed
(`Batch23_GameBuild.log`, `Package_Batch23.log`; unchanged batch-20 cooked assets). The packaged run
deployed four occupants, saved/reloaded the same assault, reconstructed four occupants, mounted them,
ran the road route and began authored dismount. Four actual Native deaths were recorded as KilledForce=4.
It then exposed a separate vehicle-budget restoration defect: reconstructing living survivors spends
another saved deployment, so four remaining reserves cannot deploy and the assault cancels with
SpawnFailed (state 4 -> 7, resolution 11, killed=4, withdrawn=4). That defect remains open. No complete
physical assault success is claimed. Both rendered clients joined this build; the first reproduced the
SKM_Manny bone-visibility ensure during the intro, disproving a headless-only explanation. Evidence:
`Server_Batch23.log`, `Client1Rendered_Batch23.log`, `Client2Rendered_Batch23.log`.

## Batch 24 — verify Native spawn identity (false positive rejected)

The new behavioral test runs actual Narrative guard and assault spawning, observes
`OnStableActorSpawned`, resolves the final GUID, round-trips each Native actor record and checks
independent destruction cleanup. Existing runtime behavior passes. UE 5.7 defaults
`bDelayOnActorSpawnedUntilFinishedSpawning` to true, so Narrative's deferred spawn sets the plugin's
metadata before the stable-actor event. The proposed earlier-registration runtime edits were
discarded; no runtime change is justified by this test. Editor/UHT and all 244 automation tests pass
(234 clean, 10 warning-bearing fixtures; zero failures/skips): `Batch24_FinalBuild.log` and
`Batch24_FinalTests`. Game/package/runtime evidence remains batch 23 because runtime is unchanged.
The vehicle-budget restoration defect remains open; passing identity registration alone does not
prove deployed survivor/vehicle persistence.

## Batch 25 — retain Narrative player identity in driving UI

Economy widgets, district management, the journal's five-guard previews and hierarchy/garrison/
district read models queried the possessed pawn. While driving, that pawn is the car, which does
not own the player's Narrative inventory or faction. Native widget and view tests reproduce zero
funds and incorrect ownership while the retained character has 1,379 currency. Early test fixture
failures (missing UMG link dependency, unregistered Territory, missing controller world registration)
were corrected independently; `Batch25_RedFixtureTests` contains the driving failures plus the
then-unregistered hierarchy fixture. Final tests use the registered Territory.

The existing management/economy character lookup now lives in
`FTerritoryNarrativeProAdapter::ResolvePlayerCharacter`. It uses Narrative's `GetOwnedCharacter`
and preserves the ordinary controller pawn fallback. The UI shares that lookup; actual balances
still come from Narrative inventory and factions from Narrative PlayerState. The waypoint query
deliberately measures distance from the currently possessed pawn. The management-point opening
check now agrees with server management validation. No Blueprint signature, RPC or durable schema
changed; no Blueprint/save migration is required. Editor-only widget tests add the UMG dependency.

Verification: Editor/UHT, Development Game and stage/pak pass; 245 automation tests pass (235 clean,
10 warning-bearing fixtures; zero failed/skipped). The exact regression covers real widget
construction, faction-before-character join order, driving, possession gaps, authority/proxy actor
roles, Native inventory save/load, independent later viewers, null inputs and plain controllers.
Actor-role and later-viewer fixtures are not a physical network replication test. Existing Native
save/default, authority, read-model/load-order and Blueprint contract suites remain green. Asset
validation compiled 72 Blueprints and checked 113 assets with zero errors/invalid assets and four
unchanged presentation warnings. Music mapping remains Unity in the Ashes / Horns of War.
Evidence: `Batch25_FinalBuild.log`, `Batch25_FinalTests`, `Batch25_GameBuild.log`,
`AssetValidation_Batch25.json`, `Package_Batch25.log`, `Stage_Batch25` (unchanged batch-20 cooked assets).
Physical runtime evidence remains batch 23; the vehicle-budget restoration defect and rendered
Manny ensure are unresolved. No overall release completion is claimed.

## Batch 26 — adaptive assault level uses Native player power while driving

`ResolveScaledEnemyLevel` read the current pawn's Narrative character level and ability system.
Possessing a Native sedan therefore substituted the car's tags for the player's perks/XP. The red
test produced level 100 from the car instead of the expected player-derived 19. Separately, adding
an offset to `MAX_int32` player power wrapped and selected level 1 instead of the configured maximum.
The isolated XP fixture initially lacked Native possession's attribute-set binding; that setup was
corrected before final verification, independently of the two demonstrated runtime defects.

CounterAttack continues to own adaptive spawn planning. It now resolves the retained player character
through the existing adapter and queries the controller's authoritative Narrative PlayerState ASC.
Relevance/range checks still use the physical pawn. The offset sum uses int64 before the final clamp.
No schedule/probability policy, force budget, Blueprint signature, RPC or save field changed. Native
XP/save/replication remains authoritative and no migration is required. Autonomous/immediate story
deployment is preserved.

The new behavior test uses an actual Native sedan with a distinct power tag, PlayerState ASC, perk
tiers and Native XP attribute save/load. It covers on-foot/driving, large positive/negative offsets,
proxy actor roles, physical range, absent pawn and disabled scaling. These isolated actor-role checks
do not replace the outstanding physical multiplayer gate. All 246 automation tests pass (236 clean,
10 warning-bearing fixtures; zero failures/skips), including existing determinism, force, authority,
Native save/load, streaming-order and Blueprint contract suites. Editor/UHT, Development Game and
stage/pak pass: `Batch26_Build.log`, `Batch26_Tests`, `Batch26_GameBuild.log`, `Package_Batch26.log`,
`Stage_Batch26`. Cooked assets remain batch 20; asset/Blueprint validation remains batch 25, since this
batch changes no asset or reflected contract. Physical runtime evidence remains batch 23 and its
vehicle-budget restore and Manny animation defects remain open.

## Batch 27 — guard cleanup survives a synchronous Native reload

A real Narrative activity-deactivation callback that loads a Territory actor record reproduced an
access violation in `ATerritoryVolume::DespawnGuards` at the old line 3503. The method held a reference
into `SpawnedGuards` across the callback; nested cleanup destroyed that actor and replacement spawning
changed the same array. Its final `Empty()` could also erase the replacement garrison. The original
method additionally lacked a runtime server-authority check.

Volume remains the guard/defender authority. Cleanup now detaches the old cohort, removes its defender
bindings/retries and releases its post slots before Native activity callbacks. It validates each old
actor after callbacks and leaves newly populated registrations intact. Manual retirement consumes no
reserve and does not queue replacements. The shared Native removal helper checks callback-invalidated
components/controllers and avoids scheduling an already-destroyed actor. Debug text describes the
retired cohort instead of falsely claiming every guard is gone. No Native source, save schema,
Blueprint signature or replication layout changed; no migration is required.

The red commandlet crashed with `EXCEPTION_ACCESS_VIOLATION` in the production cleanup method:
`Batch27_RedTests.log`. The fixed regression uses actual Narrative spawning/deactivation, a real
Native nested actor-record load, replacement post occupancy, Native post save/load, preserved reserve,
client authority rejection and a subsequent server cleanup. All 247 automation tests pass (236 clean,
11 warning-bearing fixtures; zero failed/skipped). Editor/UHT, Development Game and stage/pak pass:
`Batch27_Build.log`, `Batch27_Tests`, `Batch27_GameBuild.log`, `Package_Batch27.log`, `Stage_Batch27`.
Cooked assets and latest asset/Blueprint validation remain batches 20 and 25. Physical multiplayer
and actual World Partition streaming remain outstanding release gates.

Further source inspection rejected two provisional spawn concerns: the existing
`GPendingTerritoryGuardSpawn` guard already rejects nested guard creation during Native `SpawnNPC`,
and TriggerSet overrides are copied into `FNPCSpawnParams` before Native callbacks. No additional
spawn lock or TriggerSet-copy refactor is justified on those grounds. Post-callback owner/post/death
validation and purchase callback ordering still require investigation.

## Batch 28 — reject guard admission invalidated by Native spawn callbacks

Actual Native `OnNPCSpawned` regressions proved that an owner transfer, detached post or destroyed
post could still return spawn success and enter the old Volume's guard/defender lists. Detached-post
and owner-transfer cases also wrote a phantom active post count into the Native save record.
`Batch28_RedTests` contains the failing assertions against the original runtime.

Volume remains the guard admission authority. `TrySpawnSingleGuard` now rechecks the original
Territory GUID/world/owner, server authority, current political availability, live same-world post
binding/capacity and Native ASC death state after Native spawning. Multi-post deployment stops when
its original ownership context changes. Rejected staged NPCs use the existing bounded Native removal
adapter, disabling collision/movement and hiding the actor until its lifespan expires. This also
removes the failed-placement path's dependency on Native `DestroyNPC` delegating to a Blueprint-only
`CleanUp` event, which is unimplemented on the supported base Native controller. The first fixed test
run caught that cleanup assumption; the final runtime uses `ScheduleRemoval`.

The seven-case regression covers normal admission, public forced ownership transfer, post detach,
post destruction, multi-post owner change, client rejection, and canonical Native dead-state rejection.
Native post save/load verifies zero phantom guards and preserved reserve. All 248 tests pass (236
clean, 12 warning-bearing fixtures, zero failed/skipped): `Batch28_FinalTests`. Editor/UHT passes in
`Batch28_FinalBuild.log`. Development Game and stage/pak also pass (`Batch28_GameBuild.log`,
`Package_Batch28.log`, `Stage_Batch28`), using the unchanged batch-20 cooked assets. Latest asset and
Blueprint validation remains batch 25. No Native source, save schema, Blueprint signature or replication layout
changed; no migration is needed. The client-role fixture and Native record round-trip do not replace
physical multiplayer/World Partition verification. Purchase and post-admission garrison-event callback
atomicity remain separate audit work.

## Batch 29 — bound assault approach and vehicle-capacity planning

The unchanged planning calculations were extracted into a private pure helper used by the production
call sites, then exercised by behavioral tests. Red regressions show maximum finite power selected one approach,
an invalid configured approach limit selected 100, and 1,000 authored cars expanded to 1,000 capacity
entries before the existing eight-car difficulty budget applied (`Batch29_RedTests`). The red test
uses a safe 1,000-entry reproduction; it deliberately does not allocate MAX_int32 entries.

CounterAttack remains the planning/scheduling authority and Narrative difficulty remains the car
budget input. The helper clamps before float-to-integer conversion, handles NaN explicitly, and
enforces the existing authored maximum of eight approaches. Capacity accumulation bounds car counts
before allocation, saturates the road total to the existing eight-car budget and retains only the
eight largest capacities. Profile difficulty resolution shares the same native constant. All legal
car budgets and finite force values in the regression produce the same result as the prior complete
capacity list. Tests also cover approach-count monotonicity, reorder determinism, negative and maximum
integers, NaN, both infinities and actual Narrative difficulty enum handling. The initial fixed build
exposed NaN comparison behavior; explicit engine `IsNaN` handling fixed it.

All 249 tests pass (237 clean, 12 warning-bearing fixtures, zero failed/skipped): `Batch29_FinalTests`.
Editor/UHT passes in `Batch29_VerifiedBuild.log`; Development Game and stage/pak pass in
`Batch29_GameBuild.log`, `Package_Batch29.log`, `Stage_Batch29`, using the unchanged batch-20 cooked
assets. No Native source, save schema, Blueprint signatures,
replication layout, launch modes or physical capture flow changed. No migration is required; invalid
out-of-range authored counts now obey their existing editor limits. Existing save/authority/finite
force suites remain green. This bounded calculation test does not replace physical deployment,
late-join or World Partition release gates, or fix the separately reproduced vehicle restore defect.

## Batch 30 — preserve guard reserve totals and deterrence under overflow

Actual Native post records reproduced a server/UI/strategy inconsistency: loading additional valid
per-post reserves made each replicated Volume total -2, changed four effective reserve defenders to
zero, increased attack priority, and displayed -4 reserves in the district UI (`Batch30_RedTests`).
The post count itself loaded correctly; three derived aggregations used unchecked int32 addition.

Posts remain the exact durable reserve authority. Volume totals and CounterAttack raw aggregation
now use int64 intermediates, and the existing int32 replicated/UI read models saturate at MAX_int32.
CounterAttack still caps useful reserve defence by authorized desired staffing. The UI also saturates
across multiple Place snapshots. Pending deployment aggregation uses the same widened intermediate.
No Native code, save schema, Blueprint signature or replicated field layout changed; no migration is
needed and no post reserve is consumed or truncated by these derived calculations.

The regression builds a real authored two-Place district through the Registry, reloads real Native
post records, compares the actual CounterAttack inputs/calculator before and after additional reserves,
reads the public district operations UI, and rejects client-side snapshot mutation. The unchanged
physical capture authority and finite-force suites run with it. Physical two-client/late-join and
actual World Partition streaming are still release gates; the role fixture is not that proof.

Verification: `Batch30_Build.log`, `Batch30_Tests` (250 passed: 238 clean, 12 warning-bearing;
zero failed/skipped), `Batch30_GameBuild.log`, `Package_Batch30.log`, `Stage_Batch30` all pass.
The stage uses unchanged batch-20 cooked assets. Latest asset/Blueprint validation remains batch 25.

## Batch 31 — verified counterattack fixes; full framework audit remains open

User-reported scope: guards and assailants must fight before the player enters Place bounds;
attackers must respond to a distant damaging player; wave timing must be configurable; following
cars must not strand their occupants behind the first parked reinforcement car. The confirmed
autonomous/immediate-story decision remains in force. The full lifecycle below was re-traced
against the current source before editing. Root/plugin preflight: `7c73a84` / `9188d0f`, branch
`hoptrendy/territory-complete-audit`; only the three previously reported untracked audio assets.

Confirmed defects and changes:

- `ATerritoryGuardCharacter::CanEngageTerritoryTarget` required Contested before engaging
  any hostile. Guards now recognize a living, active assailant targeting their physical defence
  front before that state transition. Ordinary visitors retain the existing Contested policy;
  Narrative faction identity and Territory War/treaty checks still govern hostility.
- The project perception safety adapter checked activity owner against the pawn. Narrative's
  constructor owns that component on `ANarrativeNPCController`, so valid refreshes were rejected.
  The check now follows the actual owner and retains inactive/unpossessed teardown guards.
- Takeover scoring suppressed every non-local player attack goal, including a real damaging
  shooter. The existing Narrative ASC `OnDamagedBy` delegate now records at most eight transient
  damage sources, for an authored 1–120 seconds (20 default). Living, currently hostile sources
  receive temporary priority outside the local bounds; expiration, death, cancellation and treaties
  restore the local defender objective. Live GAS damage proved that eligibility alone left the
  4.0-score guard selected over the 3.5-score shooter, so only the damaged NPC temporarily suppresses
  unrelated combat goals. A second live trace exposed an older damaging guard remaining preferred;
  the most recent valid damage source now receives this temporary priority. The project NPC already
  reports Native AI damage perception; no duplicate damage
  report or combat system was added.
- The project `GoalGenerator_Hop_Attack` EQS loop used item zero for every target. Its loop index
  now feeds `GetItemScore`; the project Blueprint compiled and saved without warnings.
- Goal readiness now checks Narrative's actual goal membership, repairing a removed goal rather
  than treating a cached UObject as proof that it is still registered. Controller `SetPawn` itself
  was inspected and does not remove goals: vehicle possession causing removal is **not proven**.
- Vehicle dismount clears stale pairwise defender/assailant perception and requests Native sense
  updates. A blocked ordinary assault car may brake and use Native Mount exits after its authored
  blocked timeout, only with a complete navigation route to the walking objective. No teleport,
  vehicle collision bypass, or story-outcome event is used for this ordinary handoff. A stationary
  chassis can miss the forward obstacle probe, so ordinary arrival also accumulates its bounded
  wait while speed remains below one tenth of intended speed (capped at 175 cm/s).
- New profile `WaveStrategy`: Legacy; All Waves Together; Back to Back After Arrival; After
  Previous Wave Defeated. The selected enum is saved in the existing assault record at scheduling.
  Together uses available finite/global/route budgets, permits concurrent ingress and waits for
  physical staging clearance at shared road entrances. Back to Back waits for the preceding car's
  complete dismount but does not wait for its survivors to die. After Defeated waits for zero living
  attackers. Optional proximity policy, finite casualties, route checks and vehicle limits remain.
  Vehicle staging occupancy has a saved bounded wait before the existing spawn-failure path.

Authority: CounterAttack owns scheduling and finite deployment; Participant only adapts NPC goals,
damage context and ingress; Narrative owns activities/perception/Mount/GAS; Control owns capture;
Volume owns ownership; WorldState saves/replicates the existing record. Added record fields migrate
old saves to Legacy/zero traffic wait. No renamed Blueprint API, new tag/GUID authority or vendor
source change. Damage memory and live actors remain transient. Loaded/streamed target lookup stays
GUID-first. Mixed old/new binaries are not a supported multiplayer protocol.

Tests added/extended: native guard/assault combat without a player, Narrative damage delegate and
actual attack-goal score restoration, threat expiry/zero-damage/client/treaty rejection, authored
wave modes and save archive preservation, positive controller-owned perception plus teardown paths.
Initial harness attempts exposed a bare Narrative actor missing its stable-GUID implementation
and an abstract goal-generator fixture; those fixtures were corrected, not production contracts.
Runtime/editor compilation and UHT pass. All 252 automation tests pass (240 clean, 12 with existing
warnings), and 114 assets / 73 Blueprints validate with zero errors and four existing warnings.
`DA_CounterAttack` now selects Back to Back After Arrival, retaining autonomous activation.

Live verification in listen-server PIE (initial two-player runs, final server plus two clients):

- Both players at (-3500,10000): a finite 8-person/2-car assault recaptured an undefended Place
  through the physical handover flow. Server and client WorldState records matched.
- Three real defending guards, both players far away: guards selected Native attack goals while
  the Place remained Claimed; three attackers died, leaving 5 alive/0 reserve/3 killed with the
  second squad already deployed. Snapshot: `Live_Batch31_ThreeGuards_FarPlayers.json`.
- The second car could not find a walking route because HopDistrictTest's NavMeshBoundsVolume
  ended at X=-3990, while the blocked car stopped near X=-4386. Extended only its west coverage
  to X=-6190 (east boundary unchanged), rebuilt navigation and saved the map. Both the blocked
  car location and the road staging area now have complete paths to Blacksmith. A subsequent live
  run verified the second car braking and completing normal Native dismount for all four occupants
  (`Live_Batch31_BlockedCarDismount.json`, log 09:02:23). No teleport or nav-check bypass was introduced.
  Final three-player PIE recorded real distant-player damage at 31.61 seconds (120 → 115 health),
  Native attack-goal switch to that shooter by 32.62, and return to the guard at 52.38 after the
  20-second memory expired. Both cars completed ordinary Native dismount, including the stationary
  chassis case and the following car blocked near its predecessor. Snapshot:
  `Live_Batch31_FinalCombatAndPresence.json`. Seat diagnostics disproved a suspected
  independent dismount: the affected NPCs still occupied their Native seats. No speculative external
  mount-completion adapter is retained.
- Rapid cancellation during diagnostic setup exposed Native `Goal_Attack` references to removed
  pawns. This is tracked separately; that reset attempt is not clean combat evidence.
- All Waves Together deployed eight attackers in two concurrently arriving cars by 6.08 seconds;
  After Defeated preserved four reserves while the first wave fell from four survivors to one.
  The native strategy test covers deployment at zero survivors. These live recordings do not by
  themselves prove the later After Defeated deployment, which occurred after that recording ended.
- In the final run, a player at (-3220,1500), 410 cm outside Blacksmith's bounds, became an actual
  Native attack target. Moving that player to (-3220,590) kept it eligible inside the Place; the
  squad continued fighting the surviving registered guard. At 137 seconds all three WorldState
  read models matched: Active, planned 8, alive 7, reserve 0, killed 1, withdrawn 0, two cars.
  No vehicle-ingress timeout occurred in this final run. Initial map guard-placement rejection
  diagnostics remain separate from the explicitly spawned defending-guard fixture.

Latest validation: Editor/runtime/UHT and game builds pass; all 253 automation tests pass
(241 clean, 12 existing warnings); 114 assets / 73 Blueprints validate with zero errors and four
existing warnings. Original one-client PIE and background-throttle preferences were restored.
Fresh batch 31 cook completed with zero errors and five existing vendor tag warnings;
BuildCookRun packaged and staged successfully (`Package_Batch31.log`, `Stage_Batch31`).
The first packaged run deployed/dismounted both cars but exposed a shutdown ensure: participant
`EndPlay(Quit)` consumed survivors, resolved the assault, and emitted `OnCounterHappened`, whose
project listener created a Native notification widget during world teardown. EndPlay now counts
only actual destruction/stream removal in a live world; the scheduler rejects removal callbacks
during teardown. Native tests compare all five EndPlay reasons and verify live/reserve preservation.
The final packaged run completed both four-person vehicle squads, including the blocked second
car's normal exits, and shut down cleanly with no Error/Ensure/Assertion diagnostics in
`PackagedSmoke_Batch31.log`. This is the packaged game executable in headless server mode;
it does not replace the unavailable standalone TDAServer target or rendered skeletal verification.
No Native HUD/widget/vendor source was modified. All owned runtime/editor processes are stopped.

The prior deployed-survivor/vehicle-history reload defect and rendered SKM_Manny ensure remain
open. Standalone dedicated-server and real World Partition release gates remain blocked/unproven
as documented above.

## Batch 32 — surface roads, city routing and shared arrivals

User confirmed keep-right traffic and `/Game/HOPTRENDY/Map/L_AlMalik` as the
World Partition story map. Native ZoneGraph remains the road authority; Narrative
Mass/annotations own ambient traffic and lights, Mount owns physical vehicle exits,
CounterAttack owns finite waves, and Control/Volume still own capture and ownership.

Confirmed defects: Right road-guide offsets were inverted; independently acquired
traffic leases could be released through a different soft controller; mission cars
shared one occupied destination; the default Native A* wrapper only considered
adjacent lanes at the journey start. Initial generated junctions additionally imposed
OneLanePerDestination, which removed valid exits during Native overlap pruning.
The adapter now uses Native's default junction connections and admits gradual forward
changes to same-direction lanes with sufficient road length. Tests cover every exit
of both three- and four-mouth junctions, an intermediate turn-lane requirement, and
an obstacle in the intended steering corridor while straight ahead remains clear.

The editor adapter imports existing generator curves/sockets or derives Native
ZoneShapes from bounded road physical-surface collision samples, including the
Landscape physical-material result. No additional hand-drawn road spline is needed.
It rejects unloaded World Partition coverage, wrong/default surfaces, narrow paths,
invalid geometry and oversized samples; failed extraction preserves the old lanes.
Generated shapes are editor-only/nonspatial with editor GUIDs/source lineage, and
Native ZoneGraphData persists the network. Existing PM_Road (SurfaceType9) is reused
on five Ghost Town road mesh collision bodies. Generator and Narrative source/assets
are unchanged. Source map/mesh backups are under Saved/RoadNetworkBackups.

AlMalik has 60 roads, 25 junctions and 632 Native road lanes. Bidirectional hub
journeys pass for all 56 sampled roads in the main group. Three sampled roads,
including BP_Road_Generator8, form a separate island. Sixteen unmatched shape mouths
remain; no road across missing geometry is fabricated. Reloaded commandlet inspection
repeats the 56 passes with zero road-generator actors loaded, proving routing uses
the saved graph independently of World Partition's authoring actors.

Server-only transient arrival claims search back along the existing route for a free
drop-off with a complete Native walking path. Claims release at completion/retirement/
teardown; physically parked cars continue to occupy space. Native closed-lane lights
pause the blocked timer. Obstacle probes cover current and intended steering corridors;
safe side avoidance requires a same-direction Native lane. The prior bounded on-foot
dismount fallback remains in effect. No assault save schema or enum value changed;
existing missions compensating for the old Right/Left inversion need their offsets
reviewed. The separate survivor/car-history reload defect remains open.

Executed: Editor/runtime/UHT and game builds pass; all 260 automation tests pass
(248 clean, 12 with existing warnings). Commandlet asset validation checked 182 assets
and compiled 66 Blueprints with zero errors and four warnings. These results include
the map, road meshes/generators and existing Blacksmith music mapping. Later batches
33–35 below record rendered multiplayer/city verification and remaining cook/package gates.
See ROAD_SURFACE_NETWORK.md for actual APIs, migration and authoring limits.

## Batch 33 — actual weapon eligibility and durable combat permission

Rendered batch-32 verification exposed two additional causes of stationary assault
combat. Sword-only NPCs selected the project ranged activity because its inherited
score had no available-weapon gate and outranked melee. Both project attack activities
now query Narrative's existing GetWeaponsToAttackWith using their inherited Weapon
Types before scoring. No weapon returns zero; existing alert/hidden/reachability and
combat scores remain intact. Only the two project BPA_TerritoryAttack assets changed;
Narrative vendor graphs/source were inspected but not modified.

BTService_TerritoryAssaultPermission previously resolved the territory under the
AttackTarget or moving pawn. Combat outside the Place therefore requested a District,
City or no target, and CombatDirector correctly rejected that different identity.
Physical participants now resolve their existing saved target GUID first. Missing or
unconfigured targets fail closed, releasing prior capacity; ordinary guard fallback
behavior is unchanged. No second target authority, new save field or replication
schema was introduced.

Editor/runtime/UHT and Game builds pass. All 262 automation tests pass (250 clean,
12 with prior warnings). Two new native behavioral regressions execute the actual
project Blueprint scorers against Narrative inventory and exercise the actual BT
service/director/blackboard across boundary movement, repeated ticks, target unload,
wrong-GUID tag reuse, reload and invalid participant identity. Both are warning-free.
Live asset validation checked 123 loaded-registry assets, compiled 75 Blueprints and
reported zero errors/four warnings; commandlet registry totals differ from live totals.

Rendered server plus two clients: simultaneous eight attackers/two cars fought and
killed the defender outside the Place, completed ingress and recaptured Blacksmith.
All three worlds reported Bandits/Claimed and identical finite terminal records.
One driver withdrew after eight failed movement restarts; seven continued. The real
5-point distant damage applied, but this specific driver's retaliation was not proven
before its movement failure. The prior batch-32 recording proves another attacker
selected the damaging player; neither recording certifies every vehicle-driver case.
Evidence: Batch33_AllTests, Batch33_Play_Final.log, Live_Batch33_Final.json and
Batch33_Live_Final_Ownership.json. The log also includes explicitly identifiable
Python harness attribute errors, not production Blueprint errors.

A separate run with all players 250 meters away and two guards proved autonomous
combat and finite casualties, but exposed blocked departure staging: the first squad
left its car on the spawn pad, so the four pending reserves eventually cancelled
with SpawnFailed. This is a failed full-wave case, tracked in batch 34 below.

## Batch 34 — occupied departure staging and autonomous multiplayer verification

The existing RoadTraffic adapter can choose a free point 9–20 meters forward on the
same validated guide/Native route when a previous car occupies the entrance, preserving
at least 15 meters of driving distance. Normal Native spawn collision remains the final
admission check. The guide is trimmed to that departure so the new driver cannot turn
back into the occupied pad. Fully occupied or too-short routes retain the bounded
failure path; force and vehicle deployment budgets are unchanged. The candidate is
transient and recalculated from current collision after loading.

A native test uses the actual Narrative Sedan to verify blocked-entrance recovery,
bounded/same-lane placement, deterministic reconstruction, no path rewind, unchanged
destination, and safe rejection of short, off-road and fully occupied alternatives.
Editor/runtime/UHT and Game builds pass. All 263 automation tests pass (251 clean,
12 with existing warnings); the new departure regression is warning-free.
Rendered server plus two clients repeated the exact failed batch-33 setup: players
250 meters away, simultaneous eight attackers/two cars, two guards. Both squads
spawned, fought without player proximity, killed both guards and completed physical
recapture. Two attackers died exactly once; six surviving members retired on success.
All three replicated assault summaries agreed on Success/CaptureCompleted, planned8,
killed2, withdrawn6, pending0, deployments2. Offscreen client Volume actors retained
their previous owner while outside network relevance; returning both clients near
Blacksmith refreshed both actors to Bandits/Claimed, matching the server. The log has
no Error/Ensure/Assertion messages. Evidence: Batch34_AllTests, Batch34_Play.log,
Live_Batch34.json, Batch34_Live_Ownership.json and Batch34_Live_Returned_Ownership.json.
Batch 35 below records city ambient traffic and fresh cook/package verification.

## Batch 35 — AlMalik Native ambient traffic

All 19,610 World Partition actor descriptors were checked: AlMalik had a saved
ZoneGraph but no Mass traffic spawner. Added one nonspatial Narrative MassVehicleSpawner,
configured for 48 ambient entities using existing DA_Vehicle and Native intersection
annotations. The existing project BP_TerritoryRoadTrafficSpawner was inspected: it is
a data-only child of the same Native class. No competing traffic authority or AI was
created. Scripts/Territory/configure_almalik_traffic.py repeats this configuration and
rejects duplicate/unloaded spawners. Map backup: Batch35_TrafficBackup/L_AlMalik.umap.

Rendered AlMalik PIE produced moving BPV_Sedan_Mass cars at approximately 600 cm/s on
the generated road network. The count48 is the configured Mass budget; only nearby
high-detail actor representations were counted. Evidence snapshots are
Batch35_CityTraffic_Live.json and Batch35_CityTraffic_Live_Later.json. This is a
single-player city smoke test, not a dedicated-server or full-city traffic soak.

The run identified a missing AssetManager scan for Native mass-driver NPC definitions.
DefaultGame.ini now includes only /NarrativePro/Pro/Core/AI/Mass/Vehicles alongside the
existing /Game NPCDefinition scan; Native identities/spawn definitions remain authoritative.
The fresh packaged city successfully dumps this NPC's SpawnedData bundle with its
Native activity configuration and appearance, and spawns Native driver NPCs. The
previous invalid-primary-asset warning is absent from that run.

Separate pre-existing map issues prevent a clean city PIE result: BP_SplineCatenary
tries to destroy a pending-kill Arrow component on streamed BP_Bulb actors; the placed
BP_VehicleBase at (7180,4480,9) has no VehicleMesh or ImpactMesh skeletal asset and
triggers Chaos LocateBoneOffset's Mesh->GetSkinnedAsset ensure on cell registration.
These are outside the changed Territory/Narrative traffic implementation and are not
claimed fixed. Do not attribute that placed-actor ensure to the spawned Mass sedans.
Fresh cook/stage/package explicitly includes HopDistrictTest and AlMalik and completed
with AutomationTool exit zero after 92 minutes. Cook reports zero errors and 591 warnings.
Reviewed warnings include unconfigured vehicle wheel sockets, the road generator's
Landscape Deformation missing actor during World Partition cooking, powerline
construction references, optional Native character-creator tags and a deprecated
style redirect. These warnings remain visible in Package_Batch35.log; cook success
does not certify those assets. The final package is Stage_Batch35/Windows.

Both packaged tests use isolated UserDir folders to protect the normal campaign saves.
PackagedSmoke_Batch35_Counter.log records a listening Development Game executable on
port 7835, back-to-back four-person squads in two cars, both blocked-arrival walking
fallbacks and all eight completed ingresses. The 75-second process exits zero with no
Error/Ensure/Assertion, but does not establish terminal recapture in that time window.
It is not a compiled TDAServer result. PackagedSmoke_Batch35_City.log records exactly
one saved ZoneGraphData and one MassVehicleSpawner plus the resolved driver bundle.
Its 45-second process exits zero, but reproduces the empty-vehicle Mesh->GetSkinnedAsset
ensure and NullRHI canvas-render warnings; it is therefore not a clean city runtime pass.
The earlier rendered city recording remains the evidence for visible moving cars.

Final editor restoration reopened AlMalik with PIE stopped, 632 Native lanes, two
road groups, one count48 spawner and no dirty packages. All 19,611 World Partition
descriptors were inspected: there are no Territory volume actors in this map. The
two descriptor matches for "Territory" are labels on sky/post-process actors. City
story capture requires authored territory boundaries and approaches; the physical
counterattack verification above belongs to HopDistrictTest. No mission boundaries
were invented. Evidence: Batch35_EditorRestored.json. The rendered editor reload
also emitted a D3D12PoolAllocator backing-resource reference-count ensure after
asset tabs restored; the editor remained responsive. Rendering settings were preserved.

Older generated Stage_Batch20/21/23/25–30
folders were archived to C:/Users/Taimoor/.codex/artifacts/TDA/20260905/PreviousPackages
to leave cook/staging space. Their original project paths remain working directory
junctions; verification logs and the batch31 package were retained in the project.

## Batch 36 — deployed survivor and vehicle reconstruction

`TerritoryAssaultPersistence.cpp` separates committed survivor manifests from fresh
reserves. `FTerritoryAssaultRecord` now saves original Native NPC GUIDs, approach IDs,
physical transforms and optional vehicle/seat identities. Vehicle checkpoints retain
the Native vehicle class, logical vehicle GUID, remaining route, parking/walking
destinations and health fraction. These nested fields are SaveGame-only and excluded
from replicated/RPC read models; existing replicated force counts remain compatible.
No live actor pointers enter the campaign record.

CounterAttack still owns finite force and deployment counts. Narrative `SpawnNPC`,
save records, ASC, mount seats, controllers and activities own physical NPC behavior.
Native NPC health/MaxHealth are opted into `AttributesToSave`; inventory and attributes
are restored using the original GUID. Cached visual initialization can precede Native
BeginPlay; the adapter reapplies the loaded ASC record after default initialization
without loading inventory or controller goals twice. A saved mounted transform is
replaced by a validated staging transform for the existing remount flow.

Reconstruction runs before fresh wave scheduling, including `AfterDefeated` and
recapture countdown. It waits for old same-GUID actors to finish retirement, elects a
living passenger when the driver died, preserves occupied player cars, and restores
already-dismounted survivors on foot. New vehicle squads commit their finite car slot
and occupant manifest before Native spawn callbacks. Partial construction cannot spend
another car for the same pending seats. Restored close-combat NPC positions use actual
engine collision clearance rather than the wider spacing required for fresh formations.

Version-zero records migrate saved living counts into bounded reconstruction credits;
these credits never reset charged car counters and cannot refill on repeated migration.
Old saves do not contain per-survivor GUID/transform/health history, so migration cannot
recover information that was never written. Invalid/duplicate manifests withdraw finite
slots instead of converting them to new reserve. Route, vehicle and roster bounds are
validated. Broader malformed-record arithmetic review remains open.

Live server/two-client reload testing exposed blocked departure retries. RoadTraffic
checks physical geometry as well as Native car bounds and searches only an existing
route, within 20 meters of its start while retaining at least 15 meters of driving.
Fresh waves retry a failed car once at a clear alternative and skip that failed approach
for the rest of the update. Saved cars use the same bounded recovery on their remaining
route and retain their logical identity, damage and already-charged deployment slot.
No alternative invents a spline, off-road shortcut, fresh car allowance or capture roll.

Evidence is under `Saved/Verification/20260906_SurvivorRestore`. Final
`HealthOrderAllTests/index.json` passes 265 tests (252 clean, 13 warnings, zero
failures/skips); `HealthOrderEditorBuild.log` and `HealthOrderGameBuild.log` succeed.
The native regression includes real Narrative save archives, original GUID/health,
driver death, exhausted car budgets, repeated reload before old-actor retirement,
missing-target GUID/client rejection, close-combat placement, and physically blocked
saved-car recovery. The road test checks real prop collisions, a fully blocking box,
road-surface clearance, bounded alternatives and failure preserving the input transform.
`TrafficAllTests` and `RecoveryAllTests` retain intermediate failed regressions; the
resized static-mesh fixture was replaced with an explicit blocking collision volume.

`LiveRestore.json` completes on a rendered listen server plus two clients with all
players initially 250 meters away. At 17.38 seconds, save/load/load preserved one
first-wave survivor; at 19.88 seconds the original GUID and exact 82 health were
verified with one spent car and four untouched reserves. After the four first-wave
casualties, wave two deployed. Save/load/load at 21.50 seconds, with both car slots
spent, restored all four original survivor GUIDs by 24.97 seconds. At 42.38 seconds
all four completed ingress, and both clients matched planned8/alive4/killed4/pending0/
withdrawn0/used2. `ClientsReturned.json` then verifies that each returning client
received the four physical NPCs with matching positions and health. This controlled
story encounter disabled territory capture to keep the reload target active; strategic
autonomous recapture remains demonstrated by batch 34. `HealthOrderEditor.log` has no
Error/Ensure/Assertion from this run.

`AssetValidation_Batch36.json`: 75 Blueprints compiled, 123 assets checked, zero
errors/invalid and four existing presentation warnings (two mannequin story appearances,
missing Farm dialogue shot and zero camera blend-out). Music mappings were rechecked.
No Blueprint graph migration or Narrative source modification is required. World
Partition target identity/load readiness remain server-side gates; live actor streaming
is not fully certified by the native missing-GUID fixture. Old save migration cannot
recreate individual health/GUID history absent from legacy records.

`Package_Batch36.log` records a fresh iterative cook/stage/pak with AutomationTool
exit zero in 5 minutes 42 seconds: zero cook errors and 588 asset warnings. Although
HopDistrictTest was requested explicitly, project cook settings also included AlMalik.
The package is `C:/Users/Taimoor/.codex/artifacts/TDA/20260906/Stage_Batch36/Windows`;
placing generated staging output on C preserves space on the project drive. The
remaining warnings include road-generator and decorative construction issues already
documented above. Package success does not clear those content warnings or establish
a dedicated-server build.

`PackagedSmoke_Batch36.log` and `PackagedSmoke_Result.json` record a 75-second
Development Game listen-server smoke on localhost, using an isolated generated
UserDir and transient Hard difficulty. The process exits zero with no Error/Ensure/
Assertion, deploys two four-person Native vehicle squads, and takes both validated
blocked-arrival walking fallbacks. Seven individual ingress completions and physical
combat deaths are logged; this time window does not prove terminal recapture or
completion by every original attacker. The explicit repeated-load and client-state
proof is the rendered PIE sequence above, not this packaged smoke.

Final editor restoration: `EditorRestored.json` confirms AlMalik open, PIE stopped,
the original one-client setting restored, no dirty maps/content, 60 roads, 25 junctions,
632 Native lanes, two physical road groups and one count48 Native traffic spawner.
The same 16 unmatched mouths remain visible in the inspection report; no disconnected
physical road geometry was silently joined. Existing BackToBack profile and editor CPU
throttling were restored after the controlled multiplayer test.

## Counterattack lifecycle preflight


Source trace completed before the first counterattack fix in this audit. CounterAttack owns the
finite assault record and scheduling; Narrative owns NPC spawning, definition initialization,
activities/goals, death/GAS and vehicles. Control owns capture and Volume owns territory state.

| Transition | Source ownership and behavior | Persistence / replication / presentation / tests |
|---|---|---|
| Captured → grace | `HandleTerritoryControlChanged`, `ScheduleAssault`: admission, durable target GUID, faction rules, finite profile, budgets and deterministic cycle | Assault record and cycle high-water exported by WorldState; state event/read model; staging/admission/cycle tests |
| Grace → evaluation → warning | `AdvanceAssault`, `EvaluateAssault`: campaign clock, treaties, force, route, seeded launch decision | Seed/roll/deadlines/approaches saved; `OnAssaultChanged` feeds WorldState; evaluation monotonicity, determinism and warning tests |
| Warning → proximity → activation | `NotifyRelevantPlayers`, `ShouldActivateWaitingAssault`, `ActivateAssault`: one committed Active transition before spawning | Notification/read model through player management; activation and restore tests. User confirmed 2026-09-05: preserve autonomous attacks and explicit immediate story waves; proximity gating remains an authored option |
| Activation → physical force | `SpawnNextWave`, `SpawnParticipant`, `SpawnNarrativeVehicleParticipants`: finite reserves, route/seats/budgets; Narrative `SpawnNPC` and scoped spawn-info adapter | Counts/vehicle budgets saved; live pointers transient; controller/definition/activity contracts, finite-wave and vehicle tests |
| Registration → combat/casualties | Participant `UpdateParticipation`, Narrative activity/goal and Control registration; Narrative ASC death delegate → `Retire` → unregister pressure → exact-once removal | Live registration transient; force counts/read model durable; death, targeting, activation/casualty and integration tests |
| Capture / exhaustion → recovery | Existing Control force-capture path only after physical defence checks, or `ResolveAssault` defeated/cancelled; retire goals, slots, pressure and vehicles | Final reason/counts/timestamp saved and notified; recapture decisions, finite removal and persistence tests |
| Load / streaming / late join | `RestorePersistentState`, GUID-first `ResolveTerritory`, registration callback; WorldState save interface and replicated arrays | Survivors become finite pending reconstruction on server; clients retain read models; Narrative archive round-trip and GUID-preserving unload/rebind tests |

The current automation includes pure policy and reflection contracts as well as native integration
tests. These do not replace the outstanding physical multiplayer and World Partition release gates.

**Current product decision:** The user explicitly chose “Preserve autonomous attacks and explicit
immediate story waves” during this audit. This supersedes the older universal first-wave proximity
restriction in root AGENTS.md. Finite force, server authority, valid physical routes, Narrative NPCs,
and the existing capture authority remain mandatory in every activation mode.

## Pending audit follow-up

### Batch 37 — owner-specific state rules and war-driven scheduling (2026-09-07)

User requested per-Territory automatic/quest-driven attacks and faction-specific story
rules, rewards and earnings. Preflight checked the existing dirty branch, actual Volume,
Control, CounterAttack, Diplomacy, Economy, Definition, state-rule and editor sources,
plus Narrative event, faction/attitude, inventory and save APIs. Existing user changes,
including the modified `Content/HopDistrictTest.umap`, were preserved. No Narrative Pro
source was changed.

Confirmed gaps and changes:

- Existing state conditions/events/capabilities applied identically to all owners.
  `FTerritoryStateGameplayRules` now holds these existing named properties and the new
  economy/admission permissions. `FTerritoryStateConfig` preserves its common fields
  through this base and adds exact-tag `FactionOverrides`. Matching overrides replace
  common gameplay rules. Music/stealth remain state-level settings. Every override's
  Narrative objects are cloned per actor, using the existing duplication adapter.
- Volume validates incoming-owner entry rules and outgoing-owner exit rules, including
  Claimed→Claimed handovers. Control's validation passes the actual candidate owner;
  capture-pressure read-model updates retain the existing owner. Native state-event
  execution remains server-only and uses explicit Tales/player context.
- `CaptureTriggered` preserves legacy behavior; `WhileAtWar` also permits an initial
  finite schedule against an already-owned Place. `QuestOnly` admits explicit Narrative
  Waves only; `Disabled` admits neither kind. Exact attacker allowlists apply to both.
  Preview, direct admission, evaluation, physical activation and undeployed continuation
  use the same state gate. Activation rechecks after warning callbacks, including the
  explicit immediate path that does not wait for another scheduler tick.
- While-at-war initiation uses the existing deterministic cycle high-water, preventing
  a new initial roll after load or terminal-history trimming. Existing repeat policy,
  cooldown, quest requirements, grace, strategic calculations, treaty checks, physical
  routes, finite budgets and capture authority remain in use. Pending policy cancellation
  appends `StateRuleBlocked`; existing serialized enum values remain intact.
- Explicit immediate story events keep the user's previously approved exception to an
  automatic launch roll/staging/quest-perk gate. They cannot bypass state policy,
  attacker allowlists, diplomacy, finite forces, routes or physical capture.
- Place effective income now respects the selected owner's state permission, including
  upgrades and capital multipliers. Guard upkeep is unchanged. Production saves a soft
  Definition reference and format version so unloaded sites read authored policy.
  Version-zero legacy records wait for actor rebinding; blocked cycles expire without
  later backpay. Manual crafting remains separate from daily Place earnings.
- City/District capital amounts are authored Definition fields (compatible 1000/500
  defaults), with selected-owner reward permission and the existing quest state-rule
  pause respected. Custom rewards continue to use Narrative events. City payout logging
  reports the actual credited amount instead of claiming the requested amount succeeded.
- The validator checks override faction tags, policy enums and nested Narrative objects,
  including Wave/diplomacy diagnostics. The story outcome analyzer exposes a separate
  effective scenario per faction while preserving existing default scenario titles.

Authority/migration: no new state, faction, wallet, capture, GUID or save authority.
Volume owner/state and WorldState snapshots select rules; CounterAttack owns finite
decisions; Economy owns rates/production; Narrative owns quests and real balances.
Existing assets retain previous defaults without an asset rewrite. See
`FACTION_STATE_RULES.md` for concrete Heroes-only earnings/reward and quest-only setups.
No faction-to-story-Territory mapping was invented or saved into project assets.

Verified evidence in `Saved/Verification/20260907_FactionStateRules`:

- `EditorBuild_Release.log` and `GameBuild.log`: Editor/runtime/UHT and Development Game
  builds succeeded. `BoundaryTests/index.json`: **269 passed (256 clean, 13 fixture-warning
  tests), zero failures/skips**. Added behavioral coverage includes owner-specific
  Narrative transition events/conditions, no-op/reload reward suppression, original GUID
  rebinding, client mutation rejection, war/quest/allowlist admission, deterministic
  saved schedule reuse, production reference archive/migration and real Native capital
  inventory credits. Monotonicity and existing failure-path tests remain in the full suite.
- `LivePolicy.json`: completed rendered listen server + two clients. Heroes ownership
  enabled income 600 and explicit-only waves on all three worlds; Bandits ownership
  disabled income and both attack kinds. Both client-side force-capture attempts were
  rejected. Native save/load restored Heroes and the selected policy; always-relevant
  snapshots matched while players were distant, and local actors matched after returning.
- `LivePolicy_Attempt1_OutOfRelevance.json` is a **test false positive**, not a confirmed
  replication defect: the first harness demanded an up-to-date local actor 250m away,
  beyond its authored/default 150m network relevancy radius. The corrected harness checks
  WorldState at distance and physical actors on return. No global always-relevant actor
  override or speculative save-system modification was introduced.
- `AssetValidation_Batch37.json`: 75 Blueprints compiled, 123 assets checked, zero invalid
  assets/errors and four existing presentation warnings. A cold CommonInput dependency
  load during the first widget compile produced an engine compilation-queue ensure;
  the verification script now preloads controller-data Blueprints before widget compilation.
  `FinalValidation.log` confirms the clean rerun completed without Error/Ensure/Assertion.
  `Package.log`: cook/stage/pak succeeded in 5m17s with **0 errors / 560 existing content
  warnings**, including AlMalik through the project's existing cook settings. The final
  C++ activation-only check does not change reflected
  defaults or cooked assets; `FinalStage.log` restages the rebuilt Game executable with
  that verified cook; final staging succeeded in 1m7s. `StagedBinaryHash.json` proves the
  staged executable matches the rebuilt source executable.
- `PackagedSmoke_Result.json` / `PackagedSmoke.log`: 75-second localhost Development Game
  listen smoke exited 0, with **no Error/Ensure/Assertion**, two four-NPC vehicle squads,
  two blocked-arrival foot fallbacks and all eight explicit ingress completions. Combat
  and casualties occurred; this receipt does not assert terminal recapture from inference.
- `Batch37_Receipt.json`: temporary live-test Definition edits were restored without
  saving assets, PIE stopped, PlayNumberOfClients restored to 1, editor returned to its
  initially closed state, no background verification process retained. Root/plugin
  whitespace checks passed; all prior uncommitted work is preserved.

The full framework audit remains open. This batch does not prove every World Partition
streaming arrangement, mid-spawn save callback, transaction rollback or dedicated-server
case. The installed Epic engine still cannot build TDAServer. AlMalik still requires its
actual story TerritoryVolumes/approaches and has the previously documented content issues.

### Batch 38 — garrison purchase callbacks and reload supersession (2026-09-07)

Confirmed defects: recruitment debited Narrative currency before holding the garrison mutation
lock, allowing a synchronous currency callback to re-enter a purchase against the old target.
Failed placement then depended on an unchecked faction-based refund. Separately, an NPC spawn
callback could load the same Territory GUID and owner and still admit the old request's guard.
The new native behavioral test reproduced that last defect before the final admission fix.

`ATerritoryVolume` remains the authority for desired staffing, guard admission and its replicated
snapshot. Narrative owns the wallet, NPC creation, definition/activity initialization and save
archives. The existing Economy debit API is reused. No Narrative source was modified.

The existing mutation lock now spans placement, debit and notification. Complete placement
precedes payment; placement failure takes no money, and failed final payment removes the unpaid
deployment. The final target and snapshot are staged before Native currency callbacks execute.
A transient native load generation invalidates work superseded by Narrative deserialization,
including identical-owner/GUID reloads during NPC initialization. Rollback stops when a new
campaign supersedes it. Final success is checked after publication callbacks.

Changed production files are `TerritoryVolume.cpp/.h`; `TerritoryGuardSpawnPoint.h` adds only
a test friend. `TerritoryAuditEventProbe.h` and `TerritoryGarrisonPurchaseTests.cpp` provide
real Native NPC, inventory and save callbacks. `05_Guard_System.md` documents the actual order.
There is no new save schema, replicated property or Blueprint node; existing Blueprint callers
keep the structured mutation result. Client mutations remain rejected. The load generation is
not persisted and does not introduce another durable identity or World Partition authority.

Verified evidence in `Saved/Verification/20260907_GarrisonPurchase`:

- `EditorBuild_Final.log` and `GameBuild.log`: runtime/editor/UHT and Development Game builds
  succeeded. `AllTests/index.json`: **270 passed (256 clean, 14 fixture-warning tests), zero
  failures/skips**. The new test covers callback reentry, failed placement, changed wallet,
  same-owner/GUID reload during spawn, reload during payment, Native callback save restoration,
  reserve preservation and rejected client mutation. `FocusedTests/index.json` preserves the
  initial failing reproduction: one stale guard survived the spawn-time reload before the fix.
- `LiveGarrison.json`: completed rendered listen server plus two clients. Buying two guards
  charged 100 once (50000 to 49900); all three worlds agreed on desired/active 2, withdrawal to
  0, and Native save/load restoration to 2 with reserve 7. Both client mutation attempts were
  rejected. Final balance remained 49900. Temporary state-rule edits were restored without
  saving the Definition; the editor was closed and its original one-client setting restored.
- `AssetValidation_Batch38.json`: 75 Blueprints compiled, 123 assets checked, zero errors or
  invalid assets and four existing warnings. `Package.log`: cook/stage/pak succeeded in 5m49s,
  including AlMalik, with **0 errors / 563 warnings**. Compared with batch 37, the three newly
  logged warnings are invalid Native Character Creator tags (`Narrative.Equipment.Slot.Mesh.BaseBody`
  in the visualizer Blueprint/map and `Narrative.CharacterCreator.Scalars.SkinHue` in its option).
  Those vendor assets and the previously reported road/catenary/powerline warnings remain open.
- `PackagedSmoke_Result.json`: the 75-second localhost Development Game listen smoke exited 0
  without Error/Ensure/Assertion. Two four-NPC squads used two blocked-arrival dismounts and
  all eight vehicle ingress completions were logged. This does not assert terminal recapture.
  `StagedBinaryHash.json` proves the staged executable matches the verified Game build.
- `Preservation.json` and `Batch38_Receipt.json`: all 741 Native source files and seven protected
  project/instruction/user-content files match the rollback checkpoint. No user map or audio
  changes were overwritten. PIE and verification processes are stopped, the editor is closed,
  and the original client count and background CPU throttling settings are restored.

This batch does not establish atomic saves from every intermediate multi-NPC initialization
or withdrawal callback. The compiled TDAServer gate remains blocked by the installed engine;
listen-server testing is not a substitute for that target. The full audit remains open.

### Batch 39 — production callbacks and verified compensation (2026-09-07)

The next audit reproduced four real Native inventory defects: nested recipes could charge
and produce twice; failed input refunds/output removal still claimed a complete rollback;
an inventory reload inside an output callback allowed the old request to keep producing;
and a callback could consume an earlier output while the recipe still reported success.

Narrative inventory remains the authority for items and player saves. Economy owns recipe
scheduling, verification and compensation; WorldState persists/replicates the existing
production read models. No Narrative source is modified. The fix guards settlement through
inventory and publication callbacks, observes Native inventory loading, verifies exact
quantities after each operation, and checks every bounded compensation result. A failed
refund is reported as `RollbackIncomplete`; a reload supersedes the old request. Current
stock snapshots are published after failed compensation as well as success. A partially
settled daily cycle is consumed rather than replaying the same conversion.

The appended status values preserve the previous enum ordinals and existing save records.
There are no new replicated properties or durable pointers. Blueprint callers chaining a
recipe synchronously from `OnProductionSettled` must defer the next request until the
callback returns. The UI exposes an inventory-attention message for incomplete settlement.
Documentation is updated in `07_Economy_System.md` and `20_Resource_Production.md`.

Validation against the updated, unchanged Marketplace Narrative Pro 2.4.2 on UE 5.8.2:

- A failing regression was captured before the fix. The new native transaction test uses
  real Narrative items, removal/addition delegates, inventory loading and failure policies.
  It covers nested requests, complete/incomplete compensation, replaced inventory state,
  client rejection, output removal by a callback and save serialization of result records.
- Editor/runtime/UHT compilation passes. Rendered automation: **271 passed**, zero failed
  or skipped (15 tests emit expected fixture warnings).
- All **75 included Blueprints** compile and **123 assets** validate with zero errors and
  four existing warnings. UE 5.8 invalidated the test map's old navigation tiles; rebuilding,
  saving and reloading navigation restored both configured assault routes. The pre-rebuild
  user-modified map is backed up under `20260907_UE58Final`.
- A listen server plus two clients receive stock `[2,1,1]` after a recipe, `[4,0,0]` after
  its reverse, and `[2,1,1]` after Native world **and player-data** restoration. Actual
  server inventory matches; direct client recipe mutations are rejected.
- The UE 5.8 garrison regression buys two guards for 100, withdraws them, deliberately
  changes currency, then restores desired/active guards and the paid balance 49,900 on
  reload. Both clients converge and reject direct client mutations.

Evidence: `Saved/Verification/20260907_ProductionTransactions`, particularly `AllTests`,
`AssetValidation_Batch39.json`, `LiveProduction.json` and `LiveGarrison.json`. The first live
harness attempts used incorrect Python bindings and omitted Native's separate player-data
load; the corrected full sequence passes. These were harness defects, not gameplay fixes.
Game/cook/package migration checks are recorded separately in the tooling migration receipt.

Limits: arbitrary third-party callbacks can still prevent full compensation; this is now
reported accurately. Saving midway through every possible item callback and preservation of
custom per-instance metadata when a consumed stack must be recreated remain separate work.
The full audit and compiled dedicated-server gate remain open.

### Remaining confirmed work

- Reproduce and symbolize the batch 40 behavior-tree decorator-search crash after
  clustered casualties. Batch 41 fixes a separately reproduced passenger defect;
  its successful actual-death runs do not establish the crash's root cause.
- Continue multi-item production save-only callback and per-instance metadata review. Batch 39
  covers reentry, reload supersession and verified compensation; it does not make arbitrary
  external Native inventory callbacks globally atomic. Upgrade callback order is fixed in
  batch 17 and garrison recruitment is covered by batch 38 and its UE 5.8 regression.
- Finish assault physical spawn/restore callbacks, malformed record/arithmetic limits and client
  movement/reindex validation. Unloaded-target treaty cancellation is already covered by batch 8.
- Batch 36 verifies survivor identity/damage and vehicle history across repeated reload. Continue
  the broader mid-initialization spawn/save callback and actual World Partition streaming cases;
  no universal physical-restoration completion claim follows from the covered scenarios.
- Trace the intermittent SKM_Manny bone-visibility ensure in rendered clients; batch 21's clean rendered
  run did not establish that this was a NullRHI-only issue.
- Capital reward authoring and faction/quest policy are covered by batch 37. Continue the wider
  currency callback/settlement audit; this does not prove every payout transaction atomic.
  District income overflow and detached City lookup are fixed in batch 19.
- Complete guard/AI/Tales/navigation/UI/editor review and the live release gates.
- Finish room-by-room AlMalik lighting review, authored interior light placement,
  cinematic GPU measurement and target-display HDR calibration. Batch 40 configures
  UDS/Lumen/exposure authorities but does not establish perfect lighting.
