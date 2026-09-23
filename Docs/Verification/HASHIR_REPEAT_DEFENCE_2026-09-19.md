# Hashir repeats the defence reminder

## Evidence and cause

Two different Blacksmith fights exist. `Blacksmith_BeforeHandover` helps the
owner before capture. `Blacksmith_PostCapture` attacks the player's new Place.
Hashir's Farm trip requires the second fight to end in a real victory.

The saved quest had two authoring mistakes:

- Its Wave event was on the guard task's **Start**. Narrative starts a branch
  when the task becomes available, so the attack could begin before buying a guard.
- `SkipCompletedPreHandoverBattle` suppressed that Wave after winning the first
  fight. Hashir then requested the missing second battle through his retry line.
  The same reminder did not explain that another battle had started.

A copied `NarrativeSave0` provided useful evidence: the first battle had eight
dead attackers and was defeated; the second still had four living attackers
and four dead. This save does not show a completed second victory being lost.
The user's preceding play log also contains reinforcement vehicle spawn failures
and repeated conversations. Those failures are not proof of a victory and are
not silently converted to one.

The new stress fixture kills both earlier waves at their road entrance. Their
empty cars temporarily occupy the short staging route. The next battle remained
Active with zero living and eight pending attackers until the existing
120-second vehicle retirement timeout cleared the entrance. It then deployed
and completed successfully. This queue delay is a separate remaining traffic
usability issue; the authoring correction does not shorten that timeout.

## Changes

The existing Wave now runs on entry to the defence state, after the guard task
finishes. An earlier handover victory no longer suppresses it. A live battle at
the Place, or the exact post-capture victory, prevents another launch.
Loading a saved defence state does not fire the event again.

Hashir's reminder now says that the counterattack is unfinished and includes
reinforcements. His existing congratulations and departure conditions still
require the exact saved victory. Failed deployment, cancellation and enemy
capture never count as success.

The authoring script and network fixture were updated for the new event location.
The native regression now checks the normal player-question reply selection,
the earlier battle, duplicate prevention, cancellation, victory and save/load.
A new standalone fixture exercises two physical finite forces, a paid guard
purchase, actual Native dialogue playback and campaign reload.

## Narrative pattern and ownership

`UQuestBranch::Activate` begins tasks and runs Start events.
`UQuest::TakeBranch` and `EnterState_Internal` perform branch/state deactivation.
The event therefore belongs to `QuestState_10` Start; using branch End would
expose it to Native's repeated branch deactivation.
`UDialogueNode::GetFirstValidNPCReply` continues to select Hashir's reply.

`UTalesComponent` owns quest/dialogue progress. The existing
`UTerritoryScheduleEnemyWaveEvent` makes the server request;
`UTerritoryCounterAttackSubsystem` owns force counts and the verified result.
`ATerritoryWorldState` retains the existing save/replication record.
No Narrative Pro source or content was changed.

The full capture-to-recovery lifecycle remains unchanged: capture, configured
automatic scheduling or explicit quest request, one physical activation, finite
waves, registered attackers, casualties, existing capture or defeat, cleanup,
then saved/replicated resolution. This batch changes the quest's request timing.

## Compatibility

No save fields, state IDs, task indices, replicated fields or public pins changed.
Old guard-stage saves with an active attack continue that attack when the guard
is bought. Old defence-stage saves without the named battle retain the guarded
retry when speaking to Hashir. Existing victories remain valid. The event does
not grant ownership or bypass diplomacy and route checks.

The task still needs the target Place loaded to launch physical attackers.
No World Partition lifecycle code changed. Main-map streaming and a normal
weapon playthrough are separate acceptance work.

Verification results are recorded in [the JSON report](HASHIR_REPEAT_DEFENCE_2026-09-19.json). Project assets
live in TDA; updating the plugin alone cannot change a game's authored quest.

The standalone acceptance run passes all 13 checks, including both eight-person
forces, real reminder/congratulations playback, progression to `QuestState_13`,
campaign save/reload and a second conversation that does not request another
attack. Deaths use Native's damage Gameplay Effect, not manual task completion.
It is still not a normal weapon-combat or multiplayer-dialogue playthrough.

The extended native Hashir test and the companion quest-entry test both pass,
with 13 warnings total and no errors. Warnings include Native dialogue lifecycle
logging and the minimal world's deliberately absent physical attack profile.
Initial standalone fixture attempts started before Native player initialization
and omitted the required exit-dialogue reason. The corrected fixture checks
faction readiness, disables the authored checkpoint write during setup, and
passes a Native exit reason. Those attempts are not acceptance results.

The complete suite passes 362 tests: 323 clean, 39 with warnings, no failed or
unrun tests. The two additional warning-bearing tests compared with the previous
batch logged an Item Inspector widget warning/HTTP timeout and a Native sequence
character ASC warning. Neither is a failed Hashir assertion.

Listen server plus two clients and dedicated server plus two clients both pass
all six checks: active force replication, no early victory, client mutation
rejection, finite defeat, replicated victory and late-join victory. These
fixtures use the real capture/paid-guard transition to launch the Wave. They
do not manually execute it and do not certify multiplayer dialogue selection.

UHT, runtime and editor compilation pass. Both changed assets validate and both
Blueprints compile with zero errors or warnings. All 741 Narrative Pro source
files match the installed Marketplace source. The editor's standalone one-player
settings were restored after the network tests.

HopDistrictTest cooks and packages successfully: zero cook errors and 63
warnings. The 60-second packaged Development Game server-mode run exits zero,
but is **not a clean release smoke test**: it repeats the previous batch's six
unnamed material ShaderMap errors and missing CutscenePlayerActor callback
warning. No packaged quest playthrough was performed. These existing content
issues remain release gates, separate from the verified PIE quest correction.
