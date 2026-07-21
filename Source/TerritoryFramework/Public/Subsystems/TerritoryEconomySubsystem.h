#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryEconomySubsystem.generated.h"

class ATerritoryVolume;

USTRUCT(BlueprintType)
struct FTerritoryTreasury
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Economy")
	int32 Gold = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Economy")
	int32 IncomePerTick = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Economy")
	int32 CostsPerTick = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Economy")
	int32 TerritoryCount = 0;
};

UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryEconomySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Territory|Economy")
	int32 GetTreasury(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintCallable, Category = "Territory|Economy")
	int32 GetIncome(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintCallable, Category = "Territory|Economy")
	int32 GetCosts(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintCallable, Category = "Territory|Economy")
	bool CanAfford(const FGameplayTag& Faction, int32 Cost) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy")
	void AddToTreasury(const FGameplayTag& Faction, int32 PositiveAmount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy")
	bool TryDebitTreasury(const FGameplayTag& Faction, int32 PositiveAmount);

	UFUNCTION(BlueprintPure, Category = "Territory|Economy")
	FTerritoryTreasury GetFactionEconomy(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Economy")
	TArray<FGameplayTag> GetAllFactionsWithTreasury() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Economy")
	void RecalculateIncome(const FGameplayTag& Faction);

	UPROPERTY(BlueprintAssignable, Category = "Territory|Economy")
	FOnEconomyTick OnEconomyTickFired;

	UPROPERTY(EditDefaultsOnly, Category = "Territory|Economy")
	float TickIntervalSeconds = 300.f;

protected:
	void PerformEconomyTick();

private:
	UPROPERTY(SaveGame)
	TMap<FGameplayTag, FTerritoryTreasury> FactionTreasuries;

	FTimerHandle EconomyTickTimerHandle;

	UFUNCTION()
	void OnEconomyTick();

	UFUNCTION()
	void OnTerritoryControlChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered);
};
