# Research findings: implementation and verification

This ledger continues the research checkpoint at plugin commit `9670a1d`. Source changes below are not a release certification. The research PDF describes the original defects, not the repaired behavior.

| Finding | Resolution | Current gate |
|---|---|---|
| Player storage remains Heroes after a betrayal | Optional binding follows Native primary membership and observes Native notifications/load/possession | Native behavior and live server/two-client proof passed; TDA controller migrated |
| Last registered player silently wins storage | Priority candidates; equal highest priority blocks storage with a conflict read model | Native lifecycle tests and live replicated conflict/priority proof passed |
| More than one faction player removes sole-player fallback | Explicit candidate policy makes selection deliberate; fallback is not used to bypass a conflict | Native behavior passed; author a deliberate leader/depot when needed |
| Component says registered after displacement | Query current selection; server replicates derived status | Native behavior and live owning-client status passed |
| Open economy/district screen uses old faction | Resolve live owner faction; keep explicit fixed views | Native widget/save and already-open remote client screen passed |
| Shared membership hides a hostile pair in dialogue | War overrides the shared-faction shortcut | Native dialogue/combat agreement passed |
| Add faction event broadcasts intermediate memberships | Validate final membership and commit once; optional primary ordering | Native suite and live additive primary choice passed |
| Seven visible settings do nothing | Hide inactive controls and document actual data owners | UHT/build and authoring contract tests passed |
| Legacy save actor and old BT tasks look current | Prevent new legacy save placement and mark BT nodes with migration guidance | Both editor builds and Blueprint/data validation passed |
| Unused private navigation callbacks | Remove unbound callbacks; keep actual map marker subscriptions | Build and full suite passed |
| No disguise profile asset | Classified as optional unconfigured feature, not dead implementation | No game behavior invented |
| Whole-world load invalidates Native's current map record | Remove old guards from play immediately, defer their EndPlay record writes until Native's reader returns | Exact Native regression, both engines, and two real world/player restores passed |
| Native attack goals retain guards removed alive during load | Remove target-bound Native goals using their existing key/removal API; repeat on final guard removal | Actual Native Blueprint regression and repeated live restores passed without stale-target errors |
| Saved vehicle limits or approach charges grant extra deployments | Validate the eight-car/eight-approach limits, unique nonempty approach IDs, and matching global/per-approach spent totals before reconstruction | Both editor builds, native save regression and live server/two-client cancellation passed |
| Production continues after an item callback changes faction or depot | Recheck the original requesting account or selected depot around Native inventory mutations; compensate only in the original inventory | Two regressions reproduced the defect before the change; all 294 tests and all three build configurations pass on both engines |
| Currency callbacks reenter settlement, reload a wallet, or invalidate later upkeep accounts | Payment receipts distinguish rejection from load interruption; recheck each account; preserve loaded purchases and current history | All 295 tests on both engines, real Native world/player load and two-client wallet/history proof pass |
| Repeated NPC saves retain removed generators and load old values | Territory's Native activity-component subclass rebuilds snapshots, keeps the latest record, restores existing objects and filters explicitly retired classes | All 296 tests pass on both engines; portable controller cold-reopens in UE 5.7; live/package checks recorded below |

## Verified checkpoint

- UE 5.8.2 and UE 5.7.4 Editor/UHT/runtime/editor-module builds pass.
- Development and Shipping game compilation passes on both engines. TDA's current UE 5.8 cook, stage and package pass; the first cook attempt hit the open editor's tooling port, and the retry passed after closing the editor.
- The latest currency checkpoint passes 295 tests on each engine. UE 5.8 has 275 clean passes plus 20 with warnings; UE 5.7 has 273 clean passes plus 22 with warnings.
- HopDistrictTest listen server plus two clients passes faction replacement, additive primary choice, account conflict/priority, already-open UI refresh, and rejected client mutations.
- Two consecutive Native world restores plus Native deferred player loading preserve owners, guard counts, reserves, inventory and the host player's faction/account UI. Native world `Load` does not itself invoke player loading; the fixture uses Native's separate player-load path deliberately.
- One fresh fourth PIE world joins after those changes and receives matching faction-resource snapshots, correct membership, selection status and inventory.
- UE 5.8 validates 235 current framework/Hashir assets with zero errors and eight presentation warnings. UE 5.7 validates 118 portable assets with zero errors and four example warnings.
- The portable controller was saved and cold-reopened in UE 5.7 before copying it back to the plugin. Both example controllers now follow the owner's political faction. Other existing component instances keep their fixed binding until deliberately migrated.
- Native source remains unchanged. A PIE-dirtied Native camera was exported to a verification backup before restart; it was not saved over vendor content.
- A clean UE 5.7 host verifies zero project/theme dependencies in all 118 portable assets. TDA's RPG theme references are introduced by its own redirects, so portable dependency certification is run without those redirects.
- UE 5.7's portable content cook passes with zero errors and 48 Native/demo warnings. TDA's UE 5.8 cook/stage/package passes with zero errors and 30 existing content/tooling warnings.
- The first packaged combat run exposed a Native behavior-tree ensure followed by a garbage-collection crash after deaths. Target-goal cleanup was narrowed to actors removed alive, skips world teardown, and checks the currently registered keyed goal. Real death notifications stay with Native. The updated Development binary with the same cooked content then completes the 90-second assault smoke, including NPC/player deaths and garbage collection, with exit zero and none of those errors. This is Game server-mode evidence, not compiled dedicated-server certification.

