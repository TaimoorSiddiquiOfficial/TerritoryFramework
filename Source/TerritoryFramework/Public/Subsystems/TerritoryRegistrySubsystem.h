#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryRegistrySubsystem.generated.h"

class ATerritoryVolume;

UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Territory")
	void RegisterTerritory(ATerritoryVolume* Territory);

	UFUNCTION(BlueprintCallable, Category = "Territory")
	void UnregisterTerritory(ATerritoryVolume* Territory);

	UFUNCTION(BlueprintCallable, Category = "Territory")
	ATerritoryVolume* GetTerritoryByTag(const FGameplayTag& TerritoryTag) const;

	UFUNCTION(BlueprintCallable, Category = "Territory")
	ATerritoryVolume* GetTerritoryAtLocation(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintCallable, Category = "Territory")
	TArray<ATerritoryVolume*> GetTerritoriesOwnedByFaction(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintCallable, Category = "Territory")
	TArray<ATerritoryVolume*> GetAllTerritories() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	int32 GetTerritoryCount() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	int32 GetTerritoryCountForFaction(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintCallable, Category = "Territory")
	TArray<ATerritoryVolume*> GetChildTerritories(const FGameplayTag& ParentTag) const;

	UPROPERTY(BlueprintAssignable, Category = "Territory")
	FOnTerritoryRegistered OnTerritoryRegistered;

	UPROPERTY(BlueprintAssignable, Category = "Territory")
	FOnTerritoryRegistered OnTerritoryUnregistered;

private:
	UPROPERTY()
	TArray<TObjectPtr<ATerritoryVolume>> RegisteredTerritories;

	TMap<FGameplayTag, TWeakObjectPtr<ATerritoryVolume>> TagToTerritoryMap;
};
