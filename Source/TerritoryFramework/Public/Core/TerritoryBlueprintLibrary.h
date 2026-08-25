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

/**
 * Global Blueprint function library for territory queries.
 *
 * All functions are static (no instance needed). Most are BlueprintPure for
 * use in any graph (no exec pin). Use WorldContextObject from any actor to
 * get the subsystem.
 *
 * Quick start:
 *   GetTerritoryControl(MyActor)->IsTerritoryContested(Tag) -> bool
 *   GetTerritoryAtLocation(MyActor, Location) -> ATerritoryVolume*
 *   GetTerritoryEconomy(MyActor)->GetActorCurrency(Requester) -> int32
 *   TerritoryBlueprintLibrary::IsSameFaction(FactionA, FactionB) -> bool
 */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════════════════
	// Subsystem Access (BlueprintPure — no exec pin)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Get the territory registry subsystem (all territory registrations + spatial lookups). */
	UFUNCTION(BlueprintPure, Category="Territory|Subsystem",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory Registry"))
	static UTerritoryRegistrySubsystem* GetTerritoryRegistry(const UObject* WorldContextObject);

	/** Get the territory control subsystem (capture flow, attackers, progress tracking). */
	UFUNCTION(BlueprintPure, Category="Territory|Subsystem",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory Control"))
	static UTerritoryControlSubsystem* GetTerritoryControl(const UObject* WorldContextObject);

	/** Get the territory economy subsystem (faction treasuries, transactions, income ticks). */
	UFUNCTION(BlueprintPure, Category="Territory|Subsystem",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory Economy"))
	static UTerritoryEconomySubsystem* GetTerritoryEconomy(const UObject* WorldContextObject);

	/** Get the combat director subsystem (attack slot management for NPCs). */
	UFUNCTION(BlueprintPure, Category="Territory|Subsystem",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Combat Director"))
	static UTerritoryCombatDirector* GetTerritoryCombatDirector(const UObject* WorldContextObject);

	/** Get the diplomacy subsystem (treaties, reputation, war/peace/alliance). */
	UFUNCTION(BlueprintPure, Category="Territory|Subsystem",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory Diplomacy"))
	static UTerritoryDiplomacySubsystem* GetTerritoryDiplomacy(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Territory Queries (BlueprintPure)
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Returns the territory volume at a world location. Null if no territory contains it.
	 * Uses spatial index for O(1) lookup.
	 *
	 * Example:
	 *   Volume = GetTerritoryAtLocation(Self, PlayerLocation)
	 *   if (Volume != None) { ShowTerritoryUI(Volume); }
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory At Location"))
	static ATerritoryVolume* GetTerritoryAtLocation(const UObject* WorldContextObject, const FVector& WorldLocation);

	/**
	 * Returns a territory by its tag. Null if no territory has that tag.
	 * Tag example: Territory.Marketplace, Territory.Harbor, Territory.Castle
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory By Tag"))
	static ATerritoryVolume* GetTerritoryByTag(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag);

	/** Returns all registered territories in the world. */
	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Get All Territories"))
	static TArray<ATerritoryVolume*> GetAllTerritories(const UObject* WorldContextObject);

	/** Returns all territories owned by the given faction. */
	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territories By Faction"))
	static TArray<ATerritoryVolume*> GetTerritoriesByFaction(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	/** Returns all child territories of a parent tag (districts of a city, properties of a district). */
	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Child Territories"))
	static TArray<ATerritoryVolume*> GetChildTerritories(const UObject* WorldContextObject, const FGameplayTag& ParentTag);

	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory Count"))
	static int32 GetTerritoryCount(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Faction Territory Count"))
	static int32 GetFactionTerritoryCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	/** Fast check — true if any territory contains the location. */
	UFUNCTION(BlueprintPure, Category="Territory|Query",
		meta=(WorldContext="WorldContextObject", DisplayName="Is Territory At Location"))
	static bool IsTerritoryAtLocation(const UObject* WorldContextObject, const FVector& WorldLocation);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Economy Shortcuts
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Deprecated compatibility query. TerritoryFramework does not maintain a faction wallet. */
	UFUNCTION(BlueprintPure, Category="Territory|Economy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Faction Gold", DeprecatedFunction,
			DeprecationMessage="Use TerritoryEconomy->GetActorCurrency(Requester) instead."))
	static int32 GetFactionGold(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	/** Returns the sum of periodic income across all territories owned by faction. */
	UFUNCTION(BlueprintPure, Category="Territory|Economy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Faction Income"))
	static int32 GetFactionIncome(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	/** Returns tags of all factions with any territory ownership. */
	UFUNCTION(BlueprintPure, Category="Territory|Economy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get All Factions"))
	static TArray<FGameplayTag> GetAllFactions(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Capture Shortcuts
	// ═══════════════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintPure, Category="Territory|Capture",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Territory State"))
	static ETerritoryState GetTerritoryState(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag);

	UFUNCTION(BlueprintPure, Category="Territory|Capture",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Capture Progress"))
	static float GetCaptureProgress(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag);

	/**
	 * Immediately transfers ownership of territory to faction. Server-only.
	 * Spawns guards for new owner, despawns for old owner.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Capture",
		meta=(WorldContext="WorldContextObject", DisplayName="Force Capture Territory", DevelopmentOnly))
	static void ForceCaptureTerritory(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag, const FGameplayTag& FactionTag);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Diplomacy Shortcuts
	// ═══════════════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintPure, Category="Territory|Diplomacy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Treaty State"))
	static EDiplomacyState GetTreatyState(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB);

	/** True if factions are in Alliance or higher (War=false, Peace=false). */
	UFUNCTION(BlueprintPure, Category="Territory|Diplomacy",
		meta=(WorldContext="WorldContextObject", DisplayName="Is Allied"))
	static bool IsAllied(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB);

	/** True if factions are at War. */
	UFUNCTION(BlueprintPure, Category="Territory|Diplomacy",
		meta=(WorldContext="WorldContextObject", DisplayName="Is At War"))
	static bool IsAtWar(const UObject* WorldContextObject, const FGameplayTag& FactionA, const FGameplayTag& FactionB);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Utility
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Pure faction equality check. Returns true if both tags have the same GameplayTag value.
	 * No world context needed — purely tag comparison.
	 *
	 * Example:
	 *   if (UTerritoryBlueprintLibrary::IsSameFaction(FactionA, FactionB)) { ... }
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Utility", meta=(DisplayName="Is Same Faction"))
	static bool IsSameFaction(const FGameplayTag& FactionA, const FGameplayTag& FactionB);

	/** Converts a gameplay tag into UI-friendly text without exposing the full hierarchy. */
	UFUNCTION(BlueprintPure, Category="Territory|UI", meta=(DisplayName="Get Friendly Tag Display Name"))
	static FText GetFriendlyTagDisplayName(const FGameplayTag& Tag);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Narrative Pro Faction Bridge
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Returns all factions the actor belongs to via Narrative Pro's INarrativeTeamAgentInterface. */
	UFUNCTION(BlueprintPure, Category="Territory|Factions",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Actor Factions"))
	static FGameplayTagContainer GetActorFactions(const UObject* WorldContextObject, AActor* Actor);

	/** True if actor belongs to the specified faction. */
	UFUNCTION(BlueprintPure, Category="Territory|Factions",
		meta=(WorldContext="WorldContextObject", DisplayName="Is Actor In Faction"))
	static bool IsActorInFaction(const UObject* WorldContextObject, AActor* Actor, const FGameplayTag& FactionTag);

	/** Returns actor's primary faction tag (first from INarrativeTeamAgentInterface). Empty if none. */
	UFUNCTION(BlueprintPure, Category="Territory|Factions",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Actor Primary Faction"))
	static FGameplayTag GetActorPrimaryFaction(const UObject* WorldContextObject, AActor* Actor);

	/**
	 * Tests whether two Narrative team agents share at least one faction tag, matching
	 * Narrative Pro's UArsenalStatics::IsSameTeam semantics.
	 * No WorldContext needed — uses actor's world internally.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Factions", meta=(DisplayName="Are Actors Allied"))
	static bool AreActorsAllied(AActor* A, AActor* B);

	// ═══════════════════════════════════════════════════════════════════════════════
	// City / District Queries
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Returns all registered city volumes in the world. */
	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get All Cities"))
	static TArray<ATerritoryCity*> GetAllCities(const UObject* WorldContextObject);

	/** Returns all registered district volumes in the world. */
	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get All Districts"))
	static TArray<ATerritoryDistrict*> GetAllDistricts(const UObject* WorldContextObject);

	/** Returns the city that contains the given district. Null if district has no parent. */
	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get City For District"))
	static ATerritoryCity* GetCityForDistrict(const UObject* WorldContextObject, ATerritoryDistrict* District);

	/** True if a faction owns all districts in a city (city fully captured). */
	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Does Faction Control City"))
	static bool DoesFactionControlCity(const UObject* WorldContextObject, ATerritoryCity* City, const FGameplayTag& FactionTag);

	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Faction City Count"))
	static int32 GetFactionCityCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Faction District Count"))
	static int32 GetFactionDistrictCount(const UObject* WorldContextObject, const FGameplayTag& FactionTag);

	/** Returns all districts marked as capitols (bIsCapital=true). */
	UFUNCTION(BlueprintPure, Category="Territory|Hierarchy",
		meta=(WorldContext="WorldContextObject", DisplayName="Get Capital Districts"))
	static TArray<ATerritoryDistrict*> GetCapitalDistricts(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════════════════════
	// Debug Helpers
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Prints debug info for one territory to screen + log (owner, state, guards, income). */
	UFUNCTION(BlueprintCallable, Category="Territory|Debug",
		meta=(WorldContext="WorldContextObject", DisplayName="Print Territory Debug", DevelopmentOnly))
	static void PrintTerritoryDebug(const UObject* WorldContextObject, ATerritoryVolume* Territory, float Duration = 5.f);

	/** Prints debug info for ALL territories to screen + log. */
	UFUNCTION(BlueprintCallable, Category="Territory|Debug",
		meta=(WorldContext="WorldContextObject", DisplayName="Print All Territory Debug", DevelopmentOnly))
	static void PrintAllTerritoryDebug(const UObject* WorldContextObject, float Duration = 5.f);
};