## Remaining work in order

1. Fix generic Narrative NPC client death through a plugin/project adapter. Source inspection confirms that Native BP_NarrativeNPC calls RemoveAllGoals unconditionally, although its activity component lives on the server-only AI controller. Preserve Native death presentation and weapon behavior while adapting this path. Hashir's safe perception generator addresses a different defect. Territory controller generator-save migration is implemented; Hashir's generic controller still needs its own integration without losing inherited Native Blueprint behavior. Include the attack-target EQS and weapon decal warnings observed in the 2026-09-09 packaged smoke, described below. Also reproduce the same-team shot cancellations observed after the live world restore and check whether old targets are cleared correctly.
2. Complete actual AlMalik World Partition streaming and compiled dedicated-server certification. The live late join above does not replace either gate.
3. Complete the deferred lighting visual/performance review, then the remaining Act 1 story authoring decisions. Do not publish fresh release artifacts before the relevant gates pass.

## Authority and migration

Native PlayerState owns membership and Native inventory owns items/currency. Economy selects storage; the component is a registration adapter. WorldState publishes derived stockpile snapshots. Territory ownership is not changed by faction switching. No Narrative Pro source is modified. See [migration instructions](Faction_Integration_Migration.md).

Verification output for this batch is under project `Saved/Verification/20260908_FactionIntegrationFix`. Update this ledger from actual results before committing or publishing.

## Currency callback follow-up — 2026-09-09

Native inventory remains the currency authority. Economy now returns a temporary payment receipt with `Rejected`, `Applied`, or `Superseded` status. Native inventory loads, campaign-load start, and economy restores invalidate the old operation. Territory payments cannot recursively start another Territory payment inside a currency/history callback. An independent Native expense remains valid; the ledger records the balance at its own payment, before that later expense.

Built-in upgrades and garrison purchases use the receipt to cancel only unpaid staging. A load interruption preserves restored levels, guards and wallets without an invented refund. Upgrade checks also use the existing territory load generation, including when deserialization restores identical values. There are no new SaveGame fields, replicated fields or actor IDs. Existing Blueprint payment nodes remain callable; custom purchases should migrate to the result nodes as described in [payment callback guidance](Currency_Callback_Migration.md).

Faction payouts observe their whole account group before the first payment and stop if a load replaces it. Upkeep keeps its selected group and rechecks each account's faction, authority and funds after earlier callbacks. The deficit reports only unpaid upkeep. Transaction history is capped immediately, and WorldState ignores a stale transaction removed from the authoritative ledger or an ID already included in its snapshot.

The first regression reproduced extra nested currency, incorrect receipt balances, and stale transaction rows after inventory/history restore. The corrected tests also cover actual Native expenses, later-account reload, partial upkeep, loaded upgrade and garrison state, client-role rejection, integer boundaries, manual history limits, Blueprint flags, and save/load of completed payments. One older WorldState fixture broadcast an event without a corresponding authoritative ledger entry; it now seeds a valid unique record and separately proves that stale and repeated broadcasts are ignored.

All 295 tests pass on each engine: UE 5.8 has 275 clean passes and 20 with warnings; UE 5.7 has 273 clean passes and 22 with warnings. Editor/UHT, Development game and Shipping game builds pass on both engines. `CurrencyRed58`, `CurrencyFinalTests58`, `CurrencyFinalTests57`, and `CurrencyFinalBuilds.json` contain the evidence. All 741 local Native source files still match the installed Marketplace package.

