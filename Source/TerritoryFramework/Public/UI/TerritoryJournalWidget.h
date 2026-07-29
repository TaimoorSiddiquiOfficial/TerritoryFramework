#pragma once

#include "CoreMinimal.h"
#include "Widgets/NarrativeComboBoxString.h"
#include "UI/TerritoryActivatableWidget.h"
#include "TerritoryJournalWidget.generated.h"

class ATerritoryDistrict;
class ATerritoryVolume;
class UNarrativeCommonButtonBase;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;
class UTerritoryPlayerManagementComponent;

/** Narrative activatable Territory tab with live district, economy, and guard management flow. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryJournalWidget : public UTerritoryActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Territory|UI")
	void RefreshDistrictList();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;

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
	TObjectPtr<UVerticalBox> DistrictList;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_FilterSummary;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CommandStatus;

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
	TWeakObjectPtr<UTerritoryPlayerManagementComponent> ManagementComponent;
	TMap<FString, FGameplayTag> OwnerFilterTags;
	FString SelectedOwnerFilter;
	FString SelectedStateFilter;
	FString SearchFilter;
	bool bFiltersInitialized = false;
	int32 LastDistrictCount = -1;
	int32 LastFilterHash = 0;
	FTimerHandle RefreshTimerHandle;

	void BindTerritoryDelegates();
	void UnbindTerritoryDelegates();
	void BindManagementComponent();
	void RefreshFilterOptions();
	void UpdateSelectedDistrict(ATerritoryDistrict* District);
	bool PassesFilters(const ATerritoryDistrict* District) const;

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
	void HandleTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	UFUNCTION()
	void HandleTerritoryUnregistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	UFUNCTION()
	void HandleDistrictSelected(ATerritoryDistrict* District);

	UFUNCTION()
	void HandleGuardActionRequested(ATerritoryDistrict* District, int32 Delta);

	UFUNCTION()
	void HandleGuardManagementResult(ATerritoryVolume* Territory, bool bSuccess, FText Message, int32 RequestId);
};
