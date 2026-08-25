#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Widgets/NarrativeComboBoxString.h"
#include "UI/TerritoryActivatableWidget.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "TerritoryDistrictManagementWidget.generated.h"

class ATerritoryDistrict;
class ATerritoryDistrictManagementPoint;
class ATerritoryVolume;
class UNarrativeCommonButtonBase;
class UTerritoryPlayerManagementComponent;
class UTextBlock;
class USpinBox;

/** Native-backed District management panel. Layout is supplied by a project Widget Blueprint. */
UCLASS(Abstract, BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryDistrictManagementWidget : public UTerritoryActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void InitializeManagement(ATerritoryDistrictManagementPoint* ManagementPoint);

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	ATerritoryDistrict* GetManagedDistrict() const;

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	FGameplayTag GetManagedFaction() const;

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	int32 GetDistrictIncome() const;

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	bool CanPurchaseGuard(FText& OutFailureReason) const;

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	bool CanRemoveGuard(FText& OutFailureReason) const;

	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RefreshManagementDisplay();

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	FTerritoryDistrictOperationsView GetOperationsView() const { return OperationsView; }

	/** Blueprint-friendly bulk commands; the server bridge validates count, ownership, and funds. */
	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestAddGuards(int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestRemoveGuards(int32 Count = 1);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DistrictNameText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> OwnerText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StateText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TreasuryText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EarningsText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> GuardCountText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> GuardCostText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> NetIncomeText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ReserveGuardText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ThreatText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> AvailabilityText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ProductionText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> AddGuardButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> RemoveGuardButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> CloseButton;

	UFUNCTION(BlueprintImplementableEvent, Category="Territory|Management")
	void OnManagementRefreshed();

	private:
	TWeakObjectPtr<ATerritoryDistrictManagementPoint> ManagementPoint;
	TWeakObjectPtr<ATerritoryVolume> SelectedGarrisonTarget;
	TWeakObjectPtr<UTerritoryPlayerManagementComponent> ManagementComponent;
	TMap<FString, TWeakObjectPtr<ATerritoryVolume>> GarrisonTargetOptions;
	UPROPERTY(Transient)
	TObjectPtr<UNarrativeComboBoxString> GarrisonTargetSelector;
	UPROPERTY(Transient)
	TObjectPtr<USpinBox> GuardTargetSpinBox;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GarrisonTargetPreview;
	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> ApplyGuardTargetButton;
	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> ZeroGuardTargetButton;
	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> MaxGuardTargetButton;
	FGameplayTag ManagedFaction;
	FTerritoryDistrictOperationsView OperationsView;
	FTimerHandle RefreshTimerHandle;

	UFUNCTION()
	void HandleAddGuardClicked();

	UFUNCTION()
	void HandleRemoveGuardClicked();

	UFUNCTION()
	void HandleGarrisonTargetChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleGuardTargetSpinChanged(float NewValue);

	UFUNCTION()
	void HandleApplyGuardTargetClicked();

	UFUNCTION()
	void HandleZeroGuardTargetClicked();

	UFUNCTION()
	void HandleMaxGuardTargetClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleGuardPurchaseResult(ATerritoryVolume* Territory, bool bSuccess, FText Message, int32 RequestId);

	void BindManagementComponent();
	void BuildGarrisonControls();
	void RefreshGarrisonControls();
	void UpdateGarrisonPreview();
	void SubmitGuardTarget(int32 NewDesiredGuardCount);
};