The UE 5.8 cook/stage/package passes with zero errors and 30 existing cook warnings. Its 90-second packaged assault run exits zero without a fatal error or ensure. This run also exposes three Blueprint warnings: one null Blackboard in Native `EQSContext_AttackTarget.ProvideSingleLocation`, followed by an unowned-query completion warning, and two null `DecalPS` accesses in Native `BP_WeaponFXBase.Spawn Decal` from `GC_Burst_Unarmed`. They remain open for the compatible AI/presentation adapter pass. Do not describe this smoke as free of Blueprint errors or as compiled dedicated-server certification. See `CurrencyCookStage58.log`, `CurrencyPackagedSmoke58.log`, and `CurrencyPackage58.json`.

`LiveCurrencyCallbacks58.json` passes on the HopDistrictTest listen server and two clients. It proves rejected nested Territory credit, a valid independent Native expense, interrupted inventory load, and an actual Native world load followed by deferred player loading inside the payment sequence. The paying client receives its restored 590 balance, the other client's 5000 balance is unchanged, and both receive the same single transaction ID and historical balance of 600. Direct client credits are rejected. The initial Python comparison used a temporary Guid object's display address; the verification now compares the Guid's exported value.

Final validation checks 235 framework/Hashir assets and compiles 142 Blueprints, with zero errors and eight existing warnings. The editor is reopened on HopDistrictTest and PIE is stopped. The camera dirtied by PIE/compilation was exported to a verification backup and reloaded from disk without saving vendor content. No new release was uploaded.

## NPC generator save follow-up — 2026-09-09

Native's activity component remains the AI authority. `UTerritoryNPCActivityComponent`
adapts its existing save callbacks: each save replaces the generator snapshot;
old duplicate classes keep their latest record; missing/abstract/explicitly retired
classes are skipped. Existing configured generators receive saved values without
being initialized or bound a second time. Native delta records are expanded on a
temporary uninitialized object, so saved zero values and empty lists replace later
live changes without resetting transient state. An added regression reproduced the
incorrect `0 -> 99 -> load -> 99` result before this correction.

`BP_TerritoryNPCController` inherits `BP_NarrativeNPCController` and uses Unreal's
native component class override in its existing `NPCActivityComponent` slot.
Native Blueprint class casts and inherited behavior remain valid. The first local
controller attempt preserved the graph bodies but not Native Blueprint inheritance;
that attempt was replaced before committing. The older Territory controller asset
is unchanged. No Native source or Blueprint is copied or patched for this change.

Both portable guard/assault Blueprints and their TDA counterparts now select the
new controller. The first live check exposed that their serialized class overrides
still selected Native's controller despite the changed C++ default. The final
selection is deliberate and is covered by the Blueprint inheritance/component test.
The direct C++ guard/assault defaults use the small Native C++ controller adapter.

The regression uses Native `CreateActorRecord` and `LoadActorFromRecord`, including
controller recreation with the same component name, duplicate older snapshots,
removed generators, latest scalar state, empty lists, transient-state preservation,
retired classes, invalid classes, pre-BeginPlay load and rejected client mutations.
There are no new SaveGame fields, replicated state, faction records or actor IDs.
See [NPC activity save migration](NPC_Activity_Save_Migration.md).

All 296 tests pass on both engines: UE 5.8 has 276 clean passes and 20 with warnings;
UE 5.7 has 274 clean passes and 22 with warnings. All six Editor/Development/Shipping
build targets pass. The final Native-child controller was authored in a full UE 5.7
editor and cold-reopened successfully. The component class authoring API requires
an editor transaction; its commandlet attempt failed before saving the new asset.
Final live/package evidence is recorded with the verification files prefixed `NPC`
under `Saved/Verification/20260908_FactionIntegrationFix`.

The final UE 5.8 cook/stage/package passes with zero errors and 30 existing warnings.
The UE 5.7 incremental portable cook passes with zero errors and five warnings.
The final 90-second packaged assault exits zero without a fatal error, ensure or
null Blueprint access. Native combat and authored dismounts run. Same-team shot
cancellations also occur in this fresh run, so they are not specific to save/load;
their target and obstruction causes still need a focused audit. The earlier
EQS/decal warnings were not reproduced here and are not declared fixed.

