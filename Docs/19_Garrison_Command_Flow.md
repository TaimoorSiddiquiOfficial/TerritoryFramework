# Garrison Command Flow

This is the implemented end-to-end flow for player-managed Place guards presented through a
District command surface. District and City own strategic rights/read models, never physical
guards. The flow replaces the former implicit “capture, spawn three, charge 150 upkeep” behavior
while preserving authored Place staffing for AI/script ownership and old saves.

## Outcome

- A physical player capture defaults to zero assigned guards under `PlayerChooses`.
- If a hierarchy/legacy callback loses the explicit controller, the policy scans every
  live player controller, pawn, and player state for exact Narrative membership in the
  committed owner faction. It never chooses a first controller and never uses a parent
  faction-tag match.
- The District Command Center selects loaded registered child Places; its District row is an aggregate summary, not a garrison target.
- The player sets one absolute target with Apply, 0, Max, or the compatibility `+1/-1/+5/-5` commands.
- Recruitment is a one-time Narrative inventory debit; `GuardCost` remains recurring upkeep.
- District finance is the sum of owned Property income minus all selected-target upkeep.
- Server, save, late-join, and UI projections share one authority for each state.

## Complete lifecycle

```text
Physical/Tales capture request
  -> UTerritoryControlSubsystem validates participants, diplomacy, defenders, and context
  -> ATerritoryVolume commits owner/state/progress atomically
  -> explicit FTerritoryTransitionContext remains on the Place transition while parent read models reduce bottom-up
  -> PostCaptureGarrisonPolicy resolves desired target
       PlayerChooses + live Narrative player faction: 0
       AI/script: GuardSpawnCount
       ConfiguredForEveryOwner: GuardSpawnCount
       AlwaysUnstaffed: 0
  -> guard reconciliation applies Narrative NPC definition/activity/TriggerSets
  -> ownership + exact garrison snapshots replicate
  -> WorldState exposes client economy snapshots
  -> journal/in-world management builds one District summary plus Place target views
  -> player previews target, recruitment price, upkeep, and net result
  -> server validates and commits the absolute target once
  -> Narrative inventory debit + deployment/withdrawal + desired target commit
       complete deployment: commit and broadcast one verified result
       partial/failed deployment: remove request-created guards, refund, no target change
  -> economy recalculates and immediately republishes P&L
  -> save/load persists stable IDs, desired target, rates, active/reserve/pending counts
```

## Authority map

| Data or behavior | Authority | UI/client projection |
|---|---|---|
| Place owner/state, desired guard count, guard rates | `ATerritoryProperty` / `ATerritoryVolume::OwnershipData` | Replicated ownership data plus `FTerritoryGarrisonSnapshot` |
| Capture validation/progress | `UTerritoryControlSubsystem` | Existing capture delegates/read models |
| Physical active guards | `ATerritoryVolume` + registered `ATerritoryGuardSpawnPoint` actors | Exact replicated active/reserve/pending counts; no live NPC pointers saved or replicated as the read model |
| Recruitment/upkeep/income rates | `UTerritoryEconomySubsystem` | `ATerritoryWorldState` replicated economy snapshots for clients/late join |
| Real player funds | Narrative inventory/account | Affordability projection only; mutation occurs on server |
| City/District/Place relationship | Definition hierarchy plus territory registry | District summary plus Place garrison views |
| Widget styling/layout | Project Blueprint widgets | C++ parent injects the complete command controls into an existing vertical command stack |

## Capture and hierarchy rules

`FTerritoryTransitionContext` contains the instigator, pawn, controller, Tales component, and
requesting faction. The active context remains available during the synchronous Place ownership
event bundle. District and City then reduce their control from the complete authored child set;
they never rewrite a child owner or make a staffing decision.

Normal and forced `UTerritoryCaptureEvent` execution use that same request and context. Force mode sets explicit condition, diplomacy, and lock bypass flags on the atomic request; it no longer switches to a context-free helper that could restore the authored guard target.

The backward-compatible `UTerritoryControlSubsystem::ForceCapture` node also resolves a
deterministically selected live controller belonging to the requested Narrative faction.
`ForceCaptureWithContext` is the preferred exact-instigator API. A truly global/AI
transition remains context-free and therefore uses the authored target under
`PlayerChooses`.

No ownership probability or guard-death callback directly changes ownership. Guards being eliminated leaves the incumbent physically undefended; capture still completes through `UTerritoryControlSubsystem`.

