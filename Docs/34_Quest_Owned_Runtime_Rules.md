# Quest-Owned Territory Runtime Rules

Use a Quest Runtime Override when a Narrative Quest must temporarily control a City, District,
or Place. The Quest becomes an override layer; it does not become a second Territory system.

## Easy example

`NQ_RescueThePrisoner` takes place inside Blacksmith. While the Quest is **In Progress**:

- normal State Config conditions and rewards do not run;
- walking into the Place does not begin normal bounds/capture-point progress;
- ownership changes do not create the normal automatic counterattack;
- the Quest may still call **Capture Territory**, **Try Unlock Territory**, or **Wave of Enemies**.

When the Quest succeeds or fails, Blacksmith continues its normal rules from its current live
owner, availability, and state. An event skipped during the Quest is not replayed. This prevents
duplicate XP, money, diplomacy, or unlock rewards.

## Setup

1. Open the City, District, or Place Territory Definition.
2. Open **05 State Rules > Quest Override > Quest Runtime Overrides**.
3. Add a row and select the Narrative Quest class.
4. Keep **Active Quest State = In Progress** for the normal “pause until Quest ends” behavior.
5. Choose which primary systems pause:
   - **State Rules and State Events**;
   - **Automatic Capture and Contesting**;
   - **Automatic Counterattacks**.
6. Enable **Include Child Territories** only when a City rule should affect its Districts/Places,
   or a District rule should affect its Places.

A Place row affects only that Place. Parent aggregation always remains active: a District and City
still report the truthful control derived from their Places even while their State Config events
are paused.

## Explicit Quest work versus automatic work

| Action | During matching Quest override |
|---|---|
| Capture point / story bounds pressure | Paused when Automatic Capture is selected |
| State Config entry/exit conditions | Bypassed when State Rules is selected |
| State Config entry/exit events | Skipped once; never replayed later |
| Automatic recapture after ownership change | Paused when Counterattacks is selected |
| Recurring strategic schedule | Paused |
| Existing physical battle | Continues; actors are not silently despawned |
| Narrative Capture/Lock/Unlock event | Allowed through the authoritative mutation path |
| Narrative Wave of Enemies event | Allowed as explicit Quest work |

The Blueprint query **Is Primary Territory Rule Suspended** explains which effect is paused and
which Territory Definition supplied the matching rule.

## Wait Time condition

**Territory Wait Time Condition** is a deterministic gate, not a latent delay. It passes on the
next evaluation after its selected clock reaches `Wait Time Seconds`.

- **Narrative Campaign Elapsed Time** is saved and is the recommended story clock.
- **Current World Elapsed Time** restarts when the world starts and is useful for temporary levels.

Easy example: set Narrative Campaign time and `600` seconds to allow a rule after ten minutes of
saved campaign play. A one-shot Narrative Event is not held in memory when this condition fails;
use it on a rule/task that evaluates again.

## Immediate enemy wave

On **Wave of Enemies**, enable **Start Counterattack Immediately** for an authored ambush or Quest
climax. Immediate deployment skips grace, time window, strategic chance, warning delay, and player
proximity. It still rejects an invalid owner, diplomacy, invalid finite force, invalid Narrative NPC
definition, missing approach, or broken navigation/vehicle route. An explicit Narrative Wave does
not require the automatic counter system's secure-District, Reinforcements-capability, or profile
Quest gates. Any later automatic recurrence uses those strategic gates again.

## Multiplayer rule

Territory is shared world state. If any online Narrative player matches the assigned Quest state,
the override is active for the shared Territory. The server decides; clients do not advance
different versions of the same City, District, or Place.
