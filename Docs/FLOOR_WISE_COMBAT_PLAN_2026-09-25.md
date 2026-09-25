# Floor-wise combat, floor identity, and per-node guard activities — plan of record

**Date:** 2026-09-25
**Status:** Phases 0, 1 and 2 implemented and green. Phase 3 and 4 not started.
**Implementation state (2026-09-25):**
- Phase 0 (definition boundary, B1) — done. See §4.2.1.
- Phase 1 (floor resolution authority) — done, including the real-content run. See §5 Phase 1.
- Phase 2 (engagement gate) — **code complete and unit-green; PIE measurement outstanding.**
  See the "Phase 2 as implemented" section under §5. The gate ships **inert on the current
  map**: `HopDistrictTest` authors no floor regions, so nothing changes there until content
  authors them.
**Scope:** TerritoryFramework plugin. No Narrative Pro source or asset is modified.
**Supersedes:** nothing. This is the first plan for this feature.

Provenance markers used throughout:

- **[V]** — I read this in source or in a doc in this repository and can point at the line.
- **[B]** — reported by the floor-consumption subagent with file:line citations. I have not
  re-read these lines myself. Everything marked **[B]** was used to *correct* this document,
  which is exactly why it carries its own marker rather than borrowing **[V]**.
- **[P]** — plausible and consistent with what I have seen, but **not yet confirmed against source**.
  Every **[P]** item is also listed in §10 so it cannot be quietly promoted to fact.

---

## 1. The request this plan answers

Paraphrased from the original report, with the four asks separated because they have four
different owners and three different sizes:

1. **Floor-wise fight.** A Place declares floors. Floor 1's guards must not join a fight the
   player is having with floor 0's guards. The author must be able to lay out "attack from
   floor 0 up to 2" in one Place and "land on the roof, fight from 2 down to 0" in another.
2. **Nothing hardcoded.** Every part of this must be authored data, extensible from C++ and
   Blueprint, because Narrative Pro is the framework and Territory is its addon.
3. **City, District, Place Definitions must be right.** City contains Districts, District
   contains Places, and **only Places have floors**. Today a City and a District can author
   floors.
4. **`ActivityTag` must become a DataAsset.** It currently promises a behaviour that no code
   implements.

Plus the two asks already closed in this audit and therefore **out of scope here**: the
controller/pawn attitude split (fixed and proven in live PIE) and the level-scoped authoring
validator (shipped; `CheckPatrolContainment` and `CheckGuardDeploymentFeasibility`).

---

## 2. Verified findings — the evidence base

Everything in this section is **[V]**. This is the part that must not be re-litigated.

### 2.1 The hierarchy is already authored the way ask 3 describes

| Edge | Mechanism | Location |
|---|---|---|
| City → Districts | authored object-reference array | `TerritoryDefinition.h:825-827` |
| District → Places | authored object-reference array | `TerritoryDefinition.h:790-792` |
| Place → (nothing) | `UTerritoryPlaceDefinition` is a leaf; no child array on it or on the base | `TerritoryDefinition.h:736-779` |
| child → parent | **derived**, `VisibleAnywhere`, so designers never type a tag twice | `TerritoryDefinition.h:484-487` |
| parent writes child's tag | `RefreshHierarchyLinks` override, parent→child | `TerritoryDefinition.cpp:520-527`, `:549-558` |

Four classes, three of them siblings: abstract `UTerritoryDefinition : UPrimaryDataAsset`
(`:454`), then `UTerritoryPlaceDefinition` (`:737`), `UTerritoryDistrictDefinition` (`:783`),
`UTerritoryCityDefinition` (`:814`).

**Conclusion: there is nothing to fix in the containment model.** It already matches the
stated intent. What is missing is only the *floor* half of that intent.

### 2.2 Root cause of ask 3: `Floors` is declared on the abstract base

`Floors` is declared **once**, on `UTerritoryDefinition`, at `TerritoryDefinition.h:628`:

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards|Floor",
    meta=(TitleProperty="FloorIndex"))
TArray<FTerritoryFloorTemplate> Floors;
```

No subclass re-declares or shadows it. Because it is on the base, every City and District
asset carries it. That single fact is the whole of ask 3's problem.

### 2.3 Why the boundary is not enforced: `Floors` is missing from a deliberate three-part lock

The project already has a mechanism for "this property is Place-only but declared on the
base". All 22 other Place-only properties are handled by the same three independent steps —
none of them is moved to the subclass, precisely so that `ApplyToTerritory` can keep one
code path. `Floors` was left out of all three steps.

| Step | What it does | Location | `GuardPosts` | `Floors` |
|---|---|---|---|---|
| 1. Panel hide-list | hides the property on a non-Place | `TerritoryFrameworkEditor.cpp:317-340` (gated by `bContainsNonPlace`, `:238`, `:251`, `:312`) | hidden (`:332`) | **not in the list** |
| 2. In-memory wipe | resets the value on a non-Place | `NormalizeAggregateDefinition`, `TerritoryDefinition.cpp:29-109`, called from District ctor `:512`, City ctor `:541`, `PostLoad` `:314` | `Reset()` at `:100` | **untouched** |
| 3. Runtime zero-gate | zeroes it when applying to the actor | `ApplyToTerritory`, `TerritoryDefinition.cpp:158-181` | zeroed | never pushed at all |

### 2.4 The observable consequence, and why it is worse than "cosmetic"

Because `Floors` survives on an aggregate while `GuardPosts` is wiped and hidden, this
sequence is reachable and produces an error the author cannot act on:

1. On a District, author a floor row with `DesiredGuards = 2`. The panel offers `Floors`
   normally — it is not hidden.
2. `IsDataValid` raises **an error**: *"Floor {0} asks for {1} defenders but only {2} guard
   post(s) are assigned to it. Each post holds one guard, so add post actors on this floor or
   lower the quota."* (`TerritoryDefinition.cpp:417-424`).
3. `GetFloorGuardPostCount` (`:256-264`) counted `GuardPosts` — the array the panel **hides**
   on a District, and which `NormalizeAggregateDefinition` **Reset** on load.

So the advice in the error message names an array the author cannot see and cannot populate.
The only escape is to author `DesiredGuards = 0`, i.e. to abandon the thing they were trying
to do, with no message telling them why.

Two further details of that block, both **[V]**:

- The guard is `if (!Floors.IsEmpty())` (`:388`) — it runs for **every** definition type.
- On a duplicate floor index the loop `continue`s (`:408`) before adding to `DeclaredFloors`,
  so a post pointing at a duplicated index can still pass the `UndeclaredPostFloor` check.

### 2.5 A contradiction that must be resolved, not papered over

`TerritoryVolume.cpp:784` carries a comment defending base-class floor access, and the
`RuntimeFloorClearedEvents` map is cloned for **all** definition types
(`TerritoryVolume.cpp:787-793`) on the stated grounds that gating it on Place "would silently
drop floor beats for a District or a City".

Those beats can never fire on an aggregate, because an aggregate has no defenders:

- `RegisterDefender` hard-returns on `AggregateOnly` — `TerritoryVolume.cpp:2331`
- `SpawnGuards` / `SpawnGuardsToCount` hard-return — `:3144`, `:3150`
- `DispatchClearedFloors` has exactly one caller, `TryCompleteDefenderDefeat` (`:2613`),
  which has exactly one caller (`:2582`)

No defenders ⇒ no defender-defeat path ⇒ no per-floor clear ⇒ the cloned events are dead
entries. **The stated justification for the base-class design does not hold.** Decide this
before "fixing" §2.4, because it is the one argument in the codebase for keeping `Floors` on
the base. (Note this is the same finding as the earlier audit's "aggregate floors can never
clear"; it is repeated here because it is load-bearing for the decision in §4.2.)

**RESOLVED 2026-09-25 by B1.** The contradiction is gone rather than decided: with `Floors`
authored only on the Place, an aggregate has no floor rows to drop, so the clone loop can sit
inside the Place gate with nothing lost. There is a second reason the old justification was
already vacuous — an aggregate's floor entries always had `MaximumGuards == 0` (posts are
counted only when `bCountsPhysicalSlots`), and `IsCleared()` refuses on its `MaximumGuards > 0`
gate, so an aggregate floor beat could never fire even if the rows had existed. The comment at
`TerritoryVolume.cpp:784` has been rewritten to state that. See §4.2 "B1 as implemented".

### 2.6 Floors have no geometry, so "which floor is this actor on" has no answer today

`FTerritoryFloorTemplate` (`TerritoryDefinition.h:180-208`) has exactly four fields:

| Field | Line | Notes |
|---|---|---|
| `FloorIndex` | `:188` | `ClampMin=0` |
| `DisplayName` | `:193` | no consumer found **[V]** — display-only per its own doc |
| `DesiredGuards` | `:203` | quota |
| `FloorClearedEvents` | `:207` | `Instanced`, `TArray<TObjectPtr<UNarrativeEvent>>` |

No volume, box, anchor, extent or transform. Floor membership is the integer match
`Post.FloorIndex == FloorIndex` (`GetFloorGuardPostCount`, `:256-264`).

**This is the single hard blocker for ask 1.** Every design in §4.3 exists to answer one
question: *which floor is this actor standing on?* Nothing in the plugin can answer it today.

Two consequences already documented:

- The floor doc itself (`:171-179`) says floor state is never stored and the read model is
  regrouped from guard posts. True, and unchanged by this plan.
- `ATerritoryGuardSpawnPoint::GetFloorIndex()` (`TerritoryGuardSpawnPoint.h:150`) is the
  **only** floor accessor on an actor, and its backing field `FloorIndex` (`:173`) is
  `Transient`, written by `ApplyTerritoryDefinition` (`TerritoryGuardSpawnPoint.cpp:282`,
  `:286`). On a never-played map every post reads floor **0**, which is a *valid* floor —
  the trap that cost the validator session its first implementation.

### 2.7 What floors already do — this must not be broken

Floors are a **progress/accounting** concept today, and that machinery is real and tested:

- Per-floor garrison read model: `FTerritoryFloorSnapshot`, `TerritoryTypes.h:370`, with
  `int32 FloorIndex` (`:444`) and `TArray<FTerritoryFloorSnapshot> Floors` (`:508`).
- Built from the definition at `TerritoryVolume.cpp:2993-2999`, reading
  `TerritoryDefinition->Floors` through a **base-class** pointer.
- Per-floor cleared events dispatched by `DispatchClearedFloors` (`:2613`).
- Existing tests: `TerritoryFloorStagingTests.cpp` — floor snapshots and per-floor reserve
  counts (`:281-324`), and authoring validation (`:399` onward: clean, over-quota, stray
  post, duplicate, negative).

**The change to `Floors` in §4.2 must keep every one of those paths working.** The snapshot
builder in particular is the reason the property move has a cost (§4.2).

#### 2.7.1 Per-floor progression is already implemented — in the story layer, not in capture

**[B]** This was under-weighted in the first draft of this plan, and it is the single biggest
correction to it. Floor-cleared progression exists, is per-floor, and is tested:

- `DispatchClearedFloors` (`TerritoryVolume.cpp:2658-2706`) is called at **`:2613`, before the
  whole-Place early return** in `TryCompleteDefenderDefeat` (`:2585-2613`) — deliberately, so
  that **a floor can clear while other floors are still fighting**. It runs on every defender
  death (`OnDefenderDied` `:2471-2474` → `:2582`), so killing floor 0's last guard fires floor
  0's beat immediately, with floor 1 still manned.
- The announcement path is `OnFloorCleared.Broadcast` (`:2703`, `BlueprintAssignable` at
  `TerritoryVolume.h:869-870`) plus `DispatchFloorClearedEvents` (`:2717-2755`), which runs the
  authored instanced `FloorClearedEvents` for that one floor.
- `UTerritoryStateTask` already carries a floor filter: `TargetFloor` (`TerritoryStateTask.h:75-80`,
  `-1` = whole Place), `IsFloorFiltered()` (`:113`), `FindTargetFloor` (`TerritoryStateTask.cpp:134-143`),
  and floor-specific satisfaction rules — `AllDefendersDefeated → Floor->IsCleared()` (`:169`),
  `ReachDesiredGarrison → Floor->ActiveGuards >= FMath::Max(1, RequiredQuantity)` (`:171`).
  `HandleAllDefendersDefeated` (`:334`) deliberately does **not** complete a floor-filtered task,
  with the comment *"This delegate describes the whole Place."*
- Author-facing text already exists: *"Defeat the defenders on floor {0} of {1}"* /
  *"Assign {0} guards to floor {1} of {2}"* (`TerritoryStateTask.cpp:404-416`).
- Tested by `Tales.Tasks.FloorGarrisonObjectives` and `ProgressReadsItsOwnFloorSnapshot`
  (`TerritoryFloorStagingTests.cpp:486`, `:593`) and eleven cases in
  `TerritoryFrameworkEditor/Private/Tests/TerritoryFloorEventTests.cpp`.

**So a designer can already author "clear floor 0, then floor 1" as a story objective today.**
What is missing is the *combat* half — the guards do not respect the boundary — not the
progression half. §4.1's A4 and §6.4 are rewritten accordingly.

#### 2.7.2 The one thing per-floor progression does *not* touch

**[B]** Capture is bounds- and faction-driven and never floor-based. The only mention of floors
near capture is a designer-facing doc comment (`TerritoryVolume.h:817-830`) telling authors to
make the Bounds Shape "tall enough to include a shop's ground floor, stairs, and second floor".
`bStoryCaptureFromBounds`, `StoryBoundsContesters` and Capture Points contain no floor logic.
This is consistent with §4.4's design: the floor gate changes *engagement*, never ownership.

### 2.8 There is exactly one engagement funnel, and it is already server-only

This is the most important reuse finding in the audit for ask 1.

```
ATerritoryGuardCharacter::GetTeamAttitudeTowards      TerritoryGuardCharacter.cpp:255-278
        └─ calls CanEngageTerritoryTarget(&Other)                     :258