`LiveNPCSave58.json` passes on the actual TDA guard and two clients. Its server
controller is `BP_TerritoryNPCController`; two saved generators become three after
adding a temporary test generator, remain three across four saves, and return to
two after removal. Both clients receive the same faction and have no local NPC
controller/activity authority. Native actor-record restoration and default-value
checks are covered by the behavioral regression, separately from this live fixture.
Final validation checks 236 assets and compiles 143 Blueprints, with zero errors
and eight existing presentation warnings. All 741 Native source files still match
the installed package. PIE is stopped, the prior client-count setting is restored,
and no packages remain dirty. The Native camera was exported for verification and
reloaded without saving vendor content. No new release was published.

Hashir's generic controller and client death path are still open. This migration
does not globally redirect Native NPC classes or rewrite unrelated quest generators.
The saved attack-target, EQS and decal audits also remain separate.

## Dedicated-server prerequisite

The installed UE 5.8 distribution rejected `TDAServer` with “Server targets are not currently supported from this engine distribution.” This is an engine-distribution prerequisite, not a passing dedicated-server gate. A server-capable engine build is required for that certification. Game-server mode is a separate smoke test.

## Saved assault ledger follow-up

CounterAttackSubsystem remains the server authority for deployment charges. A nonterminal saved assault with an invalid vehicle ledger becomes `Cancelled / ConfigurationInvalid` before any physical reconstruction. Recorded deaths remain consumed; all remaining force withdraws once. Loading does not change ownership or reroll the saved seed/decision. WorldState publishes that repaired state to clients immediately. Malformed terminal history is bounded for display and remains terminal.

The regression covers negative and extreme positive counts, duplicate and missing approach identities, inconsistent totals, excess rows, repeated Native-format SaveGame serialization, a client WorldState load, and a target that has been removed from the world. Valid version-zero survivor migration, zero-charge on-foot approaches and eight spent cars remain supported. There are no new Blueprint fields or required asset edits. An inconsistent older save is cancelled conservatively because its actual remaining car credit cannot be recovered safely.

UE 5.7 passes all 293 tests (272 without warnings, 21 with warnings). UE 5.8 passes 292 tests in the headless Entry-map run; its map-dependent route test then passes with HopDistrictTest loaded, giving 293 verified passes. Both editor builds and UE 5.8 Development game compilation pass. `LiveLedgerCancellation58.json` proves matching cancellation, finite counts and unchanged decision data on the listen server and two clients. These checks do not replace the outstanding compiled dedicated-server gate.

## Production account-change follow-up

Native inventory remains the item authority. The existing private recipe executor now accepts the caller's account-validity check. Manual recipes check their explicit requesting actor's membership and inventory identity; periodic recipes check the current selected faction inventory. If membership, priority or a tie changes that account during a Native input/output callback, the existing compensation routine removes this recipe's output and returns its input in the original inventory. It never transfers the interrupted batch to a successor. A later cycle resolves its account afresh.

The red tests reproduced false success and retained outputs on the former account. The repaired regressions cover changes during both input removal and output addition, priority replacement, a tie, faction removal, Native inventory save/load, cached unloaded production sites, no replay of the cancelled cycle, and a successful next cycle on the successor. Existing tests still cover rejected client requests and incomplete compensation. No public Blueprint signature, replicated field or SaveGame schema changed.

The final suite passes 294 tests on each engine: UE 5.8 has 275 clean passes plus 19 with warnings; UE 5.7 has 273 clean passes plus 21 with warnings. UE 5.8 now runs with HopDistrictTest loaded, so its route test passes in the same complete run. Editor, Development game and Shipping game builds pass on both engines. See `ProductionFinalBuilds.json`, `ProductionRed58`, `ProductionFinalTests58` and `ProductionFinalTests57` in the verification folder.

The updated UE 5.8 cook/stage/package completes with zero errors and 30 existing content/tooling warnings. Its 90-second packaged assault run exits zero without ensures, fatal errors or stale-target Blueprint errors. This remains Development Game server-mode verification, not a compiled `TDAServer` result. The verification stage was refreshed locally; no new GitHub release was published.

`LiveProductionCompensation58.json` verifies a real Native faction event inside an inventory output callback on the listen server. The recipe reports `SettlementChanged`, the original input is restored, both outputs are absent, and the owning remote client receives the same faction and item quantities. Its direct client recipe request is rejected and the second client's inventory is unchanged. Final UE 5.8 validation checks 235 framework/Hashir assets, compiles 142 Blueprints, and reports zero errors plus eight existing warnings. All 741 local Narrative Pro source files still match the installed Marketplace package.
