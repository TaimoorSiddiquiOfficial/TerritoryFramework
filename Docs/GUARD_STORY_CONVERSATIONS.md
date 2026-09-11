# Guard story conversations

Status: requested on 2026-09-11; source and Blueprint audit complete. The coordinated
conversation adapter and playable example are still to be implemented and tested.
Existing tagged greetings do not by themselves implement this feature.

## Required player experience

A hidden player enters a Place. Two suitable guards pause at a patrol stop and
talk about the current story. The player can listen while retaining movement,
camera and stealth control. When the conversation ends, each guard continues its
own patrol or assault duty. Detection, damage, death or an urgent combat goal can
interrupt the scene. Hearing a conversation must not reveal the player.

Support both defender and assault guards. An assault guard must first finish
spawning and leave its vehicle; a conversation cannot delay an active attack.
Story authors can also select these guards for an ordinary quest or interaction.

## What exists now

| Responsibility | Existing authority to reuse |
| --- | --- |
| NPC identity, faction and tags | Narrative NPC Definition, spawn parameters and character Ability System |
| Dialogue asset and line playback | Narrative `UDialogue`, `UTalesComponent`, `UTaggedDialogueSet` |
| Quest state and completed story records | The explicitly supplied player's or party's Tales component |
| Guard movement and competing duties | Narrative activity component and goals; Territory's existing patrol goal |
| Defender post, route and finite reserve | Territory Definition and Guard Spawn Point |
| Assault membership and finite force | Counterattack scheduler and assault participant component |
| Stealth awareness | Existing Territory security/infiltration state and Narrative perception |
| Place, District, City and diplomacy checks | Existing Situation Profile and Narrative Territory conditions |

Both TDA definitions, `NPC_TerritoryBandit` and `NPC_TerritoryBanditAssault`, already
reference Native `TaggedDialogues_Bandit`. Their default owned tags currently
contain Slow Walking, with no authored story-role tags. The defender definition
uses `Triggers_Bandit`; the assault definition has no default TriggerSets.
An empty TriggerSets list alone is not proof of broken combat.

Native `FTaggedDialogue` already provides a dialogue tag, generated dialogue class,
required and blocked NPC tags, distance and cooldown. `FNPCSpawnParams` already
supports owned-tag, activity and TriggerSet overrides. Preserve definition tags
when adding a role through an override: that field replaces the default container.

## Authoring contract for the implementation

Use separate concepts for **who speaks**, **what they say** and **who is listening**:

- A role GameplayTag selects eligible guards. A role can describe a lookout,
  patrol partner or story informant. It does not change their political faction.
- A scene/dialogue tag selects the conversation. Example names are illustrative,
  not registered framework tags or working assets yet.
- An optional exact Narrative faction filter restricts the speakers. Leaving it
  empty means any faction satisfying the other authored conditions, not Heroes.
- An explicit listener pawn/controller/Tales context supplies quest conditions.
  A hidden listener can be near enough to hear without being perceived by a guard.
- Each condition must clearly say whether it checks a speaker, listener, Place
  owner or explicit faction. A Narrative condition's Target is not automatically
  the NPC currently speaking.

Reuse Territory Quest State, Situation, Diplomacy, Exposure and Disguise conditions
and existing condition groups. Quest selection, local security response and treaty
state remain distinct checks. Do not require global War merely to overhear a scene.
Do not infer an Undetected result from an unloaded or missing infiltration record.

Offer an automatic proximity trigger and an explicit Narrative event using the
same admission checks. Distance arms the scene; it never reports sight, damage,
hearing evidence, contest participation or capture progress. A patrol-stop trigger
must wait for arrival before speaking. A scene that cannot obtain its speakers or
reach its meeting point must time out and release the guards.

Start with a two-speaker scene in HopDistrictTest. Reusable authoring needs:
speaker roles, optional faction, Place scope, dialogue, story conditions, hearing
distance, cooldown/repeat policy, meeting timeout and interruption policy.
Keep game-specific dialogue text and quest rewards in project assets.

## Confirmed integration gaps

1. **One speaker in the greeting path.** The inspected Native NPC EventGraph
   supplies only Self in `FDialoguePlayParams.Speakers`. Its tagged greeting
   implementation visits nearby players' Tales components. It does not coordinate
   two guards or reserve their activities.
