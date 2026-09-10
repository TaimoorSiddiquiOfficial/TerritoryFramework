# AlMalik navigation and independent guard posts

## Confirmed defects

The actual AlMalik multiplayer fixture exposed four separate problems:

- A Place that began play before its post cells permanently seeded zero desired
  guards, despite two guards in its Definition. Capacity counted only loaded actors.
- A guard's Native death removed its Territory participation but did not free a
  tag-bound post or queue that post's reserve. The death handler searched only the
  legacy explicit actor array, excluding resolved posts.
- After corpse cleanup, unloading and reloading that post spawned a free guard.
  Post registration filled the whole staffing deficit without reading the saved
  casualty count. The live count changed from one to two while reserves stayed six.
- If only the Place unloaded, a later save overwrote the still-loaded posts'
  living slots after physical guard cleanup. Resolved tagged posts also stopped
  listening for a returning owner, leaving the new Place without its garrison.

## Change and ownership

`ATerritoryVolume::GetMaxGuardCount` counts stable post identities in the existing
Definition, together with loaded legacy posts, without counting matching actors
twice. A missing physical post still prevents deployment at that slot. The Place
retains its staffing target while the post cell loads.

The server death handler visits resolved posts and the guard's own bound post.
Each post's existing idempotent `UnregisterGuard` owns the finite reserve request.
Client callbacks cannot mutate the garrison.

`ATerritoryGuardSpawnPoint::Load_Implementation` records that Native supplied a
saved slot. Registering a saved empty post no longer recruits a free replacement.
A saved living occupant can restore at that post; an explicit reserve deployment
still spends one reserve through the existing API.

Posts now keep their registry subscription until EndPlay. Registry absence pauses
deployment and preserves saved living slots through physical cleanup and later
Native saves. Automatic reserve timers wait without cancelling their finite
request. A newly registered Place rebinds the posts and restores their occupants.

The Definition owns authored capacity, the Place owns desired staffing and its
live garrison, the post owns its finite reserve, and Narrative owns actor records
and NPC death. No Narrative Pro source, save schema, Blueprint signature or
replicated field changes are needed. Existing saves retain their staffing targets;
an already-saved zero target is not silently replaced with an asset default.

## Verification

The new native regression is
`TerritoryFramework.Guards.Regression.IndependentPostCellsKeepStaffingAndCasualties`.
It covers Place-first startup, delayed posts, Native NPC spawning and death,
duplicate death callbacks, client rejection, capacity deduplication, Native
save/load of an empty slot, and paid finite reserve replacement. It also covers
saving posts while their owner is absent, retaining pending reserve requests,
and rebinding to a different returning Place actor without refilling reserves.

Live evidence and final build results are recorded under
`Saved/Verification/20260910_AlMalikNavigation` in the TDA host. The navigation is a
temporary bounded fixture on the real city collision, not a completed city-wide
navigation bake. Both engines pass all 306 automation tests. Editor, Development
and Shipping builds pass on UE 5.7 and 5.8. Validation checks 244 assets and
compiles 147 Blueprints with zero errors and eight existing warnings. All 741
Narrative Pro source files match the installed vendor.

The UE 5.8 cook, stage and package pass, followed by a 60-second Development
Game server-mode startup with exit zero. This does not certify a separate
TDAServer binary or the capture-enabled assault lifecycle. The existing optional
intro cutscene warning remains: the demo controller calls Native's cutscene API
without a CutscenePlayerActor.

The rebuilt AlMalik fixture passes on a server and two clients:

- Startup shows two desired/active guards and six reserves on all three worlds.
- Independent post unloading and returning retains the live bindings.
- Native damage immediately empties the correct slot and queues one replacement.
  Post reload leaves it empty. Explicit replacement spends one reserve, from three
  to two, and clears that request.
- The Place cell unloads while both post cells remain. A real Native save is made
  during that absence. Two guards and all six reserves return on all three worlds.
  The native regression separately proves rebinding to a different Place actor.

The temporary test assets and three verification save files were removed. The
original AlMalik and HopDistrictTest map hashes are unchanged.

## Open assault gate

The initial force-accounting-only Story Pursuit test preserved two living
attackers, two pending reserves, its AssaultID and decision roll. It did not wait
for Narrative goal readiness and cannot certify the physical assault lifecycle.

The capture-enabled test is **not passed**. Repeated fresh attempts fail before
unloading: the project assault NPC has a definition, controller and activity
component, but `GetCharacterVisual()` returns null. `IsNarrativeSpawnReady()` stays
false and the existing 40-attempt/0.5-second goal initialization limit withdraws
each wave. The default appearance asset exists. Do not assume this is merely slow
loading or extend the timeout without tracing the missing initialization.

Next trace the project's `BP_TerritoryAssualtGuard` definition event and Native's
`OnDefinitionSet -> SpawnedData load callback -> ChangeAppearance -> SpawnCharacterVisual`
path, including primary-asset registration. No cause beyond the missing visual
has been verified, and no Narrative Pro patch or timeout bypass was made.

A separate capture-flag assertion sampled immediately after actual cell removal;
allowing replication to settle corrected that harness timing. The initialized
force then retained two living and two pending attackers. Its save/return test
exposed the garrison defect fixed in this batch, but the complete capture-enabled
playthrough still requires a fresh successful NPC initialization.
