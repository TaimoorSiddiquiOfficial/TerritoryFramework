# Territory Framework — Release Verification 2026-09-03

This report records what was actually executed. It separates automated evidence from playtests
that still require an authored map, visible clients, or human judgement.

## Verified source and persistence

- UE 5.7 `TDAEditor Win64 Development` built successfully.
- All 197 `TerritoryFramework.*` automation tests passed.
- The focused WorldState archive test round-tripped an active finite assault, a War treaty, the
  strategic capture directory, economy state, and the saved counterattack evaluation fields.
- Runtime state nested inside `FTerritoryWorldState` now carries `SaveGame`; marking only the outer
  actor properties was not enough for a real `ArIsSaveGame` archive.

## Real save-slot restart gate

The packaged game ran `Territory.Debug.StartAssaultGate`, then
`Territory.Debug.SaveReloadActiveAssault` against `TerritoryReleaseGate_PostPersistence`.

1. The first process scheduled one live assault.
2. Saving and loading reconstructed one live assault, not zero and not two.
3. The process exited completely.
4. A second independent packaged process loaded the same slot.
5. The same gate again reported one live assault before and one after reload.

Result: **passed** for the project's actual save slot and a real process restart.

## Dedicated server with two clients

A fresh packaged Game executable launched as:

```text
/Game/HopDistrictTest?listen -server -Port=7804
```

The server scheduled an immediate assault against Blacksmith. Two independent packaged clients
connected to `127.0.0.1:7804`.

- Server listener accepted both connections.
- Both clients were welcomed into `HopDistrictTest`.
- Both joins succeeded.
- Both remote players created distinct authoritative pawns.
- No `SpawnDefaultPawnAtTransform` or origin-spawn failure occurred after adding three separated
  PlayerStarts.
- The assault force used `NPC_TerritoryBanditAssault` and received only
  `Weapon_TerritoryGuardSword`.

Result: **topology and live-assault admission passed**. A visible two-player capture/guard/XP
resolution remains a manual presentation and exact-once gameplay playtest.

## Cook and package

A direct cook used `-NoAssetRegistryCache` so an old generated registry could not hide missing
dependencies. It completed with **0 errors**. A fresh Windows stage, package, and archive then
completed successfully.

Project-owned cleanup included:

- removed two missing Narrative demo weapons from the player loadout;
- removed missing CitySample material overrides from the player appearance;
- replaced the missing mobile touch interface with `None`;
- registered `GameplayCue.TakeDamage.Fire`;
- limited Narrative player/NPC definition scans to project content;
- changed ordinary Bandit, assault Bandit, and Hero guard loadouts to one Territory weapon; and
- made `HopDistrictTest` the valid game-entry map.

The clean cook's remaining warnings are Narrative Pro Character Creator tags plus project config
priority/deprecation warnings. They are not Territory runtime errors and should be repaired by the
owning plugin/project, not copied into Territory Framework.

## Road and reinforcement evidence

The current authored runtime proved the forward reinforcement path: a Narrative driver claimed the
faction sedan, travelled ten ZoneGraph route points, stopped, dismounted, and allowed the takeover
flow to finish. The following still need a hands-on authored mission pass:

- reverse boss pursuit;
- slow or blocked traffic;
- damage-triggered abandonment;
- unused-vehicle cleanup timing;
- player carjacking and in-car weapon presentation; and
- final on-foot fight navigation and camera staging.

## World Partition boundary

Automated tests cover stable directory identity, external actor removal, guard-post release, and
active-count cleanup. They cannot physically stream `HopDistrictTest`, because that map is not a
World Partition map. Final proof requires a World Partition fixture containing a Place, guard
posts, and a live finite assault.

## Headless-only Narrative UI warnings

Both `-nullrhi` clients joined correctly but Narrative's player controller attempted to read
`GameplayHUD` and `LoadingMenu` without a rendered viewport. These are Narrative headless-client
assumptions, not Territory ownership, replication, or assault failures. The manual visible-client
gate should confirm that normal UI creation eliminates them before deciding whether the project
needs a controller-side headless guard.
