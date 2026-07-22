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
};
