# Assault physical streaming lifecycle

Native host run 43 preserved the durable assault counts across a cold restart and
Place reload, but its three surviving attackers subsequently withdrew. An isolated
movement trace reproduced the cause: removing the target/floor cell left the
runtime-spawned pawns simulating gravity. They fell from approximately Z=-110 to
Z=-48000 despite the same target identity returning. The movement watchdog was
reporting the consequence, not causing the fall.

`UTerritoryAssaultParticipantComponent` uses the engine's
`PreLevelRemovedFromWorld` callback to suspend on-foot participants before their
target or movement-base level loses collision. Native `UNPCActivityComponent`
`Deactivate`/`Activate` retain its goals and stop/restart the current activity;
`UCharacterMovementComponent` retains the prior movement mode and velocity.
The existing missing-target wait also enters this suspension as a fallback.
Resume requires the exact registered target and, for grounded movement, a walkable
physical floor from `ComputeFloorDist`. Target registration or a navmesh alone
cannot authorize resumption. The goal's transient actor reference is rebound.

The same NPC/GUID, Native health/equipment and finite assault accounting remain
in place. A suspended participant contributes no capture pressure and cannot
authorize combat or a final recapture. The suspension flag is a replicated runtime
read model, not a second saved authority. Existing physical survivor records retain
the stable pose on save; a cold load uses the existing reconstruction path.
The added suspension applies after vehicle ingress, not to mounted vehicle physics.

Verification must include the existing unloaded-target automation test (now with
collision removal, repeated suspension, absent floor, authority and resume checks)
and an actual World Partition run that compares living attacker GUIDs, health and
movement before/after streaming. Directory counts alone were insufficient proof.
Build and composed-run results are recorded in the accompanying release evidence;
this explanation is not itself a passing test receipt.
