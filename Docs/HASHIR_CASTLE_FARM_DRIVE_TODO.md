# Hashir's Castle Hill Farm trip

Updated 2026-09-12. **HopDistrictTest route and remote boarding fixed; complete story acceptance remains open.**

## Confirmed cause and fix

Hashir's live `BPA_DriveToDestination` reproduced the reported missing-lane abort.
The saved ZoneGraph contained only the short assault road's two lanes. The longer
authored `ZoneShape_1` road had not been built into it. In addition, the dialogue's
drive goal used that shape's local endpoint `(7527,-2180,0)` as a world destination.
The shape origin is `(-4290,3100,0)`, so its world endpoint is `(3237,920,0)`.

The project changes are:

- `/Game/HopDistrictTest`: rebuild the existing graph to include both roads
  (four lanes). Set the story shape's reverse profile off for keep-right travel
  and clear its all-tags override; its lane profile supplies the Road tag.
  Existing road geometry and the short assault road are preserved.
- `/Game/HOPTRENDY/Character/Hashir/DBP_Hahsir`: correct `ParkDestination` and
  `CachedFinalDestination` in the existing `Goal_DriveToDestination_C_0` to
  `(3237,920,0)`. Keep its car, passenger definition, dialogue IDs and event.

The repeated `BPA_ReturnToSpawn` setup failure follows the drive abort: Hashir's
controller still possesses the car, so `GetControlledNPC()` cannot return an NPC
pawn for that fallback. A successful trip avoids that abort. This change does
**not** add failure recovery for a future missing or obstructed route.

The failed service logs segment coordinates after removing its goal. Identical
start/end debug values alone do not establish that the original requested goal
had identical endpoints. The saved destination and real lane query establish the
two authoring faults above.

## Existing owners and impact

Narrative Tales owns dialogue/quest state. The existing Add Goal event assigns
Hashir's Native drive goal; Native activities, vehicles, seating and ZoneGraph
remain responsible for travel. No Narrative source or asset was changed. There
is no new capture, vehicle, faction, save or replication authority.

This is a UE 5.8 **project-content** fix, not a plugin-content or C++ change.
No API, GameplayTag, GUID or save-record migration is introduced. Existing campaign
saves and already-running goals have not been certified against the new target.
HopDistrictTest's successful map reload is not a World Partition streaming test.
The AlMalik destination/road load-order gate remains open.

The pre-existing user versions of both edited assets were backed up before this
batch in `Saved/Verification/20260912_HashirDrive`. The changes preserve their
current authored content, rather than replacing them with older Git versions.

## Verified behavior

- Reproduced the actual live drive activity's missing-lane failure before fixing
  the assets. Hashir entered the driver seat, waited for the passenger, then
  aborted on departure. `LiveBefore.json` and `LiveBefore.log` preserve this.
- The corrected standalone trip passes nine checks: correct driver, passenger
  wait, attachment, physical departure, Native arrival, arrival near the Farm,
  stopped car, accepted exit and detached passenger. Arrival was about 17 seconds
  after the fixture began; no missing-lane or ReturnToSpawn failure recurred.
  A second run after restarting the editor also passes all nine checks through
  the saved regression script (arrival about 25 seconds, completed exit by 39).
- Reloading the saved map without rebuilding retains four lanes. The exact
  Native route query returns an approximately 8,324 cm route.
- `Scripts/Territory/verify_hashir_farm_route.py` passes 11 asset/Native API
  checks, including rejecting the old destination and an absent destination,
  preserving the short assault route, and validating both assets without warnings.
- The dialogue Blueprint compiles with zero errors and zero warnings.
- All eight existing `TerritoryFramework.Roads` automation tests pass on UE 5.8.
  One fixture reports its existing missing skeletal-mesh socket warning.
- UE 5.8 incremental cook/package and a 60-second packaged startup pass (exit 0).
  Startup uses the Development Game executable in server mode, not a compiled
  dedicated-server target. The existing optional intro-cutscene warning remains.
- A fresh comparison of 741 Narrative source files against the installed
  Marketplace package found no differences. All 347 framework source files match
  the preceding verified build. This batch does not claim a new full engine build
  or rerun of the previous 314-test suite on each engine.

The live fixture starts at the real "Come with me" node deliberately and stages
the passenger beside the car before using Native seating. It does not prove the
whole quest or normal player input. It never teleports the car or fakes arrival.
Evidence is under `Saved/Verification/20260912_HashirDrive` in TDA.

