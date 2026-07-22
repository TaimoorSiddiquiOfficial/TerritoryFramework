#include "Core/TerritoryBlueprintLibrary.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

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

// ─── Narrative Pro Faction Bridge ───

FGameplayTagContainer UTerritoryBlueprintLibrary::GetActorFactions(const UObject* WorldContextObject, AActor* Actor)
{
	if (!Actor) return FGameplayTagContainer();
	if (INarrativeTeamAgentInterface* TeamAgent = Cast<INarrativeTeamAgentInterface>(Actor))
	{
		return TeamAgent->GetFactions();
	}
	return FGameplayTagContainer();
}

bool UTerritoryBlueprintLibrary::IsActorInFaction(const UObject* WorldContextObject, AActor* Actor, const FGameplayTag& FactionTag)
{
	if (!Actor || !FactionTag.IsValid()) return false;
	if (INarrativeTeamAgentInterface* TeamAgent = Cast<INarrativeTeamAgentInterface>(Actor))
	{
		return TeamAgent->GetFactions().HasTag(FactionTag);
	}
	return false;
}

FGameplayTag UTerritoryBlueprintLibrary::GetActorPrimaryFaction(const UObject* WorldContextObject, AActor* Actor)
{
	if (!Actor) return FGameplayTag();
	if (INarrativeTeamAgentInterface* TeamAgent = Cast<INarrativeTeamAgentInterface>(Actor))
	{
		FGameplayTagContainer Factions = TeamAgent->GetFactions();
		if (Factions.Num() > 0)
		{
			return Factions.GetByIndex(0);
		}
	}
	return FGameplayTag();
}

bool UTerritoryBlueprintLibrary::AreActorsAllied(AActor* A, AActor* B)
{
	if (!A || !B) return false;
	INarrativeTeamAgentInterface* TeamA = Cast<INarrativeTeamAgentInterface>(A);
	INarrativeTeamAgentInterface* TeamB = Cast<INarrativeTeamAgentInterface>(B);
	if (!TeamA || !TeamB) return false;
	return TeamA->GetFactions().HasAny(TeamB->GetFactions());
}

// ─── City / District Queries ───

TArray<ATerritoryCity*> UTerritoryBlueprintLibrary::GetAllCities(const UObject* WorldContextObject)
{
	TArray<ATerritoryCity*> Result;
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	if (!Registry) return Result;

	TArray<ATerritoryVolume*> All = Registry->GetAllTerritories();
	for (ATerritoryVolume* Vol : All)
	{
		if (ATerritoryCity* City = Cast<ATerritoryCity>(Vol))
		{
			Result.Add(City);
		}
	}
	return Result;
}

TArray<ATerritoryDistrict*> UTerritoryBlueprintLibrary::GetAllDistricts(const UObject* WorldContextObject)
{
	TArray<ATerritoryDistrict*> Result;
	UTerritoryRegistrySubsystem* Registry = GetTerritoryRegistry(WorldContextObject);
	if (!Registry) return Result;

	TArray<ATerritoryVolume*> All = Registry->GetAllTerritories();
	for (ATerritoryVolume* Vol : All)
	{
		if (ATerritoryDistrict* D = Cast<ATerritoryDistrict>(Vol))
		{
			Result.Add(D);
		}
	}
	return Result;
}

ATerritoryCity* UTerritoryBlueprintLibrary::GetCityForDistrict(const UObject* WorldContextObject, ATerritoryDistrict* District)
{
	if (!District) return nullptr;
	return District->GetOwningCity();
}

bool UTerritoryBlueprintLibrary::DoesFactionControlCity(const UObject* WorldContextObject, ATerritoryCity* City, const FGameplayTag& FactionTag)
{
	if (!City || !FactionTag.IsValid()) return false;
	return City->AllDistrictsOwnedBy(FactionTag);
}

int32 UTerritoryBlueprintLibrary::GetFactionCityCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag)
{
	if (!FactionTag.IsValid()) return 0;
	int32 Count = 0;
	for (ATerritoryCity* City : GetAllCities(WorldContextObject))
	{
		if (City && City->AllDistrictsOwnedBy(FactionTag))
		{
			++Count;
		}
	}
	return Count;
}

int32 UTerritoryBlueprintLibrary::GetFactionDistrictCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag)
{
	if (!FactionTag.IsValid()) return 0;
	int32 Count = 0;
	for (ATerritoryDistrict* D : GetAllDistricts(WorldContextObject))
	{
		if (D && D->IsOwnedByFaction(FactionTag))
		{
			++Count;
		}
	}
	return Count;
}

TArray<ATerritoryDistrict*> UTerritoryBlueprintLibrary::GetCapitalDistricts(const UObject* WorldContextObject)
{
	TArray<ATerritoryDistrict*> Result;
	for (ATerritoryDistrict* D : GetAllDistricts(WorldContextObject))
	{
		if (D && D->IsCapitalDistrict())
		{
			Result.Add(D);
		}
	}
	return Result;
}

// ─── Debug Helpers ───

void UTerritoryBlueprintLibrary::PrintTerritoryDebug(const UObject* WorldContextObject, ATerritoryVolume* Territory, float Duration)
{
	if (!Territory) return;

	const FString DebugStr = Territory->GetDebugString();
	UE_LOG(LogTerritory, Log, TEXT("%s"), *DebugStr);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Orange, DebugStr);
	}
}

void UTerritoryBlueprintLibrary::PrintAllTerritoryDebug(const UObject* WorldContextObject, float Duration)
{
	TArray<ATerritoryVolume*> All = GetAllTerritories(WorldContextObject);
	for (ATerritoryVolume* Vol : All)
	{
		if (Vol)
		{
			PrintTerritoryDebug(WorldContextObject, Vol, Duration);
		}
	}
}
