# Faction changes and resource storage

Narrative Pro owns faction membership and the player's inventory. TerritoryFramework reads that membership and routes production to one eligible Narrative inventory. Changing faction does not move, reset or duplicate the player's money or items.

## Story faction changes

Use **Set Narrative Player Factions** in a Narrative event with the requesting player's Tales context. Replace membership for a betrayal that ends the old allegiance. Add membership only when the player should keep both affiliations.

The optional **Primary Faction** must be a political faction included in the final membership. It puts that faction first in Narrative PlayerState's existing faction container; Native save and replication retain the membership. No second faction database or saved identity is created. With no explicit primary, the first political membership remains primary. Universal attitude tags such as Hostile All do not represent territory ownership.

The event validates every added tag before calling Native `ANarrativePlayerState::SetFactions` once. A client cannot apply the event to an authoritative PlayerState. An intentionally empty Native membership stays empty; the project fallback applies only to actors without a Native team interface.

For mixed memberships, a hostile faction pair takes precedence over the same-faction dialogue shortcut. Removing the old membership is still the clearest way to author a complete betrayal.

## Choose a storage account

The **Territory Resource Account** component has two binding modes:

| Mode | Use |
|---|---|
| Fixed Faction Storage | A depot or actor that should serve one specific faction. It becomes ineligible if that actor leaves the faction. |
| Follow Owner's Political Faction | A story player's controller or character. It follows the owner's current primary political faction. |

Existing components keep **Fixed Faction Storage** until deliberately migrated. For the story player's controller, choose **Follow Owner's Political Faction**. Native faction notifications, pawn changes and completed loads refresh the binding. A one-second identity check covers Native APIs that do not broadcast a change.

When several actors register for the same faction, the eligible account with the highest **Account Priority** is selected. Equal highest priorities create a visible storage conflict and block production; startup order never chooses whose personal inventory receives the items. Give a designated depot or leader a higher priority, for example 100, while other candidates stay at 0. An account must have authority, belong to the same world and faction, and expose a real Narrative inventory.

`RegisterResourceAccount` reports whether this component is selected after callbacks finish. `IsResourceAccountRegistered` reflects current selection, including replacement or conflicts. Lower-priority components remain candidates and can be selected when the current account leaves. `UnregisterResourceAccount` removes the candidate and stops automatic rebinding until it is explicitly registered again.

The economy subsystem's old two-argument `RegisterFactionResourceAccount` call still works with priority 0. A tie now fails closed instead of silently replacing the previous account. The sole-online-player fallback remains available only when no eligible explicit account or conflict exists.

Only derived status is replicated: selected status, faction and stockpile conflict/availability. Live candidate pointers are not campaign save data. Components rebuild routing after load, possession and streaming. Native inventory remains the save authority for actual balances.

If a Native item callback changes membership or the selected depot while a production recipe is running, the recipe stops. It removes only the output from that recipe and returns its consumed input to the same original inventory. The new depot is not charged and does not receive the interrupted batch. If another callback prevents those quantities from being restored, the existing `RollbackIncomplete` status reports the remaining difference.

A cancelled periodic recipe consumes that cycle and is not replayed after load. A later cycle uses the current selected depot. A manual `ExecuteResourceRecipe` request continues to use its explicit requesting actor; changing an unrelated depot does not redirect that manual request. Completed settlement events can still begin the next story action normally.

## Screens

The economy and district screens resolve the current player's faction. Existing economy screens refresh after Native faction changes and loads. `SetDisplayFaction` with a valid tag deliberately fixes the economy screen to that faction; an empty tag resumes following the player.

## Inactive settings and older APIs

These old project controls never changed built-in gameplay. They are hidden and retained only to read older configuration. Do not copy their values over authored Territory assets automatically.

| Old control | Working authoring location |
|---|---|
| DefaultTerritoryIncome | Territory Definition: `PeriodicIncome` |
| DefaultGuardCost | Territory Definition: `GuardUpkeepPerCycle` |
| DefaultMaxConcurrentAttackers | Territory Definition: `MaxConcurrentAttackers` |
| DefaultPatrolArrivalThreshold | The Narrative activity that consumes the patrol goal |
| DefaultPatrolAcceptanceRadius | The move task in that Narrative patrol activity |
| DefaultPatrolWaitTime | `WaitTime` on each authored patrol node |
| MaxPatrolRouteNodes | Edit the guard post's patrol node array; the old value never imposed a cap |
| EconomyStartingGold | Narrative inventory configuration |
| MaxCaptureHistory | Territory Player Management component: `MaxLiveEventHistory` |

New maps use `ATerritoryWorldState` for single-player and multiplayer persistence. `ATerritorySavableData` remains loadable for old classes/saves, is no longer placeable as a new actor, and is suppressed when WorldState exists. Removing it from an old map is a migration task: retain the old save until the WorldState campaign state has been verified.

The older Request/Release Territory Permission BT tasks show deprecation guidance. Use `UBTService_TerritoryAssaultPermission` on the active combat branch so the strategic slot is held for that branch's lifetime. Narrative attack tokens still own tactical limits per target.

The optional disguise system has implementation and consumers but no configured profile in the audited TDA content. Keep it unconfigured until the story needs it; lack of an example profile does not make the subsystem a stub. Two unused private marker callbacks were removed; the existing map marker remains responsible for ownership/state refresh.

## Restoring a running world

Territory guard replacement now removes old guards from defence immediately and delays physical destruction briefly. This lets Narrative finish reading the territory's saved record before a retiring guard writes its own EndPlay record. Native attack goals targeting those guards are removed through their existing API. No death is fabricated, reserves are not charged again, and unrelated target goals remain.

Narrative's world-load call and player-load path are separate. The verification fixture uses both, then waits for replicated state and faction observation. It checks two consecutive restores and a fresh joining client. Keep using the project's normal Native player-save flow; this change does not introduce another player save system.
