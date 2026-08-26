#pragma once

#include "CoreMinimal.h"
#include "Widgets/NarrativeComboBoxString.h"
#include "UI/TerritoryActivatableWidget.h"
#include "UI/TerritoryLiveEventTypes.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "TerritoryJournalWidget.generated.h"

class ATerritoryDistrict;
class ATerritoryVolume;
class UNarrativeCommonButtonBase;
class UEditableTextBox;
class UPanelWidget;
class UProgressBar;
class UScrollBox;
class USizeBox;
class USpinBox;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UWidgetSwitcher;
class UWidgetAnimation;
class UTerritoryPlayerManagementComponent;
class UTerritoryDistrictRowWidget;
class UTerritoryLiveEventRowWidget;

/** Narrative activatable Territory tab with live district, economy, and guard management flow. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryJournalWidget : public UTerritoryActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Territory|UI")
	void RefreshDistrictList();

	/**
	 * Select one visible District, highlight its journal entry, expand its known
	 * Places, and show its detail page. This mirrors Narrative Pro's Show Quest
	 * interaction without making Territory data pretend to be Quest data.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|UI",
		meta=(DisplayName="Show District In Territory Journal"))
	void SelectDistrict(ATerritoryDistrict* District);

	UFUNCTION(BlueprintPure, Category="Territory|UI")
	FTerritoryDistrictOperationsView GetSelectedDistrictOperationsView() const;

	UFUNCTION(BlueprintCallable, Category="Territory|UI")
	void SetOperationsFilter(ETerritoryOperationsFilter Filter);

	/** Number of real District rows currently shown under Active Territories (empty-state text is excluded). */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Diagnostics")
	int32 GetActiveTerritoryEntryCount() const;

	/** Number of real District rows currently shown under Captured Territories (empty-state text is excluded). */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Diagnostics")
	int32 GetCapturedTerritoryEntryCount() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Max-width desktop column that expands to the available width on compact viewports. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USizeBox> CommandCenterResponsiveWidth;

	/** Optional authored reveal; automatically played by the framework on open. */
	UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> TerritoryReveal;

	/**
	 * Reusable entry template for both journal lists. Use a compact child of
	 * UTerritoryDistrictRowWidget, in the same role BP_QuestJournalQuest has in
	 * Narrative Pro's Quest Journal.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|UI",
		meta=(DisplayName="Territory Entry Widget Class"))
	TSubclassOf<UTerritoryDistrictRowWidget> TerritoryEntryWidgetClass;

	/** Legacy name kept so existing project widgets migrate without losing their entry class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|UI",
		meta=(DeprecatedProperty, DeprecationMessage="Use TerritoryEntryWidgetClass."))
	TSubclassOf<UTerritoryDistrictRowWidget> DistrictRowWidgetClass;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_TerritoryTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_EarningsTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_LossTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> TabSwitcher;

	/** Persistent selected-item pane, equivalent to the Quest Journal information pane. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> SelectedTerritoryInfoBox;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_CloseSelectedTerritory;

	/** Legacy bindings supported for authored widgets created before the Quest-template refactor. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USizeBox> CommandDrawer;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_CloseCommandDrawer;

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

	/**
	 * Narrative Quest Journal pattern: one bounded ScrollBox for unlocked,
	 * non-owned Districts and one for captured Districts owned by the viewer.
	 */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UScrollBox> ActiveTerritoriesBox;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UScrollBox> CapturedTerritoriesBox;

	/** Legacy list names supported during Blueprint migration. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> ActiveQuestsBox;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> FinishedQuestsBox;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> EarningsList;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> LossReportList;

	/** Runtime-built activity feed hosted by the Reports page. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> LiveEventsBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_LiveEventCount;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_IntelligenceSummary;

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> Btn_IntelligenceAll;

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> Btn_IntelligenceConflict;

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> Btn_IntelligenceControl;

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> Btn_IntelligenceEconomy;

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> Btn_IntelligenceCommand;

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> Btn_IntelligenceProduction;

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> Btn_IntelligenceDiplomacy;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_FilterSummary;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ActiveQuestCount;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_FinishedQuestCount;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ActiveTerritoryCount;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CapturedTerritoryCount;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_HeaderStatus;

	/** Dynamic identity; replaces any example/static City copy authored in the Blueprint. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_JournalEyebrow;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_JournalTitle;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_JournalSubtitle;

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

	/** Selected District controls, organized like Narrative's selected Quest details. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> CommandDetailSwitcher;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_OverviewDetailTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_PlacesDetailTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_GarrisonDetailTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_EconomyDetailTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_ProductionDetailTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_ThreatsDetailTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_DiplomacyDetailTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CommandHierarchy;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CommandOverview;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> PlaceHierarchyList;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> ProductionHierarchyList;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CommandDiplomacy;

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
	TObjectPtr<UTextBlock> Text_SelectedTerritoryTitle;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class URichTextBlock> RichText_QuestDescription;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class URichTextBlock> RichText_TerritoryDescription;

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
	TObjectPtr<UTextBlock> Text_CommandCapabilities;
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
	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> Btn_SendReinforcement;
	TMap<FString, FGameplayTag> OwnerFilterTags;
	ETerritoryIntelligenceFilter SelectedIntelligenceFilter =
		ETerritoryIntelligenceFilter::All;
	FString SelectedOwnerFilter;
	FString SelectedStateFilter;
	FString SearchFilter;
	ETerritoryOperationsFilter SelectedOperationsFilter = ETerritoryOperationsFilter::All;
	bool bFiltersInitialized = false;
	bool bResponsiveLayoutApplied = false;
	bool bCompactResponsiveLayout = false;
	bool bSelectedTerritoryInfoRequested = false;
	int32 SelectedDetailTab = 0;
	int32 LastOperationsRevision = INDEX_NONE;
	FTimerHandle RefreshTimerHandle;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTerritoryDistrictRowWidget>> TerritoryEntryWidgets;

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
	void RefreshSelectedHierarchyPanels(const FTerritoryDistrictOperationsView& View);
	void SetSelectedDetailTab(int32 TabIndex);
	void SetSelectedTerritoryInfoOpen(bool bOpen);
	UPanelWidget* GetActiveTerritoriesPanel() const;
	UPanelWidget* GetCapturedTerritoriesPanel() const;
	UWidget* GetSelectedTerritoryInfoWidget() const;
	void RefreshEntrySelection();
	UTextBlock* CreateHierarchyTextRow(const FText& Text, FName WidgetName, bool bHeading = false);
	void RefreshOperationalSummaries(const TArray<FTerritoryDistrictOperationsView>& Views);
	void RefreshCommandCenterIdentity(const TArray<FTerritoryDistrictOperationsView>& Views);
	void BuildLiveEventPanel();
	void RefreshLiveEvents();
	void SetIntelligenceFilter(ETerritoryIntelligenceFilter Filter);
	UTerritoryDistrictRowWidget* CreateOperationsRow(const FTerritoryDistrictOperationsView& View);
	bool PassesFilters(const FTerritoryDistrictOperationsView& View) const;

	UFUNCTION()
	void HandleTerritoryTabClicked();

	UFUNCTION()
	void HandleEarningsTabClicked();

	UFUNCTION()
	void HandleLossTabClicked();

	UFUNCTION()
	void HandleCloseSelectedTerritoryClicked();

	UFUNCTION()
	void HandleOverviewDetailTabClicked();

	UFUNCTION()
	void HandlePlacesDetailTabClicked();

	UFUNCTION()
	void HandleGarrisonDetailTabClicked();

	UFUNCTION()
	void HandleEconomyDetailTabClicked();

	UFUNCTION()
	void HandleProductionDetailTabClicked();

	UFUNCTION()
	void HandleThreatsDetailTabClicked();

	UFUNCTION()
	void HandleDiplomacyDetailTabClicked();

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
	void HandleSendReinforcementClicked();

	UFUNCTION()
	void HandleTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	UFUNCTION()
	void HandleTerritoryUnregistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	UFUNCTION()
	void HandleWaypointRequested(ATerritoryDistrict* District);

	UFUNCTION()
	void HandleEspionageRequested(ATerritoryDistrict* District);

	UFUNCTION()
	void HandleLiveEventWaypointRequested(FGameplayTag TerritoryTag);

	UFUNCTION()
	void HandleLiveEventsChanged();

	UFUNCTION()
	void HandleIntelligenceAllClicked();

	UFUNCTION()
	void HandleIntelligenceConflictClicked();

	UFUNCTION()
	void HandleIntelligenceControlClicked();

	UFUNCTION()
	void HandleIntelligenceEconomyClicked();

	UFUNCTION()
	void HandleIntelligenceCommandClicked();

	UFUNCTION()
	void HandleIntelligenceProductionClicked();

	UFUNCTION()
	void HandleIntelligenceDiplomacyClicked();

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
