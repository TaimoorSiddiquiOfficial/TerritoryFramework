# TODO: Hashir drives the player to Castle Hill Farm

Reported: 2026-09-12. Status: **Open — reproduce and investigate.**

After successful Blacksmith capture, Hashir should drive the player to the
Castle Hill Farm location. The reported drive aborts. This is a story progression
blocker to investigate before final Act 1 authoring. No gameplay fix is claimed.

## Reported evidence

```text
LogArsenalStatics: Error: StartLane/EndLane not valid! Unable to calculate path on zone graph. Ensure search extents/points are near a zonegraph.
LogNarrativeActivityComponent: Display: Failed to run activity BPA_ReturnToSpawn_C_0 as SetupBlackboard() failed.
LogKinematicDriving: LogArrow: 'Error: COULDNT FIND PATH - NPC WILL ABORT DRIVE TO LOCATION ACTIVITY' - SegmentStart: (X=-3831.135 Y=3239.999 Z=3.065) | SegmentEnd:(X=-3831.135 Y=3239.999 Z=3.065)
LogBlueprintUserMessages: [BTS_SimpleKinematicDrive_C_1] Error: COULDNT FIND PATH - NPC WILL ABORT DRIVE TO LOCATION ACTIVITY
```

Initial source check, not a reproduced root cause:

- Native `UArsenalStatics::GetPathOnZoneGraph` finds the nearest start and end
  lanes using the supplied search extent and Any/All/Not tag filters. It prints
  this error if **either endpoint has no valid lane**, before graph path search.
  Check endpoint projection first, then connectivity if both endpoints are valid.
- The logged driving segment has identical start and end coordinates. Trace
  whether those values are the actual requested destination, a generated segment,
  or fallback debug values. They do not establish which point is wrong.
- `UNPCActivity::RunActivity` prints the SetupBlackboard failure when the selected
  activity returns false from that setup, then stops its behavior tree. The logs
  alone do not prove that Return To Spawn belongs to Hashir or caused the drive
  failure. Capture the exact NPC/controller and activity timeline.

## Investigation checklist

- [ ] Reproduce through the actual quest: obtain it from Hashir, complete the
  intended Blacksmith capture/handover, then start the Farm trip. Record the map,
  quest state, NPC, controller, selected car, driver seat and requesting player.
  Use HopDistrictTest as the existing test fixture; verify which map produced the
  report before treating it as the exact reproduction.
- [ ] Trace the saved project `NQ_CaptureBlacksmith`, Hashir's current main dialogue
  and their events to the exact Native drive goal/activity. Check that the trip
  starts once after verified capture and receives the intended Farm destination.
- [ ] Resolve the current Hashir definition (`NPC_Hashir`), controller, activity
  configuration and spawn information. Check old `NPC_Hahsir` references without
  rewriting unrelated user edits or treating the historical spelling as authority.
- [ ] Inspect the actual generated driving service/task from the log, its parent
  asset and blackboard keys. Follow start/end values from the quest through vehicle
  ingress, route setup and every segment update. Check for an unset target, reused
  start position, stale actor reference or a legitimately completed segment.
- [ ] Inspect Native lane lookup at the car and Farm arrival point: world, height,
  search extent, Any/All/Not lane filters, lane handles and loaded ZoneGraph data.
  Verify connected travel direction and arrival access once lane lookup succeeds.
- [ ] Use existing `InspectRoadNetwork` and `ExportRoadLaneDiagnostics` tools.
  Compare Native's exact drive query with `PreviewVehicleRoute` only as a second
  diagnostic: the latter uses the counterattack route helper, so its success does
  not prove that Hashir's own activity receives valid parameters.
- [ ] Check road build and load coverage. `BuildFromRoadActors` and
  `BakeRoadSurfaces` already produce Native ZoneShapes/ZoneGraph from generator
  actors or physical surfaces. A road mesh or physical-material assignment alone
  does not establish that the required lanes were baked, connected and loaded.
  Preserve the chosen keep-right road direction.
- [ ] Trace Return To Spawn scoring and SetupBlackboard requirements separately.
  Determine whether it is a normal fallback after the drive abort, an unrelated
  NPC failure, or an activity competing with Hashir's story drive. Keep Native
  activity/goal ownership and inspect spawn data before changing activity rules.
- [ ] Verify passenger entry, correct driver ownership, destination streaming,
  stopping, exit and quest continuation. Failure must not claim arrival or leave
  the player locked in the car. Define a bounded retry or clear failure outcome
  through the existing quest/activity APIs after the cause is proven.

## Ownership and implementation constraints

Narrative Tales owns the quest and dialogue. Native NPC goals/activities own
Hashir's behavior, Native vehicles own driving and seating, and Native ZoneGraph
owns road routing. Territory's existing road editor adapter authors the lanes;
Territory's existing capture flow supplies verified Blacksmith completion.

Fix project authoring or add a minimal Territory adapter when the source evidence
requires it. Do not patch Narrative Pro, create another road graph, teleport to
hide a failed route, or change ownership to advance the quest.

The investigation is read-only so far. Any later change must identify server
authority, passenger replication, stable quest/goal IDs, save/load behavior,
World Partition destination readiness and Blueprint migration effects.

## Acceptance before closing

- [ ] The real Blacksmith-to-Farm story flow completes once with Hashir driving
  and the requesting player riding, then exits and continues the intended quest.
- [ ] The exact lane/segment failure is reproduced before the fix and absent
  afterward. Zero-length/already-arrived requests have a deliberate outcome.
- [ ] Missing lanes, disconnected routes, unloaded destination and invalid car or
  driver fail clearly without fake quest completion or endless retries.
- [ ] Relevant Native behavioral/Blueprint regression tests and builds pass.
- [ ] Server and two-client play verifies seats, movement and one quest result;
  client requests cannot advance the authoritative quest or create duplicate drives.
- [ ] Save/reload before departure and during the trip preserves the intended
  quest state without duplicate cars, passengers or completion. Test streamed
  road/destination readiness in the World Partition story-map fixture.

Relevant assets to inspect include `/Game/TerritoryFramework/NQ_CaptureBlacksmith`,
`/Game/HOPTRENDY/Character/Hashir/DBP_Hahsir`, `NPC_Hashir`, `BP_HashirController`
and `AC_HashirPacifist`, plus the actual Native DriveToDestination and
ReturnToSpawn assets selected at runtime. Asset references are investigation
targets, not a claim that their current graphs caused the fault.
