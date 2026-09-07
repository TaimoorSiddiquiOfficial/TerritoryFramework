# Conditional Territory dialogues — batch 42

## Authoring and examples

The project examples are normal, editable Narrative Dialogue Blueprints in
`/Game/TerritoryFramework/Dialogue/Retake`:

- `DBP_BlacksmithRetakePlanning`: 41 nodes for previous ownership, treaties,
  district defence, district control and city control, with repeatable questions.
- `DBP_BlacksmithRetakeHandover`: 11 nodes for a returning faction, generic
  capture, refusal, verified ownership, and conditions changing during the offer.
- `DA_Situation_Blacksmith`: the shared Place and requesting-faction binding.
- The two `DA_Recipe_*` assets retain the editable authoring rows.

These examples use `NPC_BlacksmithOwner` as their Narrative speaker. They are
ready to assign to a project interaction, quest event or story-owner dialogue
override. The existing customized Blacksmith handover graph is preserved. No
Act 1 quest, reward, forced diplomatic change or new persistent map NPC is authored
by this batch.

For another Place, duplicate the situation profile and recipe, change the
profile's Territory tag, assign it to the recipe's conditions, capture-eligibility
conditions and capture events, and change the speaker and project dialogue text.
Call `TerritoryDialogueEditorLibrary.CreateDialogueFromRecipe` with a **new**
`/Game/` asset path. Save the resulting Blueprint, then edit its Native graph.
The recipe does not remain a runtime dependency of that graph. Rebuilding a
recipe never overwrites an existing dialogue. The project creation script is
`Scripts/Territory/create_retake_dialogues.py`.

`Territory Situation Condition` works anywhere Narrative accepts a condition,
including dialogue nodes, events and quest branches. Several conditions on one
node are AND. Alternative NPC replies provide OR; Native's smaller X/Y position
selects the first valid branch. Keep the unconditional fallback last. Recipes
reject duplicate IDs, missing replies/speakers, unreachable nodes, invalid
profiles and NPC-only loops. Loops through player choices are supported.

The project Narrative dialogue menu supports four keyboard choice bindings.
The planning example keeps at most four choices on screen and uses a submenu
for District/City questions, avoiding an out-of-range Native menu binding.

## Data meaning

| Query | Meaning / authority |
| --- | --- |
| Previously owned | Requesting faction exists in the Place's verified former-owner tags. `ATerritoryVolume` records successful ownership changes. |
| Retake needed | Previously owned and currently held by another faction or unclaimed. |
| Already owned | Current owner exactly equals the resolved requesting faction. |
| Relationship with owner | Current Territory diplomacy, using Narrative faction tags as identity. |
| Faction Place count/share | Securely Claimed Places in the selected District or City. It does not count aggregate District ownership as another holding. |
| Dominant faction | Existing hierarchy reducer's strict majority of available Places. Ties and fragmented control have no dominant faction. |
| Relationship with dominant | Current relationship to that majority faction. Self is treated as allied; put the own-faction branch first. |
| District defence power | Existing counterattack scheduler's target-owner defence front: active guards, authorized reserve, guard quality, fortifications and allied support. It is planning information, never a capture roll. |

Locked Places and locked District subtrees do not contribute to holdings.
Contested/unclaimed Places remain in the denominator but contribute no secure
owner. Parent links are exact authored tags, not tag-string prefixes. Missing
children or duplicate identities make the directory assessment unknown. An
unavailable target or ancestor cannot report an available defence front.

Defence power is known only on the server when the relevant same-owner Places
match the scheduler's actual loaded front and stable identities. Incomplete
World Partition loading does not turn missing defenders into zero power.
Clients and unloaded Places can still read holdings/history from WorldState;
Native selects and sends actual conversation branches from the server.

