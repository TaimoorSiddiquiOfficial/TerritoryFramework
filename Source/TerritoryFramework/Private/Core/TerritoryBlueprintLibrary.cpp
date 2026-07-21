#include "Core/TerritoryBlueprintLibrary.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
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

bool UTerritoryBlueprintLibrary::IsSameFaction(const FGameplayTag& FactionA, const FGameplayTag& FactionB)
{
	return FactionA == FactionB && FactionA.IsValid();
}