ATerritoryGuardCharacter::CanEngageTerritoryTarget                    :280-284
        └─ EvaluateTerritoryTarget(Target, Reason)                    :286-388
UTerritoryBlueprintLibrary::CanScoreTerritoryCombatGoal  TerritoryBlueprintLibrary.cpp:376
        └─ Guard->CanEngageTerritoryTarget(Target)                    :402
```

Properties that make this the right insertion point:

- **One decision, one reason.** `EvaluateTerritoryTarget` routes all 13 exits through a single
  `Result` lambda (`:295-306`) that writes `OutReason` and emits one debug log line per
  decision, covering allows as well as refusals. A floor refusal therefore gets a reason
  string and a log line for free.
- **Already server-authoritative.** `:307`: `if (!HasAuthority()) return Result(false, ...)`.
  No new authority question is introduced by adding a gate here.
- **Already the published contract.** `Docs/Combat_Activity_Eligibility.md:6-13` documents
  that attack activities must call `Can Score Territory Combat Goal` before scoring, that
  zero pauses a goal, and that the shared melee/ranged/grenade and TDA project activities all
  include it. So gating here reaches the AI's goal selection through a documented path.
- **It answers both of the reported symptoms at once.** A guard that perceives the player
  itself, and a guard pulled in by the 3500 cm teammate notification, both reach this
  function — the second through activity goal scoring.

### 2.9 Why floor 1 joins the floor 0 fight today — three mechanisms

All **[V]** as to existence; the *relative contribution* of each is **[P]** and needs the PIE
measurement in Phase 2.

1. **Independent perception.** A floor-1 guard perceives the player on its own, passes all 13
   gates, and attacks. Nothing in `EvaluateTerritoryTarget` involves a floor —
   `grep Floor TerritoryGuardCharacter.cpp` returns nothing.
2. **Teammate notification.** `NotifyTeammatesToFightRange`, default `3500.f`, declared in
   `Plugins/NarrativePro/Source/NarrativeArsenal/Public/Settings/NarrativeCombatDeveloperSettings.h:49`,
   defaulted at `.../Private/Settings/NarrativeCombatDeveloperSettings.cpp:26`. It is
   `EditAnywhere, config, BlueprintReadOnly` — a **project setting**, tunable without touching
   vendor source — but it has **no C++ consumer in any vendor module**; it is read by vendor
   Blueprint AI assets (`GoalGenerator_Attack`, `Goal_Attack`, `BTS_Attack`,
   `GoalGenerator_Flee`, confirmed with `rg -a --text`). It is a flat 3D scalar: no floor,
   room or line-of-sight awareness.
3. **Ungated hearing**, plus 3D attack-token acquisition. **[P]** as to exact consumers.

Mechanism 1 and the *decision* half of mechanism 2 are both closed by the gate in §2.8.
Mechanism 2's *movement* half is not — see §6.2.

**[B] Confirmed by exhaustive sweep.** A module-wide search for the floor concept across
`Private/AI`, `Private/Combat`, `Private/Abilities`, `Private/Subsystems`, `Private/Interaction`,
`Private/Navigation`, `Private/Economy`, `Private/Cinematics`, `Private/Debug`, `Private/Items`
and `Private/Framework` returns **only three** hits, none of which is the authored floor:
`FMath::FloorToInt` in `TerritoryAssaultPlanningLimits.h:19` and
`TerritoryEconomySubsystem.cpp:1183` (integer maths), and the height heuristic in §2.12.
`ATerritoryGuardCharacter` contains **no `Floor` token at all**. The only three runtime readers
of a floor anywhere outside tests and the editor module are `OnFloorCleared.Broadcast`
(`TerritoryVolume.cpp:2703`), `AnnounceDefenderSpawned` (`:2714`) and `BuildFloorSnapshots`
(`:2953`). So the claim "floors never reach the combat decision" is not an inference from the
gate list above — it is exhaustively verified.

### 2.10 `ActivityTag` is inert, and its tooltip states a behaviour that does not exist

`FTerritoryGuardPatrolTemplateNode` (`TerritoryDefinition.h:79-96`):

```cpp
/** Optional Guard.Activity tag identifying the activity requested at this patrol node. */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol",
    meta=(Categories="Guard.Activity"))
