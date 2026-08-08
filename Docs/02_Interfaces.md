# Interfaces — Multi-Domain Integration Guide

TerritoryFramework provides three interfaces that can be implemented by **any actor** — not just territory volumes. This makes the territory system extensible to players, animals, vehicles, buildings, projectiles, and any other game entity.

---

## Interface Overview

```
ITerritoryOwnershipInterface    — "Who owns this thing?"
ITerritoryEconomyInterface      — "Can this faction afford it?"
ITerritoryEventReceiverInterface — "Notify me when territory changes"
```

---

## 1. ITerritoryOwnershipInterface

**Purpose:** Any actor that needs to expose territory ownership state.

### Contract

```cpp
FGameplayTag GetTerritoryOwner() const;       // Which faction owns this?
float GetTerritoryControlProgress() const;     // 0.0 to 1.0 capture progress
bool IsTerritoryContested() const;             // Is someone fighting over it?
FGameplayTag GetContestingFaction() const;     // Who is attacking (invalid if not contested)
```

### Real-World Examples

#### Example A: Player Character (Player Faction Display)

```cpp
// BP_PlayerCharacter implements ITerritoryOwnershipInterface
// The player reports which faction they belong to based on their current allegiance

FGameplayTag AMyPlayerCharacter::GetTerritoryOwner_Implementation() const
{
    // Return the player's faction — useful for territory UI
    return PlayerFactionTag;  // e.g., "Narrative.Factions.Heroes"
}

float AMyPlayerCharacter::GetTerritoryControlProgress_Implementation() const
{
    // Players don't have capture progress — return 1.0 (fully "owned")
    return 1.0f;
}

bool AMyPlayerCharacter::IsTerritoryContested_Implementation() const
{
    // Check if player is inside a contested territory
    if (UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
    {
        if (ATerritoryVolume* Terr = Registry->GetTerritoryAtLocation(GetActorLocation()))
        {
            return Terr->IsContested();
        }
    }
    return false;
}
```

**Blueprint equivalent:** Add `ITerritoryOwnership` interface to your player Blueprint. Override the three functions.

#### Example B: Vehicle (Mobile Territory Outpost)

```cpp
// A vehicle that acts as a mobile faction outpost
FGameplayTag AMyFactionVehicle::GetTerritoryOwner_Implementation() const
{
    return DriverFaction;  // Faction of whoever is driving
}

float AMyFactionVehicle::GetTerritoryControlProgress_Implementation() const
{
    return VehicleHealth / MaxHealth;  // "Capture" = destroy the vehicle
}

bool AMyFactionVehicle::IsTerritoryContested_Implementation() const
{
    return bUnderAttack;  // True when taking damage
}
```

#### Example C: Animal/Wildlife (Neutral Territory)

```cpp
// A pack animal that creates a neutral "wildlife zone"
FGameplayTag AMyWildlifeAlpha::GetTerritoryOwner_Implementation() const
{
    return FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Wildlife"));
}

float AMyWildlifeAlpha::GetTerritoryControlProgress_Implementation() const
{
    return PackStrength / 100.0f;  // Pack strength determines control
}

bool AMyWildlifeAlpha::IsTerritoryContested_Implementation() const
{
    return bBeingHunted;  // True when predator or player is nearby
}
```

#### Example D: Building (Captureable Structure)

```cpp
// A captureable guard tower
FGameplayTag AMyGuardTower::GetTerritoryOwner_Implementation() const
{
    return GarrisonFaction;
}

float AMyGuardTower::GetTerritoryControlProgress_Implementation() const
{
    return SiegeProgress;  // 0 = fully held, 1 = captured
}

bool AMyGuardTower::IsTerritoryContested_Implementation() const
{
    return bUnderSiege;
}
```

---

## 2. ITerritoryEconomyInterface

**Purpose:** Any system that needs to query or expose faction economy state.

> **Note:** This interface is an extension point for **project actors** (shops, vehicles, custom economies). `UTerritoryEconomySubsystem` provides identical methods but does not implement this interface — use the subsystem directly for framework queries.

### Contract

