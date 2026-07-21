#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryVolume.h"
#include "TerritoryHierarchy.generated.h"

UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryCity : public ATerritoryVolume
{
	GENERATED_BODY()

public:
	ATerritoryCity();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	TArray<ATerritoryVolume*> GetDistricts() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	int32 GetDistrictCount() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	bool AllDistrictsOwnedBy(FGameplayTag Faction) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	float GetCityControlPercentage(FGameplayTag Faction) const;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Territory|Hierarchy")
	void OnCityFullyCaptured(FGameplayTag CapturingFaction);
	virtual void OnCityFullyCaptured_Implementation(FGameplayTag CapturingFaction);

	UFUNCTION(BlueprintNativeEvent, Category = "Territory|Hierarchy")
	void OnCityLost(FGameplayTag PreviousFaction);
	virtual void OnCityLost_Implementation(FGameplayTag PreviousFaction);

private:
	UFUNCTION()
	void OnDistrictControlChanged(ATerritoryVolume* District, FGameplayTag OldOwner, FGameplayTag NewOwner);
};

UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryDistrict : public ATerritoryVolume
{
	GENERATED_BODY()

public:
	ATerritoryDistrict();

	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	ATerritoryCity* GetOwningCity() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	TArray<ATerritoryVolume*> GetProperties() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	bool IsCapitalDistrict() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|District")
	bool bIsCapital = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|District")
	int32 DefenderSpawnCount = 3;
};

UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryProperty : public ATerritoryVolume
{
	GENERATED_BODY()

public:
	ATerritoryProperty();

	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	ATerritoryDistrict* GetOwningDistrict() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|Property")
	int32 UpgradeLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|Property")
	int32 MaxUpgradeLevel = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|Property")
	int32 UpgradeCostPerLevel = 500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|Property")
	int32 IncomeBonusPerLevel = 25;

	UFUNCTION(BlueprintCallable, Category = "Territory|Property")
	bool CanUpgrade() const;

	UFUNCTION(BlueprintCallable, Category = "Territory|Property")
	int32 GetUpgradeCost() const;

	UFUNCTION(BlueprintCallable, Category = "Territory|Property")
	int32 GetEffectiveIncome() const;
};