2. **Repeated definitions need distinct speaker roles.** `FSpeakerInfo::GetSpeakerID`
   derives the ID from its NPC Definition. `UDialogue::InitSpeakerAvatars` puts
   speakers in a map keyed by that ID. Two rows using the same definition collide,
   even when Play Params supplies two different actor instances. Resolve distinct
   dialogue-role IDs to exact existing guards; do not select an arbitrary guard
   by shared definition or spawn duplicate NPCs to speak.
3. **Explicit speaker overrides are lost in the ordinary client call.** In the
   installed Native source, `BeginDialogue` sends class and reply IDs through
   `ClientBeginDialogue`; the client calls `SetCurrentDialogue` with default play
   parameters. A Territory adapter must preserve exact speaker binding and free
   movement on listeners. This is a source finding, not a new two-client runtime
   reproduction or a claim that every Native greeting fails.
4. **Patrol Activity Tag has no supplied consumer.** Territory copies it from
   the Definition into the patrol node. The inspected `/Game` and plugin versions
   of `BPA_TerritoryPatrol.SetPatrolPointIdx` use location, rotation and wait time;
   neither consumes Activity Tag. The header's promise that setting the tag plays
   an activity is not implemented by these assets. This field needs a real
   Narrative activity hook and an arrival/interruption regression before use.
5. **Free movement does not pause guards.** Native's NPC OnEnterDialogue movement
   lock applies to non-free-movement dialogue. A hidden-player scene must use
   interruptible Native goals/activities to stage guards, rather than locking
   every participant or deleting their existing goals.

Do not modify Native source or assets to address these gaps. Reuse its public
extension points and keep Territory's coordination limited to selecting/staging
participants and forwarding the verified scene to Narrative playback.

## Lifecycle and persistence

The server evaluates story context, selects distinct ready speakers and admits
one scene. It stages them through Native activities, rechecks the conditions at
arrival, then starts playback for the intended nearby audience. Additional players
must not start another copy on the same guards. Decide a shared scene's story
context once; do not choose conflicting dialogue branches independently per viewer.

Only the authoring-selected quest context receives story events. A per-player
"overheard" objective needs a defined hearing range/completion rule for each
listener; showing a subtitle or starting audio is not enough to award it. Use
Native quest/data-task records for completed milestones. Fix the known Native
data-task adapter listener/quantity issues before relying on concurrent listeners.

On finish or interruption, remove only the scene's own goals and effects, then let
Native rescore the remaining patrol, investigation or assault duties. Never call
Remove All Goals for scene cleanup. If either NPC dies, streams out, changes owner
or becomes unavailable, cancel the scene and release the other participant.

Persist completed milestones/cooldowns through existing save ownership with
stable scene IDs and existing NPC/post/assault identities. Do not save live actor
pointers or transient movement locks. On load, cancel unfinished staging and
re-evaluate eligibility; do not award completion or restore stale participants.
World Partition must resolve only loaded, ready participants and tolerate any
post/NPC/listener load order. Replicate enough presentation state for a late join
to bind the same speakers without replaying story events.

## Work order and acceptance checks

- [ ] Repair the outstanding perception-to-goal and stealth response defects in
  [the state audit](STATE_STEALTH_RESERVE_AUDIT_2026-09-11.md). A scene must not hide
  the existing reason a guard fails to react.
- [ ] Implement tag-based participant binding and the explicit-context Narrative
  entry point, including duplicate-definition speakers and failure reasons.
- [ ] Implement one proximity/patrol staging path through Native activities,
  with bounded waiting, combat interruption and resumption.
- [ ] Preserve server-selected speaker/line identity for multiple listeners and
  late join; ensure rewards and quest events run once in their intended context.
- [ ] Add the HopDistrictTest overheard example and easy-English tooltips. Keep
  final Act 1/Hashir dialogue writing after the framework gates pass.
- [ ] Test hidden entry, disguise, both guard types, wrong faction, missing partner,
  blocked route, listener leaving, combat, death, interrupted/repeated playback,
  reserve replacement, save/load and independent World Partition cells.
- [ ] Run behavioural, authority, Blueprint and Narrative integration tests; use
  a server with two clients to prove one hidden listener remains hidden while
  another exposed player interrupts the guards. Finish normal build/release gates.

This update changes documentation only. No runtime implementation, map, NPC
Definition, quest, save schema or replicated field changed, and no new runtime
verification is claimed.