FGameplayTag ActivityTag;                       // :95
```

The spawn-point side carries the matching tooltip (`TerritoryGuardSpawnPoint.h:94-100`) —
*"If set, the guard plays this activity at the node instead of standing idle."*

**[V] There is no reader.** Outside tests, `ActivityTag` appears in exactly three places in
the whole plugin: the struct declaration (`:95`), the spawn-point declaration
(`TerritoryGuardSpawnPoint.h:100`), and **one copy statement**,
`TerritoryGuardSpawnPoint.cpp:324` (`Node.ActivityTag = Source.ActivityTag;`). The value is
copied from a template into a runtime node and then never consulted.

Note what already exists next to it, because the replacement must not duplicate it: activity
configuration is already authorable **per post** — `UTerritoryGuardPostDefinition::ActivityConfiguration`
and `ATerritoryGuardSpawnPoint::ActivityConfigurationOverride`, with documented override
precedence (`Docs/05_Guard_System.md:192-201`). What is missing is the **per-node** activity
this field was meant to be.

### 2.11 A correction, and one item still open

- **CORRECTION — `DesiredGuards == 0` IS implemented.** The first draft of this plan claimed
  otherwise; that was wrong. The doc (`TerritoryDefinition.h:195-200`) says zero means "every
  post on this floor", and the snapshot builder implements exactly that at
  `TerritoryVolume.cpp:3015` **[B]**:
  `Entry.DesiredGuards = Floor.DesiredGuards > 0 ? Floor.DesiredGuards : MaximumGuards;`
  `TerritoryDataValidator.cpp:2684` is consistent with it (`if (Floor.DesiredGuards <= 0) continue;`
  — correctly skipping the dead-floor warning for a quota-zero floor). **Nothing to fix here.**
  The lesson is worth keeping: "validation only checks `>`" was true and irrelevant, because the
  consumer was never validation.
- `DisplayName` (`:193`) has no consumer found. **[V]** for "no consumer" in C++; **[P]** for
  whether any Blueprint reads it, because Python reflection cannot see struct members through an
  asset (`TerritoryTypes.h`'s members are `BlueprintReadOnly`, but the floor row is a `UPROPERTY`
  on a data asset and needs a Blueprint-level audit, not a disk probe).

### 2.12 There is already a second "same floor" concept in the counter-attack planner

**[B]** and important enough to name before designing anything, because it is the one place in
the codebase that already answers a floor-ish question by geometry:

`Plugins/TerritoryFramework/Source/TerritoryFramework/Public/Combat/TerritoryCounterAttackProfile.h:243`:

```cpp
float SameFloorHeightTolerance = 500.f;
```

used in `Private/Subsystems/TerritoryCounterAttackSubsystem.cpp:2211`, `:2218`, `:2848`,
`:2871-2874` as a vertical-distance *scoring* term:

```cpp
Score += HeightDifference <= FloorTolerance ? 2.f : -FMath::Min(4.f, HeightDifference / FloorTolerance);
```

This is **not** the authored floor row and must not be conflated with it — it scores which
approach and camera an attacker prefers, and it is a distance heuristic, not a membership test.
But it is a hard constraint on §4.3 for two reasons:

1. **A second authority for "are these on the same floor" is exactly what AGENTS.md §4.1
   forbids.** So `GetFloorAtLocation` and `SameFloorHeightTolerance` must not both end up
   deciding membership. Decide in Phase 1: either the planner migrates onto the resolved floor,
   or the tolerance is documented at its declaration as *approach scoring only* and the floor
   query is stated to be the sole membership authority. My recommendation is the latter — the
   planner's question genuinely is different, and rewriting a working scoring curve is not part
   of this feature.
2. **It is the natural seed for §4.3 C4's editor bake.** A floor's initial box extents can be
   derived from its own posts using the same 500 cm banding the planner already trusts, giving
   the author a sane starting volume instead of an empty box. The band is a *default*, never a
   runtime authority.

Also noted by the same sweep and worth not mistaking for this concept:
`CounterAttack.Regression.MultiFloorFallbackUses3DDistance`
(`Private/Tests/TerritoryCounterAttackTests.cpp:605-606`) is a height heuristic test, not a
floor test.

---

## 3. Authority map for the new state

AGENTS.md §4.1 requires naming the authority before implementing. This is the only new state
this plan introduces, and none of it duplicates an existing owner.

| New state or behaviour | Authority | Why not something else |
|---|---|---|
| Whether a Place declares floors | `UTerritoryDefinition` (already) | exists; §4.2 only moves it to the right class |
| **Which floor a world position is on** | `UTerritoryRegistrySubsystem` (new query) | §4.1 already assigns *"Territory registration and spatial lookup"* to this subsystem, and it already owns the spatial index (`GetTerritoryAtLocation`, `TerritoryRegistrySubsystem.h:93`; `GetTerritoriesInLocation` `:97`) |
| Floor region geometry | level actors, bound to floor rows (new actor type) | mirrors the existing guard-post pattern exactly: definition row is logical, level actor is physical. Keeps level geometry out of the definition, matching `Docs/22:83` and World Partition streaming |
| Whether a guard may engage across floors | `UTerritoryFloorCombatPolicy` DataAsset, referenced from the floor row | AGENTS.md §8: *"Prefer data assets for reusable guard, attack, reward, and balance profiles"* |
| The engagement decision itself | `ATerritoryGuardCharacter::EvaluateTerritoryTarget` (already the single authority) | §2.8 — no second decision point is created |
| Per-node guard activity | DataAsset referenced from the patrol node | replaces the inert tag; per-post activity already exists and is reused |

**No new subsystem. No second capture path. No second faction system. No new replicated
field.** The gate is server-only, so the floor query does not need to replicate.

---

## 4. Design

### 4.1 Part A — what "floor-wise fight" means, mechanically

Four separable mechanisms, in the order they matter. Conflating them is why this looks like
one problem when it is four.

| # | Mechanism | Question it answers | In this plan? |
|---|---|---|---|
| A1 | **Engagement** | may this guard attack that actor? | **yes — Phase 2** |
| A2 | **Movement** | may this guard walk to that actor? | **no — §6.2, own plan** |
| A3 | **Threat propagation** | when a fight starts, who is told? | partly, via A1 (§6.1) |
| A4 | **Progression** | when is a floor "done"? | **already implemented** — Tales floor objectives + `OnFloorCleared` + per-floor `FloorClearedEvents` (§2.7.1). Reuse it; do not rebuild it (§6.4) |

Phase 2 delivers A1 and, through the single funnel, most of A3. A2 and A4 are named here so
they are not mistaken for oversights, and they are explicitly out of scope.

### 4.2 Part B — make `Floors` Place-only (ask 3)

Two candidate approaches. **Recommendation: B1.**

#### B1 (recommended) — move the property to `UTerritoryPlaceDefinition`

Move `Floors` from `TerritoryDefinition.h:628` to the `UTerritoryPlaceDefinition` block
(`:736-779`), and move the floor accessors and the `IsDataValid` floor block with it.

Why this over B2: it makes the boundary **structurally true** rather than three-times-guarded.
A City cannot author what it does not have, so the hide-list entry, the wipe and the
misleading quota error all become unnecessary for this property. It also matches every other
statement in the codebase, which already describes floors as Place-only: the base-class
`HasAuthoredFloors` doc says *"this Place declares"* (`:712`), and
`FTerritoryGuardPostTemplate::FloorIndex` says *"a row in the **Place's** Floors array"*
(`:222`).

Cost — mechanical, and fully enumerated. Every site that reads `Floors` or a floor accessor
through a base pointer:

| Site | Current | Needed |
|---|---|---|
| `TerritoryVolume.cpp:2993-2999` | floor snapshot builder, base pointer | `Cast<UTerritoryPlaceDefinition>` guard; an aggregate then legitimately has no floors |
| `TerritoryVolume.h:943-944`, `.cpp:754`, `.cpp:788-794` | `RuntimeFloorClearedEvents` reset + per-floor clone loop, all types | Place-only — resolves §2.5 |
| `TerritoryDefinition.cpp:390-437` | `IsDataValid` floor block | move into a `UTerritoryPlaceDefinition::IsDataValid` override |
| `TerritoryDefinition.h:699`, `:702-705`, `:707-710`, `:712-715` | `FindFloor`, `GetFloorTemplate`, `GetFloorGuardPostCount`, `HasAuthoredFloors` | move to Place (public) |
| `TerritoryDataValidator.cpp:2657`, `:2682` | `HasAuthoredFloors`, `Definition->Floors` | Place cast |
| `TerritoryGuardSpawnPoint.cpp:282-286` | actor copy of the row's `FloorIndex` | no change — already per-Place |
| **no change needed** (§3 readers below) | `TerritoryVolume.cpp:2658-2706`, `:2717-2755`, `:2708-2715` (dispatch/announce) and `TerritoryStateTask.cpp`, `TerritoryBlueprintLibrary.cpp:231-256` | these read the **snapshot**, not the definition — **[B] confirmed** |

Serialization: for a **Place** asset the property name is unchanged, so the tag resolves
against the subclass's property map and existing floor data loads normally. For a City or
District the property ceases to exist, so any stored rows are dropped — which is the intent.

Blueprint migration (AGENTS.md §8): any graph that reads `Floors` through a **Place** pin
keeps working. A graph that read it through a City/District/base pin will fail to compile and
must be repointed; the migration path for that is the §4.2 B2 fallback, so a project that
needs it is not stuck.

#### B1 as implemented — 2026-09-25, and the one deviation from the table above

Implemented and verified. Two corrections to the plan text, both found by doing it:

**1. The three `BlueprintPure` accessors were NOT moved. This was a deliberate deviation.**
`Content/HOPTRENDY/Blueprint/BP_FloorCheck.uasset` (user-authored) is a live graph calling
`GetFloorTemplate`, and it names both `TerritoryDefinition` and `TerritoryPlaceDefinition`. A
bare move would leave that node pointing at a class that no longer declares the function.
Moving the data while keeping the queries on the base satisfies both AGENTS.md §8 (a Blueprint
migration path) and the user's actual ask, which is about *containment* — a City/District
holding floor data — not about which class answers a query. The queries cast to the Place and
return the honest answer for an aggregate: false, 0, empty, null. That is a true answer, not a
stub, and `TerritoryFramework.Guards.Floors.ValidationRejectsInconsistentAuthoring` asserts
the same base-class pointer still returns a **Place's real authored row**, so the query cannot
decay into a constant-return shim.

`GetFloorGuardPostCount` needed no cast at all: it counts `GuardPosts`, which stays on the base.

**2. `HasAuthoredFloors` had to leave the header.** It was an inline `{ return !Floors.IsEmpty(); }`.
Written inline on the base it cannot compile, because `UTerritoryPlaceDefinition` is still
incomplete at that point in the same header. It is now declared in the header and defined in
the `.cpp` alongside `FindFloor`, which has the same constraint.

**3. §2.5 is resolved by this change rather than by an extra decision.** The old comment at
`TerritoryVolume.cpp:784` argued the clone loop had to sit outside the Place gate or a District
or City would "silently drop floor beats". With `Floors` Place-only, an aggregate has no rows
to drop, and the claim was already unreachable: an aggregate's floor entries always had
`MaximumGuards == 0` (posts are counted only when `bCountsPhysicalSlots`), and `IsCleared()`
refuses on its `MaximumGuards > 0` gate, so an aggregate floor beat could never have fired.
The clone now lives inside a `Cast<UTerritoryPlaceDefinition>` branch and the comment states
that reasoning. Behaviourally equivalent; observable only as an empty
`GarrisonSnapshot.Floors` instead of zero-valued entries, which `UTerritoryStateTask` treats
identically.

**Blast radius, measured.** `Floors` is now in none of the four places it used to be reachable
from the base. The three direct member reads were fixed with Place casts
(`TerritoryVolume.cpp` ×2, `TerritoryDataValidator.cpp` ×1); `NormalizeAggregateDefinition` and
the editor hide-list needed no change, because neither ever mentioned `Floors` (the hide-list
never did, which was §2.3's finding, and after the move there is nothing to hide). Of the five
definition assets in the project, only `DA_Place_Blacksmith` carries a `Floors` name at all,
and it is a Place, so it resolves on the subclass; the City and both District assets contain no
`Floors` name in their name tables, so there is no orphaned property to migrate.

Verification actually performed: build `exit=0`; then 9 filters, all `exit=0 fail=0` —
`Guards.Floors` 19, `Guards` 38, `Definition` 4, `Editor.Definitions` 1,
`Editor.DataValidation` 16, `Contract` 53, `Core` 2, `Editor.Authoring` 5, `Tales` 50, `AI` 7.
The red leg was observed: `Guards.Floors.BlueprintAndReplicationContract` failed on
`Expected 'A Definition carries its floors' to be true` before the test was updated, which is
the assertion that encoded the old boundary. **Not** run and not claimed: the full 2123-test
suite, PIE, dedicated-server, two-client, cook/package, and a `BP_FloorCheck` compile in the
editor.

#### B2 (conservative fallback) — keep it on the base and join the existing lock

If BP breakage must be zero: add `Floors` to the hide-list (`TerritoryFrameworkEditor.cpp:317-340`),
add `Floors.Reset()` plus a `!Definition->Floors.IsEmpty()` term to the `bChanged` expression
in `NormalizeAggregateDefinition` (`:29-109`), and replace the quota error on a non-Place with
an honest one: floors are not authorable here and were discarded.

This is three small edits and no runtime change. It is *behaviourally* equivalent and
*structurally* a lie — a City still has a `Floors` member, it is just always empty.

**Either way, §2.5 must be resolved in the same batch**, or the base-class justification at
`TerritoryVolume.cpp:784` remains in the tree arguing against the change.

### 4.3 Part C — a floor resolution authority (the blocker for ask 1)

#### C1. Floor regions are level actors, bound to floor rows

New actor: `ATerritoryFloorVolume` — a box, plus the `FloorIndex` it represents.

Binding follows the **existing guard-post pattern exactly**, which is already implemented and
tested (definition row → owned actor → the post's own tag → spatial overlap;
`Docs/05_Guard_System.md:203-215`). Reusing that resolution order is the point: a second
binding convention would be a second authority.

Every floor volume requires an editor-baked stable GUID, exactly as guard posts do
(`Docs/05:118`). This is required by AGENTS.md §7.

**Why a level actor and not geometry in the definition:** physical placement, extents and
rotation are level data. The definition owns the *identity* of a floor (index, display name,
quota, events, policy); the level owns its *extent*. This is the same split the guard-post
system already made, and it is what lets a floor volume stream with World Partition.

#### C2. One query, in the existing registry

```
UTerritoryRegistrySubsystem::GetFloorAtLocation(
    const ATerritoryVolume* Place, const FVector& WorldLocation) const -> int32   // INDEX_NONE = unresolved
