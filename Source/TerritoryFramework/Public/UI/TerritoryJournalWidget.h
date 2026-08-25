#pragma once

#include "CoreMinimal.h"
#include "Widgets/NarrativeComboBoxString.h"
#include "UI/TerritoryActivatableWidget.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "TerritoryJournalWidget.generated.h"

class ATerritoryDistrict;
class ATerritoryVolume;
class UNarrativeCommonButtonBase;
class UEditableTextBox;
class UProgressBar;
class USizeBox;
class USpinBox;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;
class UTerritoryPlayerManagementComponent;
class UTerritoryDistrictRowWidget;

/** Narrative activatable Territory tab with live district, economy, and guard management flow. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryJournalWidget : public UTerritoryActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Territory|UI")
	void RefreshDistrictList();

	UFUNCTION(BlueprintPure, Category="Territory|UI")
	FTerritoryDistrictOperationsView GetSelectedDistrictOperationsView() const;

	UFUNCTION(BlueprintCallable, Category="Territory|UI")
	void SetOperationsFilter(ETerritoryOperationsFilter Filter);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Max-width desktop column that expands to the available width on compact viewports. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USizeBox> CommandCenterResponsiveWidth;

	/** Optional project-styled row Blueprint. Falls back to the native Narrative CommonUI row. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|UI")
	TSubclassOf<UTerritoryDistrictRowWidget> DistrictRowWidgetClass;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_TerritoryTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_EarningsTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_LossTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> TabSwitcher;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEditableTextBox> DistrictSearchBox;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeComboBoxString> DistrictOwnerFilter;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeComboBoxString> DistrictStateFilter;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeComboBoxString> DistrictOperationalFilter;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> DistrictList;

	/** Existing journal left-rail lists: unlocked/active and captured/owned districts. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> ActiveQuestsBox;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> FinishedQuestsBox;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> EarningsList;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> LossReportList;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_FilterSummary;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ActiveQuestCount;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_FinishedQuestCount;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_HeaderStatus;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TotalWeeklyEarnings;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TotalGuardCost;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_NetProfit;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TotalEarningsLost;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CommandStatus;

	/** Command-center KPI cards. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_AvailableUnlockedCount;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_OwnedDistrictCount;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ThreatenedDistrictCount;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RiskDistrictCount;

	/** Detailed selected-district command-center fields. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CommandDistrictName;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CommandOwnerState;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CommandAvailability;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CommandSecurity;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CommandFinance;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CommandThreat;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CommandAssault;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CommandApproaches;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CommandCaptureProgress;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> CommandCaptureProgressBar;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_CommandAddGuard;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_CommandRemoveGuard;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_CommandAddFiveGuards;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_CommandRemoveFiveGuards;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_OperationalSummary;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SecuritySummary;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_FinanceSummary;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_AssaultSummary;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class URichTextBlock> RichText_EarningsSummary;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class URichTextBlock> RichText_LossSummary;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SelectedEyebrow;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> QuestTitle;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class URichTextBlock> RichText_QuestDescription;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CaptureLabel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class UProgressBar> CaptureProgressBar;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class URichTextBlock> RichText_CurrentStateDescription;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_EmptySelection;

private:
	TWeakObjectPtr<ATerritoryDistrict> SelectedDistrict;
	TWeakObjectPtr<ATerritoryVolume> SelectedGarrisonTarget;
	TWeakObjectPtr<UTerritoryPlayerManagementComponent> ManagementComponent;
	TMap<FString, TWeakObjectPtr<ATerritoryVolume>> GarrisonTargetOptions;
	TArray<TWeakObjectPtr<ATerritoryVolume>> GarrisonTargetOrder;
	UPROPERTY(Transient)
	TObjectPtr<UNarrativeComboBoxString> GarrisonTargetSelector;
	UPROPERTY(Transient)
	TObjectPtr<USpinBox> GuardTargetSpinBox;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_GarrisonTargetPreview;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_GarrisonTargetName;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_GarrisonStaffing;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_GarrisonFinance;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> GarrisonStaffingProgressBar;
	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> Btn_PreviousGarrisonTarget;
	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> Btn_NextGarrisonTarget;
	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> Btn_ApplyGuardTarget;
	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> Btn_ZeroGuardTarget;
	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> Btn_MaxGuardTarget;
	TMap<FString, FGameplayTag> OwnerFilterTags;
	FString SelectedOwnerFilter;
	FString SelectedStateFilter;
	FString SearchFilter;
	ETerritoryOperationsFilter SelectedOperationsFilter = ETerritoryOperationsFilter::All;
	bool bFiltersInitialized = false;
	bool bResponsiveLayoutApplied = false;
	bool bCompactResponsiveLayout = false;
	int32 LastOperationsRevision = INDEX_NONE;
	FTimerHandle RefreshTimerHandle;

	void BindTerritoryDelegates();
	void UnbindTerritoryDelegates();
	void BindManagementComponent();
	void RefreshFilterOptions();
	void BuildGarrisonManagementControls();
	void RefreshGarrisonManagementControls(const FTerritoryDistrictOperationsView& View);
	void UpdateGarrisonTargetPreview();
	void SelectRelativeGarrisonTarget(int32 Direction);
	void SubmitSelectedGuardTarget(int32 NewDesiredGuardCount);
	void UpdateSelectedDistrict(ATerritoryDistrict* District);
	void RefreshOperationalSummaries(const TArray<FTerritoryDistrictOperationsView>& Views);
	UTerritoryDistrictRowWidget* CreateOperationsRow(const FTerritoryDistrictOperationsView& View);
	bool PassesFilters(const FTerritoryDistrictOperationsView& View) const;

	UFUNCTION()
	void HandleTerritoryTabClicked();

	UFUNCTION()
	void HandleEarningsTabClicked();

	UFUNCTION()
	void HandleLossTabClicked();

	UFUNCTION()
	void HandleSearchChanged(const FText& Text);

	UFUNCTION()
	void HandleOwnerFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleStateFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleOperationalFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleGarrisonTargetChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandlePreviousGarrisonTargetClicked();

	UFUNCTION()
	void HandleNextGarrisonTargetClicked();

	UFUNCTION()
	void HandleGuardTargetSpinChanged(float NewValue);

	UFUNCTION()
	void HandleApplyGuardTargetClicked();

	UFUNCTION()
	void HandleZeroGuardTargetClicked();

	UFUNCTION()
	void HandleMaxGuardTargetClicked();

	UFUNCTION()
	void HandleTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	UFUNCTION()
	void HandleTerritoryUnregistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	UFUNCTION()
	void HandleDistrictSelected(ATerritoryDistrict* District);

	UFUNCTION()
	void HandleGuardActionRequested(ATerritoryDistrict* District, int32 Delta);

	UFUNCTION()
	void HandleCommandAddGuardClicked();

	UFUNCTION()
	void HandleCommandRemoveGuardClicked();

	UFUNCTION()
	void HandleCommandAddFiveGuardsClicked();

	UFUNCTION()
	void HandleCommandRemoveFiveGuardsClicked();

	UFUNCTION()
	void HandleGuardManagementResult(ATerritoryVolume* Territory, bool bSuccess, FText Message, int32 RequestId);
};
