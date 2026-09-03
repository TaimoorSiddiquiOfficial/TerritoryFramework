# Territory Framework — Narrative Damage and Dual-Target Re-audit

> **Date:** 2026-09-03  
> **Targets:** UE 5.7 `TDA` and `TDAEditor`, Win64 Development  
> **Scope:** Territory runtime C++, editor module, Narrative Pro/GAS/Tales boundaries,
> counterattack damage scaling, guard death attribution, and editor mutation safety.

## Decision

The source-level integration is coherent after this remediation. Narrative Pro remains the single
authority for abilities, attributes, accepted damage, Health, invulnerability, and death.
Territory observes Narrative results and owns only territory-specific consequences such as guard
registration, kill context, capture, stealth evidence, and story events.

No Narrative Pro source or asset was changed.

## Audited surface

- 216 runtime headers/source files and 24 editor headers/source files;
- every Territory call site mentioning Narrative, Tales, GAS, abilities, effects, damage, Health,
  guard death, counterattack scaling, or editor asset mutation;
- Gameplay Ability System resolution for NPC, pawn, controller, PlayerState, and possessed-vehicle
  representations;
- all Territory automation tests plus independent Game and Editor target builds; and
- editor road creation failure ordering and Narrative vendor-package write boundaries.

## Canonical combat flow

1. A Narrative Gameplay Ability performs the attack.
2. Its Narrative Gameplay Effect supplies `SetByCaller.Damage` to the Damage meta attribute.
3. Narrative rejects invulnerable damage, subtracts accepted damage from Health, and broadcasts
   `OnDamagedBy` and `OnDealtDamage`.
4. Narrative owns its out-of-health/death state and death Gameplay Event.
5. Territory listens to those delegates for combat-task progress, defender removal, stealth
   evidence, and exact story context. It neither applies a parallel actor-damage event nor writes
   Health.

Adaptive counterattack power is intentionally separate from hit damage. A project-owned scaling
effect modifies Narrative `AttackDamage` through `SetByCaller.AttackDamage`; the subsequent
Narrative ability still calculates and applies the real hit.

## Defects fixed

### 1. Guard kills observed the wrong damage pipeline

`ATerritoryGuardCharacter` previously overrode Unreal `TakeDamage`, while Narrative attacks apply
Gameplay Effects directly to Narrative's Damage meta attribute. Narrative kills could therefore
have no killer or retain a stale killer. The guard now binds the server-side Narrative
`OnDamagedBy` delegate and records only positive accepted damage.

### 2. Combat tasks could keep an obsolete subject

When an actor provider changed to an actor whose Narrative ASC was not ready, the task kept the
previous binding. It now unbinds immediately and binds the replacement when its ASC becomes ready.
Counterparty comparison now uses ASC identity before raw avatar/owner comparison.

### 3. Counterattack scaling could silently produce an underpowered participant

A configured scaling effect was skipped when direct ASC lookup or effect-spec creation failed.
Spawn now uses the shared Narrative adapter and fails/cleans up the participant when required
scaling cannot be applied. Set-by-caller magnitude is accepted only when finite and positive.

### 4. Invalid adaptive damage data could pass editor validation

NaN was not rejected. Scaling magnitude must now be finite and non-negative; positive values still
require a scaling effect with Narrative's canonical AttackDamage modifier.

### 5. The editor helper could mutate vendor assets

The helper promised a read-only Narrative boundary but accepted classes in `/NarrativePro`.
It now rejects the entire vendor package root and directs authors to a project-owned child/copy.

### 6. Road authoring could leave partial actors after failure

The editor road helper created or modified actors before checking the guide class, route length,
ZoneGraph subsystem, and shape component. These checks now run before mutation, and newly created
actors are removed if later creation fails.

### 7. Editor builds masked Game-target include defects

The Game target exposed an incomplete Gameplay Effect type in public assault data and an
incomplete player-controller type in Gameplay Debugger code. Explicit owning includes now make the
runtime module independent of editor/unity include order.

## Verification evidence

| Gate | Result |
|---|---|
| `git diff --check` | Passed; line-ending notices only |
| UE 5.7 UHT — Game | Passed with warnings as errors |
| UE 5.7 `TDA Win64 Development` | Passed |
| UE 5.7 UHT — Editor | Passed with warnings as errors |
| UE 5.7 `TDAEditor Win64 Development` | Passed; runtime and editor DLLs linked |
| `TerritoryFramework.*` automation | Passed 200/200: 194 clean, 6 expected warning fixtures, 0 failed, 0 skipped |
| Guard Narrative damage regression | Passed, warning-free |
| Narrative vendor-mutation regression | Passed, warning-free |
| Invalid road preflight non-mutation regression | Passed, warning-free |
| Legacy actor-damage/direct-Health source scan | Passed; no runtime call or override remains |

The six warning-producing tests are existing fixtures for missing optional condition data,
transient guard definitions, and world-context teardown. None is a failed assertion or new damage
warning.

## Boundary and remaining gates

This was a source, build, and automation re-audit. It does not replace the existing packaged
multiplayer, real save-slot, cook, World Partition, and hands-on vehicle-road evidence recorded in
`RELEASE_VERIFICATION_2026-09-03.md`. A future Narrative Pro upgrade should rerun both build targets
and the 200-test namespace because its public ASC delegate and attribute contracts are external
dependencies.
