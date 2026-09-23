# Party disconnect lifecycle — 2026-09-23

This follow-up implements the logout boundary left open by the modular re-audit.
Native Tales remains the membership, dialogue and quest authority. Narrative Pro
source is unchanged. No campaign schema, asset reparenting or new replicated
property is introduced by this batch.

## Source evidence and adaptation

UE's `AGameModeBase::Logout` emits `FGameModeEvents::OnGameModeLogoutEvent` before
`AController::Destroyed` clears the PlayerState. Territory subscribes on component
registration and removes the delegate on unregister/end. Matching authority/world
events use the existing virtual Remove Party Member path and configured policy.
Custom GameModes must preserve `Super::Logout`.

`UTalesComponent::EndPlay` directly deinitializes its personal `CurrentDialogue`
field. During a nested logout, that field is cleared immediately while Native's
membership arrays remain stable for the suspended iteration. After Native returns,
the group ends safely, live departures use Native removal and destroyed connection
entries are pruned. Pending identities and delegate handles are transient.

`UNarrativePartyComponent::AddPartyMember` has no current-playback synchronization.
All initialized dialogues therefore reject joins before releasing a source party,
including untagged dialogue. Joining after conversation exit remains supported.

`UTalesComponent::ClientSelectDialogueOption_Implementation` assigns the selected
party speaker on clients. There is no public speaker/camera handover RPC. Any
departure during a spoken player reply now ends through Native. Compatible remote
owner departure during NPC dialogue keeps the same server session and node.

## Regression coverage added

- GameMode logout for non-owner, unpossessed owner and final member; exact Native
  membership, actor projection and externally owned tag preservation.
- Cross-world/client rejection, unregister/re-register and repeated logout.
- Logout inside Native begin, personal EndPlay, and invalidated member/PlayerState
  references before the outer operation returns.
- Logout inside a real speaker-grant callback; balanced Native tag contributions.
- Untagged ongoing-dialogue join rejection with the source party preserved.
- Owner/non-owner logout during an actually selected player reply.

Existing reply and presentation fixtures establish membership before dialogue or
end the current dialogue before rejoining. The editor-only network driver can close
an actual server connection; it does not call Logout or Remove Party Member itself.

## Limits

Connection closure testing is distinct from explicit member removal and from a
packet-loss timeout. Direct personal-component destruction without controller
logout, custom GameModes omitting Super, seamless travel, rendered split-screen and
all custom viewport/avatar adapters are not certified by these source changes.
Nested logout deliberately ends conversation; uninterrupted continuity is not its
contract. The complete ordinary-player capture/recruit/defend/reply/reward/cold
restart/join journey and declared-scale performance remain acceptance gates.

Use the dated external evidence report for actual run results and artifact hashes;
test declarations alone do not establish a pass.
