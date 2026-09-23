# Hashir waits for the Blacksmith defence

Follow-up: [the complete capture/guard sequence](HASHIR_REPEAT_DEFENCE_2026-09-19.md)
corrects the Wave's event location and its obsolete earlier-battle condition.
Use that report for the current authoring and remaining road-entry delay.

The player must defeat the post-capture attack before leaving with Hashir.
The earlier battle before the owner hands over the Blacksmith does not count.

## In the game

1. Capture the Blacksmith and assign a guard.
2. The quest launches its finite `Blacksmith_PostCapture` attack.
3. While attackers or reserve waves remain, Hashir says: "We cannot leave yet.
   Help the guards defeat the attackers at the Blacksmith."
4. After the attack is defeated, speak to Hashir. The quest moves to the Farm
   travel stage and his existing driving goal becomes available.

Missing a battle record, cancelling an attack, failing to find a route, or losing
the Place to the enemy does not count as victory. If an attempt is refused or
cancelled, speaking again can request another finite attack. The retry requires
no pending battle at that Place and no completed victory for this story ID.
It still checks ownership, diplomacy, route and force limits.

## Confirmed problems and changes

The Wave event's `ScenarioID` was discarded for strategic counterattacks. Both
its exact-faction and best-eligible-faction paths now pass the existing identity
through the scheduler. Strategic waves still ignore pursuit-only force and
capture overrides. No launch probability or automatic activation rule changed.

Ordinary history trimming could remove a named strategic victory. A victorious,
quest-authorized strategic record is now kept, like the existing owner
reinforcement result. This uses the existing saved and replicated record.
Repeated strategic Wave events still need authored conditions to prevent repeats;
the new identity does not silently turn them into one-time events.

Native dialogue checks reply conditions, but deliberately does not check the
initial node selected by `StartFromID`. The old trip ID is therefore a silent
router with no driving events. Its new departure reply holds the existing driving
and Farm unlock events, guarded by quest stage and the exact victory condition.
Directly selecting the old entry can no longer skip the wait.

The quest branch now also contains the existing `UTerritoryAssaultTask`, set to
Repel Attack for the same Place, Bandits faction and story ID. It sits after the
original dialogue task, preserving that task's saved array index. Reminder lines
cover guard assignment and the unfinished defence. Existing state IDs stay intact.

## Narrative reference and authority

- Native `UDialogueNode_NPC::GetReplyChain` owns reply selection;
  `UNarrativeNodeBase::ProcessEvents` owns the authored event flow.
- Native `NC_IsQuestAtState` and `BPT_PlayDialogueNode` are reused alongside the
  existing Territory assault condition and task. `UTalesComponent` owns progress.
- `UTerritoryCounterAttackSubsystem` owns admission, deployment, casualties and
  the battle result. `ATerritoryWorldState` already saves and replicates it.
- `ATerritoryVolume` and the existing capture subsystem keep ownership authority.
  Probability never awards ownership. Client requests cannot schedule the battle.
- Native `NarrativeEvent_AddGoalMulti`, driving activity and inventory-independent
  travel flow remain the project presentation. Narrative Pro was not edited.

## Lifecycle checked before editing

The Blacksmith definition already pauses automatic counterattacks while its quest
is active. Guard recruitment's Wave event makes the explicit immediate request.
`ScheduleAssault` validates and commits the record; normal automatic schedules
still use grace, evaluation, warning and their configured activation policy.
`StartAssaultImmediately` uses the existing authorized story exception.
`ActivateAssault` and `SpawnNextWave` create finite Narrative attackers;
`NotifyParticipantRemoved` accounts for their deaths. `ResolveAssault` publishes
the verified result after cleanup. Enemy capture uses the existing capture flow.
Save restoration, history trimming and WorldState replication keep the same ID.
The task and dialogue read that result; they do not keep another victory flag.

## Verification

Final results are recorded in [the JSON report](HASHIR_DEFENCE_GATE_2026-09-19.json).

| Check | Result |
|---|---|
| UE 5.8 UHT, runtime and editor build | Passed, including the corrected native fixture. |
| Full TerritoryFramework automation suite | 361 passed; 324 without warnings, 37 with warnings; zero failed or unrun. |
| New Hashir gate regression | Passed: missing/earlier/pending/cancelled/enemy-winning battles block departure; exact victory and conversation unlock it; Native save/load preserves both checkpoints. |
| Extended strategic Wave regression | Passed: exact and best-eligible faction paths keep identity, clients cannot schedule, pursuit overrides stay ignored, save/history/streamed-out lookup retain victory. |
| Dialogue and quest validation | Both valid, zero warnings; PIE stopped, zero dirty content/map packages. Authoring script rerun successfully. |
| Saved road check | All 11 checks passed. |
| Standalone physical defence | Eight attackers killed, zero alive/pending; Hashir waited, then the real conversation advanced the quest. |
| Standalone car trip | All 10 checks passed: boarding, physical driving, arrival, stop, exit and quest-list preservation. |
| Listen server plus two clients | All six checks passed, including another client joining after victory. |
| Dedicated server plus two clients | The same six checks passed, including late join. |
| Option documents | Extraction checks passed; all 787 reference rows and 263 guide options checked. |

The new minimal Hashir test reports ten Native dialogue lifecycle warnings,
with no errors. Its earlier quest-entry companion reports two warnings. These
fixtures do not certify camera or avatar presentation.

The PIE combat fixture starts at the defence checkpoint and uses the public
Control API to prepare ownership. It simulates deaths through Native's damage
Gameplay Effect. It does not prove weapon combat or replay the earlier handover.
Network checks verify the assault result and its condition; they do not certify
multiplayer conversation selection or the entire shared quest.

An initial minimal native test accidentally placed the player at the missing
Farm provider's fallback position. Native immediately completed the travel task.
The corrected fixture places the player away from that position. An exploratory
PIE session with repeated force-captures was also discarded after vehicle
deployment retries; fresh standalone and network fixtures are the acceptance runs.

## Migration and remaining acceptance work

There is no new save field, replicated property, RPC, GUID or public Blueprint
pin. Old saves have no way to identify earlier unnamed strategic battles as
`Blacksmith_PostCapture`; they are not silently marked victorious. A save at the
defence/travel checkpoint without this named result may require another defence
through Hashir's retry dialogue. Existing pending attacks must finish first.

Project assets and plugin code must be updated together. The authoring script is
`Scripts/Territory/configure_hashir_defence_gate.py` in TDA. The plugin alone does
not change another game's dialogue or quest. Its project integration tests report
an explicit skip if TDA content is absent.

Normal weapon playthrough, complete campaign reload in the main World Partition
map, multiplayer owner/dialogue interactions, packaged builds and UE 5.7 release
verification remain broader acceptance gates. Native serialization and a
streamed-out lookup test are not substitutes for those complete game checks.
