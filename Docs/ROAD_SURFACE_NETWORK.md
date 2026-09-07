# Automatic city roads and assault arrivals

Territory's editor adapter builds Unreal ZoneShapes and baked ZoneGraph lanes from
existing road generators or road physical surfaces. Narrative Mass traffic,
traffic-light annotations and lane occupancy remain the traffic authorities.
Counterattacks query the same Native lanes and continue using Narrative vehicles,
Mount exits, NPC activities and the existing Territory capture subsystem.

## L_AlMalik

The story map is `/Game/HOPTRENDY/Map/L_AlMalik`, with World Partition enabled.
`Scripts/Territory/build_almalik_roads.py` in the TDA project imports the existing
`BP_Road_Generator` curves and numbered `BP_Crossroad_Generator` sockets. Run it
from the Unreal Python console with this map and its city region loaded:

```python
exec(open(unreal.Paths.project_dir() + 'Scripts/Territory/build_almalik_roads.py').read())
```

The project `CityRoad4L` Native profile matches the 1600 cm road modules: two
400 cm lanes in each direction, keep-right. The script reuses the project's
`/Game/TerritoryFramework/PhysicalMaterial/PM_Road`, SurfaceType9, on the collision
bodies of the five road/crossroad mesh assets. It does not modify their generator
Blueprints or Narrative vendor assets. Backups and reports go to
`Saved/RoadNetworkBackups/<timestamp>`.

The first city import produced 60 road shapes and 25 junction shapes. One empty
generator, `BP_Road_Generator4`, has a single point and no meshes; the script skips
it and reports it. Initial authoring inspection found 14 unmatched road ends.
The final Native graph has 632 road lanes, two connected groups and 16 unmatched
shape mouths (including two unused crossroads exits). All 56 sampled roads in
the main connected group can reach the central test point and return. Three
sampled roads form a separate island, including `BP_Road_Generator8`; they cannot
reach the main group. The importer does not create roads across missing geometry.
Use the coordinates and source names in the inspection report to connect that
island using the road generator before assigning cross-city missions there.

The map also contains one nonspatial Narrative MassVehicleSpawner configured for
48 ambient vehicles using Native DA_Vehicle. Reapply its configuration with
Scripts/Territory/configure_almalik_traffic.py while AlMalik is loaded. Narrative
owns Mass simulation, actor LOD and intersection annotations. The project scans
Native's mass-driver NPC definition directory so their asynchronous load bundles
are available. The rendered city smoke test observed moving traffic; the complete
audit document records remaining unrelated map errors and network validation limits.
The final map inventory contains no Territory volumes. Author the intended City,
District and Place boundaries and attack approaches before using these roads for
AlMalik story counterattacks; combat verification currently uses HopDistrictTest.

## Physical surfaces without generator curves

`UTerritoryRoadNetworkEditorLibrary::BakeRoadSurfaces` accepts
`FTerritoryRoadSurfaceBakeSettings`: a stable editor-authored `BakeID`, bounds,
road physical material, Native lane profile, spacing, maximum slope and a sample
budget. This is callable from an Editor Utility Blueprint or Unreal Python.

For a mesh, assign PM_Road to its collision physical material or material physical
material. For painted terrain, assign PM_Road to the Landscape Layer Info's Phys
Material. Load the whole intended bake region in World Partition, then call
Bake Road Surfaces from an Editor Utility Blueprint with that material, CityRoad4L,
a bounded height band and a BakeID stored in the utility. Generate that GUID once
for the region and reuse it on every rebuild. Check the returned errors, warnings
and lane counts before saving. No hand-drawn spline is required.

The baker traces collision with physical-material results, reads the selected
surface on landscape layers and mesh materials/collision bodies, extracts
connected centre lines and junctions, checks carriageway width, then creates
Native ZoneShapes. A physical surface identifies drivable area; the selected
Native lane profile supplies lane count and traffic direction. Junction turns use
Native Bezier routing and Native Intersection tags. The importer uses Native's
default connection policy: imposing OneLanePerDestination on generated city
junctions removed valid exits during Native overlap pruning. Two-mouth road
connectors do not create traffic lights.

