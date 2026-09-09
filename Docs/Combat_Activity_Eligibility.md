# Combat activity eligibility

Territory attack activities must check whether the NPC may fight **now** before
scoring its Narrative attack goal. Having a remembered target is not enough.

In a custom `ScoreGoalItem` graph, call `Can Score Territory Combat Goal` with
the activity's `OwnerController` and the input `Goal`. When it returns false,
return **0**. When it returns true, continue the existing Narrative scorer.
Zero pauses a goal; a negative score tells Narrative to remove it.

The shared melee, ranged strafe and grenade activities include this check.
TDA's three project activities include it too. A project that overrides
`ScoreGoalItem` without calling these implementations must add the same check.
The shared assets are saved with UE 5.7 and can also load in UE 5.8.

For an assault NPC, the function uses its existing participant and
`CanEngageAssaultTarget` rules. Vehicle boarding and travel, an escape mission,
a retired force, an unloaded mission target, an inactive assault or diplomacy
that does not permit fighting cannot start an attack activity. For a guard, it
uses `CanEngageTerritoryTarget`, including defence of an arriving hostile force
and the current Place policy. An unpossessed controller or a controller driving
a vehicle cannot select on-foot combat. Invalid, dead, self and foreign-world
targets are rejected. This is a server query.

Ordinary Narrative NPCs keep their existing combat policy. Narrative still owns
perception, attack goals, weapon and range scoring, tactical attack tokens,
ability execution, damage and friendly-fire protection. The helper does not
create targets or remove goals, change faction identity, or write campaign state.
After arrival, the same goal can be scored again by Native activity selection.

There are no new save fields, replicated fields, stable IDs or ownership
transitions. Existing saves derive eligibility from the restored participant,
mission target and diplomacy. No Narrative Pro source or asset is changed.

## Diagnosing same-team damage messages

Native `NarrativeDamageExecCalc` reports a same-team shot when its friendly-fire
check rejects damage. This message also applies to melee damage that hits a
friendly bystander. Check the selected activity's goal and target attitude before
concluding that an NPC deliberately selected a friendly target. Keep Native's
damage protection enabled.

The September 9 pre-fix server/two-client observation recorded 569 combat
activity selections during pending vehicle ingress across two Native world/player
restores. It recorded no selected friendly target and no saved default attack
goal. The default Native attack goal and generator are not saved; deliberately
saved custom subclasses need their own restore verification.

The final run checked 3,028 samples on a listen server with two clients, including
two Native world/player restores and a peace transition. It recorded zero attack
activities during ingress, zero selected friendly targets and zero combat after
peace. Assault NPCs selected on-foot combat in 414 samples, including after each
restore. The unique temporary save was deleted. This proves the activity gate;
it does not certify every city route or World Partition scenario.

Both engine suites pass all 299 automation tests. The expanded autonomy test
executes the actual combat Blueprints, pauses them during ingress and escape,
then resumes the same goal after arrival. It also checks client authority,
foreign/null goals, self targets, unpossession and restored mission identity.
The separate inventory test uses the existing story NPC Blueprint so Native's
stable save-ID contract remains intact.
