# Territory Road Guides, Reinforcement Cars, and Chase Missions

This system joins Territory Framework road missions to Narrative Pro. It does not replace
Narrative vehicles, ZoneGraph, Mass traffic, intersections, vehicle abilities, or impact damage.

## The easy idea

Use one road for both directions:

```text
Road Guide start ================================= Road Guide end
Counterattack spawns here                           Place / chase exit
                                                     |
                                                     +-- Final-fight point
```

- A normal counterattack starts at the Road Guide start, drives to the end, parks, and the
  Narrative NPC dismounts to attack the Place.
- **Enemy Chases Player** uses the same forward direction.
- **Player Chases Enemy** starts at the end and follows the spline in reverse.
- `Left` and `Right` are relative to the direction of travel. Reverse travel automatically
  mirrors the offset, so both cars use the correct side of a two-way road.

One Road Guide is one mission route, not the whole city network. A city can have many connected
ZoneGraph roads and many Road Guides. Give each Territory approach a stable Road Guide ID. The
counterattack chooses the appropriate guide for that Place; Narrative traffic continues using the
larger city ZoneGraph network.

## Recognizable faction cars and difficulty

The car is selected in this order:

1. the attacking faction's **Signature Vehicle** in the Counterattack Profile;
2. the vehicle Blueprint stored on the selected Territory Approach.

This means the same road can carry different factions without duplicating the road. Bandits may
arrive in pickups while Regime troops arrive in black sedans. Narrative gameplay difficulty sets
the complete finite-assault car budget: Easy 1, Medium 1, Hard 2, Insane 3 unless the faction has
an explicit override. Road `Maximum Vehicle Deployments` and finite force size remain hard limits.

The chosen difficulty and used car counts live in the durable assault record. Saving and loading
cannot reset the budget and create extra cars.

## Required Unreal and Narrative systems

The project and Territory plugin explicitly enable:

- Chaos Vehicles
- ZoneGraph and ZoneGraph Annotations
- Mass Gameplay and Mass AI
- Narrative Pro

Narrative's default `Road` and `BigRoad` lane profiles already contain forward and backward
lanes. Build ZoneGraph after editing a road. A Road Guide should normally sit over that road.
Keep **Require Narrative ZoneGraph Coverage** enabled so validation catches a mission spline
that has moved away from the traffic road.

Narrative remains responsible for:

- vehicle mount, seat, animation, possession, and dismount;
- Mass vehicle spawning, lane occupancy, traffic lights, and traffic obstacle avoidance;
- the vehicle impact mesh, vehicle damage, character impact, and destructible prop response.

Territory adds only the mission decision: which road, which direction, which target, when to
stop, when the chase fails, and when temporary mission actors retire.

For a traffic-enabled world, create a Blueprint child of `TerritoryRoadTrafficControls` and a
Blueprint child of Narrative's `MassVehicleSpawner`, then assign them in Territory Framework
project settings. The controls Blueprint owns the visible route bounds; the editor setup helper
configures the spawner with Narrative's `DA_Vehicle` Mass entity by default. This keeps the level
Blueprint-first while reusing Narrative's traffic runtime.

## Make a road in a level

1. Add a Blueprint child of `TerritoryRoadGuide`. Use the Blueprint child in production levels.
2. Assign it in **Project Settings > Territory Framework > Road Guide Blueprint Class**. The
   straight-road helper deliberately refuses to place a native C++ Road Guide.
3. Draw its **Road Mission Spline** over the physical street.
4. Set **Road Guide ID**. The easiest value is the same as the Place Approach ID, for example
   `Blacksmith_WestRoad`.
5. Draw or connect the Narrative ZoneShape road below it. Use the Narrative `Road` or `BigRoad`
   lane profile.
6. Set **Final Fight Location** beside a reachable NavMesh location.
7. Build ZoneGraph and NavMesh.
8. In the Place Definition, set the approach to **Narrative Vehicle**. Leave **Road Guide ID**
   blank when it should automatically use the Approach ID, or enter a different stable ID.
9. Run Territory data validation.

For a simple street, call **Ensure Straight Vehicle Approach Road**. It creates/updates both the
two-way ZoneShape road and a matching Territory Road Guide. Curved streets should be edited by a
level designer after the helper creates the first two points.

To repair every enabled Narrative Vehicle approach in the loaded map at once, run
`Territory.Editor.EnsureVehicleRoads` in the Unreal console. It uses the configured Blueprint
class and links the map's Quest Road Controls actor only when exactly one exists.

The setup command also repairs a Narrative Mass spawner ownership edge case on the placed
Blueprint instance. This avoids a private Blueprint-CDO generator reference that can otherwise
make Unreal refuse to save the level. Narrative plugin source is not edited.

