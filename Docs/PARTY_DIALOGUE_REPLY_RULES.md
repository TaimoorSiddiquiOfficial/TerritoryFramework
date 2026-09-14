# Party dialogue reply rules

Use **Territory Narrative Party** when a multiplayer conversation needs the
server to enforce who may choose a reply. It extends Narrative's party actor and
replaces only its existing `PartyTalesComponent` class. Narrative still owns the
members, leader, dialogue, quests, node events and client messages.

## Setup

1. Create a Blueprint with **Territory Narrative Party** as its parent, or spawn
   that native actor class from server gameplay.
2. Use the actor's existing **Add Party Member** and **Remove Party Member** nodes.
   These update both Native's actor relevance and its Tales membership.
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

The adapter adds no saved field or replicated property. Membership and quest
save/load stay with Narrative; reply readiness is transient and starts empty
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
It does not finish per-member camera/input, voice/shot or player-speaker tag
handling for a shared local viewer or departing listen host. Native's final local
cleanup still needs rendered camera/input and rapid replacement acceptance.
Direct cross-party transfers can also leave Native's old actor relevance cache
out of step with component membership. Remove through the old party actor before
adding through the new one; atomic transfer remains a separate integration gate.

Fresh leader lookup does not migrate the cached controller, pawn or speaker of
an already-running dialogue. Do not treat reference cleanup as certification of
leader/host departure, disconnect or final-member cleanup. These remain release
gates along with the presentation cleanup above.

Native's immediate join/start, late remote joins, party destruction/travel and
rendered split-screen dialogue remain acceptance gates. A successful reply test does not certify cinematic
shots, voice playback or complete multiplayer story rewards.

Source references: `UNarrativePartyComponent::SelectDialogueOption`,
`UTalesComponent::ServerSelectDialogueOption_Implementation`,
`TrySelectDialogueOption`, `UDialogue::NPCFinishedTalking`,
`PlayPlayerDialogueNode`, `UNarrativePartyComponent::RemovePartyMember`,
`UTalesComponent::ExitDialogue`, `BeginPartyDialogue`, `OnRep_PartyComponent`,
and `ANarrativeParty`'s `PartyTalesComponent` slot.
Territory uses the same `SetDefaultSubobjectClass` pattern as Native's character,
NPC controller and level-sequence actor adapters.
