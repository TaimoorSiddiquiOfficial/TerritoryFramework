# Territory Framework — Complete Integration Guide

> **Plugin:** TerritoryFramework (v0.1.0)  
> **Depends on:** Narrative Pro 2.3.3  
> **Engine:** UE 5.7  
> **Docs Location:** `Plugins/TerritoryFramework/Docs/`

## Table of Contents

1. [Quick Start](01_Quick_Start.md) — 5-minute setup guide
2. [Interfaces](02_Interfaces.md) — Interface contracts, real examples, multi-domain usage
3. [Core Actors](03_Core_Actors.md) — TerritoryVolume, City, District, Property, GuardSpawnPoint
4. [Subsystems](04_Subsystems.md) — Registry, Control, Economy, Diplomacy, Combat
5. [Guard System](05_Guard_System.md) — Guard spawning, patrol routes, reserves, BT integration
6. [Narrative Pro Integration](06_Narrative_Integration.md) — Factions, GAS, Tales, Save, Navigation
7. [Economy System](07_Economy_System.md) — Treasury, income, transactions, upgrades
8. [Diplomacy System](08_Diplomacy_System.md) — Treaties, wars, reputation
9. [Map & Navigation](09_Map_Navigation.md) — Markers, territory outline drawing
10. [Save/Load](10_Save_Load.md) — Savable actors, WorldState, SavableData
11. [Debug System](11_Debug_System.md) — 16 debug toggles, DebugWidget, PIE tips
12. [Blueprint Reference](12_Blueprint_Reference.md) — All BlueprintCallable/Pure/Assignable
13. [Multiplayer Guide](13_Multiplayer.md) — Authority, replication, client behavior
14. [API Reference](14_API_Reference.md) — Complete C++ function signatures

## Document Index

| Doc | Audience | What You'll Learn |
|---|---|---|
| Quick Start | All | Place territory, assign faction, capture in PIE |
| Interfaces | C++/BP Devs | How to implement ITerritoryOwnershipInterface on ANY actor |
| Core Actors | Level Designers | Configure City/District/Property hierarchy |
| Subsystems | Programmers | Query and mutate territory state from code |
| Guard System | AI Designers | Spawn points, patrol routes, reserve guards |
| Narrative Integration | All | How Territory extends Narrative without modifying it |
| Economy | Game Designers | Faction treasury, income ticks, property upgrades |
| Diplomacy | Game Designers | War/peace/alliance treaties with Narrative attitudes |
| Map & Navigation | UI Designers | Map markers, ownership colors, outline painting |
| Save/Load | Programmers | Narrative save adapter, stable GUIDs, WorldState |
| Debug System | All | Enable debug in Project Settings, read output |
| Blueprint Reference | BP Devs | Every Blueprint-exposed function, property, delegate |
| Multiplayer | Programmers | Server authority, client replication, known limits |
| API Reference | C++ Devs | Complete function signatures with return types |
