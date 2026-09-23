# Re-audit repairs — 23 September 2026

Six focused repairs preserve Narrative Pro as the native gameplay foundation. No Narrative source was changed. Build and test receipts are in the associated projectless audit workspace; release status is recorded in its outputs/TerritoryFramework_Implementation_Plan_2026-09-23.md and final implementation report.

## Changes and authority

1. **Directional diplomacy:** imported projections carry `bNarrativeObserved`. Only explicit Territory treaty commands write bilateral Native attitudes. Native one-way callbacks cannot silently overwrite the reverse direction. Rich compatible treaties and provenance survive save and client hydration. Legacy saves without provenance retain the previous authored interpretation; lost historical directions cannot be reconstructed.
2. **Transition observers:** Territory commits reconcile guard availability, capture registrations and property upgrade reset before Narrative transition events. Scoped transition state and load-generation checks prevent callbacks from resuming a superseded transition. Existing Blueprint hooks remain.
3. **Durable admission:** `UTerritoryAssaultAdmissionTask` uses Native `UNarrativeTask` begin/tick/end, replicated progress and indexed Tales save records. It retries pre-admission refusals while active, using the existing Wave event's native settings and conditions through `TryScheduleWave`. It completes upon a matching durable scheduler record, including a cancelled attempt. It does not claim victory or automatically reroll an admitted failure. Pair it with `UTerritoryAssaultTask` for the outcome; use an explicit authored retry after cancellation. A non-empty Scenario ID is required. Blueprint Execute Event overrides are not invoked by the admission task; its Request is native scheduling configuration. `LastAdmissionFailure` is a server diagnostic, not replicated status text.
4. **Unloaded parents:** one shared pure hierarchy reducer serves loaded actors and WorldState projections. Child changes recompute strict unloaded ancestors; changed authored topology is reconciled separately. Presentation-only registration preserves an existing authoritative parent snapshot. Loaded parents still commit through their own lifecycle. Configure Campaign City Definitions so the unloaded topology is known. Missing, mismatched and duplicate child slots fail closed.
5. **Directory retirement:** `ATerritoryWorldState::RetiredDirectoryGUIDs` is explicit current-content policy. Import, export, registration and publication exclude only named stable identities. Missing actors alone never authorize deletion. Renames retain GUIDs; a new GUID may reuse an old tag. Validation rejects retirement IDs still referenced by configured definitions or placed actors. Retire each removed identity explicitly. This removes obsolete directory projections; it does not erase Native actor-save archives or historical assault records. The project policy remains empty because no actual identities were approved for retirement.
6. **Journal refresh:** the cached operations revision includes the displayed economy and transaction snapshot. Equal credits/debits therefore refresh the audit even when balance and district rows are unchanged. Rendering uses that same snapshot, through existing authoritative/client read APIs.

## Native reference and counterattack lifecycle trace

Native references are NarrativeArsenal's `Tales/QuestTask.cpp` (`BeginTask`, `EndTask`, `SetProgressInternal`) and `Tales/TalesComponent.cpp` (`PrepareForSave_Implementation`, `PerformLoad`). Native saves task progress by index, so project authoring appends tasks and retains existing indices. The restored Blacksmith Wave remains on defence state QuestState_10; QuestBranch_309 retains the dialogue and exact-scenario victory task.

The scheduler remains `UTerritoryCounterAttackSubsystem`; the admission repair does not replace any physical lifecycle stage:

| Stage | Existing implementation and durable evidence |
|---|---|
| Capture and request | Territory Control/Volume commit; `HandleTerritoryControlChanged` and authored Wave call existing admission APIs |
| Grace | `ScheduleAssault` validates authority, identity, ownership, force, diplomacy and budgets; commits AssaultID, ScenarioID, cycle, seed and grace time |
| Evaluation | `AdvanceAssault` / `EvaluateAssault` use that saved seed, policy and route selection; a failed roll is a terminal record, not a new request |
| Warning and proximity | `ScheduledWarning` becomes `WaitingForPlayerProximity`; `NotifyRelevantPlayers` presents the saved force; absent target actors wait for registration |
| Physical activation | `ActivateAssault` / `TryCommitProximityActivation` commit Active before spawning; repeated players cannot activate it again |
| Participants and casualties | Native NPC definition/spawn path and capture participation; `NotifyParticipantRemoved` consumes a participant once and updates finite living/reserve/killed/withdrawn counts |
| Outcome | `CompleteRecapture` uses existing capture flow; exhausted physical force resolves via `ResolveAssault` to AllAttackersRemoved; no probability directly writes ownership |
| Recovery | `ResolveAssault` cleans physical participants and vehicles; profile-governed recurrence remains an existing scheduler policy, separate from a completed admission task |
| Persistence and replication | `RestorePersistentState` preserves cycle/seed and reconstructs finite survivors; WorldState save/read models and `OnAssaultChangedLive` carry records to clients |

## Verification limits

New regressions cover native directional setters and save/load; observer-time guard/upgrade state and interrupted loads; actual Native quest progress save/load before and after admission; child snapshots with never-loaded ancestors; explicit retirement and tag/GUID reuse; and real inventory transactions rendered by the journal. Separate client-world fixtures test read paths and rejection, not socket transport. Dedicated PIE and packaged gameplay require distinct current receipts.

Unreal's cached source discovery omitted a newly added test despite successful compilation. Verification therefore checks each new test's exact name in the report, not only aggregate counts. The generated target Makefile was moved to scratch and rebuilt when necessary; source files and user work were not discarded.

## Integrated result

Editor/UHT and Win64 Development builds pass. The current full suite has 368 successful tests (329 clean, 39 with warnings), zero failures; all six new tests and the existing Hashir victory-gate test are present. Dedicated PIE with two clients and late joining passes the restored physical battle/victory check and observed/authored diplomacy provenance check. Native damage effects finish the battle fixture; normal player weapon combat and multiplayer dialogue selection are not claimed.

Cook/stage/package succeeds. Release acceptance remains blocked: final stopped-PIE validation marks all 124 assets valid but three presentation assets have warnings; packaged NullRHI logs six invalid shader-map errors, and DirectX 12 also reports two uncooked shader-map errors with ModularVictorianCity fallback materials. Both runtime processes exit zero. Preloading all Native controller-data assets before a fresh Blueprint compilation sweep avoids the initial HUD compilation ensure. Original failures and current results are retained in the audit workspace's implementation evidence/report. Narrative source remains unchanged (741 files match the vendor baseline), and all six preexisting tracked plugin diffs are preserved.
