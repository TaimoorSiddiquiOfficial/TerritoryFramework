# Guard response and Narrative record adapters

This continues the [state and reserve audit](STATE_STEALTH_RESERVE_AUDIT_2026-09-11.md).
The coordinated [guard conversations](GUARD_STORY_CONVERSATIONS.md) are still pending.

## Confirmed causes and changes

**Reserve guards:** the first successful Native sight callback can have strength
zero. Unreal can later replace the stored strength without broadcasting another
visibility change. Territory's observer kept the first value, while the Native
attack generator had no new callback to reconsider the now-hostile target.

`UTerritoryNPCActivityComponent` now reads the actual stored observations before
Native's existing goal rescore. It forwards changed strength, changed local
attitude, or newly initialized observations to the installed Native goal
generators. It does not manufacture full sight, create attack goals itself, run
the greeting path again, or notify removed generators. It waits for Native's
complete appearance-loading query and clears its cache on load/unpossession.

`UTerritoryStealthObserverComponent` now samples current Native sight, handles
late controller binding, and forgets stale observers. Visibility callbacks add
zero observation time; the existing timer supplies elapsed time. Invisible or
zero-strength observations cannot identify a firing player as visibly firing.

**Anonymous clues:** gunshots, impacts, corpses and distractions can fill
suspicion without proving who the hidden player is. The Stealth Profile now has
`bAnonymousEvidenceCanExpose`, disabled by default. An author can deliberately
enable the older clue-only exposure policy for a specific story situation.
Confirmed exposure remains until an explicit reset; a weak/lost sight update
does not silently make an exposed target suspicious again.

**Local Alarm:** immediate exposure can assign an investigation goal without
registering capture participation. Disabled unseen-gunfire and unseen-death
settings are now enforced by the evidence authority too. Invalid numeric evidence
and cross-world actors are rejected. Reported physical presence uses actual bounds.

**Narrative data records:** `UTerritoryNarrativeDataTask`, shown as **Wait For
Narrative Data Task**, is an inline Narrative quest task. It listens to the
explicit quest's Tales component and removes only its own listener. Native
broadcasts before committing its saved counter and omits quantity from the
notification; the adapter reads the final count on the next tick instead of
adding one. Other listeners remain connected when it ends.

## Using the data-record task

1. Add **Wait For Narrative Data Task** to a Narrative quest branch.
2. Choose the existing Narrative Data Task asset and its argument.
3. Set Required Quantity. Enable **Count Previous Completions** to accept history.
4. Have the real story action call Native **Complete Narrative Data Task** on the
   correct player or party Tales component, with the same asset and argument.

For example, record `Overheard` with argument `BlacksmithPatrol` after the player's
overhearing requirements are satisfied. Starting a subtitle alone is not proof
that the player heard a conversation. The conversation producer is not implemented
by this task.

Replace affected Native `BPT_CompleteDataTask` instances with this task when using
parallel record objectives. The vendor task can still unbind all listeners; adding
the adapter does not patch vendor assets or make that old cleanup safe.
An instance's **Retroactive** choice maps to **Count Previous Completions**.

Native's actual count is authoritative. If a project producer forwards only one
record to a party, this task cannot infer an omitted quantity; pass the intended
quantity to the Tales component that owns the party quest.

## Authority, saving, networking and migration

Narrative owns perception, goal generation, activity selection, attack tokens,
Tales records and quest progress. Territory Control still owns awareness and
capture admission; Volume still owns territory state. No faction or capture
authority was added. No Narrative source or asset was changed.

AI delivery caches and live sight observers are transient server state. They are
rebuilt from Native after readiness, load and controller changes. Native's existing
NPC replication and Territory's existing garrison snapshot project the result.
Clients cannot submit authoritative evidence or advance record-task progress.

For objectives that exclude history, the adapter stores a starting-count cursor
in Native's existing saved `MasterTaskList`, under `__territory_datatask_start|...`.
This is a cursor, not a second completed-action counter. Its key uses the quest
class, authored branch ID, task object name and record signature; it never uses a
live actor pointer or quest instance name. A fresh task start overwrites its key.
Ending a task must not erase it because Native ends old instances during load.
This preserves actions if saving happens before the deferred progress refresh.

Older saves without that cursor use the restored Native branch progress to make
one starting cursor. Renaming a branch, replacing its task, or changing the record
signature invokes that bounded migration. Keep authored IDs/names stable during a
campaign. No new save format or replicated property is required.

`ReportStealthEvidence` adds an optional **Sight Evidence Seconds** pin (default
0.25). Existing calls retain their interval; new perception adapters should pass
zero for an edge callback and elapsed seconds for sustained sight. Reports are
capped at one second. Blueprint assets are not automatically rewritten.

## Verification

The first perception/stealth batch passed 309 automation tests in both UE 5.8 and
UE 5.7. HopDistrictTest then passed a server/two-client reserve test: three Native
deaths queued three replacements, reserve stock fell from seven to four, all three
replacement guards acquired Native attack goals without manual refresh, both
clients received the same garrison state, and another player entering added no
extra force. Client evidence mutations were rejected.

All 310 automation tests pass on both engines, including immediate Local Alarm,
parallel record listeners, bulk quantities, restarting a task, and saving before
deferred progress catches up. All six Editor, Development and Shipping builds
pass. Asset validation checks 245 assets and compiles 147 Blueprints with zero
errors and eight existing warnings. UE 5.8 cook/package and a 60-second packaged
Development Game server-mode startup pass with exit zero. This is not a compiled
`TDAServer` executable. The existing optional cutscene-player warning remains.
The initial cook encountered a tooling HTTP-port collision with the open editor;
closing the editor and rerunning resolved that environment failure.

Evidence is in TDA `Saved/Verification/20260911_GuardResponse/`, including
`ReserveTraceBefore.json`, `ReserveTraceAfter.json`, `ReserveNetwork.json`, engine
test reports and build logs. All 741 local Narrative source files match the
installed UE 5.8 vendor package.

## Still remaining

- Separate defender self-defence from quest capture locks and War state events.
- Apply one consistent policy to outside shooters, treaties, disguise and each
  player's exposure; a globally contested Place must not expose every player.
- Release only stealth/bounds participation when exposure clears, preserving
  flag capture and separately authored contest participation.
- Add reserve Narrative events around the existing finite guard-post commands.
- Finish the reserve/capture pending-post checks and hand-authored condition-task wiring.
- Implement the tagged guard conversation coordination, exact speaker binding,
  client playback, patrol-stop activity hook, combat interruption and duty resumption.
- Keep the earlier cold AlMalik appearance, restored-wave crash and returning-client
  streaming gates open. This batch does not establish those as fixed.
