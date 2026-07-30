# Territory Framework — Complete Integration Guide

> **Plugin:** TerritoryFramework (v0.2.5 plus unreleased audit fixes)
> **Depends on:** Narrative Pro public APIs (integration verified against the installed 2.3.3 build)
> **Engine:** UE 5.7
> **Docs Location:** `Plugins/TerritoryFramework/Docs/`
> **Vendor rule:** Narrative Pro source/assets are read-only; all compatibility code lives in TerritoryFramework.

## Current Implementation Limits

- `ATerritoryWorldState` is the persistence/late-join projection. Server subsystem delegates keep its economy, diplomacy, capture, and assault arrays current; RepNotify handlers hydrate client query subsystems.
- Narrative inventories remain the only currency authority. Loaded player/NPC faction members participate automatically; shared/leader policies may register an explicit live Narrative account. TerritoryFramework never saves a second balance.
- Saved contests restore the leading faction/progress for decay, but attacker identities and non-leading faction progress are not persisted.
- Guard-post reserve counts and finite active counts persist. Individual pawn health/activity and live UObject pointers do not; surviving counts are reconstructed as new Narrative NPCs.
- Narrative attitudes collapse multiple friendly treaty types to the same attitude when treaty metadata is rebuilt from GameState alone; WorldState/SavableData restore the richer saved metadata directly.
- Counterattacks require a `UTerritoryCounterAttackProfile`, at least one typed approach with a valid navigation route, and Narrative NPC definitions whose class derives from `ATerritoryAssaultCharacter`. Offscreen capture simulation is intentionally disabled.

## Table of Contents

1. [Quick Start](01_Quick_Start.md) — 5-minute setup guide
2. [Interfaces](02_Interfaces.md) — Interface contracts, real examples, multi-domain usage
3. [Core Actors](03_Core_Actors.md) — TerritoryVolume, City, District, Property, GuardSpawnPoint
4. [Subsystems](04_Subsystems.md) — Registry, Control, Economy, Diplomacy, Combat
5. [Guard System](05_Guard_System.md) — Guard spawning, patrol routes, reserves, BT integration
6. [Narrative Pro Integration](06_Narrative_Integration.md) — Factions, GAS, Tales, Save, Navigation
7. [Economy System](07_Economy_System.md) — Income, transactions, upgrades, NarrativePro currency bridge
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
19. [Deep Reaudit](DEEP_REAUDIT_2026-07-30.md) — Confirmed fixes, remaining project gates, MCP findings
20. [Blueprint Extension Guide](Blueprint_Extension_Guide.md) — Subclassing patterns, Super-call requirements
21. [Blueprint Setup Tutorial](Blueprint_Setup_Tutorial.md) — Step-by-step Blueprint configuration

## Document Index

| Doc | Audience | What You'll Learn |
|---|---|---|
| Quick Start | All | Place territory, assign faction, capture in PIE |
| Interfaces | C++/BP Devs | How to implement ITerritoryOwnershipInterface on ANY actor |
| Core Actors | Level Designers | Configure City/District/Property hierarchy |
| Subsystems | Programmers | Query and mutate territory state from code |
| Guard System | AI Designers | Spawn points, patrol routes, reserve guards |
| Narrative Integration | All | How Territory extends Narrative without modifying it |
| Economy | Game Designers | Faction wealth (NarrativePro Currency), income ticks, property upgrades |
| Diplomacy | Game Designers | War/peace/alliance/non-aggression treaties with Narrative attitudes |
| Map & Navigation | UI Designers | Map markers, ownership colors, outline painting |
| Save/Load | Programmers | Narrative save adapter, stable GUIDs, WorldState |
| Debug System | All | Enable debug in Project Settings, read output |
| Blueprint Reference | BP Devs | Every Blueprint-exposed function, property, delegate |
| Multiplayer | Programmers | Server authority, client replication, known limits |
| API Reference | C++ Devs | Complete function signatures with return types |
| AI Integration | AI Designers | CombatDirector, BT tasks, Tales events/conditions |
| District Management | Game/UI Designers | In-world UI, guard purchasing, POI markers |
| Counterattack System | AI/Game Designers | Configure deterministic physical counterattacks |
| Operations UI | UI/Game Designers | Build Narrative CommonUI operations, guard, finance, and threat screens |
| Deep Reaudit | Maintainers | Findings, ownership, migrations, and verification evidence |
| Blueprint Extension Guide | C++/BP Devs | Subclassing patterns, Super-call requirements |
| Blueprint Setup Tutorial | BP Devs | Step-by-step Blueprint configuration |
