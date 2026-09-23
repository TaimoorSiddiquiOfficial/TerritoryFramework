# Modular re-audit implementation — 23 September 2026

This batch follows the remaining-work audit. It retains Narrative Pro 2.4.2 as
the authority for Tales, party playback, NPCs and save/load; no vendor source is
changed. UE 5.8.3 is the current verification target.

## Implemented boundaries

- Mixed loaded/unloaded hierarchy ancestors reconcile nearest-first. Loaded
  parents still commit through their existing hierarchy reducer and Volume.
  A no-op actor commit republishes its current projection after import.
- Explicitly retired target GUIDs cancel live assault reservations through the
  existing scheduler. Casualty/history receipts remain; ordinary stream-out
  keeps waiting. Restoring an older record applies the same retirement policy.
- `UTerritoryPartyDialogue` is an optional Native dialogue subclass with virtual
  context-transfer hooks. `OwnerDeparturePolicy` chooses supported continuation
  or safe termination. Default continuation handles remote authority owners;
  local viewport and custom-avatar transfers end safely. Existing plain Native
  dialogue assets require no migration for the safe-end policy.
- The admission task now has the normal Blueprint picker wrapper, nested Wave
  validation and a documented bounded cancellation/retry recipe. Its save/load
  regression uses shipped content and a scoped Native quest template, so it
  does not require the TDA project.
- Portable owner samples reference the stock Native appearance. TDA's actual
  level-spawned definitions retain the approved project MetaHumans. Portable
  menu/style assets use stock Native UI dependencies. TDA redirects may still
  customize their appearance at load time; do not resave the portable UI assets
  from that redirected host without rerunning the dependency gate.
- `Tools/ContentManifest.json` defines the exact 122-package content set.
  Validation rejects missing/unexpected packages and external dependencies.

## Behavioral evidence and limits

The native regression suite covers mixed hierarchy control changes/import,
retired reservation release, pending admission/reload, actor context transfer,
distance checks, context-sensitive events, speaker grants and safe termination.
Dedicated PIE with two clients retained the same server dialogue/node, moved its
controller/pawn context and cleaned the departing client's playback. This used
the normal explicit Remove Party Member path, not a socket disconnect.

TDA's real level spawners produced both approved owners on the server and two
clients (five mesh slots and six groom bindings each). The fixture placed
players/owners together for relevancy. The actual project Farm dialogue entered
its medium shot through Native RPC dispatch and exited back to the player.
It did not choose a story reply or complete the whole campaign.

Earlier wording that all Native node events use stale cached context was too
broad: standard events resolve fresh component context. Distance checks, some
condition policies, NPC-target controller context and Blueprint hooks justify
the focused adapter.

These gates remain open: rendered local/split-screen continuation, automatic
disconnect/destruction membership cleanup, mid-dialogue late joins, complete
ordinary-player campaign plus cold restart/rewards on the delivered artifact,
independent adopter acceptance, and declared-scale performance measurements.
There is no claim of universal engine/platform or Shipping gameplay acceptance.
The project material overlay requires the existing licensed Victorian pack and
project MetaHuman content; it is not part of the portable plugin.

See `PARTY_DIALOGUE_REPLY_RULES.md`, `29_Narrative_Quest_Tasks.md`, the current
content validator output and the candidate's build/test receipts for setup and
the exact verified artifact. Historical receipts do not certify later edits.
