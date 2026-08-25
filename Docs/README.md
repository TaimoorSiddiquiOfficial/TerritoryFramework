# Territory Framework — Complete Integration Guide

> **Plugin:** TerritoryFramework (v0.2.6)
> **Depends on:** Narrative Pro 2.4.2 public APIs
> **Engine:** UE 5.7
> **Docs Location:** `Plugins/TerritoryFramework/Docs/`
> **Vendor rule:** Narrative Pro source/assets are read-only; all compatibility code lives in TerritoryFramework.

## Current Implementation Limits

- `ATerritoryWorldState` is the persistence/late-join projection. Server subsystem delegates keep its economy, diplomacy, capture, and assault arrays current; RepNotify handlers hydrate client query subsystems.
- Narrative inventories remain the only currency authority. Matching online Narrative player characters are the automatic settlement cohort; NPC wallets never participate unless server gameplay registers one as the exact shared/leader account. TerritoryFramework never saves a second balance.
- Saved contests restore the leading faction/progress for decay, but attacker identities and non-leading faction progress are not persisted.
- Guard-post reserve counts and finite active counts persist. Individual pawn health/activity and live UObject pointers do not; surviving counts are reconstructed as new Narrative NPCs.
- Narrative attitudes collapse multiple friendly treaty types to the same attitude when treaty metadata is rebuilt from GameState alone; WorldState/SavableData restore the richer saved metadata directly.
- Counterattacks require a `UTerritoryCounterAttackProfile`, at least one typed approach with a valid navigation route, and Narrative NPC definitions whose class derives from `ATerritoryAssaultCharacter`, uses a Narrative NPC controller, and auto-possesses spawned actors. Offscreen capture simulation is intentionally disabled.

## Table of Contents

1. [Quick Start](01_Quick_Start.md) — 5-minute setup guide
2. [Interfaces](02_Interfaces.md) — Interface contracts, real examples, multi-domain usage
3. [Core Actors](03_Core_Actors.md) — TerritoryVolume, City, District, Property, GuardSpawnPoint
4. [Subsystems](04_Subsystems.md) — Registry, Control, Economy, Diplomacy, Combat
5. [Guard System](05_Guard_System.md) — Guard spawning, patrol routes, reserves, BT integration
6. [Narrative Pro Integration](06_Narrative_Integration.md) — Factions, GAS, Tales, Save, Navigation
7. [Economy System](07_Economy_System.md) — Income, transactions, upgrades, and Narrative currency bridge
8. [Diplomacy System](08_Diplomacy_System.md) — Treaties, wars, reputation
9. [Map & Navigation](09_Map_Navigation.md) — Markers, territory outline drawing
10. [Save/Load](10_Save_Load.md) — Savable actors, WorldState, SavableData
11. [Debug System](11_Debug_System.md) — 16 debug toggles, DebugWidget, PIE tips
12. [Blueprint Reference](12_Blueprint_Reference.md) — All BlueprintCallable/Pure/Assignable
13. [Multiplayer Guide](13_Multiplayer.md) — Authority, replication, client behavior
14. [API Reference](14_API_Reference.md) — Complete C++ function signatures
15. [AI Integration](15_AI_Integration.md) — CombatDirector, BT tasks, Tales integration
16. [District Management](16_District_Management.md) — In-world management UI, guard purchasing, POI markers
17. [Counterattack System](17_Counterattack_System.md) — Deterministic scheduling, proximity activation, finite Narrative NPC forces
18. [Operations UI](18_Operations_UI.md) — Narrative CommonUI district dashboard, filters, guards, finance, and threats
19. [Garrison Command Flow](19_Garrison_Command_Flow.md) — Complete capture, staffing, economy, replication, save, and UI lifecycle
20. [Resource Production](20_Resource_Production.md) — Modular daily inputs/outputs, Narrative item storage, crafting bridge, save, replication, and UI
21. [Current Remediation Report](REMEDIATION_2026-08-24.md) — Resolved findings, migrations, live MCP evidence, and remaining external gates
22. [Stabilization Re-audit](STABILIZATION_REAUDIT_2026-08-25.md) — Runtime/editor/Narrative integration findings, guard and assault fixes, UI redesign, release gate, and product roadmap
23. [Garrison Command Reaudit](DEEP_REAUDIT_2026-08-06_GARRISON_COMMAND.md) — Root causes, implemented fixes, MCP evidence, and remaining release gates
24. [Earlier Deep Reaudit](DEEP_REAUDIT_2026-07-30.md) — Broader confirmed fixes and MCP findings
25. [Blueprint Extension Guide](Blueprint_Extension_Guide.md) — Subclassing patterns, Super-call requirements
26. [Blueprint Setup Tutorial](Blueprint_Setup_Tutorial.md) — Step-by-step Blueprint configuration

## Document Index

| Doc | Audience | What You'll Learn |
|---|---|---|
| Quick Start | All | Place territory, assign faction, capture in PIE |
| Interfaces | C++/BP Devs | How to implement ITerritoryOwnershipInterface on ANY actor |
| Core Actors | Level Designers | Configure City/District/Property hierarchy |
| Subsystems | Programmers | Query and mutate territory state from code |
| Guard System | AI Designers | Spawn points, patrol routes, reserve guards |
| Narrative Integration | All | How Territory extends Narrative without modifying it |
| Economy | Game Designers | Narrative currency payouts, item-resource recipes, production cycles, and property upgrades |
| Diplomacy | Game Designers | War/peace/alliance/non-aggression treaties with Narrative attitudes |
| Map & Navigation | UI Designers | Map markers, ownership colors, outline painting |
| Save/Load | Programmers | Narrative save adapter, stable GUIDs, WorldState |
| Garrison Command Flow | All | End-to-end authority, capture policy, staffing transaction, P&L, replication, migration, and validation |
| Debug System | All | Enable debug in Project Settings, read output |
| Blueprint Reference | BP Devs | Every Blueprint-exposed function, property, delegate |
| Multiplayer | Programmers | Server authority, client replication, known limits |
| API Reference | C++ Devs | Complete function signatures with return types |
| AI Integration | AI Designers | CombatDirector, BT tasks, Tales events/conditions |
| District Management | Game/UI Designers | In-world UI, guard purchasing, POI markers |
| Counterattack System | AI/Game Designers | Configure deterministic physical counterattacks |
| Operations UI | UI/Game Designers | Build Narrative CommonUI operations, guard, finance, and threat screens |
| Current Remediation Report | Maintainers | Current defect status, authority impact, migrations, and verification evidence |
| Deep Reaudit | Maintainers | Findings, ownership, migrations, and verification evidence |
| Blueprint Extension Guide | C++/BP Devs | Subclassing patterns, Super-call requirements |
| Blueprint Setup Tutorial | BP Devs | Step-by-step Blueprint configuration |
