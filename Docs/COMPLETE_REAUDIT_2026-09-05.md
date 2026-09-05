# Complete TerritoryFramework re-audit — 2026-09-05

Status: **in progress**. Findings below distinguish confirmed defects from candidates.

Baseline: host `d3db8b7`, plugin `79af71e`, UE 5.7. Work is isolated on
`hoptrendy/territory-complete-audit` in both repositories. The prior 213-test build,
Blueprint validation and cook are baseline evidence, not proof of the new changes.

## Coverage ledger

| Area | Current coverage |
|---|---|
| Inventory | 255 C++ headers/implementations, 84,644 lines including tests |
| Stub/unsafe context sweep | No production TODO/FIXME/stub markers or gameplay first-player lookup found |
| Management requests | Nine server RPC paths inspected; count/sequence/cooldown checks traced into mutations |
| Economy/production | Currency routing, finite arithmetic, recipe transactions, checkpoints and callback boundaries under review |
| Property benefits | Definition payload, runtime GAS grant/revoke and existing tests inspected; regressions being added |
| Capture and hierarchy | Authoritative state transitions, availability and upgrade paths under review |
| Guards and assault | Counterattack lifecycle traced; finite guard reserve, warning/activation and callback fixes tested; physical spawn internals remain under review |
| Save/replication/streaming | WorldState cache/identity and all plugin-owned save interfaces inspected; real Narrative record/default reload tests pass; live streaming/replication gates pending |
| AI/stealth/Tales/navigation | Stealth and Tales callback/party fixes tested; remaining AI/navigation review pending |
| UI/editor/CI/packaging | Detailed review pending |

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

### Authored asset validation after batch 15

Validation passed for 113 assets and compilation passed for 72 included Blueprints: zero errors or
invalid assets. Four presentation warnings remain: the two story NPCs use prototype Narrative Manny
appearances, and FarmHandover has no authored camera shot and a zero blend-out. Blacksmith's Claimed
background is verified as Unity in the Ashes and its entry sound as Horns of War. No assets were saved
by this validation. Evidence: `AssetValidation_Batch15.json`, `AssetValidation_Batch15.log` and
`CapitalDefinitions_Batch15.json`. This does not replace cook or physical multiplayer gates.

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

- Complete the upgrade/garrison/production callback transaction review; existing debit callbacks
  occur before final gameplay commits. No atomicity completion is claimed for those paths yet.
- Finish assault physical spawn/restore callbacks, malformed record/arithmetic limits and client
  movement/reindex validation. Unloaded-target treaty cancellation is already covered by batch 8.
- Review remaining hierarchy integer sums and hardcoded capital rewards against Narrative event policy.
- Complete guard/AI/Tales/navigation/UI/editor review and the live release gates.
