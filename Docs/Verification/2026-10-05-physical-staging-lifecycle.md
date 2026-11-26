# Physical staging clock and generator lifetime

A clean Native socket-PIE composition exposed two defects after the 530-test G8
checkpoint. These are actual integration failures; the earlier gates did not prove
the full composed scenario.

The first vehicle's route was 485.6 cm. Its parked clearance overlapped the next
spawn. The automatic assault entered Active at world 274.507 / campaign 2140.585
and cancelled SpawnFailed at world 294.265 / campaign 2340.697, while the first car
remained until world 358.722. A 120 campaign-unit wait exhausted in about 12 real
world seconds under Native's accelerated clock, before the existing car's physical
120-second hard retirement. The fix counts bounded staging in world seconds and
persists elapsed time; it preserves the diagnostic campaign timestamp, launch
choices and finite vehicle/force accounting. It does not delete the blocking car,
move the entrance, reroll a decision, or relax the retirement setting.

Separately, physical stream-out logged pending-kill OwnerActivityComponent errors
from GoalGenerator_TerritoryAttack. Its EventGraph loops RunEQSQuery -> Delay ->
score goals -> Delay. Native generators are component-owned UObjects and Native
RemoveGoalGenerator only removes array membership. Actor/component EndPlay did not
cancel that child's latent work. Territory EndPlay now cancels generator-owned
latent actions and timers, including detached generators still retained by callbacks.

Regression assertions cover physical versus campaign time, same-frame idempotence,
permanent obstruction, save/cold world clock reset, active and removed generators,
and preservation of unrelated latent work. Final run receipts live outside this
source document; source presence alone is not a passed-test claim.


The repeat build also encountered UE's engine-executable-wide Live Coding mutex
from an unrelated open TDA editor. The isolated release command now explicitly
uses `-NoHotReloadFromIDE`, in addition to `-NoHotReload`. Before modifying its host,
it rejects any editor whose command line or loaded modules reference that host.
This permits an independent cold build without touching the user's live DLLs or
turning off their editor's Live Coding session.

Save snapshots also include world time since the last scheduler tick, without
mutating live state. Repeated snapshots do not double-charge the interval.
