# State, stealth, reserve and quest audit — 2026-09-11

This audit follows the current TDA source and the actual HopDistrictTest assets.
The user asked for stealth and other factors to be considered together before
changing when combat or a contest begins. It is not a completed modular-policy
refactor. Narrative Pro source and assets were not changed.

## What the current states mean

| State or value | Current meaning | Owner of the value |
|---|---|---|
| Place Unclaimed | No owner, no contesting faction, zero control progress | TerritoryVolume |
| Place Claimed | Valid owner, full owner control, no contesting faction | TerritoryVolume |
| Place Contested | A registered confrontation or capture is active; ownership has not necessarily changed | TerritoryVolume, through ControlSubsystem |
| District/City Contested | Expected children are not all available and claimed by one faction | Hierarchy reducer |
| Locked availability | Gameplay is unavailable locally or through an ancestor; political ownership remains | TerritoryVolume and hierarchy |
| Undetected / Suspicious / Exposed | Awareness of one particular player in one Place | ControlSubsystem |
| Friendly / Neutral / Hostile | Combat attitude using Narrative identity and the Territory diplomacy bridge | Narrative GameState and Territory diplomacy |

The old Locked political enum is a migration value. Current availability is a
separate field. HopDistrictTest initially shows Contested aggregate parents while
its owned Places are locked. That does not prove that anyone is fighting there.

Story bounds register a living, non-owner player as an infiltrator when stealth
is enabled. With stealth disabled, or after exposure with a contest-capable scope,
they register a **contester**. A story contester contributes no automatic capture
pressure. Multiplayer flag capture uses the separate capture-participant path.

For each participating faction, capture progress currently increases by
`DeltaTime * CaptureProgressPerSecond` only with living capture participants,
no living defenders, and allowed diplomacy. Otherwise it decays by
`DeltaTime * CaptureProgressDecayPerSecond`, clamped to 0–1. More participants
do not multiply the speed. The leader is chosen by progress, then participant
count, then faction tag order. Completion revalidates before committing ownership.
Quest overrides, availability and state entry conditions can block transitions.

## Confirmed findings

### 1. Initial defeat-task check ignored queued reserves — fixed in this batch

In real PIE, three defender deaths produced:

```text
Active 0 / Desired 3 / Maximum 7 / Reserve 7 / Pending 3
AllDefendersDefeated task preview: true (incorrect)
```

After replacement deployment:

```text
Active 3 / Desired 3 / Maximum 7 / Reserve 4 / Pending 0
AllDefendersDefeated task preview: false
```

The normal Volume defeat event already waits for pending deployments. The task's
initial evaluation did not. It now checks the existing replicated garrison
snapshot too. Unused, unqueued reserves do not block the task. No signature,
save field, replicated field or Blueprint migration is introduced.

The existing independent-post regression now also exercises this task after real
Native death callbacks, Native pending-post save/load, a replacement owner actor,
client snapshot queries, cancellation, missing targets and mismatched tags.

Rebuilt HopDistrictTest verification also started the real NQ_CaptureBlacksmith
quest during the three-replacement delay. It stayed on QuestState_0 with the
preview false at start, 1, 4, 10 and 20 seconds. Three replacements returned and
the reserve fell from seven to four. See `VerifiedQuestReserve.json`.

### 2. Reserve actors can have perception but no attack goal — reproduced, open

All three replacements had completed Native appearance loading and were allowed
to engage the player. Their activities remained patrol/return during the first
20 seconds. Later inspection found player sight on two guards but no attack goals.
Calling the existing safe parent-perception refresh created Native attack goals;
all three subsequently selected the existing melee activity.

This proves a missed perception-to-goal update in the observed spawn lifecycle.
It does not yet identify the precise lost callback or justify polling fake sight
every tick. The controller is correctly derived from Native's Blueprint; a failed
controller-parent cast is not the explanation. The test player was protected by
Native invulnerability; goal creation succeeded with that protection still active.

Next trace: initial perception delivery, generator binding, definition readiness,
and Territory attitude changes. Repair the existing Native adapter at the proven
boundary and verify sight loss, fresh reserves, restored NPCs and late joins.

### 3. Anonymous clues can identify the player — reproduced, open

Two direct Corpse evidence reports with `ConfirmedIdentity=false` changed
Claimed to Contested and exposed the player, with zero confirming observers.
The current project profile has Corpse Suspicion about 0.9744. The code exposes
on accumulated suspicion reaching 1 regardless of whether the evidence identifies
the source. Exposure also compromises the disguise and cancels configured stealth.

That conflicts with the documented anonymous corpse/distraction behavior. Keep
anonymous alarm strength distinct from confirmed identity. Repeated noise may
intensify a search; it should not silently identify a hidden player unless an
explicit authored rule allows it. Test multiple guards reporting the same clue.

### 4. Quest rules can prevent ordinary defenders from retaliating — reproduced

An ordinary guard currently needs its Place Contested **and** the relevant factions
at War. Native's personal Hostiles override does not bypass that Territory gate.
There is a separate exception for a physical assault approaching its defence front.

Blacksmith's active quest pauses State Rules/Events. Its explicit Declare War
event starts on the capture branch, after the defeat-defenders branch. In a
controlled test with War removed, an exposed player and confirmed damage still
left all three guards unable to engage. The fresh map already had War, so this
does not explain every idle guard in the default scenario.

Local self-defence, a Place contest, and global diplomacy need separate authored
responses. Do not solve this by making every hit declare global war or bypass
peace/alliance policies. Native identity and the diplomacy bridge remain owners.

### 5. Other state/stealth gaps established in source — not yet fixed

