# Combat service search lifetime

## Confirmed mechanism

Narrative Pro 2.4.2 `BTS_Attack.ReceiveTickAI` calls `Attack`. When
`CanKeepAttacking` rejects the current target, that function calls the existing
activity component's `RemoveGoal`. Removing the active goal performs activity
selection synchronously. A different activity cleans up the old behavior tree
and starts its replacement through Native's existing controller APIs.

The shipped Territory melee tree enabled `bCallTickOnSearchStart` on its Native
attack service. UE 5.8.3 invokes this optional tick from
`UBTCompositeNode::OnNodeActivation` while a search is in progress and cleanup is
not deferred. The parent `OnChildActivation` retains a pointer into the old
instance memory across this callback, then writes `CurrentChild` through it.
Synchronous cleanup can free that memory before the write.

A separate transient-tree reproduction used the same public `Cleanup` and
`StartTree` calls from a service tick. With the setting enabled, its callback ran
with search active and no cleanup suspension. The normal allocator subsequently
crashed; `-stompmalloc` stopped at the stale write in `OnChildActivation`
(installed AIModule RVA `0xc64d6`). With the setting disabled, the same callback
ran under the engine's deferred-cleanup barrier, completed one replacement and
passed with both allocators. The reproduction does not modify Engine memory or
vendor source.

## Repair and authority

Disable this setting only on the Native attack service in the plugin-owned
`BT_TerritoryAttack_Melee` asset. Its interval (0.3 seconds), random deviation
(0.2 seconds) and timer restart policy remain unchanged. An immediate search tick
is no longer promised; the attack callback follows the configured service
schedule. The shipped ranged tree already used the safe setting.

Native still owns goals, activity selection, attack execution and behavior-tree
cleanup. No controller reparenting, replacement combat authority, vendor asset
edit or Engine patch is required. This is an asset-authoring correction, with no
new save fields, durable identity changes or network protocol changes. Existing
campaigns load the corrected tree normally.

## Regression and limits

`TerritoryFramework.AI.Combat.ActivityExitOutsideSearch` reads the actual melee
and ranged assets' Native attack services and exercises their configured search
tick setting in a real engine tree. It requires an executed callback, a deferred
cleanup boundary and exactly one completed replacement. An unsafe, non-mutating
control verifies that the fixture detects the original boundary. The ordinary
suite reports failure before deliberately writing freed memory; the crashing
variant is isolated evidence, not a shipped test mode.

The Native57 composed run first reported an invalid BT node instance, then an
invalid replication handle and a memory-free crash after assault casualties.
The confirmed mechanism explains how combat-to-patrol replacement can corrupt
unrelated memory, but that run's original instruction-level cause was not
captured. Keep the failed evidence. Require fresh cold-process composed runs
after this repair; passing isolated tests alone does not close that release gate.

The correction was saved with UE 5.7.4 so its package remains readable by both
supported release hosts. Rollback requires the matching previous plugin content;
it reinstates the unsafe behavior and is not a release recommendation.
