#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryVolume.h"
#include "TerritoryHierarchy.generated.h"

class UTerritoryEconomySubsystem;
class UTerritoryNavigationMarkerComponent;
class UTerritoryProductionProfile;

/** Shared pure hierarchy rules used by City, District, UI, and native tests. */
namespace TerritoryHierarchyPolicy
{
	/** Returns a faction only when it owns strictly more than half of all children. */
	TERRITORYFRAMEWORK_API FGameplayTag FindStrictMajorityOwner(
		const TArray<FGameplayTag>& ChildOwners);

	/** True only when at least one child exists and every child has the exact faction. */
	TERRITORYFRAMEWORK_API bool AreAllChildrenOwnedBy(
		const TArray<FGameplayTag>& ChildOwners, const FGameplayTag& Faction);

	TERRITORYFRAMEWORK_API float CalculateControlFraction(
		const TArray<FGameplayTag>& ChildOwners, const FGameplayTag& Faction);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ATerritoryCity
//
/// Hierarchy: City contains Districts. City captured when all districts are owned
/// by the same faction. Use GetDistricts() / GetDistrictCount() to iterate.
///
/// Delegates: OnCityCapturedDelegate, OnCityLostDelegate — bind from Blueprint to
/// react to city-wide capture/loss events (UI, quests, economy bonuses).
// ═══════════════════════════════════════════════════════════════════════════════

UCLASS(BlueprintType, Blueprintable, meta=(DisplayName="Territory City", ToolTip="City-level territory — auto-captures when all districts belong to one faction"))
class TERRITORYFRAMEWORK_API ATerritoryCity : public ATerritoryVolume
{
	GENERATED_BODY()

public:
	ATerritoryCity();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Loaded District actors authored by the City Definition, in Definition order. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	TArray<ATerritoryVolume*> GetDistricts() const;

	/** Total authored District slots, including children currently streamed out. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	int32 GetDistrictCount() const;

	/** Check whether all required Districts in this City are owned by the supplied exact faction. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	bool AllDistrictsOwnedBy(FGameplayTag Faction) const;

	/** Return this faction's share of City control from 0 to 1. For example, 0.5 means 50%. */
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

	/** Count Districts marked as capitals within this City. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	int32 GetCapitalDistrictCount() const;

	/** Check whether this City contains a District marked as a capital. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	bool HasCapitalDistrict() const;

	// ─── Blueprint Delegates ───

	UPROPERTY(BlueprintAssignable, Category = "Territory|Hierarchy")
	FOnCityCaptured OnCityCapturedDelegate;

	/** Called when this City loses its previous full-control state. */
	UPROPERTY(BlueprintAssignable, Category = "Territory|Hierarchy")
	FOnCityLost OnCityLostDelegate;

protected:
	/** Blueprint hook called when the City reaches full control by a faction through its District results. */
	UFUNCTION(BlueprintNativeEvent, Category = "Territory|Hierarchy")
	void OnCityFullyCaptured(FGameplayTag CapturingFaction);
	virtual void OnCityFullyCaptured_Implementation(FGameplayTag CapturingFaction);

	/** Blueprint hook called when the City loses its previous full-control state. */
	UFUNCTION(BlueprintNativeEvent, Category = "Territory|Hierarchy")
	void OnCityLost(FGameplayTag PreviousFaction);
	virtual void OnCityLost_Implementation(FGameplayTag PreviousFaction);

