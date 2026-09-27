# Narrative callback invalidation — 2026-09-27

`CommitOwnershipData` captured its save-load generation after running Narrative conditions. A condition could restore an older campaign and return true, after which the stale candidate overwrote the restored state. Generation checks also ended before the authored state-event bundle: an event could restore or destroy its Territory while later rewards and ownership/state success broadcasts continued.

The commit now captures the existing generation before validation and checks generation/lifetime after callback stages. Entry/exit-condition loops stop on invalidation; state-event dispatch stops after an invalidating precondition or event. Guard teardown is checked before replacement spawning. The copied arrays and their original ordering remain intact, so editing a configured list during dispatch still affects only the next bundle.

`SetDerivedControl` now returns its internal commit result. City loss/capture presentation requires a successful derived commit and the expected final state; City, District and Place subclass hooks check callback validity before subsequent success delegates. This changes no Blueprint node or serialized field. Native callers ignoring the new bool remain source-compatible and must rebuild with the module.

## Existing Narrative contract reused

Installed Narrative `UNarrativeSaveSubsystem::LoadActorFromRecord` calls `Actor->Serialize` using `ArIsSaveGame`, then `INarrativeSavableActor::Execute_Load`. The plugin's existing `GarrisonLoadGeneration` increments during that deserialization, including identical-state reloads. The implementation extends that established invalidation mechanism; no Narrative source, save format, subsystem authority or replicated layout changed.

## Behavioral regressions

- `TerritoryFramework.Capture.Regression.NarrativeCallbacksInvalidateStaleTransition`: actual Narrative record loads during entry and exit conditions; remaining conditions suppressed; first entry-event and event-precondition reloads stop rewards; subsequent ownership/state broadcasts suppressed; a later ownership listener reload stops the later state stage; target destruction aborts remaining events; client mutation remains refused; a later valid claim succeeds once.
- `TerritoryFramework.Capture.Regression.CityDoesNotAnnounceInvalidatedDerivedCommit`: a City's entry event restores its old record while children are captured; remaining reward and City captured delegate do not run; later exact reconciliation recovers and announces capture once.

Existing state-array replacement, condition recursion, guard-callback reload and deferred-ancestor tests remain relevant regression coverage.

## Limits and validation

An already executing Unreal multicast broadcast cannot be retracted or interrupted between its listeners by this caller. The guard stops subsequent bundle stages after that broadcast returns. Effects already executed before the load are not rolled back; Narrative restore is authoritative. An invalidated commit returns false and must not be presented as success.

The parent task rebuilt UE 5.8.3 Editor and Win64 Shipping successfully. The functional refactor suite passed 501/501 tests, including both regressions above. Current dedicated PIE also verified the authored Blacksmith's seven real guards, floor counts, two-client and late-join garrison parity. That transport smoke does not exercise callback-driven campaign restoration, actual World Partition streaming or packaged gameplay. The final audit verification receipt separates those remaining gates from these passing native fixtures.