```

BlueprintPure, read-only, deterministic, no randomness. §4.1 of AGENTS.md assigns spatial
lookup to this subsystem, and it already exposes this exact shape of query
(`TerritoryRegistrySubsystem.h:93`, `:97`, `:101`). A resolved floor is never cached in a way
that can go stale on stream-in/out.

#### C3. The inert-by-default rule — this is what makes the phase safe

**If a Place has floor rows but no floor volume bound to them, the floor gate does nothing at
all and today's behaviour is preserved exactly.** The editor validator warns that floor
separation is declared but unenforced.

This matters more than it looks. It means Phase 1 can ship with zero gameplay change, the
feature is opted into per Place, and no existing map or save changes behaviour on the day the
code lands. It is also the honest answer for a streamed-out volume: unresolved ⇒ inert ⇒
visible in the validator, rather than a guard that silently stops defending.

#### C4. Where authoring help belongs

A tool that derives initial box extents from a floor's own guard posts is a useful **editor
convenience** — a starting box the author then adjusts. Seed the band with the planner's existing
`SameFloorHeightTolerance = 500.f` (§2.12), which is the same 500 cm the counter-attack
subsystem already trusts for vertical neighbourhood, so the two do not disagree on day one.

It must be an editor-time bake, never a runtime fallback, so that "which floor is this actor on"
keeps exactly one authority.

#### C5. The coexistence decision §2.12 forces

Phase 1 must state, at the declaration site, which of `GetFloorAtLocation` and
`SameFloorHeightTolerance` owns floor *membership*. Recommendation: the registry query owns
membership; the tolerance is documented as approach *scoring* only, in a comment at
`TerritoryCounterAttackProfile.h:243` — one sentence, no behaviour change, and it removes the
only plausible reading under which the plugin has two answers to one question.

### 4.4 Part D — the engagement gate (ask 1)

#### D1. Where it goes in the 13-gate order

`EvaluateTerritoryTarget` currently runs: reject unavailable/dead/self → quest pause →
same faction → protective treaty → `CombatTargetFactions` filter → **personal hostility
allow** (`:351`) → reject unless War (`:353`) → assault-front allow (`:356`) → stealth
allow (`:363`) → Contested allow (`:376`) → War-in-Claimed allow (`:383`).

**The floor gate goes between `:354` and `:356`** — after the target has been established as
a legitimate enemy, and before every *proactive* allowance.

The placement is deliberate, not arbitrary:

- **Not before personal hostility (`:351`).** That branch is what lets a guard answer real
  damage. A guard shot from the floor below must still defend itself; moving the floor gate
  above it would silently regress behaviour that is already covered by
  `TerritoryGuardResponseTests.cpp` (`:168-260`). Retaliation stays where it is.
- **Before the proactive allowances.** Gates at `:356`, `:363`, `:376`, `:383` are the ones
  that make a guard attack someone it merely *perceives* or is told about. Those are exactly
  the paths through which floor 1 joins the floor 0 fight.
- **Configurable.** `bAllowCrossFloorRetaliation` (default on) keeps the self-defence
  property explicit rather than accidental.

A floor refusal returns through the same `Result` lambda, so it gets an `FText` reason and the
existing debug log line with no extra plumbing.

#### D1a. The seven-layer recon that confirms the placement, and the one thing it rules out

The gate position above was chosen from the funnel; it has now been checked against every layer
that can make a guard join a fight. The result confirms the choice and kills an approach worth
recording so nobody attempts it later.

**Confirmed: the gate covers every consumer.** `ATerritoryGuardCharacter::GetTeamAttitudeTowards`
and the Blueprint `CanScoreTerritoryCombatGoal` both funnel into `CanEngageTerritoryTarget` →
`EvaluateTerritoryTarget`, and the guard's own AI reaches attitude by exactly one route
(`TerritoryGuardCharacter.cpp:258`). The controller-side split that was fixed earlier is what
makes this true for both the pawn and its controller. There are **four** layers that legally
receive both combatants as `AActor*` — attitude (1), the Territory stealth observer (2), the
goal generator / `CanScoreTerritoryCombatGoal` (5) and the Territory target filter (6) — and the
attitude gate is the single one that all of them pass through.

**Confirmed: there is nothing to add at the token layer.** `CanStealToken` and
`ShouldImmediatelyStealToken` are virtual on the vendor ASC and are attitude- and floor-blind,
but they are also downstream of the decision: a guard that is refused at the attitude gate never
gets far enough to claim a token. A Territory ASC subclass is therefore **not** part of this
feature. (Recorded because the override surface looks tempting and adding it would be a second
gate with no distinct effect.)

**Confirmed: the missing primitive is exactly one thing, and it is not a gate.**
`ATerritoryVolume::ContainsPoint` tests a volume spanning all authored floors, so it answers
"inside the Place" and never "on which floor". There is no `GetFloorAtLocation` /
`ResolveFloor` / `IsSameFloor` anywhere in the plugin — the only `FindFloor` is the authored-row
lookup by index, and `UTerritoryGuardSpawnPoint::GetFloorIndex()` returns the post's *authored*
floor, not a spatial answer. So Phase 1's registry query is not a convenience; every one of the
four legal seams is blocked without it. This is the plan's single hardest dependency.

**Ruled out: the teammate notification cannot be stopped at source, and must not be attempted.**
`NotifyTeammatesToFightRange` (vendor default `3500`, this project overrides it to `4000` in
`Config/DefaultEngine.ini`) has **zero C++ readers in any vendor module**; it is consumed only by
vendor Blueprint graphs (`GoalGenerator_Attack`, `Goal_Attack`, `BTS_Attack`, `GoalGenerator_Flee`),
which emit a plain `UAISense_Hearing::ReportNoiseEvent` with a 3D spherical radius — no LOS, no
floor, no volume test. There is no per-actor override, no delegate, and the setting is one global
`config` float. **Suppressing it would require editing vendor content, which AGENTS.md forbids.**

The correct design is therefore the one this plan already has: let the stimulus arrive, and
refuse the *engagement* at the gate. A floor-1 guard will "hear" the floor-0 fight and decline
it. That is a deliberate, documented consequence — it is §6.1 restated — and it is the reason
the gate must sit where D1 puts it rather than at the perception layer. The same logic applies to
hearing generally: the perception affiliation filter is effectively dead (`GetGenericTeamId` is
commented out in vendor `ANarrativeNPCCharacter`), so a heard noise becomes hostility only at
*our* attitude call. Which is again the gate.

#### D2. Resolving the two floors

| Actor | Floor from | Fallback |
|---|---|---|
| The guard | `GetFloorAtLocation(this)`, by position — so a guard that follows you upstairs changes floors | its authored post floor, `OwningTerritorySpawnPoint->GetFloorIndex()` |
| The target | `GetFloorAtLocation(Target)` | unresolved ⇒ inert |

The guard already holds its post: `OwningTerritorySpawnPoint`
(`TerritoryGuardCharacter.h:154`), replicated (`TerritoryGuardCharacter.cpp:883`), assigned
from the spawn context (`:223`). A resolved guard floor is the point of the whole feature —
it is what makes "fight from 2 down to 0" work as a journey rather than as a spawn-time pin.

#### D3. The policy DataAsset (ask 2)

`UTerritoryFloorCombatPolicy` — a DataAsset, per AGENTS.md §8.

| Field | Purpose |
|---|---|
| `EngagementPolicy` | `SameFloorOnly` / `SameOrAdjacent` / `AnyFloor` (today's behaviour, and the default for a floor with no policy) |
| `bAllowCrossFloorRetaliation` | whether damage taken from another floor permits self-defence |
| `bRequireResolvedTargetFloor` | strict mode: an unresolvable target is refused rather than admitted |
| `DisplayName` / `Description` | author-facing |

Referenced from `FTerritoryFloorTemplate` (per floor), with a Place-level default on the
definition. Per-floor, not per-post, because a floor is the unit the author thinks in — and
the floor row is already the unit that owns `DesiredGuards` and `FloorClearedEvents`.

The asset is where a project adds its own rule without touching the gate: a Blueprint subclass
overrides one `BlueprintNativeEvent` (`DecideFloorEngagement`) that receives the guard, the
target, both floors and the policy. That is the "extensible from Blueprint" half of ask 2.

### 4.5 Part E — `ActivityTag` becomes a DataAsset (ask 4)

#### E1. Replace the inert tag with a typed reference

Remove `FGameplayTag ActivityTag` (`TerritoryDefinition.h:95`) and the copy at
`TerritoryGuardSpawnPoint.cpp:324`, and replace with a typed DataAsset reference on the patrol
node. AGENTS.md §8 prefers typed references and says to use GameplayTags for *stable semantic
identity*, not as a lookup key — so the reference is typed and the DataAsset carries the tag
for identity and diagnostics.

`UTerritoryGuardActivityDefinition` (DataAsset):

| Field | Purpose |
|---|---|
| `ActivityTag` | identity/diagnostics only — see the warning below; must NOT be a lookup key |
| **`Goal`** (instanced `UNPCGoalItem`) | **the load-bearing field.** Resolution target — see E1a |
| `TriggerSetOverrides` | reused vendor mechanism, same as the post level (`Docs/05:186`) |
| `WaitOverride` | optional; composes with the node's existing `WaitTime` (`:90`) |

#### E1a. §10 item 1 is answered — the vendor type is a *goal*, and a tag lookup would be a second authority

The plan flagged "the exact vendor type a per-node activity should reference" as unverified.
It has now been traced, and the answer changes the table above rather than filling in a blank.

**Narrative Pro has no tag-driven activity selector.** `ActivityGroup::GroupTag` is inert, and
the group-matching loop in `NPCActivityComponent.cpp:261-267` is commented out in vendor
source. So an `ActivityTag → activity` lookup implemented in Territory would not be an adapter;
it would be **a second activity-selection authority**, which AGENTS.md §4.1 forbids and §3 tells
us to search for before creating. `ActivityTag` therefore survives only as semantic identity
and debug metadata, exactly as AGENTS.md §8 says a tag should be used — never as the key that
resolves the activity.

**The supported extension point is a goal, added to the existing activity component.** The path
is already demonstrated end to end in this codebase: `UTerritoryPatrolGoal` +
`InitializeTerritoryPatrolGoal()` (`TerritoryGuardCharacter.cpp:1131-1166`) reaches
`UNPCActivityComponent::AddGoal` (`:871-876`), and the vendor's own
`UNPCSpawnComponent::OptionalGoal` plus the demo `NPCSpawner_BanditPatrolTest` show the same
idea as authored content. Selection is then a **score competition** re-evaluated on a
`RescoreInterval` timer (vendor default `0.5f`) — not a tag lookup.

Consequences for this phase, all of which must be honoured or E3 fails silently:

- The DataAsset holds an **`EditInlineNew UNPCGoalItem`** (class or instance), and the runtime
  resolves it into `AddGoal`. There is no `ActivityClass` field for a "what to play".
- A goal added this way **competes on score**; it does not force the activity. If the node's
  activity must win, the score has to be authored to win, the same way
  `RefreshClosestHostilePlayerPriority` (`TerritoryGuardCharacter.cpp:588-674`) already transiently
  boosts a goal's `DefaultScore` and calls `PerformActivitySelection(true)`, with
  `TerritoryAssaultTargetPolicy::RestoreGoalScores` to undo it. That existing pair is the
  precedent to reuse rather than invent a new forcing mechanism.
- `UNPCActivity::bIsInterruptable` is **inert** in vendor source, so it cannot be used to
  arbitrate between this node activity and a combat interruption. Do not build on it.
- Two incidental vendor defects found while tracing this, both worth knowing before Phase 3
  relies on vendor behaviour: `BPA_Idle` appears unreachable as shipped (no vendor asset
  creates a `Goal_Idle` instance), and the scores `Docs/15_AI_Integration.md` §3 claims for
  `BPA_Idle` / `BPA_Patrol` are not present in vendor source.

**E1's original `GoalClass` row was too weak** — a bare class is not enough, because the
vendor's goal carries authored parameters (`Try Add Attack Goal From Actor` builds goals from a
perception event with scores attached). The field is an instanced goal, not a class reference.

#### E2. It must compose with, not duplicate, the per-post activity

Per-post activity already exists and is documented with a precedence chain
(`Docs/05:192-201`). The new node reference slots into that chain as the **most specific**
level, above `ActivityConfigurationOverride`:

```
node activity  >  spawn point override  >  GuardPostDefinition  >  territory fallback
```

#### E3. The requirement that keeps this honest

The field must be **read and applied at runtime**, proven in live PIE, before this is called
done. The current field's only sin is that it was never read; replacing it with a nicer asset
that is also never read would reproduce the defect exactly, and AGENTS.md §9 forbids it.

There is **no companion quota fix to make** — `DesiredGuards == 0` was believed to be
unimplemented and is not (§2.11). This phase is only the field replacement.

---

## 5. Phases

Each phase is independently testable and each is written to be seen failing first
(AGENTS.md §12, and the project's own "a green test is not evidence until you have seen it go
red").

### Phase 0 — Definition boundary (ask 3). No gameplay change.

**DONE 2026-09-25. Implemented as B1 (with the one deviation recorded in §4.2).** All four
bullets below are closed: `Floors` and the floor `IsDataValid` block are on the Place; §2.5 is
resolved by the change itself; the misleading quota error is structurally impossible now
because an aggregate cannot hold a floor row; `DesiredGuards == 0` was correctly left alone.
Build `exit=0`, 9 test filters `exit=0 fail=0`, red leg observed. What Phase 0 did **not**
prove, and no phase can: that a `BP_FloorCheck` node still compiles — that needs the editor.

- Move `Floors` to `UTerritoryPlaceDefinition` (B1), or apply the three-part lock (B2).
- Resolve §2.5 in the same batch.
- Fix the misleading quota error so it can never name a hidden array again.
- `DesiredGuards == 0` needs **no** fix — it is implemented (§2.11). Do not "fix" it.
- Tests: floors round-trip on a Place; a City/District has no floors and reports nothing;
  a Place's existing floor snapshot and per-floor reserve counts are unchanged
  (`TerritoryFloorStagingTests.cpp:281-324` must stay green); the five authoring-validation
  cases (`:399`+) stay green; a new failure-path test that a City with pre-existing floor
  data loses it and does not error confusingly.
  *As implemented:* the "a City/District has no floors" half is a reflection assertion in the
  contract test (red before the change: `HasProperty(CityClass, "Floors")` was true), and the
  behavioural half is in `ValidationRejectsInconsistentAuthoring` — an aggregate validates
  clean, and the same base-class pointer still returns a Place's real floor row, so the query
  cannot become a constant-return stub.

### Phase 1 — Floor resolution authority. No gameplay change (C3). 

- `ATerritoryFloorVolume` + editor-baked GUID. **DONE** (uncommitted; the plugin's last commit is
  `7158403`): `Source/TerritoryFramework/Public/Core/TerritoryFloorVolume.h` and its `.cpp`.
  Editor-baked `FloorVolumeGUID`, `BeginPlay` hard-errors rather than lazily baking, `EndPlay`
  unregisters, `ContainsPoint` deliberately identical to `ATerritoryVolume::ContainsPoint`.
- `UTerritoryRegistrySubsystem::GetFloorAtLocation`. **DONE.** Keyed by Place tag, most-specific
  region wins with the volume GUID breaking an exact tie; `INDEX_NONE` means "no separation
  declared here" and callers must treat it as permission to proceed, never as an error.
  Nine tests in `Source/TerritoryFramework/Private/Tests/TerritoryFloorVolumeTests.cpp`, green, and
  mutation-proved against four targeted defects (inverted most-specific rule, neutered GUID
  tie-break, widened negative-index guard, neutered Z containment) — each mutation failed exactly
  the predicted test. Noted honestly: the inverted most-specific mutation and the missing
  tie-break mutation both over-determine `ResolveIdenticalOverlapByStableIdentity`.
- Validator: `UTerritoryDataValidator::CheckFloorVolumes`. **DONE**, with two deviations from the
  wording above, both listed under §6.4 below so neither is taken silently.

#### R1/R3 deviations from the plan's literal wording

The plan said: *a floor row with no bound volume warns that separation is declared but unenforced;
a volume bound to a non-existent floor row errors; two volumes claiming the same floor of the same
Place in overlapping space warns (ambiguity).* Two of those three are implemented exactly. Two
narrowings were applied to the other two, and they are recorded here rather than assumed:

- **R1 is scoped to floor rows that actually carry guard posts** (`GetFloorGuardPostCount > 0`),
  not to every floor row. A row with no post cannot hold a guard, so it cannot be separated from
  anything: it exists to drive `FloorClearedEvents`, and warning on it would flag every story-only
  floor. The plan's word "unenforced" is satisfied by the narrower rule — an unenforced floor is
  only a failure when something stands on it. Pinned by
  `PostlessFloorIsNotReported`; the widening mutation (drop the post-count gate) fails exactly
  that test.
- **R3 warns on *different-floor* overlaps, not same-floor ones.** The plan said "two volumes
  claiming the same floor … in overlapping space warns (ambiguity)". Two regions of the *same*
  floor resolve to the same index, so no point in the overlap has two answers and the resolver has
  nothing to choose between — that is redundant authoring, not ambiguity, and warning there would
  be noise. The genuinely ambiguous case is two *different* floors sharing space, where the
  resolver's most-specific rule silently decides. Pinned by `TwoRegionsOfOneFloorAreNotReported`
  and `FloorVolumesOnDifferentFloorsOverOneSpaceAreReported`; the mutation that also reports
  same-floor overlap fails exactly the first of those.
- **The overlap test under-reports on purpose.** An axis-aligned bounds test alone would
  false-warn on a rotated pair, so the bound is only an admission filter and the answer is
  confirmed with the runtime's own containment query in both directions. A pair whose regions cross
  without putting a corner inside the other is therefore not reported. Safe direction for a
  warning: it under-reports rather than inventing an ambiguity. Also pinned by
  `RotatedNeighbourSharingOnlyABoundCornerIsNotReported`.

Ten tests in
`Source/TerritoryFrameworkEditor/Private/Tests/TerritoryFloorVolumeValidatorTests.cpp`; the whole
`TerritoryFramework.Editor.DataValidation` filter is 26/26 green, and five targeted mutations each
failed exactly the predicted test — except the "stop honouring which Place a region is bound to"
mutation, which failed seven and so over-determines
`ARegionDoesNotSeparateAPlaceItIsNotBoundTo`. That test cannot distinguish keying regions by the
Place *definition pointer* from keying them by the Place *tag*: the two are behaviourally identical
here, so no mutation isolates the scoping direction alone.

Two things the validator deliberately does not do, both because the alternative is a second
authority: no region is required to sit inside its Place's Bounds Shape (a floor is a stage for
combat, not a claim on ownership), and a Place definition bound to a region whose Place actor is
absent from this level is **not** reported — a definition can legitimately be bound while its Place
lives in another streaming sublevel, and AGENTS.md §7 forbids that false positive. Note the
consequence, which is real: `ValidateWorld` had to pin `ATerritoryFloorVolume` alongside the other
actor classes, or a region that had not streamed in would be indistinguishable from a floor with no
region authored and R1 would warn about correctly separated floors.

- Validator, second rule (§6.4): a floor whose row can never be cleared because the **whole-Place**
  `DesiredGuardCount` is below the sum of its posts' reach. `TerritoryVolume.cpp:2957-2964`
  documents this consequence in source; nothing warns about it. This is a *different* failure
  from the already-shipped "floor can never be staffed" warning and must not be merged with it.
  **Still open** — needs a read of the staffing-order logic (`SpawnGuardsToCount`,
  `TrySpawnReserveGuard`, `HasAvailableSlot`) before it can be written without inventing a rule.
  Verified so far: the mechanism is real (`FTerritoryFloorSnapshot::IsCleared()` requires
  `ReserveGuards == 0`) and it is genuinely distinct from the shipped warning.
- One comment at `TerritoryCounterAttackProfile.h:243` stating membership vs. scoring (§C5).
- Tests: resolution inside/outside/above a volume; determinism for a repeated query; overlap
  tie-break; unbound ⇒ `INDEX_NONE` ⇒ gate inert; WP stream-out ⇒ `INDEX_NONE`, no crash; the
  two new validator warnings red-first.

#### Real-content run, 2026-09-25 — what the checks say about `HopDistrictTest`

The plan's verification asked for this and it was run. Route: `Scripts/Territory/
validate_hopdistrict_headless.py`, a driver that calls the project's own `validate_assets()` gate
from `UnrealEditor-Cmd.exe` (Play and Simulate necessarily stopped, per AGENTS.md §2), because
`editor_validation.py` itself only runs inside a live editor and no editor session was available.
Report: `Saved/EditorValidation/hopdistrict_report.json`; message text from the run log's
`AssetCheck` lines, since `validate_assets_with_settings` returns counts, not messages.

**Result: `status: completed`, 6 assets checked, 5 valid, 1 invalid (the map), 1 warning.** The five
definitions — City, two Districts, Blacksmith, Farm — are all valid, so `IsDataValid`'s floor rules
(negative index, duplicate index, `DesiredGuards` above the floor's post count, a post naming an
undeclared floor) pass on real content. The map's findings are the map's, not the definitions':

```text
Error:   BP_Property_Blacksmith1: vehicle approach 'Blacksmith_WestRoad' has invalid deployment,
         drive, awareness, or retirement settings
