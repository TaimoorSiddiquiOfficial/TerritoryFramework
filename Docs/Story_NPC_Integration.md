# Story NPC setup

Use `BP_TerritoryStoryNPC` as the parent of a project story NPC. It inherits
Narrative's `BP_NarrativeNPC`, including its combat, inventory, appearance and
save behavior. It does not copy or change Narrative graphs.

The supplied `BP_TerritoryStoryNPCController` uses the existing Territory activity
save adapter. Its **Old Saved Goal Generators To Ignore** list is empty. Each
project must choose which old classes it has replaced. See
[NPC activity saves](NPC_Activity_Save_Migration.md).

## Death on multiplayer clients

Narrative Pro 2.4.2's NPC Blueprint calls `RemoveAllGoals` during death. The activity
component belongs to its AI controller, which does not exist on remote clients.
The Native C++ death path also calls `SetRagdoll`, which sends an unowned RPC from
a remote NPC. The story NPC handles these two paths separately:

- On the server, it calls the original Narrative parent death event with the
  original parameters. Native still handles death, saves, AI and weapon policy.
- On clients, `UpdateNarrativeNPCClientDeathPresentation` reads the current death
  flag from the replicated Narrative ASC. It updates movement, collision, the
  interaction prompt, marker and child visuals. It removes the equipped weapon
  visual according to Native's multiplayer policy. It never changes inventory,
  health, death state, saves or ragdoll state.

Native's ragdoll replication still controls the body. The client helper does not
create an AI controller or send an RPC. It rejects server actors, missing actors,
missing ASCs and actors being removed. Repeated calls use the current ASC state;
an old event argument cannot undo a newer death or revive state.

The Blueprint also refreshes on the next tick after BeginPlay and after
`CharacterVisualInitialized`. This matters for late join and a visual being
recreated: the death flag may arrive before event binding, and Native registers
the marker and equips items after its visual-ready delegate fires. Calling only
the death helper from `HandleDeath` can leave a dead late-join NPC showing **Talk**.
The supplied Blueprint contains both required initialization paths.

## Hashir in TDA

`NPC_Hashir` now selects the project child `BP_Hashir`. That child selects
`BP_HashirController`, which inherits the shared story controller. Hashir's
controller ignores the exact old Native `GoalGenerator_Attack` class when loading
saved generators. His current `AC_HashirPacifist` still uses all eight Native
activities and `GoalGenerator_Hop_SafePerception`. The replacement and unrelated
quest generators are preserved. There is no global class redirect.

His definition GUID remains `04684A2B4E88AEF6BF9899906AF9460C`. The existing
`NPC_Hahsir` character ID and `Hahsir` NPC ID are deliberately unchanged. The
dialogue, appearance and faction fields are unchanged by this integration.

Native NPC records contain the controller record. Recreated NPCs use the current
definition and controller class, then load that record into the existing named
activity component. Native's generic NPC `ShouldRespawn` returns false: its
spawner recreates it, rather than the save system dynamically spawning its old
recorded class. Already running actors need recreation to adopt the new classes.

## Verification and limits

The native regression checks authority rejection, controllerless clients,
repeated death updates, restored live presentation, unchanged health and unchanged
ragdoll state. The editor regression runs the actual shared Blueprint, its parent
inheritance, BeginPlay, visual delegate and deferred updates across engine frames.
The existing activity-save regressions cover retired Native-format records,
duplicate snapshots, restored defaults and load before BeginPlay.

In HopDistrictTest, a listen server and two clients receive both Hashir greeting
lines. A real Native actor save restores currency and recreates a removed test
generator. Four saves retain two current generator records, without growth. Death
removes the Native actor record and gives the existing clients and a fresh late
join matching dead/ragdoll/**Loot** presentation.

This is not a complete AlMalik World Partition test or compiled dedicated-server
certification. The later [combat eligibility audit](Combat_Activity_Eligibility.md)
found no saved default attack goals or selected friendly targets and fixed combat
selection during vehicle travel. Native activity restart after death and the
separate EQS/weapon warnings remain under audit. The story NPC evidence is in TDA's
`Saved/Verification/20260909_HashirIntegration`.
