# Dead guard and assault collision

Dead defenders and assault NPCs use Narrative Pro's normal ragdoll. Their bodies
still collide with the ground and can be looted. They do not block the player or
the camera, and cannot become a surface the player stands on.

## Why the camera moved

In HopDistrictTest, both NPC types were dead and their pelvis bodies were already
simulating physics. However, their capsules and skeletal meshes still blocked the
Camera channel. The invisible capsule remained upright around the fallen body.
Camera collision tests could hit either of these moving shapes.

The fix changes only Pawn and Camera responses and the step-up setting. It does
not disable world collision, change the physics asset, tune the camera, or replace
Narrative's ragdoll movement. Living NPC collision settings are left alone.

## Death, revival and multiplayer

`ATerritoryGuardCharacter` and `ATerritoryAssaultCharacter` read the current death
flag from the Narrative Ability System Component. The existing Native ragdoll
notification, death handler, BeginPlay and visual-ready hooks refresh the local
collision settings. This covers either order of death and ragdoll replication.
Late clients also reuse `UpdateNarrativeNPCClientDeathPresentation` after setup.
This restores Native loot collision and the Loot prompt when initialization ran
after the death notification. A dead guard's default-weapon retry stops instead
of trying to equip a weapon on the corpse.

The small `FTerritoryDeathCollisionState` helper remembers the actual component
settings before applying the corpse policy. Repeated refreshes do not overwrite
that snapshot. Revival restores those settings, including runtime overrides.
The helper does not grant revival to a finite attacker that has already died.

Death, health, NPC saves and ragdoll replication remain owned by Narrative. Assault
casualties still pass through the original participant and counterattack systems.
No new campaign field, replicated death flag, timer or Tick loop is added. Dead
NPCs remain excluded from Native actor records. Their collision snapshot is local
and is not serialized; a newly spawned actor starts with its authored settings.

## Blueprint setup

Keep the parent Handle Death call. Forward **Killed Actor**, **Killed Actor ASC**,
and **Is Dead** from the event into that parent call. Do not leave Is Dead at its
default false value. TDA's replacement `BP_TerritoryAssualtGuard` had the last wire
missing; the repair preserves its other graph content.

Use the Territory guard or assault parent class to receive this policy. A project
that overrides Handle Death without calling its parent must restore that call.
Project scripts should not turn corpse Pawn/Camera blocking back on afterwards.
This fix does not change unrelated story NPC classes or vendor assets.

## Verification

The native regression exercises real capsule camera traces, collision with the
player and world, loot channel preservation, repeated refresh, runtime-setting
restoration, and both Territory character hooks. TDA's Blueprint migration test
checks the replacement assault's death wire when that project asset exists.

For a project playtest:

1. Start HopDistrictTest with a listen server and two clients.
2. Kill one defender and one deployed assault NPC through normal Narrative damage.
3. Check that both bodies fall onto the floor. Walk across each body and rotate
   the camera around it. The player should not climb the corpse or push the camera.
4. Check the Loot interaction on both clients, then join with another client and
   repeat it. Late join must preserve the same death, ragdoll and loot state.
5. Save and reload. Dead attackers must stay removed from the finite force and
   must not return as living capture participants.

The automated Native save checks cover corpse record removal and finite casualty
state. A full AlMalik World Partition stream-out/stream-in playtest is still a
separate map gate; a passing HopDistrictTest run does not prove that gate.

Current build and live multiplayer results are listed in the
[remaining-work checkpoint](ROADMAP_AND_REMAINING.md). Detailed local evidence is
in TDA's `Saved/Verification/20260910_CorpseCollision` folder.