Warning: BP_Property_Blacksmith1: guard spawn point 'BP_TerritoryGuardSpawnPoint7' patrol node 0 at
         V(X=-2860.00, Y=60.00, Z=50.00) lies outside this Place; its guards leave the territory bound
Warning: BP_Property_Blacksmith1: guard spawn point 'BP_TerritoryGuardSpawnPoint' patrol node 2 at
         V(X=-2860.00, Y=60.00, Z=50.00) lies outside this Place; its guards leave the territory bound
Warning: BP_Property_Blacksmith1: guard spawn point 'BP_TerritoryGuardSpawnPoint2' patrol node 2 at
         V(X=-3290.00, Y=60.00) lies outside this Place; its guards leave the territory bound
Warning: BP_Property_Blacksmith1: guard spawn point 'BP_TerritoryGuardSpawnPoint5' patrol node 0 at
         V(X=-2430.00, Y=-220.00, Z=510.00) lies outside this Place; its guards leave the territory bound
Warning: BP_Property_Blacksmith1: floor 0 staffs 5 guard post(s) but no floor volume in this level
         claims it; guards on that floor engage targets on every floor, and its floor-cleared events
         cannot be gated on that floor being emptied
Warning: BP_Property_Blacksmith1: floor 1 staffs 2 guard post(s) but no floor volume in this level
         claims it; guards on that floor engage targets on every floor, and its floor-cleared events
         cannot be gated on that floor being emptied
