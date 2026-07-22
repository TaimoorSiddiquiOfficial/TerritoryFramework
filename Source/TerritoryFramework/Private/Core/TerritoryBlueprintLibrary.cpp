#include "Core/TerritoryBlueprintLibrary.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Core/TerritoryVolume.h"
#include "Engine/World.h"

UTerritoryRegistrySubsystem* UTerritoryBlueprintLibrary::GetTerritoryRegistry(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
}

UTerritoryControlSubsystem* UTerritoryBlueprintLibrary::GetTerritoryControl(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
}

UTerritoryEconomySubsystem* UTerritoryBlueprintLibrary::GetTerritoryEconomy(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
}

UTerritoryCombatDirector* UTerritoryBlueprintLibrary::GetTerritoryCombatDirector(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UTerritoryCombatDirector>() : nullptr;
}

UTerritoryDiplomacySubsystem* UTerritoryBlueprintLibrary::GetTerritoryDiplomacy(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UTerritoryDiplomacySubsystem>() : nullptr;
}

ATerritoryVolume* UTerritoryBlueprintLibrary::GetTerritoryAtLocation(const UObject* WorldContextObject, const FVector& WorldLocation)
{
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	return Registry ? Registry->GetTerritoryAtLocation(WorldLocation) : nullptr;
}

ATerritoryVolume* UTerritoryBlueprintLibrary::GetTerritoryByTag(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag)
{
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	return Registry ? Registry->GetTerritoryByTag(TerritoryTag) : nullptr;
}

TArray<ATerritoryVolume*> UTerritoryBlueprintLibrary::GetAllTerritories(const UObject* WorldContextObject)
{
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	return Registry ? Registry->GetAllTerritories() : TArray<ATerritoryVolume*>();
}

TArray<ATerritoryVolume*> UTerritoryBlueprintLibrary::GetTerritoriesByFaction(const UObject* WorldContextObject, const FGameplayTag& FactionTag)
{
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	return Registry ? Registry->GetTerritoriesOwnedByFaction(FactionTag) : TArray<ATerritoryVolume*>();
}

TArray<ATerritoryVolume*> UTerritoryBlueprintLibrary::GetChildTerritories(const UObject* WorldContextObject, const FGameplayTag& ParentTag)
{
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	return Registry ? Registry->GetChildTerritories(ParentTag) : TArray<ATerritoryVolume*>();
}

int32 UTerritoryBlueprintLibrary::GetTerritoryCount(const UObject* WorldContextObject)
{
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	return Registry ? Registry->GetTerritoryCount() : 0;
}

int32 UTerritoryBlueprintLibrary::GetFactionTerritoryCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag)
{
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	return Registry ? Registry->GetTerritoryCountForFaction(FactionTag) : 0;
}

bool UTerritoryBlueprintLibrary::IsTerritoryAtLocation(const UObject* WorldContextObject, const FVector& WorldLocation)
{
	return GetTerritoryAtLocation(WorldContextObject, WorldLocation) != nullptr;
}

int32 UTerritoryBlueprintLibrary::GetFactionGold(const UObject* WorldContextObject, const FGameplayTag& FactionTag)
{
	UTerritoryEconomySubsystem* Economy = GetTerritoryEconomy(WorldContextObject);
	return Economy ? Economy->GetTreasury(FactionTag) : 0;
}

int32 UTerritoryBlueprintLibrary::GetFactionIncome(const UObject* WorldContextObject, const FGameplayTag& FactionTag)
{
	UTerritoryEconomySubsystem* Economy = GetTerritoryEconomy(WorldContextObject);
	return Economy ? Economy->GetIncome(FactionTag) : 0;
}

TArray<FGameplayTag> UTerritoryBlueprintLibrary::GetAllFactions(const UObject* WorldContextObject)
{
	UTerritoryEconomySubsystem* Economy = GetTerritoryEconomy(WorldContextObject);
	return Economy ? Economy->GetAllFactionsWithTreasury() : TArray<FGameplayTag>();
}

ETerritoryState UTerritoryBlueprintLibrary::GetTerritoryState(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag)
{
	ATerritoryVolume* Territory = GetTerritoryByTag(WorldContextObject, TerritoryTag);
	return Territory ? Territory->GetTerritoryState() : ETerritoryState::Unclaimed;
}

float UTerritoryBlueprintLibrary::GetCaptureProgress(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag)
{
	ATerritoryVolume* Territory = GetTerritoryByTag(WorldContextObject, TerritoryTag);
	return Territory ? Territory->GetControlProgress() : 0.f;
}

void UTerritoryBlueprintLibrary::ForceCaptureTerritory(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag, const FGameplayTag& FactionTag)
{
	UTerritoryControlSubsystem* Control = GetTerritoryControl(WorldContextObject);
	ATerritoryVolume* Territory = GetTerritoryByTag(WorldContextObject, TerritoryTag);
	if (Control && Territory)
	{
		Control->ForceCapture(Territory, FactionTag);
	}
}

EDiplomacyState UTerritoryBlueprintLibrary::GetTreatyState(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB)
{
	UTerritoryDiplomacySubsystem* Diplomacy = GetTerritoryDiplomacy(WorldContextObject);
	return Diplomacy ? Diplomacy->GetDiplomacyState(FactionA, FactionB) : EDiplomacyState::None;
}

bool UTerritoryBlueprintLibrary::IsAllied(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB)
{
	UTerritoryDiplomacySubsystem* Diplomacy = GetTerritoryDiplomacy(WorldContextObject);
	return Diplomacy ? Diplomacy->IsAllied(FactionA, FactionB) : false;
}

bool UTerritoryBlueprintLibrary::IsAtWar(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB)
{
	UTerritoryDiplomacySubsystem* Diplomacy = GetTerritoryDiplomacy(WorldContextObject);
	return Diplomacy ? Diplomacy->IsAtWar(FactionA, FactionB) : false;
}

bool UTerritoryBlueprintLibrary::IsSameFaction(const FGameplayTag& FactionA, const FGameplayTag& FactionB)
{
	return FactionA == FactionB && FactionA.IsValid();
}