```cpp
// Legacy query names retained for interface compatibility; they are not wallets.
int32 GetTreasury(FGameplayTag Faction) const;       // Deprecated; no faction balance
bool CanAfford(FGameplayTag Faction, int32 Cost) const; // Deprecated; only Cost==0 succeeds

int32 GetActorCurrency(AActor* Requester) const;       // Narrative inventory currency
int32 GetPeriodicIncome(FGameplayTag Faction) const;   // Income rate
bool CanActorAfford(AActor* Requester, int32 Cost) const;
```

### Real-World Examples

#### Example A: Shop Keeper (Checks Player Can Afford)

```cpp
// A shop that checks the requesting player's Narrative inventory
bool AMyShopKeeper::CanPlayerAffordItem(int32 ItemCost)
{
    return CanActorAfford(PlayerCharacter, ItemCost);
}
```

#### Example B: Vehicle Repair Station (Faction-Shared Economy)

```cpp
// A repair station that charges the requesting player's Narrative inventory
int32 AMyRepairStation::GetRepairCost_Implementation() const
{
    return RepairCostPerVehicle;
}

bool AMyRepairStation::TryRepair(AVehicle* Vehicle)
{
    if (!CanActorAfford(Vehicle->GetInstigator(), GetRepairCost_Implementation()))
    {
        return false;  // Faction can't afford
    }
    // Debit the same exact account through UTerritoryEconomySubsystem
    // Repair the vehicle
    return true;
}
```

#### Example C: Crafting Station (Resource-Based Economy)

```cpp
// A crafting station can keep non-currency materials separate from Narrative currency.
bool AMyCraftingStation::CanPayMaterials(AActor* Requester, int32 Cost) const
{
    return Requester && GetMaterialCount(Requester) >= Cost;
}
```

---

## 3. ITerritoryEventReceiverInterface

**Purpose:** Any actor that needs to react to territory changes — without polling.

### Contract

```cpp
void OnTerritoryControlChanged(FGameplayTag TerritoryTag, FGameplayTag OldOwner, FGameplayTag NewOwner);
void OnTerritoryContested(FGameplayTag TerritoryTag, FGameplayTag ContestingFaction);
void OnTerritoryUncontested(FGameplayTag TerritoryTag);
void OnTerritoryStateChanged(FGameplayTag TerritoryTag, ETerritoryState NewState);
```

### Real-World Examples

#### Example A: Music System (Changes Music on Territory Capture)

```cpp
// A music manager that changes the soundtrack when territory changes hands
void AMyMusicManager::OnTerritoryControlChanged_Implementation(
    FGameplayTag TerritoryTag, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
    if (NewOwner == PlayerFaction)
    {
        PlayVictoryMusic();  // Player captured territory!
    }
    else if (OldOwner == PlayerFaction)
    {
        PlayDefeatMusic();   // Player lost territory!
    }
    else
    {
        PlayNeutralMusic();  // AI vs AI — neutral observation
    }
}
```

#### Example B: Weather System (Atmosphere Changes with Ownership)

```cpp
// Weather changes based on who controls the region
void AMyWeatherSystem::OnTerritoryControlChanged_Implementation(
    FGameplayTag TerritoryTag, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
    if (NewOwner == FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits")))
    {
        SetWeather(WeatherType::Stormy);     // Bandits bring chaos
    }
    else if (NewOwner == FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes")))
    {
        SetWeather(WeatherType::Clear);      // Heroes bring order
    }
}
```

#### Example C: NPC Spawner (Faction-Specific NPCs on Capture)

```cpp
// A spawner that changes its NPC roster when territory changes hands
void AMyFactionSpawner::OnTerritoryControlChanged_Implementation(
    FGameplayTag TerritoryTag, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
    // Despawn old faction NPCs
    DespawnAllNPCs();

    // Spawn new faction NPCs based on owner
    if (NewOwner == BanditsFaction)
    {
        SpawnBanditPatrol();
    }
    else if (NewOwner == SoldiersFaction)
    {
        SpawnSoldierGarrison();
    }
}

void AMyFactionSpawner::OnTerritoryContested_Implementation(
    FGameplayTag TerritoryTag, FGameplayTag ContestingFaction)
{
    // Spawn alarm guards when territory is contested
    SpawnAlarmGuards();
}
```