## Absolute staffing transaction

All mutation routes end at `ATerritoryVolume::TrySetDesiredGuardCount(Requester, NewTarget)`:

1. Verify server authority, claimed state, exact requester faction, range, capacity, and definition for an increase.
2. Compute the complete deployment/withdrawal plan and checked recruitment multiplication.
3. Debit the requester's Narrative account only for the increase.
4. Deploy all required guards or roll the request back completely and refund.
5. Cancel pending reserves when lowering; withdraw only active guards above the new target.
6. Commit `DesiredGuardCount`, refresh economy/replication/save projections, and broadcast one structured result.

Lowering remains legal when assigned guards are already dead or a migrated faction currently has no guard definition. This prevents the player from being trapped in unwanted upkeep.

## Profit and loss

```text
District income = sum(GetEffectiveIncome()) for owned child Properties
Recurring upkeep = sum(DesiredGuardCount * GuardCost) for District + Properties
District net = District income - recurring upkeep
Target increase price = increase * GuardRecruitmentCost
```

The target preview shows local active/desired/maximum/reserve/pending counts, one-time recruitment, recurring upkeep, income, and projected net. District summary cards show the aggregate. Recruitment and upkeep can share the same authored numeric default but are separate persisted concepts.

## UI entry points

| Surface | Behavior |
|---|---|
| `WBP_HopTerritoryJournalWidget` / `UTerritoryJournalWidget` | Remote command center for an owned District; selects District/Property and submits an absolute target through the PlayerController component |
| `UTerritoryDistrictManagementWidget` | In-world range-gated management point using the same selected-target view and mutation |
| `WBP_TerritoryCommandRow` | Selects the District and forwards compatibility delta commands to the journal's selected target |
| Economy/operations widgets | Read the shared operations/economy projections and transaction reasons |

Dynamic option lists are rebuilt only when the District/Property set changes, so periodic live refresh does not close or reset a player's open selector.

## Replication, save, streaming, and migration

- `FTerritoryGarrisonSnapshot` replicates active, desired, maximum, reserve, and pending counts with one RepNotify delegate.
- `OwnershipData` persists desired count, recurring guard cost, and recruitment price through the Narrative save interface.
- Spawn points persist stable editor GUID, reserve, pending, and active restoration counts; invalid GUIDs are never replaced at runtime, and live actor pointers are never campaign state. Legacy per-post active/pending counts above one are bounded to the new one-slot contract on load.
- Explicit-array and `OwnerTerritoryTag`/proximity posts form one unique runtime union. Each loaded point contributes exactly one active slot, so World Partition registration order cannot duplicate capacity or defence.
- `ATerritoryWorldState` is the global replicated economy/transaction snapshot and calls `ForceNetUpdate` after rate/ledger changes.
- Registration and hierarchy resolution tolerate World Partition load order; management accepts only the selected District or a currently loaded registered child Property.
- Old saves retain their saved desired value, including legacy 3. Missing recruitment price migrates from `InitialGuardRecruitmentCost`. The player may immediately set an owned target to zero.
- Blueprint callers can migrate from `TryPurchaseGuards`/`TryRemoveGuards` to the absolute target API without changing the underlying authority; the delta APIs remain compatibility wrappers.

## Implemented validation

- Native contract tests cover reflected policy, recruitment, snapshot, RPC, and Blueprint view fields.
- Behavioral regression covers player-faction target 0, unrelated AI target preservation, lowering a target with zero surviving guards, one-slot tag-bound capacity, and exact authored X/Y deployment.
- UI revision regression covers pending reserve changes.
- Existing counterattack determinism/monotonicity tests remain in the TerritoryFramework suite.
- Runtime and Editor modules, UHT, and the full `TerritoryFramework` automation prefix are release gates.
- MCP Blueprint validation compiles and structurally audits the supplied widget assets against the rebuilt C++ parents.

## Deliberate limits

- The journal may manage an owned target remotely; the in-world management screen still requires the player's pawn inside the management point range.
- A target increase fails atomically if every requested physical NPC cannot be placed. It does not silently buy a smaller garrison.
- Unloaded World Partition child Properties are not remote mutation targets until registered; saved/replicated durable state remains authoritative when they stream back in.
- Offscreen counterattack capture remains disabled; physical attackers and the existing capture flow remain required.