## Repeatable PIE test

In a Development Editor build, start PIE and run:

```text
Territory.Debug.StartStoryPursuit Territory.HavenReach.MarketSquare.Blacksmith Narrative.Factions.Bandits
```

This example assumes Heroes have already Claimed Blacksmith and Bandits are hostile. A faction
cannot attack its own Place; the command prints the live state, owner, profile, and force facts
when normal admission rejects the request. Replace both tags for another Place or faction. In
multiplayer PIE, the command automatically
routes to the authoritative server world. `Scheduled` means the configured story pursuit entered
the normal road/vehicle lifecycle; `Rejected` means normal validation refused it (for example an
owner conflict, missing force, invalid route, or an encounter already in progress). The command
does not exist in Shipping builds and never changes authored campaign data.

## Traffic during a mission

Assign a Narrative `QuestRoadControls` actor to **Narrative Traffic Controls** on the Road Guide.
That actor supplies its box area, Mass entity types, traffic-light overrides, and lane rules.
Narrative currently resolves one Quest Road Controls actor for Mass spawn filtering. Assign the
same world controller to every Road Guide that may run at the same time. Territory reference-counts
that shared controller across all guides, so one mission cannot switch traffic off while another
mission still uses it. A different controller is rejected while the world lease is occupied.

Easy examples:

- Set the Quest Road Controls Mass setup to ordinary sedans for normal city traffic.
- Use a slow vehicle entity type and a small count for a chase where the player must dodge traffic.
- Override an intersection side to create an authored road block.
- Use `Mission Traffic Vehicle Count Override = 0` for an intentionally cleared road.

Do not use a random invisible vehicle spawn as a chase obstacle. Traffic should come from the
same built ZoneGraph lanes as the player and target.

## Vehicle awareness and impacts

The possessed mission car uses three configurable box probes: centre, left, and right.

- A distant obstacle gradually reduces the desired speed.
- An obstacle inside **Emergency Stop Distance** applies full braking.
- When side avoidance is enabled, a blocked centre probe applies only a small correction toward
  the clearer side.
- Multi-floor roads are separated by **Probe Half Height**, so traffic above or below does not
  block this car.

These probes do not implement damage. Narrative's vehicle overlap mesh and impact curves still
handle characters, physical props, destructibles, and vehicle self-damage.

## Player chases enemy

`Start Territory Story Pursuit / Boss Chase` exposes these rules:

- **Maximum Chase Distance**: closest co-op player allowed distance from the target car.
- **Chase Distance Grace Seconds**: the target escapes only after every player remains too far
  away for the complete grace period.
- **Vehicle Abandon Health Fraction**: when Narrative vehicle health reaches this fraction, the
  target stops and dismounts.
- **Abandon Damaged Vehicle For Final Fight**: after dismount, Narrative navigation moves the
  target to the Road Guide final-fight point. Killing the finite target resolves the encounter.

A blocked target can also abandon after **Abandon After Blocked Seconds**. Set it to zero when a
traffic jam should never force the final-fight handoff.

Outcomes are deliberately different:

| Situation | Result |
|---|---|
| Target reaches the road exit | `TargetEscaped` |
| Every player loses range for the grace time | `ChaseDistanceLost` |
| Vehicle is disabled or blocked | Target abandons car; assault remains active |
| Player kills the finite target | `AllAttackersRemoved` |

The durable assault record exposes `bStoryTargetAbandonedVehicle`. Quest/UI code can listen to
`OnAssaultChanged` to present “The capo left the vehicle—finish the fight.”

## Cleanup rules

Reinforcement and pursuit cars are transient campaign actors. After the assault resolves:

1. Territory stops vehicle input and ends Road Guide mission traffic.
2. The car stays for **Earliest Retirement Delay**, allowing dismount/death presentation to end.
3. It remains while a player is inside **Player Keep Alive Distance**.
4. It retires after **Hard Retirement Timeout** when empty.
5. If a player possesses or carjacks it, Territory releases cleanup ownership and never deletes
   that car under the player.

This prevents abandoned counterattack cars from building up around a Territory without making
nearby vehicles visibly disappear.

## Validation checklist

- Every enabled approach has one unique Approach ID.
- Explicit Road Guide IDs resolve to exactly one level actor.
- Road Guide spline has two or more different points.
- Every required sample is close to a built Narrative ZoneGraph lane.
- Vehicle class derives from `NarrativeVehicleBase` and has a usable mount seat.
- Spline end and final-fight point have complete NavMesh routes.
- Braking Distance is greater than Emergency Stop Distance.
- Hard Retirement Timeout is greater than Earliest Retirement Delay.
- Quest Road Controls uses a bounded count and an intentional box area.
