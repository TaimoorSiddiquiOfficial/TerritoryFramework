#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "TerritoryBlueprintLibrary.generated.h"

class ATerritoryVolume;
class UTerritoryRegistrySubsystem;
class UTerritoryControlSubsystem;
class UTerritoryEconomySubsystem;
class UTerritoryCombatDirector;
class UTerritoryDiplomacySubsystem;
class ATerritoryCity;
class ATerritoryDistrict;

UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ─── Subsystem Access ───

	UFUNCTION(BlueprintCallable, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static UTerritoryRegistrySubsystem* GetTerritoryRegistry(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static UTerritoryControlSubsystem* GetTerritoryControl(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static UTerritoryEconomySubsystem* GetTerritoryEconomy(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static UTerritoryCombatDirector* GetTerritoryCombatDirector(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static UTerritoryDiplomacySubsystem* GetTerritoryDiplomacy(const UObject* WorldContextObject);

	// ─── Territory Queries ───

	UFUNCTION(BlueprintCallable, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static ATerritoryVolume* GetTerritoryAtLocation(const UObject* WorldContextObject, const FVector& WorldLocation);

	UFUNCTION(BlueprintCallable, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static ATerritoryVolume* GetTerritoryByTag(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag);

	UFUNCTION(BlueprintPure, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static TArray<ATerritoryVolume*> GetAllTerritories(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static TArray<ATerritoryVolume*> GetTerritoriesByFaction(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	UFUNCTION(BlueprintPure, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static TArray<ATerritoryVolume*> GetChildTerritories(const UObject* WorldContextObject, const FGameplayTag& ParentTag);

	UFUNCTION(BlueprintPure, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static int32 GetTerritoryCount(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static int32 GetFactionTerritoryCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	UFUNCTION(BlueprintPure, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static bool IsTerritoryAtLocation(const UObject* WorldContextObject, const FVector& WorldLocation);

	// ─── Economy Shortcuts ───

	UFUNCTION(BlueprintPure, Category = "Territory|Economy", meta = (WorldContext = "WorldContextObject"))
	static int32 GetFactionGold(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	UFUNCTION(BlueprintPure, Category = "Territory|Economy", meta = (WorldContext = "WorldContextObject"))
	static int32 GetFactionIncome(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	UFUNCTION(BlueprintPure, Category = "Territory|Economy", meta = (WorldContext = "WorldContextObject"))
	static TArray<FGameplayTag> GetAllFactions(const UObject* WorldContextObject);

	// ─── Capture Shortcuts ───

	UFUNCTION(BlueprintPure, Category = "Territory|Capture", meta = (WorldContext = "WorldContextObject"))
	static ETerritoryState GetTerritoryState(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag);

	UFUNCTION(BlueprintPure, Category = "Territory|Capture", meta = (WorldContext = "WorldContextObject"))
	static float GetCaptureProgress(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag);

	UFUNCTION(BlueprintCallable, Category = "Territory|Capture", meta = (WorldContext = "WorldContextObject"))
	static void ForceCaptureTerritory(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag, const FGameplayTag& FactionTag);

	// ─── Diplomacy Shortcuts ───

	UFUNCTION(BlueprintPure, Category = "Territory|Diplomacy", meta = (WorldContext = "WorldContextObject"))
	static EDiplomacyState GetTreatyState(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB);

	UFUNCTION(BlueprintPure, Category = "Territory|Diplomacy", meta = (WorldContext = "WorldContextObject"))
	static bool IsAllied(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB);

	UFUNCTION(BlueprintPure, Category = "Territory|Diplomacy", meta = (WorldContext = "WorldContextObject"))
	static bool IsAtWar(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB);

	// ─── Utility ───

	UFUNCTION(BlueprintPure, Category = "Territory")
	static bool IsSameFaction(const FGameplayTag& FactionA, const FGameplayTag& FactionB);

	// ─── Narrative Pro Faction Bridge ───

	/** Returns all factions for an actor via Narrative Pro's INarrativeTeamAgentInterface. */
	UFUNCTION(BlueprintPure, Category = "Territory|Factions", meta = (WorldContext = "WorldContextObject"))
	static FGameplayTagContainer GetActorFactions(const UObject* WorldContextObject, AActor* Actor);

	/** Returns true if the actor belongs to the given faction. */
	UFUNCTION(BlueprintPure, Category = "Territory|Factions", meta = (WorldContext = "WorldContextObject"))
	static bool IsActorInFaction(const UObject* WorldContextObject, AActor* Actor, const FGameplayTag& FactionTag);

	/** Returns the first faction tag for an actor, or empty if none. */
	UFUNCTION(BlueprintPure, Category = "Territory|Factions", meta = (WorldContext = "WorldContextObject"))
	static FGameplayTag GetActorPrimaryFaction(const UObject* WorldContextObject, AActor* Actor);

	/** Returns true if two actors are on the same team (via Narrative Pro's UArsenalStatics::IsSameTeam). */
	UFUNCTION(BlueprintPure, Category = "Territory|Factions")
	static bool AreActorsAllied(AActor* A, AActor* B);

	// ─── City / District Queries ───

	/** Returns all cities registered in the world. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy", meta = (WorldContext = "WorldContextObject"))
	static TArray<ATerritoryCity*> GetAllCities(const UObject* WorldContextObject);

	/** Returns all districts registered in the world. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy", meta = (WorldContext = "WorldContextObject"))
	static TArray<ATerritoryDistrict*> GetAllDistricts(const UObject* WorldContextObject);

	/** Returns the city that owns the given district, or nullptr. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy", meta = (WorldContext = "WorldContextObject"))
	static ATerritoryCity* GetCityForDistrict(const UObject* WorldContextObject, ATerritoryDistrict* District);

	/** Returns true if a faction controls all districts in a city. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy", meta = (WorldContext = "WorldContextObject"))
	static bool DoesFactionControlCity(const UObject* WorldContextObject, ATerritoryCity* City, const FGameplayTag& FactionTag);

	/** Returns the number of cities fully controlled by a faction. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy", meta = (WorldContext = "WorldContextObject"))
	static int32 GetFactionCityCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	/** Returns the number of districts fully controlled by a faction. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy", meta = (WorldContext = "WorldContextObject"))
	static int32 GetFactionDistrictCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	/** Returns all capital districts registered in the world. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy", meta = (WorldContext = "WorldContextObject"))
	static TArray<ATerritoryDistrict*> GetCapitalDistricts(const UObject* WorldContextObject);

	// ─── Debug Helpers ───

	/** Prints debug info for a specific territory to screen and log. */
	UFUNCTION(BlueprintCallable, Category = "Territory|Debug", meta = (WorldContext = "WorldContextObject"))
	static void PrintTerritoryDebug(const UObject* WorldContextObject, ATerritoryVolume* Territory, float Duration = 5.f);

	/** Prints debug info for all territories to screen and log. */
	UFUNCTION(BlueprintCallable, Category = "Territory|Debug", meta = (WorldContext = "WorldContextObject"))
	static void PrintAllTerritoryDebug(const UObject* WorldContextObject, float Duration = 5.f);
};
