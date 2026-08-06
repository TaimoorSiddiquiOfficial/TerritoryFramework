# Multiplayer Guide — Authority, Replication, Client Behavior

## Authority Model

| System | Server Authority | Client Behavior |
|---|---|---|
| Territory ownership | ✅ Server | Receives via RepNotify |
| Capture progression | ✅ Server (timer) | Read-only via replicated OwnershipData |
| Economy treasury | ✅ Server (timer) | Read-only (no client timer) |
| Diplomacy | ✅ Server | Read-only via Narrative GameState |
| Guard spawning | ✅ Server | Guards replicate normally |
| Combat permissions | ✅ Server | BT tasks run server-side for AI |
| Counterattack schedule/spawn/casualties | ✅ Server | Replicated record + physical NPC replication |

## Replicated Properties

### ATerritoryVolume
| Property | Replicated? | RepNotify? |
|---|---|---|
| OwnershipData | ✅ | ✅ OnRep_OwnershipData |
| TerritoryGUID | SaveGame only | — |

### ATerritoryProperty
| Property | Replicated? | RepNotify? |
|---|---|---|
| UpgradeLevel | ✅ | ✅ OnRep_UpgradeLevel |

### ATerritoryGuardCharacter
| Property | Replicated? | Purpose |
|---|---|---|
| TerritoryHomeTransform | ✅ | Resolved home transform used by return activities |
| OwningTerritory | ✅ | Territory back-reference |
| OwningTerritorySpawnPoint | ✅ | Authoritative one-slot spawn-point back-reference |

### ATerritoryWorldState
| Property | Replicated? |
|---|---|
| ReplicatedTreasuries | ✅ |
| ReplicatedTransactions | ✅ |
| ReplicatedTreaties | ✅ |
| ReplicatedReputation | ✅ |
| ReplicatedDiplomacyHistory | ✅ |
| ReplicatedCaptureSummaries | ✅ |
| ReplicatedAssaults | ✅ RepNotify |

RepNotify handlers hydrate client-side economy, diplomacy, and counterattack query subsystems. Clients do not mutate those maps.

## Authority Enforcement

Gameplay-facing capture, ownership, lock, economy, diplomacy, WorldState, and combat mutations are generally marked `BlueprintAuthorityOnly`:
- Blueprint calls on clients are rejected by the generated authority gate
- Actor C++ mutations should use `HasAuthority()`; world-subsystem mutations should reject `NM_Client` or require `GetAuthGameMode()`
- `BlueprintAuthorityOnly` does not protect direct native C++ calls
- Registry registration, guard spawn-point bookkeeping, and diplomacy `LoadFromGameState` are intentionally not authority-marked APIs
- `LoadFromGameState` is authority-only; client diplomacy comes from WorldState RepNotify
- **Tales events** (`TerritoryCaptureEvent`, `TerritoryLockEvent`, `TerritoryUnlockEvent`) explicitly skip on `NM_Client` — no client-side capture mutations

## Timer Scheduling

| Timer | Interval | Where |
|---|---|---|
| Capture tick | 0.1s (configurable) | Server only (NM_Client check) |
| Economy tick | 300s (configurable) | Server only (NM_Client check) |
| Treaty expiration | 10s (configurable) | Server only (NM_Client check) |
| Registry bounds poll | 2s | Server only (NM_Client check) |
| Counterattack lifecycle | 2s (configurable) | Server only |

## Client UI Behavior

Clients should:
1. Read territory state from replicated `OwnershipData` (auto-updated via RepNotify)
2. Bind to `OnTerritoryControlChanged` delegate (fires on RepNotify)
3. Do not call subsystem mutation functions; Blueprint authority metadata and native runtime guards reject supported client mutation paths
4. Use `GetTerritoryAtLocation` for read-only spatial queries (works on clients)

## Known Multiplayer Limitations

1. **Capture actor identities are server-only** — clients receive leading `ControlProgress`, state, owner, and contesting faction, not the ControlSubsystem's per-faction participant maps.
2. **Capture participants are transient** — a saved contest restores its leading faction/progress for decay, but actor identities and non-leading progress are not saved.
3. **Assault records are replicated; live pointers are not** — physical attacker actors replicate normally, while campaign save/load reconstructs survivors from finite counts.
4. **Offscreen assault simulation is disabled** — warning/waiting assaults produce no pawns and no capture pressure until a relevant player enters the activation radius.
5. **Runtime verification remains required** — dedicated-server/two-client PIE must prove one-time proximity activation, immediate death removal, and late-join state for each project NPC configuration.

## Dedicated Server Setup

1. Place exactly one `ATerritoryWorldState`; it is always relevant and provides live late-join projections plus save snapshots
2. All territory volumes auto-replicate (`bReplicates = true`)
3. All spawned guards auto-replicate (inherited from `ANarrativeNPCCharacter`)
4. Subsystem timers auto-start on server only

## Listen Server

Same as dedicated server — the host has authority, clients receive replicated state.
