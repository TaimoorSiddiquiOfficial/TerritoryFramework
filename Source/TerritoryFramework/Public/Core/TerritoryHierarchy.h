#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryVolume.h"
#include "TerritoryHierarchy.generated.h"

class UTerritoryEconomySubsystem;

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

	UFUNCTION()
	void OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	void BindToDistrict(ATerritoryVolume* District);
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|District")
	bool bIsCapital = false;
};

UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryProperty : public ATerritoryVolume
{
	GENERATED_BODY()

public:
	ATerritoryProperty();

	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	ATerritoryDistrict* GetOwningDistrict() const;

	UPROPERTY(SaveGame, BlueprintReadWrite, ReplicatedUsing = OnRep_UpgradeLevel, Category = "Territory|Property")
	int32 UpgradeLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|Property")
	int32 MaxUpgradeLevel = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|Property")
	int32 UpgradeCostPerLevel = 500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|Property")
	int32 IncomeBonusPerLevel = 25;

	UFUNCTION(BlueprintPure, Category = "Territory|Property")
	bool CanUpgrade() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Property")
	int32 GetUpgradeCost() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Property")
	int32 GetEffectiveIncome() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Property")
	bool TryUpgrade();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Property")
	void SetUpgradeLevel(int32 NewLevel);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_UpgradeLevel();

	UFUNCTION(BlueprintImplementableEvent, Category = "Territory|Property")
	void OnUpgradeLevelChanged(int32 NewLevel);
};
