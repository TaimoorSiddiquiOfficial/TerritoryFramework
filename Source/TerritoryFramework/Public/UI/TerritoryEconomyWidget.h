#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "TerritoryEconomyWidget.generated.h"

class UTerritoryEconomySubsystem;
class UTerritoryProductionSiteRowWidget;
class UTerritoryResourceRowWidget;
class UTextBlock;
class UVerticalBox;

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

	/** Get the owning player's Narrative inventory balance. */
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

	UFUNCTION(BlueprintPure, Category = "Territory|Economy|UI")
	int64 GetNetIncome() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Economy|UI")
	bool IsOperatingAtDeficit() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Economy|UI")
	FTerritoryEconomyOperationsView GetEconomyOperationsView(int32 MaxRecentTransactions = 10) const;

	/** Optional modular row classes used when the matching BindWidget containers exist. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Economy|UI|Resources")
	TSubclassOf<UTerritoryResourceRowWidget> ResourceRowClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Economy|UI|Resources")
	TSubclassOf<UTerritoryProductionSiteRowWidget> ProductionSiteRowClass;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EconomyFactionText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EconomyTreasuryText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EconomyIncomeText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EconomyCostsText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EconomyTerritoryCountText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EconomyNetText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EconomyHealthText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EconomyRecentActivityText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EconomyProductionSummaryText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EconomyStorageStatusText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> ResourceStockpileRows;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> ProductionSiteRows;

	UFUNCTION(BlueprintCallable, Category="Territory|Economy|UI")
	void RefreshEconomyDisplay();

	/** Called every economy tick with updated snapshot */
	UFUNCTION(BlueprintImplementableEvent, Category = "Territory|Economy|UI")
	void OnEconomyUpdated(FGameplayTag Faction, const FTerritoryEconomySnapshot& Snapshot);

	/** Called when a transaction is recorded for the display faction */
	UFUNCTION(BlueprintImplementableEvent, Category = "Territory|Economy|UI")
	void OnTransactionRecorded(const FTerritoryTransaction& Transaction);

	/** Shared read model for Blueprint layouts that compose their own resource modules. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Territory|Economy|UI|Resources")
	void OnEconomyOperationsUpdated(const FTerritoryEconomyOperationsView& View);

private:
	FGameplayTag DisplayFaction;

	/** Client-side polling fallback timer — refreshes data periodically in case delegate broadcasts are missed. */
	FTimerHandle ClientPollTimerHandle;

	/** Seconds between client polling refreshes. */
	static constexpr float ClientPollInterval = 5.f;

	UFUNCTION()
	void HandleEconomyTick(FGameplayTag Faction, FTerritoryEconomySnapshot Snapshot);

	UFUNCTION()
	void HandleTransactionRecorded(const FTerritoryTransaction& Transaction);

	UFUNCTION()
	void HandleProductionSettled(const FTerritoryProductionResult& Result);

	void BindDelegates();
	void UnbindDelegates();

	/** Client polling fallback — queries current data and fires OnEconomyUpdated. */
	void ClientPollRefresh();
	void RefreshResourcePanels(const FTerritoryEconomyOperationsView& View);

	UTerritoryEconomySubsystem* GetEconomySubsystem() const;
};
