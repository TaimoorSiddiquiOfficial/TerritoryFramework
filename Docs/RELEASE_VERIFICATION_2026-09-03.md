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

### Rendered follow-up

A later pass ran the packaged listener with two real windowed clients. Both clients rendered the
world, Territory HUD, contested Haven Reach state, remote player, vehicle arrival, ten-point road
route, stop, and dismount. The longer session corrected an earlier assumption that all UI warnings
were headless-only. It exposed two project-owned Blueprint timing defects:

- `BP_TerritoryAssualtGuard1` called `RemoveAllGoals` again after its safe native death parent,
  through a nullable activity component; and
- the project Narrative controller entered the vendor Local Init graph before possession had
  created `GameplayHUD`.

The redundant assault cleanup node was removed while preserving weapon-drop presentation. A later
packaged two-client audit proved a fixed startup delay was not a lifecycle contract. The project
controller now calls Territory's latent `Wait For Narrative Gameplay HUD` before its parent
BeginPlay graph. Narrative's normal possession/PlayerState path creates the HUD and initializes its
ASC; Territory neither creates a duplicate HUD nor modifies vendor content. Remote server
controllers bypass the local-HUD gate immediately.

A refreshed packaged listener admitted two clients with zero `GameplayHUD`, `LoadingMenu`,
`Accessed None`, Blueprint runtime, divide-by-zero, null-ASC, or Territory readiness warnings in
the server and client logs. The headless clients still report an engine skeletal-mesh visibility
ensure under NullRHI; it is not emitted by Territory or the HUD lifecycle graph.

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

The current authored runtime proved the forward reinforcement path and complete car-wave policy.
On Narrative Hard difficulty, the first sedan carried one driver plus three passengers, travelled
ten ZoneGraph route points, stopped, and dismounted. Two members died without spawning a partial
replacement car. After all four members resolved, a second sedan arrived with another driver plus
three passengers. The following still need a hands-on authored mission pass:

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

## Narrative Pro cue warnings kept outside Territory ownership

The rendered test also proved two remaining warnings are inside Narrative Pro assets rather than
Territory ownership or assault state:

- `GC_Burst_Unarmed` calls its generic `Spawn Decal` graph on the dedicated server, where Niagara
  returns no decal component and the vendor graph dereferences `DecalPS` without a validity check;
- `GC_TakeDamage` can read `GetCharacterVisual` before a replicated character's asynchronous visual
  is available.

The project Territory sword ability did have one incorrect inherited setting: it emitted the
firearm `GameplayCue.Weapon.Fire` on every melee swing. That tag is now empty, avoiding unrelated
muzzle-fire cue RPCs and per-net-update cue-budget warnings from Territory sword attacks. The two
generic Narrative cue graphs remain vendor-owned and should be fixed by a Narrative Pro update or a
project-level vendor override, never by changing Territory's gameplay authority.

## 2026-09-04 final regression refresh

- UE 5.7 Game and Editor Development targets: **passed**.
- Full Territory automation: **208/208 passed** (202 clean, 6 intentional warning fixtures).
- Fresh full cook: **6,906 discovered packages / 6,722 runtime packages, zero cook errors**.
- Windows stage/package/archive: **passed**.
- Active-assault in-process save/reload: **passed** with `liveBefore=1`, `liveRestored=1`,
  `liveAfter=1`, `unexpectedLive=0`, and `duplicateLiveIDs=0`.
- Independent packaged-process restart from the same slot: **passed** with the same exact live set.
- Packaged two-client HUD lifecycle gate: **passed** with both joins and no Territory/UI runtime
  errors.
- Hard vehicle-wave playtest: **passed** for two full four-seat cars and no partial-car top-up.
