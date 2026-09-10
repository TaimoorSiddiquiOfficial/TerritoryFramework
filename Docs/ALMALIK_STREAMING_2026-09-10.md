# AlMalik streaming audit — 2026-09-10

## Confirmed defect

A temporary Place and two guard posts were authored through the existing City
Definition synchronizer in AlMalik, then assigned to a Runtime Data Layer.
Server streaming and streaming out were enabled for PIE. A listen server and
client observed the actors in a real `/Memory/` World Partition cell.

A Territory Capture event changed the Place from Heroes to Bandits, and the
Place was locked. Both worlds observed the changes. Unloading and reactivating
the Data Layer physically removed and recreated the actors. The Place returned
as Heroes and unlocked. This also failed after initializing a unique Narrative
save and placing an always-loaded TerritoryWorldState: the old records existed,
but were not updated at stream-out.

TerritoryVolume and TerritoryGuardSpawnPoint already load Native actor records
in BeginPlay. Neither wrote their live values before streaming cleanup. Native's
pre-removal callback only removes actor GUID lookup entries; it does not create
these records. Saving guard counts from individual EndPlay functions would also
depend on whether the Place already destroyed its guards.

## Change

TerritoryWorldState subscribes to the engine's pre-level-removal delegate. On the
server it writes affected Territory actors and guard posts through
`UNarrativeSaveSubsystem::SaveSingleActor` before actor cleanup. When a departing
Place references posts in another cell, those posts are included before its
guards are retired. Unrelated worlds, clients, shutdown and Native load callbacks
cannot use this path. EndPlay detaches the delegate.

If the project reaches WorldState BeginPlay without a Native save object,
WorldState asks Native's public `InitializeSaveSystem` to apply its own URL and
new-game policy. It does not invent a save slot or write a file.

ATerritoryVolume remains the owner/state authority. Narrative's record map is the
only actor persistence store. WorldState remains the global persistence adapter
and replicated read model. There are no added SaveGame properties, replicated
fields, Blueprint mutations, vendor changes or save-version migrations.

## Map setup and verification scope

The original AlMalik inventory contained 19,726 actor descriptors and no Territory
actors. The temporary fixture is verification content, not the story layout.
Real story authoring must include one TerritoryWorldState with Is Spatially
Loaded disabled, outside unloadable runtime Data Layers.

The native regression passes on UE 5.7 and 5.8. It covers stale record replacement,
fresh post records, owner/lock restore, exhausted reserves, active count before
cleanup, both load orders, client/world/shutdown rejection and delegate removal.
Both full suites pass 305 tests. The new test has zero warnings. Editor,
Development and Shipping builds pass on both engines. Validation checks 244
assets and compiles 147 Blueprints with zero errors and eight existing warnings.
All 741 Narrative Pro source files match the installed vendor.

The rebuilt UE 5.8 physical test passes on a server and two clients:

- A new session obtains its Native save object without a manual Save action.
- The changed Bandits owner and story lock survive an actual cell cycle.
- Both guard post records are present after unloading.
- In a separate living-guard run, old guards disappear on all three worlds.
- Exactly two guards return. All three replicated garrison summaries show two
  active guards, six reserves and zero pending deployments.

The first guard harness incorrectly queried server-owned post reserves on the
clients. Their public UI uses the replicated garrison summary. Repeating the
cycle against that real read model passes; this was a harness correction, not a
gameplay defect. The original map files remain unchanged and the scratch assets
and editor-only actors were removed after verification.

The active finite-assault probe is **not passed**. The test area has no loaded
NavMeshBoundsVolume or RecastNavMesh. Five projections around the entry and Place
return no navigable point, and the actual route check cancels the assault. The
editor's descriptor inventory contains no navigation matches either. Navigation
must be authored and built before this part can be exercised. Guards appearing
at exact authored posts does not prove that an assault can walk to the Place.
Separate post/Place cell removal and a returning client also remain open.

AlMalik multiplayer startup separately reports `BP_PowerLineSystem` missing
electric-box references, catenary/Bulb Arrow components pending deletion, and
`WBP_CinematicOverlay` missing an object. These need their own source/Blueprint
audit; they were not caused or repaired by this persistence change.

Verification output is under `Saved/Verification/20260910_AlMalikStreaming`.
The initial package attempt failed because its local HTTP listener conflicted
with the open editor on port 8000. The closed-editor retry passed cook, stage and
package. A 60-second Development Game server-mode startup exited with code zero,
with the already tracked missing cutscene-player warning. This is not a compiled
TDAServer target or a certified AlMalik assault playthrough.
