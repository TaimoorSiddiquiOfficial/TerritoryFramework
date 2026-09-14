# Party dialogue reply rules

Use **Territory Narrative Party** when a multiplayer conversation needs the
server to enforce who may choose a reply. It extends Narrative's party actor and
replaces only its existing `PartyTalesComponent` class. Narrative still owns the
members, leader, dialogue, quests, node events and client messages.

## Setup

1. Create a Blueprint with **Territory Narrative Party** as its parent, or spawn
   that native actor class from server gameplay.
2. Use the actor's existing **Add Party Member** and **Remove Party Member** nodes.
   These update both Native's actor relevance and its Tales membership. The
   Territory component's same nodes also keep its Native party actor in sync.
3. On `PartyTalesComponent`, choose Narrative's **Party Dialogue Control Policy**:
   **Party Leader Controlled** or **All Party Members Controlled**.
4. Start the shared Dialogue Blueprint through that component's **Begin Dialogue**.
5. Keep using the local player's personal Tales **Try Select Dialogue Option**
   node. Narrative sends the option ID and derives the player identity on the
   server. Do not send player replies directly to the party's inherited server RPC.

For a custom replicated party actor, add **Territory Narrative Party Component**
instead of Narrative Party Component. That actor remains responsible for Native
member relevance. Existing plain Narrative parties are not replaced at runtime.
Reparent a project party Blueprint or change its authored component to opt in.
No vendor Blueprint or source file needs editing.

## What the server checks

A join requires a server-owned controller and matching PlayerState in the same
world. On a Narrative player controller, use its actual **Get Tales Component**
result. A second Tales component cannot join on that player's behalf. A Native
party actor requires a Narrative PlayerState; custom actors using only the
component may use another PlayerState class. If PlayerState is not ready yet,
the join returns false without changing membership. Retry after it is ready.

Adding an existing member again returns false. A transfer first asks the old
party to remove the member. If it refuses, the new party does not add them.
Native still publishes its normal Leave and Joined callbacks. If a story
callback redirects the player to another party or immediately removes them,
the outer join returns false and preserves the story's final membership.

Territory parties update Native's actor member list and replication relevance
before their membership callbacks run. Transfers from a stock Native party
also reconcile that actor's lists after its removal returns. A custom stock
party's own callback order remains its responsibility. Custom replicated actors
that do not derive from Native's party actor still own their relevance policy.

The selector must be a current member, with a valid authoritative controller and
matching PlayerState in the same world. A leader-only party also requires the
actual Native leader. Missing selectors, departed members and unknown policies
are rejected. Clients cannot supply another player's identity through the normal
personal Tales RPC: it accepts an option ID, then resolves its own controller.

Manual choices wait for Narrative's **Dialogue Replies Available** event. A new
NPC line, new dialogue, finish or component unregister clears that readiness.
The selected option must belong to the current initialized Native session, and
the NPC reply chain must be empty. Readiness is consumed before Native executes
node events. Repeated requests cannot replay a consumed option.

Narrative selects routing and automatic replies before publishing its reply UI.
The adapter preserves that server path and attributes the automatic reply to
the Native party leader. Designers keep the existing auto-select flags and
Narrative dialogue settings. No second reply policy, RPC or dialogue state
machine is introduced.

**Can Member Choose Dialogue Reply** is an optional server-only Blueprint query
for the membership/policy check. It does not say whether the current line is
ready for a choice. Client UI continues to use Native's replicated member states
and the authored policy; the server always validates the actual request.

## Compatibility and remaining limits

### Player speaker tags

Keep using the Dialogue Blueprint's **Player Speaker Info → Owned Tags**.
Territory records the one contribution that Native adds for each party member.
When that member leaves, it removes that contribution before **Leave Party**
callbacks. Tags from a personal dialogue, an ability or another grant remain.
An empty Owned Tags list needs no setup. Existing Dialogue Blueprints keep their
parent class; no reparenting, tag rename or vendor edit is needed.

Native also adds a separate contribution for the original player avatar. That
contribution remains until Native ends the shared dialogue. Migrating the active
avatar/controller and releasing its contribution early is still pending. Leaving
the original player avatar is therefore not yet complete camera/input cleanup.

Use the party component's normal **Begin Dialogue**, **Set Current Dialogue** and
**Exit Dialogue** lifecycle. Custom C++ dialogue overrides must retain Native's
begin/end behavior. The grant record holds weak runtime references and is never
saved or replicated; the actual tags still replicate through Native's ASC.
Component re-registration keeps an active record; replacement, end and level
teardown retire it. A rejected null or lower-priority replacement preserves it.

