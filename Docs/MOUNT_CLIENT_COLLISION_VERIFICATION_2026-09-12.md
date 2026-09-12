# Remote mount collision verification — 2026-09-12

## Problem and root cause

Hashir's remote passenger could start Native's mount ability but fail before
attachment. While the server car waited, client car positions drifted by over
two metres. The server disabled Hashir's seated capsule; simulated clients kept
`QueryAndPhysics`, with capsule Vehicle and car Pawn responses both blocking.
The attached capsule therefore pushed its own car on those clients. Native's
entry move used that displaced local car position.

A controlled PIE diagnostic changed only the seated NPC's client capsule to
`NoCollision`. The stationary cars converged to within a centimetre, and Native
passenger boarding and driving succeeded. An observer also needed the mounted
player's simulated capsule disabled. Client navigation is enabled in this project;
no change to Native's move task or client navigation was needed.

## Change, authority and migration

`UTerritoryMountPresentationComponent` follows attachment to Native's
`UMountComponent`. It temporarily disables the capsule on simulated clients,
restores its previous mode on exit, and handles late attachment/component arrival.
It has no server polling, world scan, RPC, seat claim or campaign save field.
Native keeps attachment, occupancy, animation, owning-client mount abilities,
physics and save authority. Dead/ragdolled characters and ownership handoff do
not have collision restored over Native's handling.

Territory defender and assault constructors include this component. TDA's
`/Game/HOPTRENDY/Character/Hashir/BP_Hashir` and
`/Game/TerritoryFramework/Framework/BP_TerritoryPlayerCharacter` add it in their
component trees. Other player/story NPC Blueprints opt in as described in
[the setup guide](MOUNT_CLIENT_COLLISION.md). No asset path, tag, GUID, enum,
existing Blueprint signature or save-record migration is introduced.

The pre-existing player Blueprint content was retained. Exact pre-change copies
of both edited assets are in the evidence directory. The already-unsaved Hashir
dialogue was preserved and saved before the full build; it is separate from this
component change and is not included in its scoped commit.

## Verification

Evidence directory in TDA: `Saved/Verification/20260912_HashirBoarding`.

| Check | Result |
|---|---|
| UE 5.8 TDA Editor, Development and Shipping | All passed |
| UE 5.7 compatibility host Editor, Development and Shipping | All passed |
| UHT | Passed on both engines |
| UE 5.8 Territory automation | 315 passed; 0 failed; 0 not run; 22 passed with existing warnings |
| UE 5.7 Territory automation | 315 passed; 0 failed; 0 not run; 24 passed with existing warnings |
| New Native mount regression | Passed on both engines; 0 errors and 0 warnings |
| Two edited project Blueprints | Both compile and validate; 0 errors and 0 warnings |
| Live listen host, two clients and a late third client | All 16 recorded checks passed |
| Narrative Pro source comparison | 741 files; 0 differences from installed UE 5.8 Marketplace package |
| UE 5.8 incremental cook/package and 60-second startup | Both exit 0; current Development executable and edited project assets |

The native regression
`TerritoryFramework.AI.Regression.NativeMountClientCollisionAndRestore` uses
real Narrative characters and a real mount component. It exercises authority,
repeat updates, exact restoration, owner-client exclusion, deactivation, death,
external collision changes, removed/late mount data, ownership handoff and an
actual component SaveGame archive round trip into a new character. The restored
character derives its own mode; no old body's cache is serialized.

`Scripts/Territory/verify_hashir_farm_network_pie.py` records the real Native drive
activity. Hashir waits for the passenger, a third client joins while he is seated,
and the remote player boards through Native's own controller interaction events.
The car physically departs and Native reports arrival near `(3237,920,0)`. After
stopping, the client requests exit through the same Native event path. All four
worlds end with detached characters and their original capsule collision.

The recorded run reports arrival at about 43 seconds and completes exit checks
at about 58 seconds. Its checks also require stationary/arrived client cars to
be within 5 cm of the server. `NetworkRegression.json` contains every sample,
the chosen player ID and all 16 results. Separate diagnostic snapshots cover
late joining after the passenger had already boarded.

Native controller events are queued through `SetTimerForNextTick`, outside
editor Python's actor-script guard. The fixture stages only the authoritative
passenger and aims the client camera until Native sees the car. It does not
claim seats directly, teleport the car, override collision or fabricate arrival.
Enhanced Input injection did not trigger the event in the earlier diagnostic;
this report does not claim a hardware keyboard/focus test.

An initial unit fixture tried to spawn an abstract Native vehicle base. That
test setup was corrected to a concrete pawn carrying the real mount component;
the subsequent complete reports above supersede the archived fixture failures.
An asset-validation attempt during PIE could not resolve the editor assets;
the recorded validation result was rerun successfully after stopping PIE.

## Remaining limits

The network run uses a listen server. A compiled dedicated-server gameplay run,
returning-client story state and AlMalik World Partition streaming are still
release gates. Component load-order tests and live late joining are not a full
city streaming certification. UE 5.7 verifies code in the compatibility host;
the UE 5.8 TDA project assets and multiplayer trip were not migrated down to 5.7.

Package startup uses the Development Game executable in server mode, not a
compiled `TDAServer` target. The existing optional `CutscenePlayerActor` warning
and config-staging allow-list warnings remain. No route abort or mount-component
failure occurred in this startup smoke; it does not exercise a full passenger trip.

The fixture deliberately starts at the real drive dialogue node. Full Blacksmith
capture/handover, quest result, durable arrival dialogue, once-only continuation,
mid-trip save/recovery and invalid-route cleanup remain in
[Hashir's checklist](HASHIR_CASTLE_FARM_DRIVE_TODO.md). A successful drive is not
quest completion. Broader finite reserve, coordinated guard conversation,
AlMalik and release artifact work remains in [the roadmap](ROADMAP_AND_REMAINING.md).
