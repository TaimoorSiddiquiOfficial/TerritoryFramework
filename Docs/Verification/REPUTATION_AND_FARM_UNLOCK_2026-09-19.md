# Reputation and Farm unlock follow-up

19 September 2026. This is a focused follow-up to the project report `Docs/RECENT_CHANGES_REVIEW_2026-09-19.md`. It does not certify the whole framework for release.

## What changed

### Reputation uses a chosen campaign faction

The old fallback could pick the first connected player's faction. It has been removed. The server must choose the shared reputation subject with `SetReputationSubjectFaction`. With no subject, scores can still change, but reputation cannot automatically change treaties.

The subject is shared campaign data, not a separate score history for each player. A player joining another faction does not silently move this shared ledger. A betrayal quest must deliberately choose whether to change the subject. Changing it does not immediately rewrite existing scores or treaties.

`UTerritoryDiplomacySubsystem` remains the owner of this data. `ATerritoryWorldState` now saves and replicates both the subject and the flag that says whether reputation created a treaty. A change to the subject alone also updates clients.

Treaty ownership is now recorded before callbacks run. If a quest reacts to a reputation-created war by declaring a ceasefire, that ceasefire stays quest-authored. Later reputation changes cannot remove it by mistake. Reasserting the same treaty state also correctly transfers control to the quest.

### Farm owner appears on unlock

Both Farm definitions now use the existing `UTerritoryOwnerHandoverEvent` in **Locked / Exit Events**. The event targets `Territory.HavenReach.CastleHill.Farm`. It reveals the owner before capture and waits for normal interaction; it does not force a dialogue to open.

This follows the requested design: **the owner appears when the Farm unlocks**. The existing Blacksmith ownership condition, automatic capture mode and other state rules remain in place. The existing story-owner spawner owns the NPC lifecycle. No second spawn system was added.

Changed assets:

- `/Game/TerritoryFramework/Definitions/DA_Place_Farm`
- `/TerritoryFramework/Definitions/DA_Place_Farm`

### Explicit force capture reaches the final commit

The public mutation request already allowed the server to bypass story conditions. The final ownership commit ignored that request and checked them again. This caused a valid explicit override to fail.

`UTerritoryControlSubsystem` now passes that option into `ATerritoryVolume::CommitOwnershipData`. It applies only to that call. Normal captures still check their conditions, and clients cannot use it to gain ownership. The result still goes through the same ownership, save and replication path.

### An unready player cannot pass an ownership condition

The live Farm check exposed a startup boundary: Narrative may return no factions before the player state is ready. The old ownership condition treated that as a world-only call and accepted any Claimed owner.

An explicit pawn or controller with no usable faction now fails the condition. Tales-only calls resolve their participant through `UTalesComponent`. A deliberate call with no participant keeps the documented world-level behavior. An explicitly configured Required Owner and the existing special-state exceptions retain their meaning.

This query stores no new state. It rechecks the current Narrative faction every time, including after a faction change or a restored territory owner.

## Narrative Pro patterns reused

| Existing reference | Extension used |
|---|---|
| `ANarrativeGameState::SetFactionAttitude` | Existing Territory diplomacy bridge; no extra faction database. |
| `UNarrativeCondition::CheckCondition` and `UNarrativeNodeBase::AreConditionsMet` | Ownership condition, including Narrative's inherited Not behavior. |
| `ANarrativePlayerCharacter::GetFactions`, `ANarrativePlayerController::GetFactions` | Live identity from Narrative player state; no hardcoded player faction. |
| `UTalesComponent::GetOwningPawn` and `GetOwningController` | Explicit participant context when only Tales is supplied. |
| `UNarrativeEvent` | Existing owner-handover event in the definition's state rules. |
| `UNarrativeSaveSubsystem::CreateActorRecord` and `LoadActorFromRecord` | Actual save records used by the regression tests. |

No Narrative Pro source files were changed. Strategic assault activation behavior was not changed in this batch.

