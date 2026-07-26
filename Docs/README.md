# Territory Framework — Complete Integration Guide

> **Plugin:** TerritoryFramework (v0.2.4)
> **Depends on:** Narrative Pro 2.3.3
> **Engine:** UE 5.7
> **Docs Location:** `Plugins/TerritoryFramework/Docs/`
> **Tests:** 47/47 automation test suites passing (contract + functional + integration)

## Current Implementation Limits

- `ATerritoryWorldState` is a replicated save snapshot, not a continuously synchronized authoritative store. Call `ExportPersistentState()` before relying on its economy, diplomacy, reputation, or transaction arrays.
- Faction wealth is derived from online Narrative character inventories. Offline and NPC-only factions do not have a persistent TerritoryFramework currency balance.
- Saved contests restore the leading faction/progress for decay, but attacker identities and non-leading faction progress are not persisted.
- Individual guard health, activity, reserve counts, and active rosters are not persisted. Claimed territories recreate a fresh guard population after load.
- Narrative attitudes collapse multiple friendly treaty types to the same attitude when treaty metadata is rebuilt from GameState alone; WorldState/SavableData restore the richer saved metadata directly.

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
17. [Blueprint Extension Guide](Blueprint_Extension_Guide.md) — Subclassing patterns, Super-call requirements
18. [Blueprint Setup Tutorial](Blueprint_Setup_Tutorial.md) — Step-by-step Blueprint configuration

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
| Blueprint Extension Guide | C++/BP Devs | Subclassing patterns, Super-call requirements |
| Blueprint Setup Tutorial | BP Devs | Step-by-step Blueprint configuration |