Use one fully loaded collision height band per bake. The tool rejects an area
with intersecting unloaded World Partition actor descriptors. It samples the top
visible collision surface in that band; an overpass and the street underneath
must be baked in separate bounded bands. Collision must block Visibility.
Buildings, missing collision, narrow paint and excessive slopes must not become
vehicle routes. It is an editor bake, not an expensive city scan every runtime tick.

Reusing the same BakeID updates that bake's shapes and removes only obsolete
shapes belonging to it. A failed extraction preserves the previous Native lanes.
Generated actors have normal editor GUIDs and are nonspatial. Native ZoneGraphData
persists the derived network; generated authoring shapes are editor-only and are
excluded from cooked gameplay. Physical road meshes and landscape still stream
normally. A baked lane is not proof that collision or walking navigation is
currently loaded at a destination.

Inspect the finished roads with `InspectRoadNetwork`. `PreviewVehicleRoute`
calls the actual counterattack route builder without spawning or reserving force.
Use it for cross-city journeys and bridges before enabling missions. Automatic
geometry cannot infer one-way signs, forbidden turns or priority from an asphalt
surface alone; those remain Native lane/junction authoring choices.

## Shared arrivals and traffic

`UTerritoryRoadTrafficSubsystem` maintains server-only transient arrival claims.
The driver searches backward along its existing route, checking other claims,
physical Narrative vehicles and a complete Native walking path. It trims its
route to a free position instead of sharing a permanently occupied final point.
`ArrivalSearchDistance` defaults to 2500 cm and `ArrivalSpacing` to 1000 cm.
These settings are on `FTerritoryVehicleAwarenessSettings` in each approach.

If the previous car remains on the entrance pad, a later squad can stage 9–20 meters
forward on the same existing route, with at least 15 meters left to drive. It checks
physical vehicle occupancy and still passes Native spawn collision. The driving route
starts at that selected point; it does not send the new car back to the old entrance.
Short or fully occupied routes retain a bounded failure rather than overlapping cars.

Claims are keyed by the live vehicle and durable AssaultID. They are not campaign
state and are not replicated to widgets. They are released on driver completion,
retirement and streaming/destruction, or disappear with the world. Physical parked
cars still block a space after its claim is released. Reload recreates claims for
reconstructed vehicles; it does not grant additional soldiers or car budgets.

A squad blocked for `AbandonAfterBlockedSeconds` (default 12) can use Native Mount
to leave its stopped vehicle when a complete walking route reaches the fight.
Story escape targets retain their explicit abandonment policy. Finite force,
wave timing, capture admission and casualty accounting remain with their existing
authorities. This does not resolve the separately recorded deployed-survivor car
budget defect in assault reload.

Possessed mission cars read Native closed-lane traffic-light annotations. Waiting
at a light does not accumulate the ordinary stuck timer. Side avoidance requires
a same-direction Native lane with sufficient clearance; it does not authorize an
oncoming-lane overtake or an off-road shortcut. If passing is not supported by the
road and clearance, the squad brakes and uses the walking fallback.

Automatic routes filter for the Native Road tag and follow outgoing lane
connections. Native same-direction adjacency supports later turn-lane changes
with a forward, gradual transition requiring 1800 cm of road. Routes reject
backward traversal and try nearby directional endpoint lanes. Pedestrian lanes
cannot satisfy vehicle routes. Obstacle checks cover both straight ahead and
the intended steering corridor before a turn or merge.

## Migration and validation

`ATerritoryRoadGuide` previously placed `Right` on negative spline-right, which is
the left side in Unreal. Right now uses positive spline-right, reversed correctly
for reverse travel. Existing missions that compensated for the old inversion
must swap their Left/Right choice or remove the compensating offset. Center is
unchanged. No enum values or campaign save layout were renamed.

Shared Native traffic leases validate world and authority, cap requested traffic,
discard dead controllers and release a streamed guide's own leases. Releasing a
lease uses the controller actually acquired; it does not load another soft target.

Native tests cover direction, bounded arrival geometry, authority/world rejection,
deterministic surface topology, narrow/malformed grids, loop preservation, actual
physical collision baking, failed-rebuild preservation, idempotent generator
imports, Native routing and shape serialization. See the complete audit record
for executed live, multiplayer, save/load, cook and World Partition gates; do not
treat these native tests alone as release approval.
