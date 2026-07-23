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

### ATerritoryWorldState
| Property | Replicated? |
|---|---|
| ReplicatedTreasuries | ✅ |
| ReplicatedTransactions | ✅ |
| ReplicatedTreaties | ✅ |
| ReplicatedReputation | ✅ |
| ReplicatedCaptureSummaries | ✅ |

## Authority Enforcement

All mutation functions are marked `BlueprintAuthorityOnly`:
- Blueprint calls on clients are silently rejected
- C++ callers should use `GetAuthGameMode()` check as runtime backstop
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
3. NOT call subsystem mutation functions (rejected by authority)
4. Use `GetTerritoryAtLocation` for read-only spatial queries (works on clients)

## Known Multiplayer Limitations

1. **Economy subsystem state not replicated to clients** — client-side economy queries return empty/stale data. Use `ATerritoryWorldState` for client-visible economy.
2. **Capture progression not visible on clients** — the `TerritoryCaptureState` map in `UTerritoryControlSubsystem` is server-only. Clients see state changes via `OwnershipData` RepNotify when capture completes.
3. **Transaction history not streamed to clients** — the full ledger lives on the server. `ATerritoryWorldState::ReplicatedTransactions` is the client-visible subset.

## Dedicated Server Setup

1. Place `ATerritoryWorldState` in the level (required for multiplayer)
2. All territory volumes auto-replicate (`bReplicates = true`)
3. All spawned guards auto-replicate (inherited from `ANarrativeNPCCharacter`)
4. Subsystem timers auto-start on server only

## Listen Server

Same as dedicated server — the host has authority, clients receive replicated state.