- Local Alarm promises investigation, but immediate exposure skips investigator
  assignment and also skips contest registration. The ordinary guard combat gate
  then rejects the player while the Place stays Claimed.
- Territory Conflict and Faction War scopes both register a contester. State Events
  decide diplomacy in both cases; the scope alone does not declare War.
- The perception adapter rejects player sources outside the Place bounds before
  processing damage or hearing. This matters for distant shooters.
- Clearing exposure resets awareness but does not itself remove an existing
  contest registration. Bounds reconciliation does not release it merely because
  the player became hidden again. Registration ownership needs careful treatment
  so a stealth reset cannot erase a separate multiplayer capture participant.
- Sight suspicion uses a fixed quarter-second amount per report/observer. Native
  perception callbacks and the refresh timer can both report sight. This is not a
  pure elapsed-time rate and needs duplicate-report and multi-observer tests.
- Capture eligibility/progress checks living defenders, while the defeat event
  also waits for pending reserves. A long reserve delay needs a regression proving
  whether capture can finish during that gap. No capture-policy change is made here.
- The initial zero-defender task query also needs a separate missing-post/load-order
  audit. The queued-reserve fix does not prove that an unloaded post is defeated.

## Reserve events and NarrativeDataTask

Automatic reserves already belong to each Guard Spawn Point: finite inventory,
one active slot, saved pending requests, spawn delay, bounded placement retries
and owner-change policy. The live test spent exactly three reserves for three
new guards. An empty patrol route can intentionally hold a guard at its post.

There is no built-in Territory Narrative event for changing automatic reserve
deployment or explicitly deploying an enemy owner's reserves. Set Garrison Target
is a staffing command, not a reserve-wave event. Add a thin, server-only event
around existing post/Volume commands with an explicit Territory, optional owner
faction filter, finite count and a verified result. Do not make another spawner.

Native `NarrativeDataTask` is a named record plus argument. A producer must call
`CompleteNarrativeDataTask` on the correct authoritative Tales component.
`MasterTaskList` stores normalized `taskname_argument` counts. Native's internal
record function no longer advances quest branches itself. Native also supplies
`BPT_CompleteDataTask` and `NC_HasCompletedDataTask`. The Blueprint task binds the
Tales data-task delegate, adds one progress for a matching notification, and can
read past counts when Retroactive is enabled. No automatic Territory producer for
the user's new task was found. For Territory state, use the existing Territory
task wrappers, which listen to the actual Volume/registry delegates.

The inspected Native `BPT_CompleteDataTask` End Task graph calls **Unbind All Events**
on the shared Tales delegate. This can disconnect other active listeners. It also
adds one progress per live notification even when the record call writes a larger
quantity. These are additional integration findings requiring parallel-task and
bulk-quantity regressions before story use. Extend a Territory-owned task adapter
around Native's public records/delegates; never patch the vendor Blueprint.

Native quest state/branch Conditions arrays are not evaluated automatically in
the inspected runtime. Territory Cascade Recipes add Wait For Narrative Conditions
tasks for that reason. The hand-authored Blacksmith start state has a Territory
condition but its first branch has only the defender task. Audit that graph with
the existing condition-task adapter before relying on its displayed condition.

## Modular implementation plan

Extend the existing Stealth Profile and State Config, keeping one authority per
value. A useful rule evaluates these factors in this order:

1. Server, actor lifetime, availability, owning Place and quest policy.
2. Real Narrative faction, perceived disguise faction, treaty and explicit faction tags.
3. Evidence type, strength, age, location, confirmed identity and current sight.
4. Native Stealth Rating, invisibility, disguise quality/clearance and state override.
5. Response: ignore, investigate, local defence, register contest, or run an explicit
   diplomacy/story event. These are distinct outcomes, not one numeric threshold.
6. Capture mode, living defenders and finite pending defenders before ownership progress.

Provide an easy-English reason for the chosen outcome, using a read-only evaluation
result shared by debug UI and Narrative conditions. Do not add another faction,
capture, perception, quest or save subsystem. Keep current behaviors available as
explicit profile choices; introduce safer stealth defaults with documented migration.

Required scenarios: hidden entry; partial sight; confirmed sight; unseen shots and
corpses; damage from outside; accepted/burned disguise; one exposed and one hidden
player; peace/alliance; quest-controlled combat; reserve arrival/exhaustion;
save/load; actor streaming; and a joining client. Guards must retain Native combat
activities and attack-token limits throughout.

## Evidence and remaining release gates

Local evidence: `Saved/Verification/20260911_StateReserveAudit/` in TDA.
`AnonymousEvidence.json`, `ReserveSnapshots.json` and `QuestDiplomacyProbe.json`
contain the controlled observations. The early `LiveSnapshots.json` rows used a
dead player and are not valid stealth-entry tests; subsequent probes assert a
living, explicitly controlled pawn.

Both engines pass all 307 automation tests: UE 5.8 has 285 clean passes and 22
passes with warnings; UE 5.7 has 283 and 24. There are no failures or skipped tests.
All six Editor/Development/Shipping builds pass. Validation checks 245 assets and
compiles 147 Blueprints with zero errors and eight existing warnings. The added
asset is the user's NewQuestTask draft, restored from its captured default fields
and saved after it was not retained across the editor level reload.

The UE 5.8 cook/package and 60-second Development Game server-mode smoke test pass
with exit zero. This is not a compiled TDAServer test. No new networking fields
were added; a fresh two-client gameplay run is still required for the broader
policy changes. The map, Blacksmith definition, stealth profile and existing save
slots were unchanged by the controlled probes.

The cold AlMalik appearance load, post-load behaviour-tree defeat crash, and
returning-client World Partition fixture remain open from September 10. This
audit does not certify city roads, lighting, release artifacts or Act 1 story work.