## Verification

Final results from this batch:

| Check | Result |
|---|---|
| UE 5.8 runtime/editor build and UHT | Passed. Final build: `Build58Verified.log`. |
| Full Territory automation suite | **359 passed, 0 failed, 0 not run.** 324 passed without warnings; 35 passed with warnings. |
| Five new native regression tests | All passed without warnings. Covers reputation subject, treaty save/replication, quest callbacks, force-capture conditions and participant readiness. |
| Listen server with two clients | All 7 reputation/network checks passed. |
| Dedicated server with two clients in PIE | All 7 reputation/network checks passed. |
| Farm unlock with an explicit player | All 8 checks passed, including prerequisite refusal, owner reveal before capture, no forced dialogue and no duplicate owner. |
| Farm definition asset validation | Both assets valid; 0 warnings. PIE was stopped for validation. |
| Blueprint gate before PIE | 0 errored Blueprints reported. |
| Option extraction, full reference and easy-English guide checks | All passed. |
| Project and plugin whitespace checks; three Python scripts | Passed. |

The new participant-readiness test initially exposed a fixture mistake: Narrative's `SetFactions` ignores an empty container. The corrected fixture uses `RemoveFaction`; the final full run above is green. This does not hide the earlier failed run.

Raw evidence is in the project's `Saved/Verification/20260919_Reputation/` folder. A compact copy of the results is included in [the verification JSON](REPUTATION_AND_FARM_UNLOCK_2026-09-19.json). Original PIE settings were restored to one-player Listen Server with Run Under One Process enabled. There were no dirty editor packages when verification closed.

The Farm integration fixture stages Blacksmith ownership through the server mutation API. It tests the prerequisite and reveal event; it does **not** simulate defeating the Blacksmith guards or completing Hashir's quest.

The network fixtures use real PIE networking with two clients. They check subject selection, scores, treaty ownership, client mutation refusal, automatic war removal and quest-authored treaty protection. They do not prove the entire campaign or every late-join timing case.

## Migration and authoring

- If automatic reputation diplomacy is enabled, choose the campaign subject explicitly on the server. Existing Blueprint setter pins are unchanged.
- Older saves without a subject pause automatic treaty decisions until the story selects one. Old treaty records without an ownership flag are treated as authored, avoiding accidental removal.
- Player faction changes do not automatically transfer owned territories. Author that consequence as an explicit story action when needed.
- Existing native ownership-commit calls remain source compatible; the new optional argument defaults to false.
- Ownership conditions called with an unready participant now fail instead of accepting any owner. Retry after Narrative identity is ready. Use a deliberate world-only call only when that is the intended rule.
- Neither the Farm change nor the condition change creates a new persistent GUID or a second territory registry. Ownership checks still require the target territory to be loaded.

The options inventory and easy-English guide were refreshed. Their checks cover all 787 editable options and the guide's 263 explained options. A missing explicit ToolTip string alone is not proof of a missing Unreal tooltip: header comments can also supply tooltips.

## Still remaining

1. Play the complete Blacksmith planning, immediate reinforcement, victory, owner handover and Hashir-to-Farm sequence without test setup shortcuts. Verify owner interaction from each relevant client.
2. Exercise assault guards against hidden players, outside shooters and defenders alone; check finite waves and blocked vehicle arrivals.
3. Check the light presets visibly in Narrative shots through UDS day/night changes.
4. Resolve intentional project/plugin definition differences, recruitment access policy and Asset Manager scan/dependency issues from the earlier report.
5. Complete campaign save/reload, World Partition gameplay, supported-version cook/package and UE 5.7 release checks. The current UE 5.8 editor tests are not a packaged dedicated-server certification.
6. Review and commit the mixed working tree, update the project's plugin pointer and complete the GitHub upload. The earlier project upload was blocked by Git LFS allowance; this batch did not retry it.

The next gameplay acceptance step is item 1. This follow-up is local work; no new commit or push was made during it.