```

What this establishes, and what it does not:

- **R1 fires on the real map exactly as predicted, and it is the reported symptom measured.**
  Blacksmith authors two staffed floors — floor 0 with 5 posts, floor 1 with 2 — and **no floor
  region exists anywhere in the level**, so nothing separates them. That is "floor 1 guards spawn
  too at the same time when player fights with floor 0 guards", now an editor-time finding with
  counts rather than a report. The content fix (authoring `ATerritoryFloorVolume` actors) is new
  level content and therefore the user's call, not something this change should author: the plan's
  own instruction is to report these findings rather than edit content.
- **The four patrol warnings are the other reported symptom** ("some guard goes patrolling outside
  of territory bound"), and they are pre-existing check output, not new. Note the fourth: floor 1's
  patrol node sits at `Z=510`, so it leaves the Place in Z — a second-order point for Phase 2, since
  a floor region that separates floor 1 by height does not also make the *Place's* bounds reach it.
- **The one error is pre-existing and out of this change's scope.** It comes from
  `CheckCounterAttackConfig` (`TerritoryDataValidator.cpp:2141-2143`), one of the five conditions in
  that message's list, and is unrelated to floors. Evidence that this change did not cause it: the
  working-tree diff of the validator adds 232 lines and changes no counter-attack or vehicle line
  (`git diff -U0 | grep -E '^[-+].*(CounterAttack|Vehicle)'` is empty). Which of the five conditions
  fails is a counter-attack-scope follow-up: it needs the approach's authored numbers, not a code
  reading.
- **`CheckGuardDeploymentFeasibility` stayed silent on Blacksmith, and that is consistent with its
  documented limit rather than evidence that guard placement is fine.** The check predicts the
  *pre-spawn* refusals only (`ResolveGuardDeploymentTransform` + `IsGuardSpawnLocationClear`); the
  post-spawn `IsPlacementAcceptable` refusal, which the earlier audit measured on Blacksmith's
  posts, is explicitly not predicted because predicting it would mean re-deriving how a spawned
  capsule settles. So this run cannot confirm or refute that refusal, and must not be read as
  clearing it. That defect still needs its own PIE measurement.
- **The gate is red, and stays red until content is authored.** `editor_validation.py` treats one
  warning as failure, so `passed: false` here is the intended state for a map with no floor regions.
  This is the honest reading of AGENTS.md §2's "treat blocked validation or missing assets as a
  failed preflight" — reported, not papered over by editing content.

Two traps cost a session on this route, both now guarded in the driver so a later run cannot
re-learn them:

1. **An unquoted `.uproject` path is a silent wrong-project run.** PowerShell's `Start-Process`
   joins an `-ArgumentList` array with spaces and adds no quotes, so a project path containing a
   space is split and the engine falls back to its own default project. The gate then reports the
   map as *missing*, and the registry is correctly empty — measured: `Paths.project_dir()` was
   `C:/Users/Taimoor/AppData/Local/UnrealEngine/5.8/`, `assets_by_path("/Game")` was 0 while
   `assets_by_path("/Engine")` was 5250. Every reader agreed the map did not exist, which reads
   exactly like content loss. The driver now compares the editor's project directory against its own
   and reports `status: "wrong_project"` instead of validating anything.
2. **`py` runs before the asset registry has scanned, and `is_loading_assets()` is not the signal.**
   The callback is false *before* the scan starts, so a wait keyed on it concludes "ready" at 0.3s
   and queries an empty store. Waiting must also not block: `time.sleep()` inside the `py` command
   holds the game thread, and the scan needs that thread to tick — a 180-second sleep produced a
   184.7-second scan that finished only once the sleep ended. The driver polls from a slate
   post-tick callback for `get_assets_by_path("/Game")` to become non-empty. With the project
   correctly open this took 1.0s and found 64,330 assets.

### Phase 2 — The engagement gate (ask 1). Behaviour change; needs PIE.

#### Phase 2 as implemented — 2026-09-25

**The gate.** `ATerritoryGuardCharacter::EvaluateTerritoryTarget` gained a refusal between the
faction-War rejection and the first *proactive* allowance, calling a new private helper
`IsFloorSeparationRefused`. The helper's own comment is the contract: *"Nothing is refused that
the player did not already earn the right to be attacked for."* Everything above that point
(personal hostility) and everything below it (Contested, War-in-Claimed, exposure, assault front)
is unchanged in kind — only the proactive half is now floor-scoped.

**Two deviations from the plan above, both deliberate, both required by AGENTS.md §9/§4.1:**

| Planned | Shipped | Why |
|---|---|---|
| `bAllowCrossFloorRetaliation` on the policy | **dropped** | Retaliation is answered *above* the gate, and `FTerritoryGuardBehaviorTemplate::bAllowPersonalRetaliation` already owns "does this guard answer damage at all". A floor-scoped twin would be either permanently-true (§9: an inert flag) or a second authority for one behaviour (§4.1). Pinned by test: the strictest policy plus real `DamagedBy` damage still allows the cross-floor answer, and the definition-level flag still governs it independently. |
| `bRequireResolvedTargetFloor` on the policy | **dropped** | Making an unresolved target floor a refusal would let a player step one centimetre off an authored region and become immune. Inert-allow is the safe default and matches Phase 1's `INDEX_NONE` semantics. Pinned by test: *"A target outside every region is not treated as being on another floor."* |

**Resolution order, now load-bearing:** the guard's floor is its **position** first
(`Registry->GetFloorAtLocation`), falling back to its **post's** `GetFloorIndex()` — so a guard
that patrolled off an authored region keeps its post's floor rather than silently becoming
unseparated. The target's floor is **position only**; unresolved ⇒ allow. The **defender's** floor
decides which policy applies, so one guard always answers to exactly one policy. Per-floor row
(`FTerritoryFloorTemplate::CombatPolicy`) wins over Place default
(`UTerritoryPlaceDefinition::DefaultFloorCombatPolicy`) wins over null.

**Evidence — three legs, the third being the one that matters.** Per the standing rule that a
green test is not evidence until it has been seen to go red, and that a test must be shown to
discriminate the *decision*, not merely the code's existence:

| Leg | Code state | `TerritoryFramework.Guards.Floors` |
|---|---|---|
| Red | gate absent | 21 pass / **5 fail** — all and only floor-refusal assertions |
| Green | gate shipped | **26 pass** / 0 fail |
| Mutation | gate moved *above* personal hostility | 25 pass / **1 fail** — exactly `RetaliationSurvivesSeparation`, nothing else |

The mutation leg is the proof that `RetaliationSurvivesSeparation` pins the gate's **placement**,
which §D1 identifies as the whole design decision. It fails with the intended message: *"Expected
'A guard damaged from the floor below still answers it under Same Floor Only' to be true."*

**Adjacent suites, run one filter at a time** (per the multi-filter-sweep hazard): `Guards` 45,
`Editor` 51, `Contract` 53, `Capture` 12, `Integration` 12, `Floors` 9, `Functional` 8, `AI` 7,
`Hierarchy` 6, `Definition` 4, `SaveLoad` 2. All green.

**One real gap this sweep caught, now fixed.** `TerritoryFramework.Editor.Authoring.AllDataAssetMenusAndFactories`
failed on the new class — a new `UPrimaryDataAsset` needs a named creation factory and an asset
action, which the batch had omitted. Added `UTerritoryFloorCombatPolicyFactory`, the
`Register(...)` action entry under the Combat menu, and registration in the shared
`FTerritoryDataAssetDetails` layout list (without which the asset's numbered `00 Display` /
`01 Separation` categories would be decorative while every sibling honours them). 5/5 green after.

#### Phase 2 — still outstanding

- **PIE measurement** (AGENTS.md §12 lifecycle): floor 0 fight observed, floor 1 not joining,
  floor-1 guard still patrolling (not frozen), guard following the player upstairs and engaging on
  arrival; plus the relative contribution of the three mechanisms in §2.9 by turning
  `NotifyTeammatesToFightRange` down as a control. **Not yet performed.**
- **Dedicated server + two clients.** Not yet performed. The gate is already authority-only (§2.8),
  so clients already answer Neutral; this proves the change did not alter that.
- **Known limitation — floor laundering by movement.** A guard that physically chases onto another
  floor adopts *that* floor's policy, because the guard floor is position-first. The movement-
  leashing phase owns this; it is recorded here so it is not mistaken for a bug in the gate.
- **Known limitation — the user's roof scenario needs content.** "Player lands on the roof and
  fights 2→0" requires the roof to be authored into a floor region. That is authoring, not code.
- **The two mechanisms in §2.9 other than the gate are not addressed by Phase 2:** the teammate
  notification at 3500 cm plain 3D distance, and ungated hearing. The gate stops *proactive*
  engagement; it does not stop a floor-1 guard being *told* about a floor-0 fight. Measuring the
  residual is exactly what the PIE control above is for.

#### Phase 2 — the original test list (all written; all green)

- **Monotonic and directional:** a same-floor target is admitted; a cross-floor target is
  refused; with `SameOrAdjacent`, ±1 is admitted and ±2 is not. ✅
- **Retaliation preserved:** a cross-floor target that has damaged the guard is still admitted. ✅
- **Inert:** a Place with floors but no volumes behaves exactly as before (asserts the *allow*
  cases, not just that nothing crashed). ✅
- **Both funnel entries:** refused through `GetTeamAttitudeTowards` **and** through
  `CanScoreTerritoryCombatGoal`. ✅
- **Failure path:** no authority ⇒ false; missing policy ⇒ `AnyFloor`; null target. ✅
- **Determinism:** the same (guard, target, position) yields the same verdict across repeated
  calls and after a re-index. ✅
- **Post floor as fallback:** a guard off its region keeps its post's floor. ✅

### Phase 3 — Per-node activities (ask 4).

- Replace `ActivityTag` with the typed reference; wire the precedence chain (E2).
- PIE: a guard actually plays the node activity, and returns to patrol afterwards.
- Test: the reference resolves through precedence; a null reference falls back one level.

### Phase 4 — Docs and memory.

- New: this file, kept as the plan of record.
- `Docs/05_Guard_System.md` — the floor policy, the new gate, and the ActivityTag replacement.
- `Docs/21_Definition_Assets.md:186-196` and `Docs/22:88` (checklist item 7) both claim Data
  Validation errors on parent physical values. **[V]** for the claim; it is **not** implemented
  for any of the 22 properties. Correct the claim or implement it — do not leave a doc
  describing a check that does not exist.
- `Docs/38_Options_Guide.md` has no City or District section at all. Add one.
- Memory: update the floor/validator entries; the `ActivityTag` entry becomes "closed".

---

## 6. Second- and third-order consequences

Mapped explicitly, because several of these are the difference between "the gate works" and
"the feature works".

### 6.1 Perception still fires; the goal scores zero

A floor-1 guard still *sees* the fight. The gate makes its goal score 0, and
`Docs/Combat_Activity_Eligibility.md:9` says **zero pauses a goal; a negative score removes
it**. So the expected behaviour is a paused attack goal — but the guard must still run its
**patrol** goal. **Risk: a floor-gated guard freezes in place.** This must be an explicit PIE
assertion in Phase 2, not an assumption.

### 6.2 Movement is a different mechanism from engagement

`EvaluateTerritoryTarget` decides whether to *attack*. It does not decide whether to *walk*.
A notified floor-1 guard can still path toward the floor-0 fight and stand at the top of the
stairs. The user's "attack from floor 0 to 2" is a movement story, so **A2 is required for the
feature to feel complete** and is deliberately a separate plan (§4.1, §6.4). Do not claim
floor-wise combat is done on the strength of Phase 2 alone.

### 6.3 Downgrading the attitude answer has a blast radius

`GetTeamAttitudeTowards` (`:255-278`) returns `Hostile` when the gate passes and `Neutral`
otherwise, so a floor gate makes cross-floor relations read **Neutral** rather than Hostile.
Anything that reads guard attitude is affected — vendor AI decisions, friendly-fire checks,
any HUD enemy indicator **[P] on the exact consumers**. Two known-safe branches: the disguise
path (`:262-271`) returns `Friendly` independently and is unaffected; and clients already
never reach the `Hostile` branch, because `:307` returns false without authority. The Phase 2
test set must include the attitude-visible consequences, not just the boolean.

### 6.4 Per-floor progression already exists — do not rebuild it

The first draft of this plan said progression was missing and would fire all at once. **That was
wrong** (§2.7.1 **[B]**). `DispatchClearedFloors` is called at `TerritoryVolume.cpp:2613`
*before* the whole-Place early return, precisely so a floor clears while other floors still
fight, and `UTerritoryStateTask` already has a `TargetFloor` filter with per-floor satisfaction
rules. Both are tested.

Two consequences:

- **A4 is mostly delivered.** "Clear floor 0, then floor 1 is the fight" is authorable today as
  a floor-filtered Tales objective plus the floor's `FloorClearedEvents`. Do not add a parallel
  progression system — that would be a second authority for floor completion, and `IsCleared()`
  (`TerritoryTypes.h:433-440`) is already the one rule both the objective and the event read.
- **What is genuinely missing is only A1 (engagement) and A2 (movement).** That is a smaller
  feature than the first draft implied, and Phase 2 is correspondingly lower-risk.

One real trap remains, and it is already documented in source rather than fixed:
`TerritoryVolume.cpp:2957-2964` records that a post the Place's `DesiredGuardCount` never reaches
"pins its floor uncleared, which stops that floor's FloorClearedEvents from ever firing". A
floor can be authored correctly, staffed correctly, and still dead because the *whole-Place*
target was set too low. That belongs in Phase 1's validator work — a floor whose row is
unreachable given the Place's own `DesiredGuardCount` deserves the same warning as one with no
deployable post.

### 6.5 Attack tokens

Narrative's tactical attack tokens are 3D. A gate that stops a guard attacking must also stop
it **holding a token**, or floor-1 guards will squat on the limited token pool while
contribution-zero. **[P]** — confirm whether token acquisition happens after goal selection
(and is therefore already covered) or independently. This is a Phase 2 verification item.

### 6.6 Save/load, replication, World Partition

- **Save:** no new persisted state. Floor identity is authored; a guard's floor is *derived*
  from position at query time, never saved. This is deliberate — saving a guard's floor would
  create a second authority for it and a migration burden. Floor volumes must carry
  editor-baked stable GUIDs (AGENTS.md §7), as guard posts already do.
- **Replication:** the gate is server-only and introduces no replicated field. An unresolved
  (streamed-out) floor volume must fail **inert**, not fail-closed, so a stream-out cannot
  silently disarm a garrison.
- **World Partition:** floor volumes stream with their level. Resolution must not cache across
  a stream event.

### 6.7 Two clients

Server authority is unchanged. The one thing to watch is that a client's local
`GetTeamAttitudeTowards` never returns `Hostile` anyway (§2.8), so the gate adds no new
divergence — assert that rather than assume it.

---

## 7. Required tests, mapped to AGENTS.md §10

| §10 requirement | Where it lands |
|---|---|
| 1. Native behavioural | Phase 2 gate tests |
| 2. Save/load | Phase 1/2: no new save state — assert floor volumes round-trip by GUID and that a guard's floor is never persisted |
| 3. Server authority | Phase 2: no authority ⇒ refused, on both funnel entries |
| 4. Two-client / late-join | Phase 2 PIE |
| 5. World Partition / load order | Phase 1: stream-out ⇒ `INDEX_NONE` ⇒ inert |
| 6. Blueprint contract | Phase 2: `DecideFloorEngagement` override; Phase 3: precedence chain |
| 7. Narrative integration | Phase 3: the node activity is actually played |
| 8. Failure path | Each phase |
| 9. Determinism | Phase 1 resolution, Phase 2 verdict stability |
| 10. Regression for the exact bug | Phase 2: floor 1 does not join a floor 0 fight — the reported symptom, asserted directly |

Monotonicity for this feature (the AGENTS.md §10 pattern, adapted):

```text
More floors than the policy admits must never increase engagement.
A guard's engagement must never increase when the target moves to a floor further away.
Unresolved floor geometry must never reduce a guard's engagement below today's behaviour.
```

---

## 8. Open decisions for the user

1. **§4.2 B1 or B2** — move `Floors` to the Place class (structurally correct, ~6 mechanical
   call sites, needs a BP migration note) or the three-part lock (3 small edits, zero BP
   impact, leaves the property present-but-always-empty on aggregates). My recommendation is
   **B1**.
2. **§2.5** — resolve the aggregate floor-beat contradiction in the same batch, or leave it and
   keep the base-class design? B1 forces this; B2 does not.
3. **§6.2** — is movement leashing (A2) in scope now, or is engagement-only acceptable as a
   first release? The feature will feel incomplete without A2.
4. **§6.4 — struck as a question.** Per-floor progression already exists and is per-floor
   (§2.7.1). The only decision left is whether Phase 1 warns about the "whole-Place target too
   low pins a floor uncleared" trap, or whether that stays a source comment. My recommendation
   is to warn.
5. **§4.5 E1** — typed DataAsset reference per node (recommended) or keep the tag and add a
   tag→activity library asset?
6. **§6.3** — are there HUD or vendor systems reading guard attitude that a cross-floor
   `Neutral` would visibly change?
7. **§C5** — comment the counter-attack height tolerance as scoring-only (recommended), or
   migrate the planner onto the resolved floor? The first is one line and no behaviour change;
   the second removes the duplicate concept outright at the cost of touching a working scoring
   curve that is outside this feature.

---

## 9. References

Source (plugin), all **[V]**:

- `Source/TerritoryFramework/Public/Core/TerritoryDefinition.h`
  — `:79-96` patrol node struct (`ActivityTag` at `:95`); `:100-169` guard behaviour template;
  `:180-208` floor template (`DesiredGuards` doc `:195-200`); `:211-228` guard post template
  (`FloorIndex` doc `:222`); `:454` base class; `:484-487` derived parent tag; `:628` `Floors`
  declaration; `:699`/`:702-705`/`:707-710`/`:712-715` floor accessors; `:736-779` Place;
  `:783` District (`Places` `:792`, `bIsCapital` `:796`); `:814` City (`Districts` `:827`).
- `Source/TerritoryFramework/Private/Core/TerritoryDefinition.cpp`
  — `:29-109` `NormalizeAggregateDefinition` (`GuardPosts.Reset()` `:100`);
  `:122-213` `ApplyToTerritory` (zero-gate `:158-181`); `:256-264` `GetFloorGuardPostCount`;
  `:304-315` `PostLoad`; `:388-437` floor validation (`:388` guard, `:408` duplicate
  `continue`, `:417-424` quota error); `:439-491` hierarchy validation; `:512`, `:541` ctors;
  `:520-527`, `:549-558` `RefreshHierarchyLinks`.
- `Source/TerritoryFramework/Private/Core/TerritoryGuardCharacter.cpp`
  — `:255-278` `GetTeamAttitudeTowards`; `:280-284` `CanEngageTerritoryTarget`; `:286-388`
  `EvaluateTerritoryTarget` (result lambda `:295-306`, authority gate `:307`, personal
  hostility `:351`, War rejection `:353`, assault front `:356`, stealth `:363`, Contested
  `:376`, War-in-Claimed `:383`); `:223` post assignment; `:883` `DOREPLIFETIME`.
- `Source/TerritoryFramework/Public/Core/TerritoryGuardCharacter.h` — `:80`
  `CanEngageTerritoryTarget`; `:154` `OwningTerritorySpawnPoint`.
- `Source/TerritoryFramework/Private/Core/TerritoryBlueprintLibrary.cpp:376-402` —
  `CanScoreTerritoryCombatGoal` → `CanEngageTerritoryTarget`.
- `Source/TerritoryFramework/Private/Core/TerritoryVolume.cpp` — `:784` base-class floor
  comment; `:787-793` cleared-event clone; `:2331` `RegisterDefender` aggregate return;
  `:2582`, `:2613` `DispatchClearedFloors` chain; `:2993-2999` floor snapshot; `:3144`, `:3150`
  spawn aggregate returns.
- `Source/TerritoryFramework/Public/Core/TerritoryGuardSpawnPoint.h` — `:94-100` `ActivityTag`
  tooltip; `:150` `GetFloorIndex`; `:173` `FloorIndex` (`Transient`).
- `Source/TerritoryFramework/Private/Core/TerritoryGuardSpawnPoint.cpp` — `:282`, `:286`
  floor write; `:324` the only `ActivityTag` copy.
- `Source/TerritoryFramework/Public/Subsystems/TerritoryRegistrySubsystem.h` — `:93`
  `GetTerritoryAtLocation`; `:97` `GetTerritoriesAtLocation`; `:101` `GetTerritoriesInBox`.
- `Source/TerritoryFramework/Public/Core/TerritoryTypes.h` — `:370` `FTerritoryFloorSnapshot`;
  `:444` `FloorIndex`; `:508` `Floors`.
- `Source/TerritoryFramework/Public/Combat/BTService_TerritoryAssaultPermission.h` — precedent
  for a Territory-owned gating service (`:32-34` guard-owning-territory preference).
- `Source/TerritoryFrameworkEditor/Private/TerritoryFrameworkEditor.cpp:317-340` — the 22-entry
  hide-list (`GuardPosts` `:332`), gated by `bContainsNonPlace` (`:238`, `:251`, `:312`).
- `Source/TerritoryFrameworkEditor/Private/DataValidation/TerritoryDataValidator.cpp` — my
  level checks: `CheckPatrolContainment` `:2473-2513`, `CheckGuardDeploymentFeasibility`
  `:2515`, floor rollup `:2653-2690` (`HasAuthoredFloors` `:2657`, `Floors` `:2682`).
- `Source/TerritoryFramework/Private/Tests/TerritoryFloorStagingTests.cpp` — `:281-324`
  snapshot/reserve tests; `:399`+ validation cases.
- `Source/TerritoryFrameworkEditor/Private/Tests/TerritoryGuardResponseTests.cpp:168-260` —
  the personal-hostility and policy assertions the floor gate must not regress.

Floor-consumption map, all **[B]** — cited by the sweep that produced §2.7.1, §2.9's
confirmation and §2.12, and **not** re-read by me:

- Definition side: `TerritoryDefinition.cpp:237-243` `FindFloor`; `:245-254` `GetFloorTemplate`;
  `:256-264` `GetFloorGuardPostCount`; `:390-437` `IsDataValid` (negative `:397-402`, duplicate
  `:403-409`, quota `:416-424`, undeclared post floor `:427-435`).
- Actor side: `TerritoryGuardSpawnPoint.h:149-150` `GetFloorIndex`; `:167-173` the transient
  `FloorIndex`; `TerritoryGuardSpawnPoint.cpp:282-286` the single writer.
- Snapshot: `TerritoryVolume.h:808-809` `GarrisonSnapshot` (`ReplicatedUsing`); `.cpp:2877-2891`
  `RefreshGarrisonSnapshot` (publishes only on inequality, `:2885`); `.cpp:2916-3026`
  `BuildFloorSnapshots` — `:2929-2949` the accumulators, `:2953` the actor floor read, `:2993`
  the undeclared early-out, `:3015` the `DesiredGuards == 0` implementation, `:3024` per-floor
  `bCountsKnown`; `TerritoryTypes.h:433-440` `IsCleared`; `:442-453` `operator==`.
- Dispatch: `TerritoryVolume.cpp:2582` → `:2585-2613` `TryCompleteDefenderDefeat` (dispatches
  floors at `:2613` **before** the whole-Place return); `:2658-2706` `DispatchClearedFloors`;
  `:2708-2715` `AnnounceDefenderSpawned`; `:2717-2755` `DispatchFloorClearedEvents`; `:3372-3378`
  the spawn call site; `TerritoryGuardSpawnPoint.cpp:709-715` the reserve-abandonment entry.
- Story layer: `TerritoryStateTask.h:75-80` `TargetFloor`, `:113` `IsFloorFiltered`;
  `TerritoryStateTask.cpp:134-143` `FindTargetFloor`, `:153-175` `IsObjectiveSatisfiedBy`,
  `:272-283` `EvaluateCurrent`, `:334` `HandleAllDefendersDefeated`, `:341-363`
  `HandleGarrisonChanged`, `:404-416` the authored objective text.
- Blueprint surface: `TerritoryBlueprintLibrary.h:130-169` (three floor queries),
  `TerritoryVolume.h:552-553` `GetGarrisonSnapshot`, `:869-870` `OnFloorCleared`,
  `TerritoryTypes.h:911-935` the two floor delegates.
- Persisted state: only per-post — `TerritoryGuardSpawnPoint.h:493-511` (`CurrentReserveCount`,
  `PendingReserveSpawns`, `SavedActiveGuardCount`, `SpawnPointGUID`). **Nothing per-floor is
  `SaveGame`**, asserted by `TerritoryFloorStagingTests.cpp:779-790`.
- Transient state: `TerritoryVolume.h:943-944` `RuntimeFloorClearedEvents`; `:977`
  `FloorsAnnouncedInDefeatCascade` (**a bare member, not a `UPROPERTY`** — never saved or
  replicated).
- Second height-band concept: `TerritoryCounterAttackProfile.h:243`;
  `TerritoryCounterAttackSubsystem.cpp:2211`, `:2218`, `:2848`, `:2871-2874`.
- The 14-site change checklist this plan's §4.2 table is derived from, and the two test suites:
  `TerritoryFloorStagingTests.cpp` (10 tests, incl. the property-contract test `:727-790`) and
  `TerritoryFrameworkEditor/Private/Tests/TerritoryFloorEventTests.cpp` (11 tests) with
  `TerritoryFloorEventProbe.h`/`.cpp`.

Vendor (read-only reference, never edited):

- `Plugins/NarrativePro/Source/NarrativeArsenal/Public/Settings/NarrativeCombatDeveloperSettings.h:49`
  and `.../Private/Settings/NarrativeCombatDeveloperSettings.cpp:26` —
  `NotifyTeammatesToFightRange = 3500.f`. Read only by vendor Blueprint assets
  (`GoalGenerator_Attack`, `Goal_Attack`, `BTS_Attack`, `GoalGenerator_Flee`).

Docs in this repository:

- `Docs/05_Guard_System.md` — `:51-85` combat policy gates; `:87-113` patrol; `:114-146`
  reserves; `:173-223` guard-post definitions, override precedence `:192-201`, placement and
  patrol overlap `:203-215`; `:242-258` attack-goal lifecycle.
- `Docs/22_Hierarchy_Availability_and_Unlocks.md` — `:14-24` the authority-by-layer model;
  `:73-74` "Guard posts are Place-only"; `:80-88` designer checklist (item 1 "physical
  settings only on Place Definitions", item 7 "parent physical values are errors").
- `Docs/Combat_Activity_Eligibility.md` — `:1-13` the goal-scoring gate contract; `:16-23`
  what `CanScoreTerritoryCombatGoal` covers; `:25-33` what Narrative still owns.
- `Docs/21_Definition_Assets.md:186-196` — claims Data Validation on parent physical values
  that is **not** implemented.
- `Docs/38_Options_Guide.md` — no City or District section.
- `Docs/FLOOR_WISE_COMBAT_PLAN_2026-09-25.md` — this file.

---

## 10. Not yet verified — do not treat as fact

Every item here is a **[P]** above and must be checked before the phase that depends on it.
Items resolved by the floor-consumption sweep are struck through with the answer, so they are
not re-opened.

1. ~~**§4.5** — the exact Narrative Pro type a per-node activity should reference.~~ **Answered
   — and it changed the design.** Narrative Pro has **no** tag-driven activity selector
   (`ActivityGroup::GroupTag` is inert; the group loop at `NPCActivityComponent.cpp:261-267` is
   commented out), so a tag→activity lookup would be a second activity authority. The supported
   extension point is an instanced `UNPCGoalItem` resolved into
   `UNPCActivityComponent::AddGoal`, which `UTerritoryPatrolGoal` +
   `InitializeTerritoryPatrolGoal()` already demonstrates end to end, and selection is a score
   competition on a `RescoreInterval` timer. Full consequences in §4.5 E1a — including that
   `UNPCActivity::bIsInterruptable` is inert and cannot arbitrate.
2. **§6.5 — narrowed, still open.** The sweep confirms `Private/Combat` contains **no** floor
   logic, so the 3D attack-token pool is definitely floor-blind, and the token layer is
   downstream of the attitude gate, so a refused guard never reaches it (§4.4 D1a). What is
   still unverified is *ordering*: whether a token is acquired before or after the goal score.
   The ungated-hearing half is **answered**: hearing cannot be made floor-aware at the sense
   layer, and the notification that provokes it cannot be suppressed without editing vendor
   content (§4.4 D1a). **Phase 2 verification item — narrowed to the ordering question only.**
3. **§2.11** — whether any Blueprint reads `FTerritoryFloorTemplate::DisplayName` or the patrol
   node's `ActivityTag`. Python reflection cannot see struct members through an asset, so this
   needs a BP-level audit, not a disk probe. **Still open.** One data point now exists:
   `Content/HOPTRENDY/Blueprint/BP_FloorCheck.uasset` was read at the byte level during B1 and
   its strings reference `TerritoryFloorTemplate:DisplayName` alongside `FloorIndex`,
   `DesiredGuards` and `FloorClearedEvents` — so at least one project graph does bind the
   `DisplayName` member. It is **not** evidence of a *read* that changes behaviour; that still
   needs the graph inspected in the editor.
4. ~~§4.2 — the remaining base-class floor readers.~~ **Answered, and now implemented.** The
   complete reader/writer set is the 14-site checklist in §9's floor-consumption block; the only
   definition-side readers needing a cast were `TerritoryVolume.cpp:2993`, `:788-794` and
   `TerritoryDataValidator.cpp:2657`, `:2682`. Everything else reads the **snapshot**, not the
   definition. All three casts are in, and the reader set is now empty of base-pointer reads —
   see §4.2 "B1 as implemented".
5. **§6.1 — still open, and still the main risk in Phase 2.** That a paused attack goal does not
   also stop the patrol goal. This is a PIE assertion, not a code reading. A frozen garrison
   would be a worse bug than the one being fixed. **Phase 3 makes this sharper**: since a
   per-node activity resolves to a goal that *competes on score* rather than replacing anything
   (§4.5 E1a), "activity paused but patrol goal still winning" is now a concrete shape to test,
   not a hypothetical.
6. **§6.3** — the HUD/vendor consumers of guard attitude. **Answered for the AI route** (§4.4
   D1a): exactly one attitude path exists for a Territory guard's AI
   (`TerritoryGuardCharacter.cpp:258`), plus the Blueprint `CanScoreTerritoryCombatGoal`, and
   both pass through the gate. What remains unverified is the **non-AI** consumer — any
   HUD/UI that reads attitude directly for a *display* purpose, which would change appearance
   without changing behaviour. Lower risk than originally stated, and Phase 2 is where it shows.
7. ~~§6.4 — the timing of per-floor cleared-event dispatch.~~ **Answered and corrected**
   (§2.7.1): dispatch is per-floor and fires while other floors still fight. The remaining part
   of item 7 — that a low whole-Place `DesiredGuardCount` can pin a floor uncleared forever — is
   carried forward as Phase 1 validator work (§6.4).
8. **§2.12 — the coexistence decision.** Whether `TerritoryCounterAttackProfile::SameFloorHeightTolerance`
   should gain a "scoring only, not membership" comment (§C5) or be migrated onto the resolved
   floor. My recommendation is the comment, but the planner's four call sites have not been read
   in full by me. **Phase 1 item.** The recon strengthened the case for the comment: the field is
   read **only** at `TerritoryCounterAttackSubsystem.cpp:2211`, `:2218`, `:2848`, `:2871-2874`,
   i.e. it is a reserve-staging presentation setting, and it is under
   `bUsePlayerRelativeReserveStaging` — so it is even narrower than §2.12 assumed, and the risk
   of it being mistaken for a membership answer is real precisely because the name is so
   suggestive.
