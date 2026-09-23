# Directional diplomacy repair — 2026-09-23

Status: full UHT/Runtime/Editor build succeeded. All 11 diplomacy tests passed, including the new directional regression. Integrated release acceptance remains pending.

## Problem and boundary

Narrative Pro 2.4.2 permits different attitudes in each direction. Its `ANarrativeGameState::GetFactionAttitudeTowardsFaction` prefers the source direction unless Neutral, and `SetFactionAttitude` writes and broadcasts one direction. Territory previously merged these values with hostility precedence, treated the merged value as an authored bilateral treaty and wrote it back. Merely importing faction data could therefore change Native combat behavior.

The compatibility seam still uses Native's public `FactionAllianceMap` for enumeration and `SetFactionAttitude` for explicit commands. No vendor source changes.

## Implementation

- `FTreatyRecord::bNarrativeObserved` and `FReplicatedTreaty::bNarrativeObserved` identify strategic observations. Observations never write back on synchronization, removal or restoration.
- Explicit Territory diplomacy actions clear observation provenance, including same-state commands. Those actions deliberately establish bilateral policy.
- The adapter records whether raw directions disagree. Compatible symmetric rich treaties retain their state, expiry and ownership; this includes Neutral ceasefires. An incompatible external Native change becomes an observation and leaves the opposite direction intact.
- Native callbacks and load completion refresh and publish the strategic projection. The projection still prioritizes hostility for strategic planning; it is not a claim that both Native combat directions are hostile.
- Save and replica hydration carry provenance. Existing saves lack this field and retain the authored interpretation. Previously overwritten directional data cannot be reconstructed automatically; it must come from original faction authoring or a known-good Native save.

## Executed regression coverage

`TerritoryFramework.Diplomacy.Regression.DirectionalObservationAndExplicitCommands` exercises actual Native setters, world-start synchronization, the Narrative actor save/load path, stale observed saves, explicit same-state commands, timed trade and ceasefire metadata, reversed actor load order, and one-way data without inventing a reverse entry. It also exercises OnRep hydration in two separate client-mode worlds and rejection of client mutations. This does not substitute for socket-based multiplayer validation.

## Required gates

Full UHT/Runtime/Editor build and focused diplomacy automation passed. The complete suite after the transition repair and approved Blacksmith gate restoration passed all 364 tests (326 clean, 38 with warnings). Dedicated-server and two-client integration and package/save smoke remain release gates. Existing unrelated work was preserved. Local receipts: projectless audit workspace work/batch1-build.log, work/batch1-diplomacy-tests/index.json and work/batch2-restored-tests/index.json.