For a fresh standalone regression, start one-player PIE in HopDistrictTest, wait
for the player and Hashir to load, then run
`Scripts/Territory/verify_hashir_farm_drive_pie.py` through editor Python. It writes
`LiveDriveRegression.json`; wait for `passed: true` before stopping PIE. Do not run
this fixture in multiplayer or use a dialogue editor template as a live event.

## Multiplayer boarding — fixed and verified

The first network attempt showed remote boarding releasing its slot before
attachment and a stationary car drifting on clients. The follow-up diagnostic
confirmed that simulated seated characters still had capsule physics enabled.
Their capsules pushed their own car away from the server position. Changing
only that client collision made the cars converge and Native boarding succeed.

`UTerritoryMountPresentationComponent` now follows Native mount attachment on
simulated clients. It disables the seated capsule and restores its prior mode
on exit. Territory guards include it; the TDA player and Hashir Blueprints now
add it. Server seating, owner-client abilities and Narrative Pro remain unchanged.
This follow-up is a plugin C++ and project Blueprint change; the earlier route
batch above was project-content only. No new durable state or RPC is introduced.

The recorded listen-host/two-client regression passes all 16 checks, including
a third client joining while Hashir is seated, remote passenger boarding,
physical departure, Native arrival, stopped-car agreement, client-requested exit
and restored collision on every client. It stages the server passenger beside
the car and queues Native's controller `Begin Interact`/`End Interact` events on
a native timer. It does not claim hardware keyboard focus or the preceding quest.
See [setup](MOUNT_CLIENT_COLLISION.md) and
[verification](MOUNT_CLIENT_COLLISION_VERIFICATION_2026-09-12.md). The old failed
network evidence remains under `20260912_HashirDrive`; passing evidence is under
`Saved/Verification/20260912_HashirBoarding`.

For a fresh network regression, use a listen host and two clients, run
`Scripts/Territory/verify_hashir_farm_network_pie.py`, then use a native editor call
to begin the live drive node on the server controller printed by the script.
Wait for `NetworkRegression.json` to report `passed: true` before stopping PIE.

Two earlier test attempts crashed through invalid test invocation: executing an
editor dialogue template without a live world, and synchronously invoking a
client RPC path from editor Python. Unreal's actor-script guard makes those RPCs
local and can recurse. Keep them separate from gameplay evidence. Native timer
or input-driven calls are required for network testing; do not repeat these
invalid template/client calls. The failed attempt logs are preserved.

## Story and save/load findings — still open

- The normal dialogue success branch uses `NC_IsQuestSucceeded` for
  `NQ_CaptureBlacksmith`. The successful physical trip was tested from its drive
  node, so the full capture/handover/garrison/wave-to-dialogue flow remains unproven.
- "Go Capture Castle Hill Farm" is a following dialogue node with a 500 cm
  `BPC_CheckDistance` condition. That is a condition for selecting a dialogue node,
  not a persistent arrival listener. Author and verify a durable arrival and
  continuation step through Tales; also prevent replaying the trip/reward after
  its intended completion. Do not equate finishing the drive with quest success.
- The current goal does not save itself. Native's player interaction loader
  deliberately does not restore seat occupancy, and controlled vehicles return
  no save GUID. Mid-trip save/resume therefore needs an explicit story checkpoint
  or compatible adapter, with one car/driver/passenger and no duplicate completion.
- Native's route curve omitted an intermediate turn point in the inspected query.
  The flat test map drive arrived, but tight road-corner/obstacle clearance is not
  certified. Investigate through the existing route adapter if the city fixture
  reproduces corner cutting; do not patch Native or create another road graph.

## Remaining acceptance

- [ ] Normal input: obtain the quest, complete Blacksmith through its intended
  stages, speak to Hashir, board, arrive, exit and continue once.
- [x] Remote passenger boarding, vehicle movement, exit and capsule restoration
  on a listen host and two clients, plus a late third client.
- [ ] Full quest result in multiplayer; compiled dedicated-server and returning-client gates.
- [ ] Missing/unloaded lanes, disconnected roads, invalid car/driver and blocked
  entry recover clearly without fake completion or endless fallback retries.
- [ ] Save/reload before departure, during the trip and after arrival; preserve
  intended story progress without duplicate cars, goals or rewards.
- [ ] AlMalik World Partition road/destination readiness and physical clearance.

Broader release gates remain in [the roadmap](ROADMAP_AND_REMAINING.md).
