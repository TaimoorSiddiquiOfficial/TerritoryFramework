# Territory Framework — Complete Integration Guide

> **Plugin:** TerritoryFramework (v0.2.7)
> **Depends on:** Narrative Pro 2.4.2 public APIs
> **Engine:** UE 5.7
> **Docs Location:** `Plugins/TerritoryFramework/Docs/`
> **Vendor rule:** Narrative Pro source/assets are read-only; all compatibility code lives in TerritoryFramework.

## Start Here

Read [Territory Framework — Easy Complete Guide](00_Easy_Complete_Guide.md) first. It explains
the complete system in simple English, including the Castle Hill Farm Locked-versus-Contested
example, DataAsset versus saved runtime state, hierarchy, capture, guards, diplomacy,
counterattacks, production, stealth, UI, events, conditions, multiplayer, and debugging.

The numbered documents below are the advanced reference.

## Current Implementation Limits

- `ATerritoryWorldState` is the persistence/late-join projection. Server subsystem delegates keep its economy, diplomacy, capture, and assault arrays current; RepNotify handlers hydrate client query subsystems.
- Narrative inventories remain the only currency authority. Matching online Narrative player characters are the automatic settlement cohort; NPC wallets never participate unless server gameplay registers one as the exact shared/leader account. TerritoryFramework never saves a second balance.
- Saved contests restore the leading faction/progress for decay, but attacker identities and non-leading faction progress are not persisted.
- Guard-post reserve counts and finite active counts persist. Individual pawn health/activity and live UObject pointers do not; surviving counts are reconstructed as new Narrative NPCs.
- Narrative attitudes collapse multiple friendly treaty types to the same attitude when treaty metadata is rebuilt from GameState alone; WorldState/SavableData restore the richer saved metadata directly.
- Counterattacks require a `UTerritoryCounterAttackProfile`, at least one typed approach with a valid navigation route, and Narrative NPC definitions whose class derives from `ATerritoryAssaultCharacter`, uses a Narrative NPC controller, and auto-possesses spawned actors. Offscreen capture simulation is intentionally disabled.

## Numbered learning path

The chapter number is also the recommended reading order. Every number is unique; historical
audits are kept outside this learning path so they cannot be mistaken for current setup guidance.

| Chapter | Guide | Purpose |
|---:|---|---|
| 00 | [Easy Complete Guide](00_Easy_Complete_Guide.md) | Plain-English source of truth and Castle Hill examples |
| 01 | [Quick Start](01_Quick_Start.md) | Place, configure, and capture a Territory in five minutes |
| 02 | [Interfaces](02_Interfaces.md) | Reusable ownership, economy, and event contracts |
| 03 | [Core Actors](03_Core_Actors.md) | City, District, Place, guards, capture, and management actors |
| 04 | [Subsystems](04_Subsystems.md) | Registry, control, economy, diplomacy, combat, and queries |
| 05 | [Guard System](05_Guard_System.md) | Spawn posts, patrols, reserves, collision, and behavior |
| 06 | [Narrative Pro Integration](06_Narrative_Integration.md) | Factions, Tales, GAS, save, AI, navigation, UI, and vendor boundary |
| 07 | [Economy System](07_Economy_System.md) | Currency, transactions, upgrades, and Narrative inventory |
| 08 | [Diplomacy System](08_Diplomacy_System.md) | War, peace, alliance, reputation, attitude, and dialogue |
| 09 | [Map and Navigation](09_Map_Navigation.md) | POIs, compass, map markers, tracking, and outlines |
| 10 | [Save and Load](10_Save_Load.md) | Stable identities, WorldState, SavableData, and late join |
| 11 | [Debug System](11_Debug_System.md) | Logs, visual overlays, reports, commands, and PIE diagnosis |
| 12 | [Blueprint Reference](12_Blueprint_Reference.md) | Blueprint functions, properties, delegates, events, and conditions |
| 13 | [Multiplayer Guide](13_Multiplayer.md) | Server authority, replication, ownership, and clients |
| 14 | [C++ API Reference](14_API_Reference.md) | Complete native symbol catalog |
| 15 | [AI Integration](15_AI_Integration.md) | Combat direction, attack tokens, goals, patrols, and Tales |
| 16 | [District Management](16_District_Management.md) | In-world access point, actions, perks, and guard purchasing |
| 17 | [Counterattack System](17_Counterattack_System.md) | Scheduling, staging, finite waves, capture goal, and vehicles |
| 18 | [Operations UI](18_Operations_UI.md) | Command Center, intelligence, notifications, lists, and controls |
| 19 | [Garrison Command Flow](19_Garrison_Command_Flow.md) | Capture-to-staffing lifecycle and management authority |
| 20 | [Resource Production](20_Resource_Production.md) | Inputs, outputs, cycles, inventory, upgrades, and notifications |
| 21 | [Territory Definition Assets](21_Definition_Assets.md) | One modular authoring source per City, District, and Place |
| 22 | [Hierarchy, Availability, and Unlocks](22_Hierarchy_Availability_and_Unlocks.md) | Bottom-up control and top-down availability rules |
| 23 | [Road Missions](23_Road_Missions.md) | ZoneGraph roads, reinforcement arrivals, traffic, and chases |
| 24 | [Stealth Infiltration](24_Stealth_Infiltration.md) | Evidence, investigation, multi-floor stealth, and escalation |
| 25 | [Disguise and Double-Agent Missions](25_Disguise_and_Double_Agent_Missions.md) | Clothing identity, checkpoints, exposure, and diplomacy safety |
| 26 | [Narrative Abilities, Skills, and Territory](26_Narrative_Abilities_Skills_and_Territory.md) | Existing abilities, capability growth, damage, and integration choices |
| 27 | [Narrative Music and State Audio](27_Narrative_Music_and_State_Audio.md) | State music, cues, hierarchy fallback, and multiplayer playback |
| 28 | [Story Outcome Preview](28_Story_Outcome_Preview.md) | Read-only scenario summaries generated from Definitions |
| 29 | [Territory Narrative Quest Tasks](29_Narrative_Quest_Tasks.md) | Capture, state, garrison, assault, and disguise tasks |
| 30 | [Community Narrative Quest Tasks](30_Community_Narrative_Tasks.md) | Boss, chase, movement, GAS, combat, and AI task library |
| 31 | [CI Artifacts](31_CI_Artifacts.md) | Unreal 5.7/5.8 self-hosted builds and downloadable artifacts |
| 32 | [Narrative Quest Cascade Recipes](32_Narrative_Quest_Cascade_Recipes.md) | Reusable story graphs that generate normal Narrative Quests |
| 33 | [Territory Asset Creation Menu](33_Territory_Asset_Creation_Menu.md) | Dedicated Add/New category for every Territory Definition, profile, recipe, and core Blueprint template |
| 34 | [Quest-Owned Runtime Rules](34_Quest_Owned_Runtime_Rules.md) | Pause primary state/capture/counter rules while a Narrative Quest owns the story flow |

