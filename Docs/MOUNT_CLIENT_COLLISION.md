# Native mount collision on remote clients

## Problem

In HopDistrictTest, Hashir entered the driver's seat on the server. Native disabled
his capsule collision there. Other clients received the attachment and movement
mode, but his capsule still blocked the car. Client physics pushed the car away
from the server's position, sometimes by over two metres. The remote passenger's
entry move then used a different car position and could fail before attachment.
An observing client also needs the passenger's capsule disabled after boarding.

The controlled diagnostic changed only the seated NPC's client capsule collision.
The stationary cars then converged to the server, and Native passenger boarding
and physical driving succeeded. This does not change the previous road fix.

## Setup

Add **Territory Mount Presentation** to a Narrative player or story NPC Blueprint
that can ride a mount. Territory defender and assault characters include it by
default. Use one component per character. Add it to the character, not the car.

The component follows the character's existing attachment to an actor with Native's
`UMountComponent`. On a simulated client character, it disables the capsule while
seated and restores the previous collision mode after exit. Native's locally
executing ability continues to handle the owning player. A transition to owning
client control discards the old presentation cache so the exit animation retains
control of collision timing.

Root-transform notifications apply the change promptly. A local check every 0.1
seconds also handles a late or removed mount component and attachment load order.
There is no world scan and the component does not tick on the server.

It does not change the mesh's collision channels, choose seats, claim occupancy,
start abilities, drive, complete quests, change faction, or replicate another
vehicle transform. Native attachment, seating, GAS, physics and save interfaces
remain the authorities. A later collision change or Native death/ragdoll handling
takes priority over restoring the cached mode.

## Save and migration

The cache is local and transient. No new save fields, GameplayTags, GUIDs or RPCs
are introduced. A restored or newly joined client derives the presentation from
Native's current attachment. This does **not** implement mid-trip story save/resume.

Existing Territory guard Blueprint children inherit the new component. Other
Narrative characters opt in by adding it. Narrative Pro source and assets must
remain unchanged. The public `RefreshMountPresentation` hook performs the same
local reconciliation; it is not a server seating command.

## Regression coverage

`TerritoryFramework.AI.Regression.NativeMountClientCollisionAndRestore` exercises
real Narrative character capsules and Native's mount component. It checks server
rejection, repeated updates, owner-client behavior, exact restoration, deactivation,
later collision overrides, death, mount removal/reappearance, ownership handoff,
and a save/load round trip into a new character. It verifies that presentation
does not claim any seats or add a replicated authority.

Live verification and build results are recorded in the
[dated verification report](MOUNT_CLIENT_COLLISION_VERIFICATION_2026-09-12.md)
and TDA's `Saved/Verification/20260912_HashirBoarding` directory. The complete
Blacksmith-to-Farm story, arrival dialogue and mid-trip recovery remain separate
acceptance items in [Hashir's trip checklist](HASHIR_CASTLE_FARM_DRIVE_TODO.md).
