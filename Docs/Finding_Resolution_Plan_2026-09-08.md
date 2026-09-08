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

## Verified checkpoint

- UE 5.8.2 and UE 5.7.4 Editor/UHT/runtime/editor-module builds pass.
- Development and Shipping game compilation passes on both engines. TDA's current UE 5.8 cook, stage and package pass; the first cook attempt hit the open editor's tooling port, and the retry passed after closing the editor.
- Both engines pass 293 tests. UE 5.7 reports 273 passes without warnings plus 20 passes with warnings; these are 293 passes, not 273 total.
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

1. Finish the production/refund settlement audit, including account changes during item callbacks. Saved vehicle ledger validation is implemented and verified as described below; it does not impose an invented upper limit on authored total infantry force.
2. Fix generic Narrative NPC client death through a plugin/project adapter. Source inspection confirms that Native BP_NarrativeNPC calls RemoveAllGoals unconditionally, although its activity component lives on the server-only AI controller. Preserve Native death presentation and weapon behavior while adapting this path. Hashir's safe perception generator addresses a different defect. Test old saved goal generators deliberately.
3. Complete actual AlMalik World Partition streaming and compiled dedicated-server certification. The live late join above does not replace either gate.
4. Complete the deferred lighting visual/performance review, then the remaining Act 1 story authoring decisions. Do not publish fresh release artifacts before the relevant gates pass.

## Authority and migration

Native PlayerState owns membership and Native inventory owns items/currency. Economy selects storage; the component is a registration adapter. WorldState publishes derived stockpile snapshots. Territory ownership is not changed by faction switching. No Narrative Pro source is modified. See [migration instructions](Faction_Integration_Migration.md).

Verification output for this batch is under project `Saved/Verification/20260908_FactionIntegrationFix`. Update this ledger from actual results before committing or publishing.

## Current build limitation

The installed UE 5.8 distribution rejected `TDAServer` with “Server targets are not currently supported from this engine distribution.” This is an engine-distribution prerequisite, not a passing dedicated-server gate. A server-capable engine build is required for that certification. Game-server mode is a separate smoke test.

## Saved assault ledger follow-up

CounterAttackSubsystem remains the server authority for deployment charges. A nonterminal saved assault with an invalid vehicle ledger becomes `Cancelled / ConfigurationInvalid` before any physical reconstruction. Recorded deaths remain consumed; all remaining force withdraws once. Loading does not change ownership or reroll the saved seed/decision. WorldState publishes that repaired state to clients immediately. Malformed terminal history is bounded for display and remains terminal.

The regression covers negative and extreme positive counts, duplicate and missing approach identities, inconsistent totals, excess rows, repeated Native-format SaveGame serialization, a client WorldState load, and a target that has been removed from the world. Valid version-zero survivor migration, zero-charge on-foot approaches and eight spent cars remain supported. There are no new Blueprint fields or required asset edits. An inconsistent older save is cancelled conservatively because its actual remaining car credit cannot be recovered safely.

UE 5.7 passes all 293 tests (272 without warnings, 21 with warnings). UE 5.8 passes 292 tests in the headless Entry-map run; its map-dependent route test then passes with HopDistrictTest loaded, giving 293 verified passes. Both editor builds and UE 5.8 Development game compilation pass. `LiveLedgerCancellation58.json` proves matching cancellation, finite counts and unchanged decision data on the listen server and two clients. These checks do not replace the outstanding compiled dedicated-server gate.
