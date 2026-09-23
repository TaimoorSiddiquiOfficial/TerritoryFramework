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

### Modular owner departure

`PartyTalesComponent` exposes **Owner Departure Policy**:

- **Continue with compatible dialogue adapter** (default): a Dialogue Blueprint
  parented to **Territory Party Dialogue** can continue the same line when its
  cached remote owner leaves for another remote member. The adapter transfers
  Native's controller, pawn, player-speaker caches and avatar tag grant. It does
  not restart playback or replay node events. Remote client copies retain their
  own local viewing controller.
- **End the conversation safely**: Native exits for the group before Leave Party
  callbacks. Story logic can offer a fresh conversation explicitly.

Reparent only party dialogue assets that require continuation. Ordinary Native
dialogues still work; if their cached owner leaves, the default policy safely
ends them because they do not implement the context adapter. Non-owner removal
continues playback, and final-member removal always ends it.

`UTerritoryPartyDialogue::CanTransferPartyContext` and `TransferPartyContext` are
the C++ extension seam for projects with custom avatars or viewport ownership.
The built-in adapter deliberately rejects a local owner/recipient or a custom
player avatar: those require camera/input and shot migration specific to the
project. The component then uses Native's safe exit. A custom adapter must move
all affected caches and balance only its own grants without restarting events;
returning false selects safe exit. Do not modify Narrative Pro.

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

Native also adds a separate contribution for the player avatar. A supported
`UTerritoryPartyDialogue` transfer moves that contribution to the new avatar;
the safe-end fallback lets Native remove it during ordinary dialogue cleanup.
Rendered camera/input acceptance remains separate from tag accounting.

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

**Add Party Member** rejects a join into every initialized dialogue, including
ones with no player speaker tags, before removing the player from their old party. Native does not
yet synchronize that ongoing conversation to the joining player. End the
conversation before adding the member. Late-join synchronization remains open;
this rejection prevents divergent playback and unbalanced speaker tags.

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
The departing member can start a personal conversation without corrupting the old
party's dialogue. When the context adapter accepts a transfer, the remaining
members keep their current conversation and line; otherwise Native ends it first.
The local presentation bridge also drops the old personal reference when Native
replicates a leave or party switch. An old leave cannot clear a new personal
conversation or a different party's conversation.

On a client with no remaining local party member, Native closes the old local
dialogue immediately. This avoids leaving its cleanup until the old party actor
disappears. The server's shared dialogue and a client copy still used by another
local member remain alive.

The optional context adapter completes remote-owner transfer using Native's
protected dialogue extension points. Standard Native node events already resolve
fresh component context; distance checks, some condition policies, NPC-target event
controller context and Blueprint node hooks can still consume cached context.
The adapter updates those caches and transfers the separate avatar grant as well
as the party component's existing member cleanup. Local viewport transfers and
custom avatars fail closed by default. Use Territory parties on both sides of a
transfer; stock Native source parties retain their own departure behavior.

When the final member leaves, Territory calls Native's group **Exit Dialogue**
while that member is still registered. Native can then remove its avatar/member
tags and send the departing client its normal exit. This happens before **Leave
Party** callbacks, so those callbacks can begin a personal dialogue after the
old dialogue has ended. **Can Be Exited** does not keep an empty party alive;
that setting controls the player's normal dialogue-exit action. An empty party's
**Begin Dialogue** returns false. Reconnect members before starting dialogue.
Parties with remaining members follow the configured owner-departure policy.

Native's delayed blend/input cleanup and concurrent authored cinematics still need
rendered acceptance on each supported viewport topology.

### Disconnect and teardown

The registered authority component listens to Unreal's
`FGameModeEvents::OnGameModeLogoutEvent`. `AGameModeBase::Logout` broadcasts this
before `AController::CleanupPlayerState`, so normal disconnect/controller teardown
uses the same Native removal and configured owner-departure policy. The listener
filters the world and unbinds on unregister/end. A custom GameMode must retain
its normal `Super::Logout` call. Direct component destruction without controller
logout and seamless travel remain separate acceptance cases.

When logout occurs inside the guarded Native begin, replacement, reply selection
or tag/exit callbacks, Territory immediately detaches the personal shared-dialogue
alias. It defers membership edits until Native's current member iteration returns,
then ends the group safely and removes the captured connection identities. Live
members leave through Native; already destroyed members cannot receive a Leave
callback. These weak pending references are temporary bookkeeping, not saved or
replicated campaign state. This nested path deliberately does not promise continuity.

A departure during a **spoken player reply** always ends the conversation, even
if the departing member is not the cached owner. Native's selected party speaker
is delivered to client copies and has no supported camera handover RPC. The
default adapter cannot infer a safe continuation for every remaining viewport.
During NPC dialogue, compatible remote-owner transfers retain the existing line.
The ordinary explicit Remove Party Member path enforces the same boundary.

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