Membership edits and another begin/replacement are rejected while Native is
inside a synchronous dialogue/tag transition. Schedule such story changes after
the current call returns, for example on the next tick. An exit requested inside
those callbacks is deferred until the transition finishes and applies only to
that exact dialogue, so an old finish cannot close its replacement.

**Add Party Member** rejects a join into an already-running dialogue with player
speaker tags, before removing the player from their old party. Native does not
yet synchronize that ongoing conversation to the joining player. End the
conversation before adding the member. Late-join synchronization remains open;
this rejection prevents a player receiving blocking tags without a conversation.

The adapter adds no saved field or replicated property. Native stores quest
history; live party membership is not a campaign save record. After loading,
reconnect players explicitly through Add Party Member. Reply readiness is transient and starts empty
when a component registers. An active conversation must publish a fresh Native
replies-available event after re-registration, or be restarted. This avoids
guessing that an in-progress NPC line has finished.

For a listen-server party containing only remote players, the server uses
Narrative's actual party leader as its dialogue controller. Clients retain
their own local viewing controller. This follows Native's dedicated-server
pattern and avoids an empty controller/pawn context for dialogue conditions.

When a member leaves a Territory party, the adapter clears that member's personal
reference to the shared dialogue before Native runs **Leave Party** callbacks.
The departing member can start a personal conversation without closing the old
party's dialogue. The remaining members keep their current conversation and line.
The local presentation bridge also drops the old personal reference when Native
replicates a leave or party switch. An old leave cannot clear a new personal
conversation or a different party's conversation.

On a client with no remaining local party member, Native closes the old local
dialogue immediately. This avoids leaving its cleanup until the old party actor
disappears. The server's shared dialogue and a client copy still used by another
local member remain alive.

This is the first part of the chosen **continue for remaining members** policy.
It does not finish per-member camera/input, voice/shot or the separate avatar-tag
handling for a shared local viewer or departing listen host. Native's final local
cleanup still needs rendered camera/input and rapid replacement acceptance.
Validated transfers now reconcile Native actor/component membership and reject
failed departures. This does not migrate an active dialogue's cached speaker,
tags, camera or controller. Use Territory parties on both sides for the verified
personal-dialogue reference cleanup. A stock Native source retains its own
active-dialogue departure behavior.

Fresh leader lookup does not migrate the cached controller, pawn or speaker of
an already-running dialogue. Do not treat reference cleanup as certification of
leader/host departure while others remain, or disconnect/destruction that bypasses
removal. These remain release gates along with the presentation cleanup above.

When the final member leaves, Territory calls Native's group **Exit Dialogue**
while that member is still registered. Native can then remove its avatar/member
tags and send the departing client its normal exit. This happens before **Leave
Party** callbacks, so those callbacks can begin a personal dialogue after the
old dialogue has ended. **Can Be Exited** does not keep an empty party alive;
that setting controls the player's normal dialogue-exit action. An empty party's
**Begin Dialogue** returns false. Reconnect members before starting dialogue.
Parties with remaining members continue the same conversation.

This final-member cleanup does not migrate an earlier-departed avatar's cached
camera/controller while others continue. Native's delayed blend/input cleanup
and concurrent authored cinematics still need acceptance. Disconnect/destruction
does not automatically call Remove Party Member and remains a separate gate.

Native's immediate join/start, late remote joins, party destruction/travel and
rendered split-screen dialogue remain acceptance gates. A successful reply test does not certify cinematic
shots, voice playback or complete multiplayer story rewards.

Source references: `UNarrativePartyComponent::SelectDialogueOption`,
`UTalesComponent::ServerSelectDialogueOption_Implementation`,
`TrySelectDialogueOption`, `UDialogue::NPCFinishedTalking`,
`PlayPlayerDialogueNode`, `UNarrativePartyComponent::RemovePartyMember`,
`UNarrativePartyComponent::AddPartyMember`, `ANarrativeParty::IsNetRelevantFor`,
`UTalesComponent::ExitDialogue`, `BeginPartyDialogue`, `OnRep_PartyComponent`,
and `ANarrativeParty`'s `PartyTalesComponent` slot.
Territory uses the same `SetDefaultSubobjectClass` pattern as Native's character,
NPC controller and level-sequence actor adapters.
