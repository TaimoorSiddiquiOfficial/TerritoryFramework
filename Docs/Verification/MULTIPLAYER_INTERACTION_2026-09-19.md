# Multiplayer management and doors — 19 September 2026

## What was wrong

The District Management prompt appeared on a joining player's screen, but using
it did not open the panel. The host could open it. Narrative completed the
interaction on the server. `OpenManagementWidget` then returned because that
server controller was not a local player controller.

The project doors had a separate problem. Their actors did not replicate, even
though their graphs called a multicast event. In HopDistrictTest, opening a door
on the host left both clients' doors closed. The old event also could not tell a
later player that the door was already open.

## Changes and ownership

- Narrative's `UPlayerInteractionComponent` still finds the target, handles the
  hold time and asks the server to complete the interaction.
- The existing `UTerritoryPlayerManagementComponent` now delivers the approved
  menu request to its owning client. The server checks the point, world, pawn,
  current faction, district availability, claimed state and distance.
- The receiving client uses the existing `OpenManagementWidget` and Narrative
  CommonUI stack. A local distance and permission check runs again. Other players
  do not receive the panel. Reopening reuses the existing matching menu.
- `ATerritoryVolume` and the hierarchy reducer still own District control.
  Guard purchases and other menu actions retain their existing server checks.
- Project `BP_2Door` and `BP_Door_Base` now replicate their existing
  `DoorIsOpen?` value with RepNotify. That value drives the existing timelines on
  the server and clients. Only authority can toggle it from On Interacted.
  `BP_Door_Bunglow` inherits the repair. HopDistrictTest was resaved with the
  updated door instances.

The Native reference is `ANarrativePlayerController::ClientShowHUDNotification`
and `UTalesComponent::ClientBeginDialogue`: the server sends presentation through
the player's owned controller/component. No RPC was placed on an unowned world
interaction actor. Narrative's existing interactable event and actor replication
are reused for the doors. No Narrative Pro source or content was edited.

## Community API and migration

`SendOpenManagementPoint(ATerritoryDistrictManagementPoint*)` is available on the
player's management component. It is a Blueprint authority-only request. `true`
means the server sent an eligible request; it does not promise that a missing
local HUD can show a menu. `ClientOpenManagementPoint` is the private, reliable
owning-client RPC. Normal designers continue using the existing management point;
no input mapping or Blueprint rewiring is needed for the panel.

There are no new saved fields, faction stores or stable IDs. Native save/load
restores District ownership as before; a menu request itself is not saved.
Door state remains session state, now also delivered to later connections. This
change does not add campaign persistence to project doors.

The old project `Open Door (multicast)` event was removed. Its only call was inside
each repaired door. The map and child Blueprint have no calls to it. Future door
logic should change the existing open state on the server, then let RepNotify
update the visuals. The single-player timelines, angles and interaction durations
are preserved. The user's pre-existing BP_2Door edits were backed up before the
network graph changes in the local evidence folder's `Before` directory.

## Verification

| Check | Result |
|---|---|
| UE 5.8 runtime/editor build and UHT | Passed |
| UE 5.8 Development Game build | Passed |
| Full Territory automation | 362 passed: 325 clean, 37 with warnings; zero failures |
| New management regression | Passed without warnings |
| Relevant asset validation, with PIE stopped | 7 valid, zero warnings |
| Changed door Blueprint compilation | All 3 passed without warnings |
| Listen host and two clients | Correct panel and player isolation; client opens/closes/reopens door; late join sees open meshes and can open management |
| Dedicated server and two clients | Same checks passed; a third, late client can manage; single-door and child-door replication also passed |
| Narrative source integrity | All 741 source files match the installed Marketplace baseline |
| HopDistrictTest cook/stage/package | Completed, exit 0; 63 cook warnings, zero cook errors |
| Packaged Development game, server mode | Ran for 60 seconds and exited 0; **not a clean smoke gate** because of 6 material shader-map errors |

The native regression covers accepted ownership, missing point, client authority
rejection, distance, wrong faction, story lock, contested state, Narrative
save/load, registry removal/re-registration and a point from another world. It
also checks the Blueprint and reliable client RPC contracts. A first version of
the fixture incorrectly attempted direct aggregate ownership; the corrected
fixture uses the existing hierarchy authority. The final complete suite passed.

The live fixture is `Scripts/Territory/verify_management_interaction_pie.py` in TDA.
It captures Blacksmith through Territory Control and sends Native Begin/End
Interact events using next-tick timers. Menu tests use the actual interaction
target, network request and active widget. The main double door uses the same
input path. Extra single/child-door checks fire the Native completion delegate
and verify replicated animation; they do not test approaching those two doors.
These checks do not certify physical keyboard focus or every project interaction.
A separate controlled check started Hashir's dialogue on host and joining client;
it did not test the entire multiplayer quest conversation.

One repeated PIE session crashed during late join in PythonScriptPlugin and
CoreUObject. The log is retained as `RepeatedPIECrash.log`; its root cause is not
resolved by this gameplay fix. A fresh editor completed the full dedicated
fixture. An earlier test-script attempt used a method not exposed to Python; it
was corrected to the Native completion delegate before the final run.

The packaged run logged `Loading a material resource None with an invalid
ShaderMap!` six times during map load. The affected assets are not named by this
log; their cause remains to be traced. It also logged a missing
`CutscenePlayerActor` from the project controller's loading-screen callback into
Native Play Cutscene. The same cutscene warning exists in the 14 September
packaged baseline. Neither finding is claimed fixed here. Cook warnings include
the missing mobile touch-interface package, the item-inspector Drop button event,
character-creator tags and old Narrative demo actors. The new package was staged
separately at `D:/TFWork/TerritoryInteractionSep19/Stage`.

Detailed local logs and JSON receipts are in
`Saved/Verification/20260919_MultiplayerInteraction` in TDA. The full campaign in
AlMalik, real World Partition travel, every interaction type and UE 5.7 release
acceptance remain separate gates. Registry lifecycle testing is not a complete
World Partition playthrough. Dedicated PIE is not a compiled TDAServer build;
the installed engine distribution does not support that separate target.
