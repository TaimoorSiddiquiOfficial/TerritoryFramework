#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryEconomyWidget.generated.h"

class UTerritoryEconomySubsystem;

/**
 * Base widget for displaying faction economy information.
 * Extend in Blueprint to create custom economy HUD elements.
 * Auto-binds to economy tick delegate for live updates.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryEconomyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Set which faction's economy to display */
	UFUNCTION(BlueprintCallable, Category = "Territory|Economy|UI")
	void SetDisplayFaction(const FGameplayTag& Faction);

	/** Get current display faction */
	UFUNCTION(BlueprintPure, Category = "Territory|Economy|UI")
	FGameplayTag GetDisplayFaction() const;

	/** Get current treasury gold for the display faction */
	UFUNCTION(BlueprintPure, Category = "Territory|Economy|UI")
	int32 GetCurrentGold() const;

	/** Get current income per tick for the display faction */
	UFUNCTION(BlueprintPure, Category = "Territory|Economy|UI")
	int32 GetCurrentIncome() const;

	/** Get current costs per tick for the display faction */
	UFUNCTION(BlueprintPure, Category = "Territory|Economy|UI")
	int32 GetCurrentCosts() const;

	/** Get number of territories owned by the display faction */
	UFUNCTION(BlueprintPure, Category = "Territory|Economy|UI")
	int32 GetTerritoryCount() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Called every economy tick with updated snapshot */
	UFUNCTION(BlueprintImplementableEvent, Category = "Territory|Economy|UI")
	void OnEconomyUpdated(FGameplayTag Faction, const FTerritoryEconomySnapshot& Snapshot);

	/** Called when a transaction is recorded for the display faction */
	UFUNCTION(BlueprintImplementableEvent, Category = "Territory|Economy|UI")
	void OnTransactionRecorded(const FTerritoryTransaction& Transaction);

private:
	FGameplayTag DisplayFaction;

	UFUNCTION()
	void HandleEconomyTick(FGameplayTag Faction, FTerritoryEconomySnapshot Snapshot);

	UFUNCTION()
	void HandleTransactionRecorded(const FTerritoryTransaction& Transaction);

	void BindDelegates();
	void UnbindDelegates();

	UTerritoryEconomySubsystem* GetEconomySubsystem() const;
};