	/** Blueprint hook called when a child District is captured within this City. */
	UFUNCTION(BlueprintNativeEvent, Category = "Territory|Hierarchy")
	void OnDistrictCapturedInCity(ATerritoryVolume* District, FGameplayTag OldOwner, FGameplayTag NewOwner);
	virtual void OnDistrictCapturedInCity_Implementation(ATerritoryVolume* District, FGameplayTag OldOwner, FGameplayTag NewOwner);

private:
	UFUNCTION()
	void OnDistrictControlChanged(ATerritoryVolume* District, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void OnDistrictStateChanged(ATerritoryVolume* District, ETerritoryState NewState);

	UFUNCTION()
	void OnDistrictAvailabilityChanged(ATerritoryVolume* District, ETerritoryAvailability NewAvailability);

	UFUNCTION()
	void OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	void BindToDistrict(ATerritoryVolume* District);
	void ReconcileDerivedControl(ATerritoryVolume* ChangedDistrict = nullptr);
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Return this District's loaded parent City, if available. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	ATerritoryCity* GetOwningCity() const;

	/** Loaded Place actors authored by the District Definition, in Definition order. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	TArray<ATerritoryVolume*> GetProperties() const;

	/** Check whether this District's definition marks it as a capital. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	bool IsCapitalDistrict() const;

	/** Returns the number of properties owned by a given faction. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	int32 GetPropertyCountForFaction(FGameplayTag Faction) const;

	/** Returns true if all properties in this district are owned by the given faction. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	bool AllPropertiesOwnedBy(FGameplayTag Faction) const;

	/** Returns the faction that owns the majority of properties, or empty if no majority. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	FGameplayTag GetMajorityPropertyOwner() const;

	/** Sum of effective income from child properties currently held by the district owner. */
	UFUNCTION(BlueprintPure, Category = "Territory|Economy")
	int32 GetEffectiveIncome() const;

	UPROPERTY(Transient)
	bool bIsCapital = false;

	/** Bonus income multiplier applied when this district is a capital (2x by default). */
	UPROPERTY(Transient)
	float CapitalIncomeMultiplier = 2.0f;

	// ─── Blueprint Delegates ───

	UPROPERTY(BlueprintAssignable, Category = "Territory|Hierarchy")
	FOnDistrictCaptured OnDistrictCapturedDelegate;

protected:
	virtual void OnOwnershipChanged_Implementation(FGameplayTag OldOwner, FGameplayTag NewOwner) override;

	/** Blueprint hook called when the District reaches full control through its Place results. */
	UFUNCTION(BlueprintNativeEvent, Category = "Territory|Hierarchy")
	void OnDistrictFullyCaptured(FGameplayTag CapturingFaction);
	virtual void OnDistrictFullyCaptured_Implementation(FGameplayTag CapturingFaction);

private:
	UFUNCTION()
	void OnPropertyControlChanged(ATerritoryVolume* Property, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void OnPropertyStateChanged(ATerritoryVolume* Property, ETerritoryState NewState);

	UFUNCTION()
	void OnPropertyAvailabilityChanged(ATerritoryVolume* Property, ETerritoryAvailability NewAvailability);

	UFUNCTION()
	void OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	void BindToProperty(ATerritoryVolume* Property);
	void ReconcileDerivedControl(ATerritoryVolume* ChangedProperty = nullptr);
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Exact authored District. Returns null rather than guessing from tag prefixes. */
	UFUNCTION(BlueprintPure, Category = "Territory|Hierarchy")
	ATerritoryDistrict* GetOwningDistrict() const;

	UPROPERTY(SaveGame, ReplicatedUsing=OnRep_UpgradeLevel)
	int32 UpgradeLevel = 0;

	UPROPERTY(Transient)
	int32 MaxUpgradeLevel = 3;

	UPROPERTY(Transient)
	int32 UpgradeCostPerLevel = 500;

	UPROPERTY(Transient)
	int32 IncomeBonusPerLevel = 25;

	/** Optional item production definition. Null is a valid capturable, non-producing Property. */
	UPROPERTY(Transient)
	TObjectPtr<UTerritoryProductionProfile> ProductionProfile = nullptr;

	/** Return the Place's current upgrade level. */
	UFUNCTION(BlueprintPure, Category = "Territory|Property")
	int32 GetUpgradeLevel() const { return UpgradeLevel; }

	/** Return the reusable production configuration assigned to this Place. */
	UFUNCTION(BlueprintPure, Category = "Territory|Property|Production")
	UTerritoryProductionProfile* GetProductionProfile() const { return ProductionProfile; }

	/** Check whether the Place meets the current requirements for another upgrade. */
	UFUNCTION(BlueprintPure, Category = "Territory|Property")
	bool CanUpgrade() const;

	/** Return the calculated cost for the next Place upgrade. */
	UFUNCTION(BlueprintPure, Category = "Territory|Property")
	int32 GetUpgradeCost() const;

	/** Return the Place's income after its current upgrade and other applicable income modifiers. */
	UFUNCTION(BlueprintPure, Category = "Territory|Property")
	int32 GetEffectiveIncome() const;

	/** Request a Place upgrade on the server. Check the result before showing success or granting benefits. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Property")
	bool TryUpgrade(AActor* Requester);

	/** Internal restore/reset helper. Blueprint gameplay must use TryUpgrade. */
	void SetUpgradeLevel(int32 NewLevel);

	// ─── Blueprint Delegates ───

	UPROPERTY(BlueprintAssignable, Category = "Territory|Hierarchy")
	FOnPropertyCaptured OnPropertyCapturedDelegate;

	/** Blueprint hook called after this Place completes a verified capture. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory|Property")
	void OnPropertyCaptured(FGameplayTag NewOwner);
	virtual void OnPropertyCaptured_Implementation(FGameplayTag NewOwner);

protected:
	// Override base class ownership change to invoke property-specific side effects
	// on every ownership path (direct capture, hierarchy cascade, quest event).
	virtual void OnOwnershipChanged_Implementation(FGameplayTag OldOwner, FGameplayTag NewOwner) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_UpgradeLevel();

	/** Blueprint hook called after the Place's upgrade level changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Territory|Property")
	void OnUpgradeLevelChanged(int32 NewLevel);

private:
	/** Update plugin state without invoking Blueprint or inventory callbacks. */
	void ApplyUpgradeLevel(int32 NewLevel);
	void PublishUpgradeLevelChange(int32 OldLevel);
};
