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
| OwningTerritorySpawnPoint | ✅ | Spawn point back-reference; null for random fallback guards |

### ATerritoryWorldState
| Property | Replicated? |
|---|---|
| ReplicatedTreasuries | ✅ |
| ReplicatedTransactions | ✅ |
| ReplicatedTreaties | ✅ |
| ReplicatedReputation | ✅ |
| ReplicatedDiplomacyHistory | ✅ |
| ReplicatedCaptureSummaries | ✅ |

## Authority Enforcement

Gameplay-facing capture, ownership, lock, economy, diplomacy, WorldState, and combat mutations are generally marked `BlueprintAuthorityOnly`:
- Blueprint calls on clients are rejected by the generated authority gate
- Actor C++ mutations should use `HasAuthority()`; world-subsystem mutations should reject `NM_Client` or require `GetAuthGameMode()`
- `BlueprintAuthorityOnly` does not protect direct native C++ calls
- Registry registration, guard spawn-point bookkeeping, and diplomacy `LoadFromGameState` are intentionally not authority-marked APIs
- **Tales events** (`TerritoryCaptureEvent`, `TerritoryLockEvent`, `TerritoryUnlockEvent`) explicitly skip on `NM_Client` — no client-side capture mutations

## Timer Scheduling

| Timer | Interval | Where |
|---|---|---|
| Capture tick | 0.1s (configurable) | Server only (NM_Client check) |
| Economy tick | 300s (configurable) | Server only (NM_Client check) |
| Treaty expiration | 10s (configurable) | Server only (NM_Client check) |
| Registry bounds poll | 2s | Server only (NM_Client check) |

## Client UI Behavior

Clients should:
1. Read territory state from replicated `OwnershipData` (auto-updated via RepNotify)
2. Bind to `OnTerritoryControlChanged` delegate (fires on RepNotify)
3. Do not call subsystem mutation functions; Blueprint authority metadata and native runtime guards reject supported client mutation paths
4. Use `GetTerritoryAtLocation` for read-only spatial queries (works on clients)

## Known Multiplayer Limitations

1. **Economy subsystem state is not replicated directly** — client-side economy queries return empty/stale data. `ATerritoryWorldState` provides a client-visible snapshot only after `ExportPersistentState()` or explicit setters update it.
2. **Capture actor identities are server-only** — clients receive the leading `ControlProgress`, state, owner, and contesting faction through replicated `OwnershipData`, but not the ControlSubsystem's per-faction attacker/progress maps.
3. **WorldState is snapshot-based** — economy, diplomacy, reputation, transaction, and capture arrays update on export or explicit setter calls rather than streaming continuously.
4. **Capture participants are not persisted** — a saved contest restores its leading faction/progress via `RestoreCaptureState` and the capture summary is synced back from WorldState, but attacker actor identities and non-leading faction progress are lost.

## Dedicated Server Setup

1. Place `ATerritoryWorldState` if clients need a replicated persistence snapshot, and export it at the appropriate save/sync points; the actor is always network relevant
2. All territory volumes auto-replicate (`bReplicates = true`)
3. All spawned guards auto-replicate (inherited from `ANarrativeNPCCharacter`)
4. Subsystem timers auto-start on server only

## Listen Server

Same as dedicated server — the host has authority, clients receive replicated state.
