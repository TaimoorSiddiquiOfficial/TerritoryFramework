#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryVolume.h"
#include "TerritoryHierarchy.generated.h"

class UTerritoryEconomySubsystem;
class UTerritoryNavigationMarkerComponent;

// ═══════════════════════════════════════════════════════════════════════════════
// ATerritoryCity
// ═══════════════════════════════════════════════════════════════════════════════

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

	/** Returns the faction that controls the majority of districts, or empty if no majority. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	FGameplayTag GetMajorityOwner() const;

	/** Returns true if the city is fully controlled by one faction. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	bool IsFullyCaptured() const;

	/** Returns the capturing faction if fully captured, or empty tag. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	FGameplayTag GetCapturingFaction() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	int32 GetCapitalDistrictCount() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	bool HasCapitalDistrict() const;

	// ─── Blueprint Delegates ───

	UPROPERTY(BlueprintAssignable, Category = "Territory|Hierarchy")
	FOnCityCaptured OnCityCapturedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Territory|Hierarchy")
	FOnCityLost OnCityLostDelegate;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Territory|Hierarchy")
	void OnCityFullyCaptured(FGameplayTag CapturingFaction);
	virtual void OnCityFullyCaptured_Implementation(FGameplayTag CapturingFaction);

	UFUNCTION(BlueprintNativeEvent, Category = "Territory|Hierarchy")
	void OnCityLost(FGameplayTag PreviousFaction);
	virtual void OnCityLost_Implementation(FGameplayTag PreviousFaction);

	UFUNCTION(BlueprintNativeEvent, Category = "Territory|Hierarchy")
	void OnDistrictCapturedInCity(ATerritoryVolume* District, FGameplayTag OldOwner, FGameplayTag NewOwner);
	virtual void OnDistrictCapturedInCity_Implementation(ATerritoryVolume* District, FGameplayTag OldOwner, FGameplayTag NewOwner);

private:
	UFUNCTION()
	void OnDistrictControlChanged(ATerritoryVolume* District, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	void BindToDistrict(ATerritoryVolume* District);

	void CascadeCaptureToProperties(ATerritoryVolume* District, FGameplayTag NewOwner);
};

// ═══════════════════════════════════════════════════════════════════════════════
// ATerritoryDistrict
// ═══════════════════════════════════════════════════════════════════════════════

UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryDistrict : public ATerritoryVolume
{
	GENERATED_BODY()

public:
	ATerritoryDistrict();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	ATerritoryCity* GetOwningCity() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	TArray<ATerritoryVolume*> GetProperties() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	bool IsCapitalDistrict() const;

	/** Returns the number of properties owned by a given faction. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	int32 GetPropertyCountForFaction(FGameplayTag Faction) const;

	/** Returns true if all properties in this district are owned by the given faction. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	bool AllPropertiesOwnedBy(FGameplayTag Faction) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|District")
	bool bIsCapital = false;

	/** Bonus income multiplier applied when this district is a capital (2x by default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|District",
		meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "10.0"))
	float CapitalIncomeMultiplier = 2.0f;

	// ─── Blueprint Delegates ───

	UPROPERTY(BlueprintAssignable, Category = "Territory|Hierarchy")
	FOnDistrictCaptured OnDistrictCapturedDelegate;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Territory|Hierarchy")
	void OnDistrictFullyCaptured(FGameplayTag CapturingFaction);
	virtual void OnDistrictFullyCaptured_Implementation(FGameplayTag CapturingFaction);

private:
	UFUNCTION()
	void OnPropertyControlChanged(ATerritoryVolume* Property, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	void BindToProperty(ATerritoryVolume* Property);
};

// ═══════════════════════════════════════════════════════════════════════════════
// ATerritoryProperty
// ═══════════════════════════════════════════════════════════════════════════════

UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryProperty : public ATerritoryVolume
{
	GENERATED_BODY()

public:
	ATerritoryProperty();

	virtual void BeginPlay() override;

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

	// ─── Blueprint Delegates ───

	UPROPERTY(BlueprintAssignable, Category = "Territory|Hierarchy")
	FOnPropertyCaptured OnPropertyCapturedDelegate;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory|Property")
	void OnPropertyCaptured(FGameplayTag NewOwner);
	virtual void OnPropertyCaptured_Implementation(FGameplayTag NewOwner);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_UpgradeLevel();

	UFUNCTION(BlueprintImplementableEvent, Category = "Territory|Property")
	void OnUpgradeLevelChanged(int32 NewLevel);
};
