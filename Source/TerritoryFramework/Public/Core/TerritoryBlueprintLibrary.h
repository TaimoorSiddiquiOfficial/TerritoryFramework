#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryBlueprintLibrary.generated.h"

class ATerritoryVolume;
class UTerritoryRegistrySubsystem;
class UTerritoryControlSubsystem;
class UTerritoryEconomySubsystem;
class UTerritoryCombatDirector;

UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static UTerritoryRegistrySubsystem* GetTerritoryRegistry(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static UTerritoryControlSubsystem* GetTerritoryControl(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static UTerritoryEconomySubsystem* GetTerritoryEconomy(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static UTerritoryCombatDirector* GetTerritoryCombatDirector(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static ATerritoryVolume* GetTerritoryAtLocation(const UObject* WorldContextObject, const FVector& WorldLocation);

	UFUNCTION(BlueprintCallable, Category = "Territory", meta = (WorldContext = "WorldContextObject"))
	static ATerritoryVolume* GetTerritoryByTag(const UObject* WorldContextObject, const FGameplayTag& TerritoryTag);

	UFUNCTION(BlueprintPure, Category = "Territory")
	static bool IsSameFaction(const FGameplayTag& FactionA, const FGameplayTag& FactionB);
};