The example's strong/weak boundary is **10 defence power**, an editable example
threshold, not a new global balance rule. Use the profile's `Inspect Territory
Situation` Blueprint query to inspect the report and unknown-data reason.

## Handover and story policy

Multiplayer retains the existing flag-based automatic capture flow. Planning is
optional; conversation never gates that progress. After a successful recapture,
the handover example's `Recaptured` branch is a reaction with no ownership event.
Only a Place explicitly using `bStoryCaptureFromBounds` offers a handover in these
new examples. Their eligibility conditions enable `bRequireStoryCaptureFlow`
both on the offer and on the event, so switching back to flag capture invalidates
a cached offer. The flag actor and capture subsystem remain the capture authority.
This follows the Place's authored mode, allowing story capture in cooperative
quests without imposing it on domination-style multiplayer. The new option
defaults false to preserve older authored eligibility conditions.

`TerritoryCaptureEligibilityCondition` and `TerritoryCaptureEvent` accept the
same optional situation profile. Existing assets with no profile keep their
authored target/faction fields. Eligibility checks living defenders, availability,
diplomacy, existing capture policy, and outgoing/incoming faction state conditions
with the exact Tales participant context. Existing automatic-capture quest
suspension remains a gate of this eligibility condition; an explicit quest-owned
transition can use the existing unforced Capture Event directly with its own
authored quest conditions. Do not force capture merely to bypass that gate.

Native may cache an offered player response. The handover event therefore has
its own eligibility condition and executes at the **end** of the selected player
line. The existing capture subsystem validates and commits the ownership change.
Only then does a conditional NPC reply confirm current ownership. A ceasefire,
new defender or unmet faction/story condition takes the failure reply. The
example sets `bForceCapture=false` and never writes ownership directly.

Attach project-owned Native reward events to a verified outcome and use faction
conditions plus the existing quest/state replay policy where required. No reward
or income account is created by a situation query.

## Persistence, compatibility and audit fixes

`FTerritoryOwnershipData.FormerOwningFactions` is a saved field written only by
the existing atomic ownership commit. Caller-proposed history cannot fabricate
or erase it. WorldState's existing saved/replicated capture summary projects the
same field for streaming and joining clients. There is no separate history
subsystem. Same-owner changes do not append history.

Older saves contain no verified former-owner history. They start empty; this
does not assert that the faction never owned the Place. Subsequent verified
losses are recorded. Loading an earlier campaign replaces later history rather
than merging campaigns. Narrative's actor load callback now republishes the
ownership summary even when garrison counts did not change, fixing stale
ownership/history in the directory after in-place reload.

The recurring assault scheduler also rejects `MAX_int32` occurrences before
incrementing or reserving another evaluation cycle. Unlimited recurrence cannot
wrap into a negative or fresh occurrence. The saved decision and roll remain
unchanged. All other finite force, diplomacy, autonomous/immediate activation,
casualty and existing capture policies retain their authorities. The full
assault lifecycle trace remains in [batch 41](CASUALTY_DRIVER_LOSS_2026-09-07.md).

No Narrative Pro source or content is changed. New reflected fields require a
full rebuild and editor restart; existing Blueprint pins are preserved.

## Verification

Evidence is collected under `Saved/Verification/20260907_ConditionalDialogue`.
The native tests exercise holdings and malformed directory data, real Narrative
actor save/load, an earlier-save rollback, streamed-out history, current treaty
branches, known/unknown power, faction state conditions, cached-offer rejection,
server authority, actual capture-event execution, recurrence boundary restore,
and Native editor graph generation/compilation.

Verified source and live-editor results:

- `Build_Editor_FlagGate.log` and `Build_Game_FlagGate.log`: UE 5.8.2 Editor,
  runtime, UHT and Development Game builds succeeded.
- `AllTests_FlagGate/index.json`: 279 passed (262 clean, 17 with expected fixture
  warnings), zero failures or not-run tests. The new story-flow regression also
  rejects a cached offer after switching a Place back to automatic capture.
- `AssetValidation_Batch42.json`: 77 Blueprints compiled, 128 assets checked,
  zero errors/invalid assets. Four existing presentation warnings remain: two
  prototype owner appearances and Farm dialogue's missing shot/zero blend-out.
- `Live_NativeRPC.json`: seven live cases on a listen server plus two clients:
  war and alliance owner branches, City allied-control submenu, known low district
  defence, cached-offer ceasefire rejection, successful unforced story retake,
  and post-recapture reaction. The server and requesting client agree; ownership
  and former-owner history reach both clients, and the other client has no dialogue.
- `FinalRecipes.json`: 41/11 recipe nodes; at most four/two player choices.
  `NarrativeSourceHashes.json`: all 741 vendor source files match the installed
  Fab package, with no differences.
- `CookStage_WithExamples.log`: Windows HopDistrictTest cook/stage/package
  succeeded, zero errors and 30 content/tooling warnings. The five new assets
  were explicitly requested using the cook commandlet's `-PACKAGE` list.
  `PackagedExamples.json` independently lists all five in the staged IoStore
  container; no persistent packaging setting or map reference was needed.
- `PackagedSmoke_Process.json`: a 90-second Development Game server-mode smoke
  on HopDistrictTest exited zero after an immediate Hard assault was scheduled
  and physical combat ran. No Blueprint Runtime Error, Accessed None, C++ ensure,
  assertion or fatal occurred. Offline Steam initialization and existing Native
  gameplay diagnostics remain warnings. `StagedBinaryHash.json` confirms the
  staged executable matches the verified Game build. This is not TDAServer proof.

Harness notes: Unreal's Python execution guard forces actor function callspace
local, so Python-driven Native BeginDialogue is not a valid network test. The
final run called Native BlueprintCallable entry points through `pie_call_function`
and used Python only for fixture setup/readback. Earlier failed probe JSONs and a
late snapshot after a terminal line had ended are retained and are not pass
evidence. The final run has no Blueprint Runtime Error, Accessed None, C++ ensure,
assertion or fatal. The first concurrent cook hit an editor MCP listener port
collision; subsequent packaging runs close the editor first.
The initial `-CookDir` package-style path did not enumerate these unassigned
assets: that option expects a filesystem directory. The final explicit package
list and container inspection are the evidence that the examples were included.

These checks do not close the full framework audit. A compiled dedicated-server
target, physical AlMalik World Partition assault streaming, the old unsymbolized
behavior-tree crash and final interior/HDR image review remain in the
[current roadmap](ROADMAP_AND_REMAINING.md).