## Choose by role

- **New community developer:** 00, 01, 33, 06, 21, 22, 29, 30, then 32.
- **Level or mission designer:** 03, 05, 17, 21–30, then 32–34.
- **AI/combat designer:** 05, 08, 15, 17, 23–26, and 30.
- **UI designer:** 09, 16, 18, 27–30.
- **Blueprint developer:** 02, 04, 12, 29, 30, 32, 33, plus the extension/setup appendices below.
- **C++ or multiplayer developer:** 02, 04, 10, 13, 14, and 31.

## Appendices

- [Blueprint Extension Guide](Blueprint_Extension_Guide.md) — safe subclassing and Super-call rules.
- [Blueprint Setup Tutorial](Blueprint_Setup_Tutorial.md) — detailed Blueprint setup walkthrough.
- [Story Capture and Combat Staging](StoryCaptureAndCombatStaging.md) — authored owner handover and staged-fight patterns.
- [Remaining Work and Roadmap](ROADMAP_AND_REMAINING.md) — current release gates, engineering debt, and ranked gameplay ideas.
- [Changelog](CHANGELOG.md) — chronological implementation history.

## Release evidence and historical audits

These reports explain what was found and tested on their stated dates. Use the numbered guides
above for current setup; use reports for traceability.

- [Rendered HUD and authoring follow-up — 2026-09-05](RENDERED_FOLLOWUP_2026-09-05.md)
- [Territory Framework + Narrative Pro deep audit — 2026-09-05](TERRITORY_NARRATIVE_PRO_DEEP_AUDIT_2026-09-05.md)
- [Narrative damage and dual-target re-audit — 2026-09-03](NARRATIVE_DAMAGE_REAUDIT_2026-09-03.md)
- [Complete system re-audit — 2026-09-01](COMPLETE_SYSTEM_REAUDIT_2026-09-01.md)
- [Vision and complete re-audit — 2026-08-25](VISION_AND_COMPLETE_REAUDIT_2026-08-25.md)
- [Stabilization re-audit — 2026-08-25](STABILIZATION_REAUDIT_2026-08-25.md)
- [Current remediation evidence — 2026-08-24](REMEDIATION_2026-08-24.md)
- [Audit summary — 2026-08-09](AUDIT_SUMMARY_2026-08-09.md)
- [Cascade audit — 2026-08-09](CASCADE_AUDIT_2026-08-09.md)
- [Deep re-audit — 2026-08-09](DEEP_REAUDIT_2026-08-09.md)
- [Final re-audit — 2026-08-09](FINAL_REAUDIT_2026-08-09.md)
- [Release gates — 2026-08-09](RELEASE_GATES_2026-08-09.md)
- [Garrison Command re-audit — 2026-08-06](DEEP_REAUDIT_2026-08-06_GARRISON_COMMAND.md)
- [Earlier deep re-audit — 2026-07-30](DEEP_REAUDIT_2026-07-30.md)