#### Example D: UI Widget (Updates Territory Status Display)

```cpp
// A HUD widget that updates when ownership changes
void UMyTerritoryHUDWidget::OnTerritoryControlChanged_Implementation(
    FGameplayTag TerritoryTag, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
    // Update the territory status panel
    FString StatusText = FString::Printf(TEXT("%s captured by %s"),
        *TerritoryTag.ToString(), *NewOwner.ToString());
    UpdateStatusText(FText::FromString(StatusText));

    // Flash the territory icon
    PlayFlashAnimation();
}
```

#### Example E: AI Director (Difficulty Scaling)

```cpp
// An AI director that scales difficulty based on territory control
void AMyAIDirector::OnTerritoryControlChanged_Implementation(
    FGameplayTag TerritoryTag, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
    // If player gains territory, increase enemy activity elsewhere
    if (NewOwner == PlayerFaction)
    {
        IncreaseEnemyActivityInOtherTerritories(TerritoryTag);
    }
}
```

#### Example F: Vehicle Spawn Point (Faction Vehicles)

```cpp
// A vehicle depot that spawns faction-appropriate vehicles
void AMyVehicleDepot::OnTerritoryControlChanged_Implementation(
    FGameplayTag TerritoryTag, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
    DespawnVehicles();

    if (NewOwner == HeroesFaction)
    {
        SpawnVehicle(HeroVehicleClass);
    }
    else if (NewOwner == BanditsFaction)
    {
        SpawnVehicle(BanditVehicleClass);
    }
}
```

---

## How to Subscribe to Events

### C++ Method

```cpp
// In your actor's BeginPlay:
void AMyActor::BeginPlay()
{
    Super::BeginPlay();

    if (UTerritoryControlSubsystem* Control = GetWorld()->GetSubsystem<UTerritoryControlSubsystem>())
    {
        Control->OnTerritoryControlChanged.AddDynamic(this, &AMyActor::HandleTerritoryChange);
    }
}

// Or subscribe to a specific territory:
void AMyActor::BeginPlay()
{
    Super::BeginPlay();

    ATerritoryVolume* MyTerritory = ...; // Get your territory
    MyTerritory->OnTerritoryOwnershipChanged.AddDynamic(this, &AMyActor::HandleTerritoryChange);
}
```

### Blueprint Method

1. Get the `TerritoryControl` subsystem (via `GetTerritoryControl` from BlueprintLibrary)
2. Bind event to `OnTerritoryControlChanged` delegate (subsystem-level)
3. Or get a specific territory actor and bind to its `OnTerritoryOwnershipChanged`

---

## Multi-Domain Interface Matrix

| Domain | OwnershipInterface | EconomyInterface | EventReceiverInterface |
|---|---|---|---|
| **Player Character** | Report player faction | Check if player can afford | Update HUD on capture |
| **NPC/Enemy** | Report NPC faction | — | Despawn on territory loss |
| **Vehicle** | Report driver faction | Vehicle repair costs | Change spawn on capture |
| **Animal/Wildlife** | Wildlife faction | — | Flee when contested |
| **Building/Tower** | Garrison faction | Upgrade costs | Toggle defenses |
| **Shop/Vendor** | — | Faction treasury check | Change inventory on capture |
| **Music/Audio** | — | — | Change soundtrack |
| **Weather** | — | — | Change atmosphere |
| **AI Director** | — | — | Scale difficulty |
| **Projectile** | — | — | Award bonus damage in own territory |
| **Quest Giver** | — | — | Lock/unlock quest on capture |
| **Minimap/Map** | — | — | Update territory colors |

---

## Best Practices

1. **Don't implement interfaces on world subsystems** — they're for actors only
2. **Use BlueprintNativeEvent** — allows BP override without C++
3. **Cache faction tags** — `FGameplayTag::RequestGameplayTag` is not free
4. **Unbind delegates in EndPlay** — prevent dangling references
5. **Check validity** — always null-check before accessing territory actors
6. **Use the registry** — `GetTerritoryAtLocation` uses spatial index (O(1))
