#include "UI/TerritoryJournalWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/RichTextBlock.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UI/TerritoryDistrictRowWidget.h"
#include "UI/TerritoryLiveEventRowWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "Widgets/NarrativeCommonTextBlock.h"
#include "Widgets/NarrativeSpinBox.h"
#include "CommonTextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "TimerManager.h"

namespace
{
	enum class ETerritoryGeneratedTextRole : uint8
	{
		Body,
		Heading,
		Muted
	};

	void StyleGeneratedTerritoryText(UTextBlock* Text, int32 FontSize,
		const FLinearColor& Color,
		ETerritoryGeneratedTextRole Role = ETerritoryGeneratedTextRole::Body)
	{
		if (!Text)
		{
			return;
		}
		if (UNarrativeCommonTextBlock* CommonText =
			Cast<UNarrativeCommonTextBlock>(Text))
		{
			const UTerritoryDeveloperSettings* Settings =
				GetDefault<UTerritoryDeveloperSettings>();
			TSubclassOf<UCommonTextStyle> Style;
			if (Settings)
			{
				switch (Role)
				{
				case ETerritoryGeneratedTextRole::Heading:
					Style = Settings->TerritoryHeadingTextStyle.LoadSynchronous();
					break;
				case ETerritoryGeneratedTextRole::Muted:
					Style = Settings->TerritoryMutedTextStyle.LoadSynchronous();
					break;
				case ETerritoryGeneratedTextRole::Body:
				default:
					Style = Settings->DefaultTerritoryTextStyle.LoadSynchronous();
					break;
				}
			}
			if (Style)
			{
				CommonText->SetStyle(Style);
				CommonText->SetAutoWrapText(true);
				// The style owns typography; the call site still owns semantic state colour.
				CommonText->SetColorAndOpacity(FSlateColor(Color));
				return;
			}
		}
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
	}

	void StyleGeneratedTerritorySurface(UBorder* Border, const FLinearColor& Fill,
		const FLinearColor& Outline, float Radius = 10.f)
	{
		if (!Border) return;
		FSlateBrush Brush;
		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
		if (UTexture2D* PanelTexture = Settings
			? Settings->TerritoryPanelTexture.LoadSynchronous() : nullptr)
		{
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.SetResourceObject(PanelTexture);
			Brush.ImageSize = FVector2D(PanelTexture->GetSizeX(), PanelTexture->GetSizeY());
			Brush.Margin = FMargin(0.035f, 0.22f);
			Brush.TintColor = FSlateColor(FLinearColor(
				0.72f + Outline.R * 0.28f,
				0.72f + Outline.G * 0.28f,
				0.72f + Outline.B * 0.28f, Fill.A));
			Border->SetBrush(Brush);
			return;
		}
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			Radius, FSlateColor(Outline), 1.f);
		Border->SetBrush(Brush);
	}

	bool StyleGeneratedTerritoryProgress(UProgressBar* ProgressBar,
		bool bUseAuthoredFill)
	{
		if (!ProgressBar) return false;
		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
		UTexture2D* FrameTexture = Settings
			? Settings->TerritoryProgressFrameTexture.LoadSynchronous() : nullptr;
		UTexture2D* FillTexture = bUseAuthoredFill && Settings
			? Settings->TerritoryProgressFillTexture.LoadSynchronous() : nullptr;
		if (!FrameTexture && !FillTexture) return false;

		FProgressBarStyle Style = ProgressBar->GetWidgetStyle();
		if (FrameTexture)
		{
			FSlateBrush Frame;
			Frame.DrawAs = ESlateBrushDrawType::Box;
			Frame.SetResourceObject(FrameTexture);
			Frame.ImageSize = FVector2D(FrameTexture->GetSizeX(), FrameTexture->GetSizeY());
			Frame.Margin = FMargin(0.04f, 0.28f);
			Frame.TintColor = FSlateColor(FLinearColor::White);
			Style.BackgroundImage = Frame;
		}
		if (FillTexture)
		{
			FSlateBrush Fill;
			Fill.DrawAs = ESlateBrushDrawType::Box;
			Fill.SetResourceObject(FillTexture);
			Fill.ImageSize = FVector2D(FillTexture->GetSizeX(), FillTexture->GetSizeY());
			Fill.Margin = FMargin(0.02f, 0.24f);
			Fill.TintColor = FSlateColor(FLinearColor::White);
			Style.FillImage = Fill;
		}
		ProgressBar->SetWidgetStyle(Style);
		return FillTexture != nullptr;
	}

	const FString AllOwnersOption = TEXT("All owners");
	const FString AllStatesOption = TEXT("All states");
	const FString AllOperationsOption = TEXT("All districts");
	const FString UnlockedOption = TEXT("Unlocked");
	const FString AvailableOption = TEXT("Available");
	const FString OwnedOption = TEXT("Owned");
	const FString ManageableOption = TEXT("Manageable");
	const FString UnderAttackOption = TEXT("Under attack");
	const FString ContestedOption = TEXT("Contested");
	const FString LockedOption = TEXT("Locked");
	const FString FinancialRiskOption = TEXT("Financial risk");
	const FString ProducingOption = TEXT("Producing");
	const FString ProductionBlockedOption = TEXT("Production blocked");
	const FString MissingInputsOption = TEXT("Missing production inputs");
	const FString StorageFullOption = TEXT("Resource storage full");

	ETerritoryOperationsFilter GetOperationsFilter(const FString& Option)
	{
		if (Option == UnlockedOption) return ETerritoryOperationsFilter::Unlocked;
		if (Option == AvailableOption) return ETerritoryOperationsFilter::Available;
		if (Option == OwnedOption) return ETerritoryOperationsFilter::Owned;
		if (Option == ManageableOption) return ETerritoryOperationsFilter::Manageable;
		if (Option == UnderAttackOption) return ETerritoryOperationsFilter::UnderAttack;
		if (Option == ContestedOption) return ETerritoryOperationsFilter::Contested;
		if (Option == LockedOption) return ETerritoryOperationsFilter::Locked;
		if (Option == FinancialRiskOption) return ETerritoryOperationsFilter::FinancialRisk;
		if (Option == ProducingOption) return ETerritoryOperationsFilter::Producing;
		if (Option == ProductionBlockedOption) return ETerritoryOperationsFilter::ProductionBlocked;
		if (Option == MissingInputsOption) return ETerritoryOperationsFilter::MissingInputs;
		if (Option == StorageFullOption) return ETerritoryOperationsFilter::StorageFull;
		return ETerritoryOperationsFilter::All;
	}

	FString GetOperationsOption(ETerritoryOperationsFilter Filter)
	{
		switch (Filter)
		{
		case ETerritoryOperationsFilter::Unlocked: return UnlockedOption;
		case ETerritoryOperationsFilter::Available: return AvailableOption;
		case ETerritoryOperationsFilter::Owned: return OwnedOption;
		case ETerritoryOperationsFilter::Manageable: return ManageableOption;
		case ETerritoryOperationsFilter::UnderAttack: return UnderAttackOption;
		case ETerritoryOperationsFilter::Contested: return ContestedOption;
		case ETerritoryOperationsFilter::Locked: return LockedOption;
		case ETerritoryOperationsFilter::FinancialRisk: return FinancialRiskOption;
		case ETerritoryOperationsFilter::Producing: return ProducingOption;
		case ETerritoryOperationsFilter::ProductionBlocked: return ProductionBlockedOption;
		case ETerritoryOperationsFilter::MissingInputs: return MissingInputsOption;
		case ETerritoryOperationsFilter::StorageFull: return StorageFullOption;
		case ETerritoryOperationsFilter::All:
		default: return AllOperationsOption;
		}
	}

	FString GetStateOption(const ATerritoryDistrict* District)
	{
		if (!District)
		{
			return FString();
		}

		const UEnum* StateEnum = StaticEnum<ETerritoryState>();
		return StateEnum
			? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(District->GetTerritoryState())).ToString()
			: FString();
	}

	int32 GetLiveEventListRevision(const TArray<FTerritoryLiveEvent>& Events)
	{
		uint32 Hash = GetTypeHash(Events.Num());
		for (const FTerritoryLiveEvent& Event : Events)
		{
			Hash = HashCombineFast(Hash, Event.GetPresentationRevision());
		}
		return static_cast<int32>(Hash);
	}
}

UPanelWidget* UTerritoryJournalWidget::GetActiveTerritoriesPanel() const
{
	if (ActiveTerritoriesBox)
	{
		return ActiveTerritoriesBox.Get();
	}
	if (WidgetTree)
	{
		if (UPanelWidget* AuthoredPanel = Cast<UPanelWidget>(
			WidgetTree->FindWidget(TEXT("ActiveTerritoriesBox"))))
		{
			return AuthoredPanel;
		}
	}
	return ActiveQuestsBox ? Cast<UPanelWidget>(ActiveQuestsBox.Get())
		: WidgetTree
			? Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("ActiveQuestsBox")))
			: nullptr;
}

UPanelWidget* UTerritoryJournalWidget::GetCapturedTerritoriesPanel() const
{
	if (CapturedTerritoriesBox)
	{
		return CapturedTerritoriesBox.Get();
	}
	if (WidgetTree)
	{
		if (UPanelWidget* AuthoredPanel = Cast<UPanelWidget>(
			WidgetTree->FindWidget(TEXT("CapturedTerritoriesBox"))))
		{
			return AuthoredPanel;
		}
	}
	return FinishedQuestsBox ? Cast<UPanelWidget>(FinishedQuestsBox.Get())
		: WidgetTree
			? Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("FinishedQuestsBox")))
			: nullptr;
}

UWidget* UTerritoryJournalWidget::GetSelectedTerritoryInfoWidget() const
{
	if (SelectedTerritoryInfoBox)
	{
		return SelectedTerritoryInfoBox.Get();
	}
	if (WidgetTree)
	{
		if (UWidget* AuthoredInfo = WidgetTree->FindWidget(
			TEXT("SelectedTerritoryInfoBox")))
		{
			return AuthoredInfo;
		}
	}
	return CommandDrawer ? CommandDrawer.Get()
		: WidgetTree ? WidgetTree->FindWidget(TEXT("CommandDrawer")) : nullptr;
}

int32 UTerritoryJournalWidget::GetActiveTerritoryEntryCount() const
{
	int32 Count = 0;
	if (const UPanelWidget* Panel = GetActiveTerritoriesPanel())
	{
		for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
		{
			const UWidget* Child = Panel->GetChildAt(Index);
			Count += Child && Child->IsA<UTerritoryDistrictRowWidget>() ? 1 : 0;
		}
	}
	return Count;
}

int32 UTerritoryJournalWidget::GetCapturedTerritoryEntryCount() const
{
	int32 Count = 0;
	if (const UPanelWidget* Panel = GetCapturedTerritoriesPanel())
	{
		for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
		{
			const UWidget* Child = Panel->GetChildAt(Index);
			Count += Child && Child->IsA<UTerritoryDistrictRowWidget>() ? 1 : 0;
		}
	}
	return Count;
}

void UTerritoryJournalWidget::RefreshEntrySelection()
{
	for (UTerritoryDistrictRowWidget* Entry : TerritoryEntryWidgets)
	{
		if (!Entry)
		{
			continue;
		}
		const bool bIsSelected = Entry->GetDistrict() == SelectedDistrict.Get();
		Entry->SetSelected(bIsSelected);
		if (!bIsSelected)
		{
			Entry->SetExpanded(false);
		}
	}
}

void UTerritoryJournalWidget::SelectDistrict(ATerritoryDistrict* District)
{
	bSelectedTerritoryInfoRequested = false;
	FTerritoryDistrictOperationsView View;
	if (District && UTerritoryUIBlueprintLibrary::BuildDistrictOperationsView(
		this, District, GetOwningPlayer(), View))
	{
		// The detailed command console is a reward for capture. Unlocked enemy
		// rows only expand their name-only Place directory and espionage action.
		bSelectedTerritoryInfoRequested =
			UTerritoryUIBlueprintLibrary::IsDistrictCapturedOwned(View);
	}
	UpdateSelectedDistrict(District);
	RefreshEntrySelection();
}

void UTerritoryJournalWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// UISpec-authored widgets do not have to expose every child as a Blueprint
	// variable. BindWidgetOptional therefore remains a migration convenience,
	// while name lookup is the reliable runtime contract for the two primary
	// Quest-Journal-style lists and their labels.
	if (WidgetTree)
	{
		if (!ActiveTerritoriesBox)
		{
			ActiveTerritoriesBox = Cast<UScrollBox>(
				WidgetTree->FindWidget(TEXT("ActiveTerritoriesBox")));
		}
		if (!CapturedTerritoriesBox)
		{
			CapturedTerritoriesBox = Cast<UScrollBox>(
				WidgetTree->FindWidget(TEXT("CapturedTerritoriesBox")));
		}
		if (!Text_ActiveTerritoryCount)
		{
			Text_ActiveTerritoryCount = Cast<UTextBlock>(
				WidgetTree->FindWidget(TEXT("Text_ActiveTerritoryCount")));
		}
		if (!Text_CapturedTerritoryCount)
		{
			Text_CapturedTerritoryCount = Cast<UTextBlock>(
				WidgetTree->FindWidget(TEXT("Text_CapturedTerritoryCount")));
		}
		if (!SelectedTerritoryInfoBox)
		{
			SelectedTerritoryInfoBox = WidgetTree->FindWidget(
				TEXT("SelectedTerritoryInfoBox"));
		}
	}
	if (!GetActiveTerritoriesPanel() || !GetCapturedTerritoriesPanel())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("Territory Journal cannot populate Active/Captured lists: authored panel bindings are missing."));
	}

	if (Btn_TerritoryTab)
	{
		Btn_TerritoryTab->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleTerritoryTabClicked);
		Btn_TerritoryTab->SetButtonText(NSLOCTEXT("TerritoryJournal", "TerritoryTab", "OPERATIONS"));
	}
	if (Btn_EarningsTab)
	{
		Btn_EarningsTab->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleEarningsTabClicked);
		Btn_EarningsTab->SetButtonText(NSLOCTEXT("TerritoryJournal", "EarningsTab", "CONTROLLED"));
	}
	if (Btn_LossTab)
	{
		Btn_LossTab->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleLossTabClicked);
		Btn_LossTab->SetButtonText(NSLOCTEXT("TerritoryJournal", "LossTab", "ACTIVITY"));
	}
	UNarrativeCommonButtonBase* CloseSelectedButton = Btn_CloseSelectedTerritory
		? Btn_CloseSelectedTerritory : Btn_CloseCommandDrawer;
	if (CloseSelectedButton)
	{
		CloseSelectedButton->OnClicked().AddUObject(
			this, &UTerritoryJournalWidget::HandleCloseSelectedTerritoryClicked);
		CloseSelectedButton->SetButtonText(NSLOCTEXT(
			"TerritoryJournal", "CloseSelectedTerritory", "CLOSE"));
	}
	if (Btn_OverviewDetailTab)
	{
		Btn_OverviewDetailTab->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleOverviewDetailTabClicked);
		Btn_OverviewDetailTab->SetButtonText(NSLOCTEXT("TerritoryJournal", "OverviewDetailTab", "OVERVIEW"));
	}
	if (Btn_PlacesDetailTab)
	{
		Btn_PlacesDetailTab->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandlePlacesDetailTabClicked);
		Btn_PlacesDetailTab->SetButtonText(NSLOCTEXT("TerritoryJournal", "PlacesDetailTab", "PLACES"));
	}
	if (Btn_GarrisonDetailTab)
	{
		Btn_GarrisonDetailTab->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleGarrisonDetailTabClicked);
		Btn_GarrisonDetailTab->SetButtonText(NSLOCTEXT("TerritoryJournal", "GarrisonDetailTab", "GARRISON"));
	}
	if (Btn_EconomyDetailTab)
	{
		Btn_EconomyDetailTab->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleEconomyDetailTabClicked);
		Btn_EconomyDetailTab->SetButtonText(NSLOCTEXT("TerritoryJournal", "EconomyDetailTab", "ECONOMY"));
	}
	if (Btn_ProductionDetailTab)
	{
		Btn_ProductionDetailTab->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleProductionDetailTabClicked);
		Btn_ProductionDetailTab->SetButtonText(NSLOCTEXT("TerritoryJournal", "ProductionDetailTab", "PRODUCTION"));
	}
	if (Btn_ThreatsDetailTab)
	{
		Btn_ThreatsDetailTab->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleThreatsDetailTabClicked);
		Btn_ThreatsDetailTab->SetButtonText(NSLOCTEXT("TerritoryJournal", "ThreatsDetailTab", "THREATS"));
	}
	if (Btn_DiplomacyDetailTab)
	{
		Btn_DiplomacyDetailTab->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleDiplomacyDetailTabClicked);
		Btn_DiplomacyDetailTab->SetButtonText(NSLOCTEXT("TerritoryJournal", "DiplomacyDetailTab", "DIPLOMACY"));
	}
	if (Btn_CommandAddGuard)
	{
		Btn_CommandAddGuard->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleCommandAddGuardClicked);
		Btn_CommandAddGuard->SetButtonText(NSLOCTEXT("TerritoryJournal", "CommandAddGuard", "+ 1 GUARD"));
	}
	if (Btn_CommandRemoveGuard)
	{
		Btn_CommandRemoveGuard->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleCommandRemoveGuardClicked);
		Btn_CommandRemoveGuard->SetButtonText(NSLOCTEXT("TerritoryJournal", "CommandRemoveGuard", "- 1 GUARD"));
	}
	if (Btn_CommandAddFiveGuards)
	{
		Btn_CommandAddFiveGuards->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleCommandAddFiveGuardsClicked);
		Btn_CommandAddFiveGuards->SetButtonText(NSLOCTEXT("TerritoryJournal", "CommandAddFiveGuards", "+ 5 GUARDS"));
	}
	if (Btn_CommandRemoveFiveGuards)
	{
		Btn_CommandRemoveFiveGuards->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleCommandRemoveFiveGuardsClicked);
		Btn_CommandRemoveFiveGuards->SetButtonText(NSLOCTEXT("TerritoryJournal", "CommandRemoveFiveGuards", "- 5 GUARDS"));
	}
	BuildGarrisonManagementControls();
	if (DistrictSearchBox)
	{
		DistrictSearchBox->OnTextChanged.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleSearchChanged);
	}
	if (DistrictOwnerFilter)
	{
		DistrictOwnerFilter->OnSelectionChanged.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleOwnerFilterChanged);
	}
	if (DistrictStateFilter)
	{
		DistrictStateFilter->OnSelectionChanged.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleStateFilterChanged);
	}
	if (DistrictOperationalFilter)
	{
		DistrictOperationalFilter->OnSelectionChanged.AddUniqueDynamic(
			this, &UTerritoryJournalWidget::HandleOperationalFilterChanged);
	}

	BindTerritoryDelegates();
	BindManagementComponent();
	BuildLiveEventPanel();
	RefreshFilterOptions();
	SetSelectedDetailTab(SelectedDetailTab);
	RefreshDistrictList();
	bSelectedTerritoryInfoRequested = SelectedDistrict.IsValid();
	SetSelectedTerritoryInfoOpen(bSelectedTerritoryInfoRequested);
	if (Btn_TerritoryTab) Btn_TerritoryTab->SetIsSelected(true);
	if (Btn_EarningsTab) Btn_EarningsTab->SetIsSelected(false);
	if (Btn_LossTab) Btn_LossTab->SetIsSelected(false);
	if (TerritoryReveal)
	{
		PlayAnimation(TerritoryReveal);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&UTerritoryJournalWidget::RefreshDistrictList,
			1.f,
			true);
	}
}

void UTerritoryJournalWidget::NativeDestruct()
{
	if (Btn_TerritoryTab)
	{
		Btn_TerritoryTab->OnClicked().RemoveAll(this);
	}
	if (Btn_EarningsTab)
	{
		Btn_EarningsTab->OnClicked().RemoveAll(this);
	}
	if (Btn_LossTab)
	{
		Btn_LossTab->OnClicked().RemoveAll(this);
	}
	if (Btn_CloseSelectedTerritory)
	{
		Btn_CloseSelectedTerritory->OnClicked().RemoveAll(this);
	}
	if (Btn_CloseCommandDrawer)
	{
		Btn_CloseCommandDrawer->OnClicked().RemoveAll(this);
	}
	if (Btn_OverviewDetailTab) Btn_OverviewDetailTab->OnClicked().RemoveAll(this);
	if (Btn_PlacesDetailTab) Btn_PlacesDetailTab->OnClicked().RemoveAll(this);
	if (Btn_GarrisonDetailTab) Btn_GarrisonDetailTab->OnClicked().RemoveAll(this);
	if (Btn_EconomyDetailTab) Btn_EconomyDetailTab->OnClicked().RemoveAll(this);
	if (Btn_ProductionDetailTab) Btn_ProductionDetailTab->OnClicked().RemoveAll(this);
	if (Btn_ThreatsDetailTab) Btn_ThreatsDetailTab->OnClicked().RemoveAll(this);
	if (Btn_DiplomacyDetailTab) Btn_DiplomacyDetailTab->OnClicked().RemoveAll(this);
	if (Btn_CommandAddGuard) Btn_CommandAddGuard->OnClicked().RemoveAll(this);
	if (Btn_CommandRemoveGuard) Btn_CommandRemoveGuard->OnClicked().RemoveAll(this);
	if (Btn_CommandAddFiveGuards) Btn_CommandAddFiveGuards->OnClicked().RemoveAll(this);
	if (Btn_CommandRemoveFiveGuards) Btn_CommandRemoveFiveGuards->OnClicked().RemoveAll(this);
	if (GarrisonTargetSelector)
	{
		GarrisonTargetSelector->OnSelectionChanged.RemoveDynamic(
			this, &UTerritoryJournalWidget::HandleGarrisonTargetChanged);
	}
	if (GuardTargetSpinBox)
	{
		GuardTargetSpinBox->OnValueChanged.RemoveDynamic(
			this, &UTerritoryJournalWidget::HandleGuardTargetSpinChanged);
	}
	if (Btn_PreviousGarrisonTarget) Btn_PreviousGarrisonTarget->OnClicked().RemoveAll(this);
	if (Btn_NextGarrisonTarget) Btn_NextGarrisonTarget->OnClicked().RemoveAll(this);
	if (Btn_ApplyGuardTarget) Btn_ApplyGuardTarget->OnClicked().RemoveAll(this);
	if (Btn_ZeroGuardTarget) Btn_ZeroGuardTarget->OnClicked().RemoveAll(this);
	if (Btn_MaxGuardTarget) Btn_MaxGuardTarget->OnClicked().RemoveAll(this);
	if (Btn_SendReinforcement) Btn_SendReinforcement->OnClicked().RemoveAll(this);
	if (Btn_IntelligenceAll) Btn_IntelligenceAll->OnClicked().RemoveAll(this);
	if (Btn_IntelligenceConflict) Btn_IntelligenceConflict->OnClicked().RemoveAll(this);
	if (Btn_IntelligenceControl) Btn_IntelligenceControl->OnClicked().RemoveAll(this);
	if (Btn_IntelligenceEconomy) Btn_IntelligenceEconomy->OnClicked().RemoveAll(this);
	if (Btn_IntelligenceCommand) Btn_IntelligenceCommand->OnClicked().RemoveAll(this);
	if (Btn_IntelligenceProduction) Btn_IntelligenceProduction->OnClicked().RemoveAll(this);
	if (Btn_IntelligenceDiplomacy) Btn_IntelligenceDiplomacy->OnClicked().RemoveAll(this);
	if (DistrictSearchBox)
	{
		DistrictSearchBox->OnTextChanged.RemoveDynamic(this, &UTerritoryJournalWidget::HandleSearchChanged);
	}
	if (DistrictOwnerFilter)
	{
		DistrictOwnerFilter->OnSelectionChanged.RemoveDynamic(this, &UTerritoryJournalWidget::HandleOwnerFilterChanged);
	}
	if (DistrictStateFilter)
	{
		DistrictStateFilter->OnSelectionChanged.RemoveDynamic(this, &UTerritoryJournalWidget::HandleStateFilterChanged);
	}
	if (DistrictOperationalFilter)
	{
		DistrictOperationalFilter->OnSelectionChanged.RemoveDynamic(
			this, &UTerritoryJournalWidget::HandleOperationalFilterChanged);
	}

	UnbindTerritoryDelegates();
	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnGuardPurchaseResult.RemoveDynamic(
			this, &UTerritoryJournalWidget::HandleGuardManagementResult);
		ManagementComponent->OnLiveEventsChanged.RemoveDynamic(
			this, &UTerritoryJournalWidget::HandleLiveEventsChanged);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	Super::NativeDestruct();
}

void UTerritoryJournalWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshDistrictList();
	if (TerritoryReveal)
	{
		PlayAnimation(TerritoryReveal);
	}
}

void UTerritoryJournalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!CommandCenterResponsiveWidth)
	{
		return;
	}

	const float AvailableWidth = MyGeometry.GetLocalSize().X;
	if (AvailableWidth <= 0.f)
	{
		return;
	}
	const bool bCompact = AvailableWidth < 800.f;
	if (bResponsiveLayoutApplied && bCompactResponsiveLayout == bCompact)
	{
		return;
	}

	if (UBorderSlot* ResponsiveSlot = Cast<UBorderSlot>(CommandCenterResponsiveWidth->Slot))
	{
		ResponsiveSlot->SetHorizontalAlignment(bCompact ? HAlign_Fill : HAlign_Center);
		ResponsiveSlot->SetVerticalAlignment(VAlign_Fill);
		bCompactResponsiveLayout = bCompact;
		bResponsiveLayoutApplied = true;
	}
}

void UTerritoryJournalWidget::BindTerritoryDelegates()
{
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleTerritoryRegistered);
			Registry->OnTerritoryUnregistered.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleTerritoryUnregistered);
		}
	}
}

void UTerritoryJournalWidget::UnbindTerritoryDelegates()
{
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(this, &UTerritoryJournalWidget::HandleTerritoryRegistered);
			Registry->OnTerritoryUnregistered.RemoveDynamic(this, &UTerritoryJournalWidget::HandleTerritoryUnregistered);
		}
	}
}

void UTerritoryJournalWidget::BindManagementComponent()
{
	APlayerController* PlayerController = GetOwningPlayer();
	UTerritoryPlayerManagementComponent* Component = PlayerController
		? UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(PlayerController)
		: nullptr;
	if (ManagementComponent.Get() == Component)
	{
		return;
	}

	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnGuardPurchaseResult.RemoveDynamic(
			this, &UTerritoryJournalWidget::HandleGuardManagementResult);
		ManagementComponent->OnLiveEventsChanged.RemoveDynamic(
			this, &UTerritoryJournalWidget::HandleLiveEventsChanged);
	}
	ManagementComponent = Component;
	LastLiveEventRevision = INDEX_NONE;
	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnGuardPurchaseResult.AddUniqueDynamic(
			this, &UTerritoryJournalWidget::HandleGuardManagementResult);
		ManagementComponent->OnLiveEventsChanged.AddUniqueDynamic(
			this, &UTerritoryJournalWidget::HandleLiveEventsChanged);
	}
}

void UTerritoryJournalWidget::BuildLiveEventPanel()
{
	if (LiveEventsBox || !WidgetTree) return;
	UVerticalBox* ReportsStack = Cast<UVerticalBox>(
		WidgetTree->FindWidget(TEXT("ReportsStack")));
	if (!ReportsStack) return;

	UBorder* FeedSurface = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("LiveEventFeedSurface"));
	FeedSurface->SetPadding(FMargin(8.f, 6.f));
	StyleGeneratedTerritorySurface(FeedSurface,
		FLinearColor(0.012f, 0.012f, 0.012f, 0.97f),
		FLinearColor(0.82f, 0.68f, 0.f, 0.48f), 2.f);
	if (UVerticalBoxSlot* FeedSlot = ReportsStack->AddChildToVerticalBox(FeedSurface))
	{
		FeedSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	}
	UVerticalBox* FeedStack = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("LiveEventFeedStack"));
	FeedSurface->SetContent(FeedStack);
	Text_LiveEventCount = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("Text_LiveEventCount"));
	StyleGeneratedTerritoryText(Text_LiveEventCount, 13,
		FLinearColor(1.f, 0.84f, 0.f, 1.f),
		ETerritoryGeneratedTextRole::Heading);
	FeedStack->AddChildToVerticalBox(Text_LiveEventCount);
	SelectedIntelligenceFilter = ETerritoryIntelligenceFilter::All;
	LiveEventsBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("LiveEventsBox"));
	FeedStack->AddChildToVerticalBox(LiveEventsBox);
	RefreshLiveEvents();
}

void UTerritoryJournalWidget::RefreshLiveEvents()
{
	if (!LiveEventsBox) return;
	const TArray<FTerritoryLiveEvent> AllEvents = ManagementComponent.IsValid()
		? ManagementComponent->GetTerritoryIntelligence(
			ETerritoryIntelligenceFilter::All, true)
		: TArray<FTerritoryLiveEvent>();
	const TArray<FTerritoryLiveEvent>& Events = AllEvents;
	const int32 Revision = GetLiveEventListRevision(Events);
	if (LastLiveEventRevision == Revision)
	{
		return;
	}
	LastLiveEventRevision = Revision;
	LiveEventsBox->ClearChildren();
	for (const FTerritoryLiveEvent& Event : Events)
	{
		UTerritoryLiveEventRowWidget* Row =
			CreateWidget<UTerritoryLiveEventRowWidget>(
				this, UTerritoryLiveEventRowWidget::StaticClass());
		if (!Row) continue;
		Row->InitializeLiveEvent(Event);
		Row->OnWaypointRequested.AddUniqueDynamic(
			this, &UTerritoryJournalWidget::HandleLiveEventWaypointRequested);
		if (UVerticalBoxSlot* RowSlot = LiveEventsBox->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 2.f));
		}
	}
	if (Text_LiveEventCount)
	{
		Text_LiveEventCount->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "LiveEventCount",
				"LIVE NOTIFICATIONS  //  {0}"), FText::AsNumber(Events.Num())));
	}
	if (Events.IsEmpty())
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
			UNarrativeCommonTextBlock::StaticClass(), TEXT("NoLiveTerritoryEvents"));
		Empty->SetText(AllEvents.IsEmpty()
			? NSLOCTEXT("TerritoryJournal", "NoLiveEvents",
				"No Territory notifications have been recorded yet.")
			: NSLOCTEXT("TerritoryJournal", "NoFilteredIntelligence",
				"No reports match this intelligence category."));
		StyleGeneratedTerritoryText(Empty, 12,
			FLinearColor(0.48f, 0.54f, 0.53f, 1.f),
			ETerritoryGeneratedTextRole::Muted);
		LiveEventsBox->AddChild(Empty);
	}
}

void UTerritoryJournalWidget::BuildGarrisonManagementControls()
{
	if (GuardTargetSpinBox || !WidgetTree) return;
	UVerticalBox* PlannerHost = Cast<UVerticalBox>(
		WidgetTree->FindWidget(TEXT("GarrisonPlannerHost")));
	if (!PlannerHost)
	{
		PlannerHost = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("CommandStack")));
	}
	if (!PlannerHost) return;

	UBorder* PlannerCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("GarrisonPlannerCard"));
	PlannerCard->SetPadding(FMargin(12.f, 10.f));
	StyleGeneratedTerritorySurface(PlannerCard,
		FLinearColor(0.014f, 0.014f, 0.014f, 0.97f),
		FLinearColor(0.82f, 0.68f, 0.f, 0.62f), 2.f);
	PlannerHost->AddChild(PlannerCard);
	UVerticalBox* PlannerStack = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("GarrisonPlannerStack"));
	PlannerCard->SetContent(PlannerStack);

	UTextBlock* Heading = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("Text_GarrisonPlannerHeading"));
	Heading->SetText(NSLOCTEXT("TerritoryJournal", "GarrisonPlannerHeading",
		"GARRISON STAFFING PLAN"));
	StyleGeneratedTerritoryText(Heading, 16, FLinearColor(0.95f, 0.96f, 0.95f, 1.f));
	PlannerStack->AddChild(Heading);

	Text_GarrisonTargetName = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("Text_GarrisonTargetName"));
	Text_GarrisonTargetName->SetText(NSLOCTEXT(
		"TerritoryJournal", "NoPlannerTargetName", "SELECT A GARRISON TARGET"));
	StyleGeneratedTerritoryText(Text_GarrisonTargetName, 18, FLinearColor(1.f, 0.84f, 0.f, 1.f));
	PlannerStack->AddChild(Text_GarrisonTargetName);

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	TSubclassOf<UNarrativeCommonButtonBase> ButtonClass = Settings
		? Settings->DefaultNarrativeButtonClass.LoadSynchronous() : nullptr;
	const TSubclassOf<UCommonButtonStyle> ButtonStyle = Settings
		? Settings->DefaultTerritoryButtonStyle.LoadSynchronous() : nullptr;
	auto ApplyTerritoryStyle = [ButtonStyle](UNarrativeCommonButtonBase* Button)
	{
		if (Button && ButtonStyle)
		{
			Button->SetStyle(ButtonStyle);
		}
	};
	if (ButtonClass)
	{
		UHorizontalBox* TargetNavigation = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("GarrisonTargetNavigation"));
		PlannerStack->AddChild(TargetNavigation);
		Btn_PreviousGarrisonTarget = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
			ButtonClass, TEXT("Btn_PreviousGarrisonTarget"));
		ApplyTerritoryStyle(Btn_PreviousGarrisonTarget);
		Btn_PreviousGarrisonTarget->SetButtonText(NSLOCTEXT(
			"TerritoryJournal", "PreviousGarrisonTarget", "PREVIOUS POST"));
		Btn_PreviousGarrisonTarget->SetToolTipText(NSLOCTEXT(
			"TerritoryJournal", "PreviousGarrisonTargetTip", "Select the previous District or Property garrison."));
		Btn_PreviousGarrisonTarget->OnClicked().AddUObject(
			this, &UTerritoryJournalWidget::HandlePreviousGarrisonTargetClicked);
		TargetNavigation->AddChild(Btn_PreviousGarrisonTarget);
		Btn_NextGarrisonTarget = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
			ButtonClass, TEXT("Btn_NextGarrisonTarget"));
		ApplyTerritoryStyle(Btn_NextGarrisonTarget);
		Btn_NextGarrisonTarget->SetButtonText(NSLOCTEXT(
			"TerritoryJournal", "NextGarrisonTarget", "NEXT POST"));
		Btn_NextGarrisonTarget->SetToolTipText(NSLOCTEXT(
			"TerritoryJournal", "NextGarrisonTargetTip", "Select the next District or Property garrison."));
		Btn_NextGarrisonTarget->OnClicked().AddUObject(
			this, &UTerritoryJournalWidget::HandleNextGarrisonTargetClicked);
		TargetNavigation->AddChild(Btn_NextGarrisonTarget);
	}

	UTextBlock* TargetHeading = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("Text_AssignedGuardTargetHeading"));
	TargetHeading->SetText(NSLOCTEXT(
		"TerritoryJournal", "AssignedGuardTargetHeading", "PROPOSED ASSIGNED GUARDS"));
	StyleGeneratedTerritoryText(TargetHeading, 12, FLinearColor(0.65f, 0.69f, 0.68f, 1.f));
	PlannerStack->AddChild(TargetHeading);

	GuardTargetSpinBox = WidgetTree->ConstructWidget<UNarrativeSpinBox>(
		UNarrativeSpinBox::StaticClass(), TEXT("GuardTargetSpinBox"));
	GuardTargetSpinBox->SetToolTipText(NSLOCTEXT("TerritoryJournal", "GuardTargetTip",
		"Set the exact assigned guard target. The preview shows recruitment and recurring upkeep."));
	GuardTargetSpinBox->SetMinValue(0.f);
	GuardTargetSpinBox->SetMinSliderValue(0.f);
	GuardTargetSpinBox->SetDelta(1.f);
	GuardTargetSpinBox->SetMinFractionalDigits(0);
	GuardTargetSpinBox->SetMaxFractionalDigits(0);
	GuardTargetSpinBox->OnValueChanged.AddUniqueDynamic(
		this, &UTerritoryJournalWidget::HandleGuardTargetSpinChanged);
	PlannerStack->AddChild(GuardTargetSpinBox);

	GarrisonStaffingProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("GarrisonStaffingProgressBar"));
	const bool bUsesAuthoredProgressFill = StyleGeneratedTerritoryProgress(
		GarrisonStaffingProgressBar, true);
	GarrisonStaffingProgressBar->SetFillColorAndOpacity(bUsesAuthoredProgressFill
		? FLinearColor::White : FLinearColor(0.08f, 0.88f, 0.62f, 1.f));
	PlannerStack->AddChild(GarrisonStaffingProgressBar);

	Text_GarrisonStaffing = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("Text_GarrisonStaffing"));
	StyleGeneratedTerritoryText(Text_GarrisonStaffing, 14, FLinearColor(0.95f, 0.96f, 0.95f, 1.f));
	PlannerStack->AddChild(Text_GarrisonStaffing);
	Text_GarrisonFinance = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("Text_GarrisonFinance"));
	StyleGeneratedTerritoryText(Text_GarrisonFinance, 14, FLinearColor(0.96f, 0.72f, 0.38f, 1.f));
	PlannerStack->AddChild(Text_GarrisonFinance);
	Text_CommandCapabilities = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("Text_CommandCapabilities"));
	StyleGeneratedTerritoryText(Text_CommandCapabilities, 12,
		FLinearColor(0.74f, 0.78f, 0.76f, 1.f), ETerritoryGeneratedTextRole::Muted);
	PlannerStack->AddChild(Text_CommandCapabilities);

	if (!ButtonClass) return;

	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("GarrisonTargetActions"));
	PlannerStack->AddChild(Actions);
	Btn_ApplyGuardTarget = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
		ButtonClass, TEXT("Btn_ApplyGuardTarget"));
	ApplyTerritoryStyle(Btn_ApplyGuardTarget);
	Btn_ApplyGuardTarget->SetButtonText(NSLOCTEXT("TerritoryJournal", "ApplyGuardTarget", "APPLY PLAN"));
	Btn_ApplyGuardTarget->SetToolTipText(NSLOCTEXT("TerritoryJournal", "ApplyGuardTargetTip",
		"Submit the exact target to the authoritative server."));
	Btn_ApplyGuardTarget->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleApplyGuardTargetClicked);
	Actions->AddChild(Btn_ApplyGuardTarget);
	Btn_ZeroGuardTarget = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
		ButtonClass, TEXT("Btn_ZeroGuardTarget"));
	ApplyTerritoryStyle(Btn_ZeroGuardTarget);
	Btn_ZeroGuardTarget->SetButtonText(NSLOCTEXT("TerritoryJournal", "ZeroGuardTarget", "EMPTY POST"));
	Btn_ZeroGuardTarget->SetToolTipText(NSLOCTEXT("TerritoryJournal", "ZeroGuardTargetTip",
		"Withdraw this garrison and reduce its future upkeep to zero."));
	Btn_ZeroGuardTarget->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleZeroGuardTargetClicked);
	Actions->AddChild(Btn_ZeroGuardTarget);
	Btn_MaxGuardTarget = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
		ButtonClass, TEXT("Btn_MaxGuardTarget"));
	ApplyTerritoryStyle(Btn_MaxGuardTarget);
	Btn_MaxGuardTarget->SetButtonText(NSLOCTEXT("TerritoryJournal", "MaxGuardTarget", "FULL CAPACITY"));
	Btn_MaxGuardTarget->SetToolTipText(NSLOCTEXT("TerritoryJournal", "MaxGuardTargetTip",
		"Set this garrison to its authored physical capacity."));
	Btn_MaxGuardTarget->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleMaxGuardTargetClicked);
	Actions->AddChild(Btn_MaxGuardTarget);

	UHorizontalBox* ReinforcementActions = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("ReinforcementActions"));
	PlannerStack->AddChild(ReinforcementActions);
	Btn_SendReinforcement = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
		ButtonClass, TEXT("Btn_SendReinforcement"));
	ApplyTerritoryStyle(Btn_SendReinforcement);
	Btn_SendReinforcement->SetButtonText(NSLOCTEXT(
		"TerritoryJournal", "SendReinforcement", "SEND 1 RESERVE"));
	Btn_SendReinforcement->SetToolTipText(NSLOCTEXT(
		"TerritoryJournal", "SendReinforcementTip",
		"Immediately fill one empty assigned post using an existing reserve. This does not recruit a new guard."));
	Btn_SendReinforcement->OnClicked().AddUObject(
		this, &UTerritoryJournalWidget::HandleSendReinforcementClicked);
	ReinforcementActions->AddChild(Btn_SendReinforcement);
}

void UTerritoryJournalWidget::RefreshGarrisonManagementControls(
	const FTerritoryDistrictOperationsView& View)
{
	TWeakObjectPtr<ATerritoryVolume> Preferred = SelectedGarrisonTarget;
	SelectedGarrisonTarget.Reset();
	TMap<FString, TWeakObjectPtr<ATerritoryVolume>> NewOptions;
	TArray<FString> NewOptionOrder;
	GarrisonTargetOrder.Reset();

	for (const FTerritoryGarrisonOperationsView& Garrison : View.GarrisonTargets)
	{
		if (!Garrison.Territory) continue;
		const FString TypeLabel = Garrison.bDistrictGarrison ? TEXT("District") : TEXT("Property");
		const FString Option = FString::Printf(TEXT("%s — %s [%s]"),
			*Garrison.DisplayName.ToString(), *TypeLabel, *Garrison.TerritoryTag.ToString());
		NewOptions.Add(Option, Garrison.Territory);
		NewOptionOrder.Add(Option);
		GarrisonTargetOrder.Add(Garrison.Territory);
		if (Preferred.Get() == Garrison.Territory)
		{
			SelectedGarrisonTarget = Garrison.Territory;
		}
	}
	bool bOptionsChanged = NewOptions.Num() != GarrisonTargetOptions.Num();
	if (!bOptionsChanged)
	{
		for (const TPair<FString, TWeakObjectPtr<ATerritoryVolume>>& Pair : NewOptions)
		{
			const TWeakObjectPtr<ATerritoryVolume>* Existing = GarrisonTargetOptions.Find(Pair.Key);
			if (!Existing || *Existing != Pair.Value)
			{
				bOptionsChanged = true;
				break;
			}
		}
	}
	GarrisonTargetOptions = MoveTemp(NewOptions);
	if (GarrisonTargetSelector && bOptionsChanged)
	{
		GarrisonTargetSelector->ClearOptions();
		for (const FString& Option : NewOptionOrder)
		{
			GarrisonTargetSelector->AddOption(Option);
		}
	}
	if (!SelectedGarrisonTarget.IsValid())
	{
		for (const FTerritoryGarrisonOperationsView& Garrison : View.GarrisonTargets)
		{
			if (Garrison.bManageable && Garrison.MaximumGuards > 0)
			{
				SelectedGarrisonTarget = Garrison.Territory;
				break;
			}
		}
	}
	if (!SelectedGarrisonTarget.IsValid() && View.GarrisonTargets.Num() > 0)
	{
		SelectedGarrisonTarget = View.GarrisonTargets[0].Territory;
	}
	if (GarrisonTargetSelector && SelectedGarrisonTarget.IsValid())
	{
		for (const TPair<FString, TWeakObjectPtr<ATerritoryVolume>>& Pair : GarrisonTargetOptions)
		{
			if (Pair.Value == SelectedGarrisonTarget)
			{
				GarrisonTargetSelector->SetSelectedOption(Pair.Key);
				break;
			}
		}
	}
	const bool bHasSeveralTargets = GarrisonTargetOrder.Num() > 1;
	if (Btn_PreviousGarrisonTarget) Btn_PreviousGarrisonTarget->SetIsEnabled(bHasSeveralTargets);
	if (Btn_NextGarrisonTarget) Btn_NextGarrisonTarget->SetIsEnabled(bHasSeveralTargets);
	if (GuardTargetSpinBox)
	{
		const ATerritoryVolume* Target = SelectedGarrisonTarget.Get();
		const float Maximum = Target ? Target->GetMaxGuardCount() : 0.f;
		GuardTargetSpinBox->SetMaxValue(Maximum);
		GuardTargetSpinBox->SetMaxSliderValue(Maximum);
		GuardTargetSpinBox->SetValue(Target ? Target->GetDesiredGuardCount() : 0.f);
		GuardTargetSpinBox->SetIsEnabled(Target != nullptr && Maximum > 0.f);
	}
	if (Text_CommandCapabilities)
	{
		TArray<FString> CapabilityLines;
		CapabilityLines.Add(TEXT("STRATEGIC CONTROLS"));
		for (const FTerritoryCommandCapabilityView& Capability : View.CommandCapabilities)
		{
			CapabilityLines.Add(FString::Printf(TEXT("[%s] %s — %s"),
				Capability.bGranted ? TEXT("ONLINE") : TEXT("LOCKED"),
				*Capability.DisplayName.ToString(),
				*Capability.AvailabilityReason.ToString()));
		}
		Text_CommandCapabilities->SetText(FText::FromString(
			FString::Join(CapabilityLines, TEXT("\n"))));
	}
	UpdateGarrisonTargetPreview();
}

void UTerritoryJournalWidget::UpdateGarrisonTargetPreview()
{
	FTerritoryGarrisonOperationsView View;
	const bool bHasTarget = UTerritoryUIBlueprintLibrary::BuildGarrisonOperationsView(
		this, SelectedGarrisonTarget.Get(), GetOwningPlayer(), View);
	const int32 Proposed = GuardTargetSpinBox
		? FMath::RoundToInt(GuardTargetSpinBox->GetValue()) : View.DesiredGuards;
	if (!bHasTarget)
	{
		const FText Missing = NSLOCTEXT("TerritoryJournal", "NoGarrisonTarget",
			"No loaded garrison is available for this district.");
		if (Text_GarrisonTargetName) Text_GarrisonTargetName->SetText(
			NSLOCTEXT("TerritoryJournal", "MissingGarrisonTargetName", "NO GARRISON TARGET"));
		if (Text_GarrisonStaffing) Text_GarrisonStaffing->SetText(Missing);
		if (Text_GarrisonFinance) Text_GarrisonFinance->SetText(FText::GetEmpty());
		if (Text_GarrisonTargetPreview) Text_GarrisonTargetPreview->SetText(Missing);
		if (GarrisonStaffingProgressBar) GarrisonStaffingProgressBar->SetPercent(0.f);
	}
	else
	{
		const int32 Recruitment = FMath::Max(0, Proposed - View.DesiredGuards)
			* View.RecruitmentCostPerGuard;
		const int64 ProposedUpkeep = static_cast<int64>(Proposed) * View.UpkeepPerGuard;
		const int64 ProposedNet = View.PeriodicIncome - ProposedUpkeep;
		if (Text_GarrisonTargetName)
		{
			Text_GarrisonTargetName->SetText(FText::Format(
				NSLOCTEXT("TerritoryJournal", "GarrisonTargetName", "{0}  |  {1}"),
				View.DisplayName,
				View.bDistrictGarrison
					? NSLOCTEXT("TerritoryJournal", "DistrictPostType", "DISTRICT POST")
					: NSLOCTEXT("TerritoryJournal", "PropertyPostType", "PROPERTY POST")));
		}
		const FText Staffing = FText::Format(
			NSLOCTEXT("TerritoryJournal", "GarrisonStaffingReadout",
				"ACTIVE  {0}    ASSIGNED  {1} -> {2}    CAPACITY  {3}\nRESERVE  {4}    DEPLOYING  {5}"),
			FText::AsNumber(View.ActiveGuards), FText::AsNumber(View.DesiredGuards),
			FText::AsNumber(Proposed), FText::AsNumber(View.MaximumGuards),
			FText::AsNumber(View.ReserveGuards), FText::AsNumber(View.PendingDeployments));
		const FText Finance = FText::Format(
			NSLOCTEXT("TerritoryJournal", "GarrisonFinanceReadout",
				"RECRUIT NOW  {0}    INCOME  {1}/CYCLE\nUPKEEP  {2}/CYCLE    PROJECTED NET  {3}/CYCLE"),
			FText::AsNumber(Recruitment), FText::AsNumber(View.PeriodicIncome),
			FText::AsNumber(ProposedUpkeep), FText::AsNumber(ProposedNet));
		if (Text_GarrisonStaffing) Text_GarrisonStaffing->SetText(Staffing);
		if (Text_GarrisonFinance) Text_GarrisonFinance->SetText(Finance);
		if (Text_GarrisonTargetPreview)
		{
			Text_GarrisonTargetPreview->SetText(FText::Format(
				NSLOCTEXT("TerritoryJournal", "LegacyGarrisonTargetPreview", "{0}\n{1}\n{2}"),
				View.DisplayName, Staffing, Finance));
		}
		if (GarrisonStaffingProgressBar)
		{
			GarrisonStaffingProgressBar->SetPercent(View.MaximumGuards > 0
				? FMath::Clamp(static_cast<float>(Proposed) / View.MaximumGuards, 0.f, 1.f)
				: 0.f);
		}
	}
	const bool bCanSubmit = bHasTarget && View.bManageable && Proposed != View.DesiredGuards
		&& Proposed >= 0 && Proposed <= View.MaximumGuards;
	if (Btn_ApplyGuardTarget) Btn_ApplyGuardTarget->SetIsEnabled(bCanSubmit);
	if (Btn_ZeroGuardTarget) Btn_ZeroGuardTarget->SetIsEnabled(
		bHasTarget && View.bManageable && View.DesiredGuards > 0);
	if (Btn_MaxGuardTarget) Btn_MaxGuardTarget->SetIsEnabled(
		bHasTarget && View.bManageable && View.DesiredGuards < View.MaximumGuards);
	if (Btn_SendReinforcement)
	{
		Btn_SendReinforcement->SetIsEnabled(
			bHasTarget && View.bCanSendReinforcements);
		Btn_SendReinforcement->SetToolTipText(View.bCanSendReinforcements
			? NSLOCTEXT("TerritoryJournal", "SendReinforcementReady",
				"Deploy one reserve into an empty assigned post now. The staffing target and recruitment balance do not change.")
			: View.ReinforcementFailureReason);
	}
}

void UTerritoryJournalWidget::SelectRelativeGarrisonTarget(int32 Direction)
{
	if (GarrisonTargetOrder.Num() == 0 || Direction == 0)
	{
		return;
	}
	int32 CurrentIndex = GarrisonTargetOrder.IndexOfByPredicate(
		[this](const TWeakObjectPtr<ATerritoryVolume>& Candidate)
		{
			return Candidate == SelectedGarrisonTarget;
		});
	if (CurrentIndex == INDEX_NONE)
	{
		CurrentIndex = 0;
	}
	const int32 NextIndex = (CurrentIndex + Direction % GarrisonTargetOrder.Num()
		+ GarrisonTargetOrder.Num()) % GarrisonTargetOrder.Num();
	SelectedGarrisonTarget = GarrisonTargetOrder[NextIndex];
	if (GuardTargetSpinBox && SelectedGarrisonTarget.IsValid())
	{
		const float Maximum = SelectedGarrisonTarget->GetMaxGuardCount();
		GuardTargetSpinBox->SetMaxValue(Maximum);
		GuardTargetSpinBox->SetMaxSliderValue(Maximum);
		GuardTargetSpinBox->SetValue(SelectedGarrisonTarget->GetDesiredGuardCount());
		GuardTargetSpinBox->SetIsEnabled(Maximum > 0.f);
	}
	if (GarrisonTargetSelector && SelectedGarrisonTarget.IsValid())
	{
		for (const TPair<FString, TWeakObjectPtr<ATerritoryVolume>>& Pair : GarrisonTargetOptions)
		{
			if (Pair.Value == SelectedGarrisonTarget)
			{
				GarrisonTargetSelector->SetSelectedOption(Pair.Key);
				break;
			}
		}
	}
	UpdateGarrisonTargetPreview();
}

void UTerritoryJournalWidget::SubmitSelectedGuardTarget(int32 NewDesiredGuardCount)
{
	BindManagementComponent();
	ATerritoryVolume* Target = SelectedGarrisonTarget.Get();
	if (!ManagementComponent.IsValid() || !Target) return;
	if (Text_CommandStatus)
	{
		Text_CommandStatus->SetText(NSLOCTEXT("TerritoryJournal", "SubmittingTarget",
			"Submitting garrison staffing target..."));
	}
	ManagementComponent->RequestSetGuardTargetForTerritory(Target, NewDesiredGuardCount);
}

void UTerritoryJournalWidget::RefreshFilterOptions()
{
	const TArray<FTerritoryDistrictOperationsView> VisibleViews =
		UTerritoryUIBlueprintLibrary::GetPlayerVisibleDistrictOperationsViews(
			this, GetOwningPlayer(), ETerritoryOperationsFilter::All);

	if (!bFiltersInitialized)
	{
		SelectedOwnerFilter = AllOwnersOption;
		SelectedStateFilter = AllStatesOption;
	}

	OwnerFilterTags.Empty();
	if (DistrictOwnerFilter)
	{
		DistrictOwnerFilter->ClearOptions();
		DistrictOwnerFilter->AddOption(AllOwnersOption);
	}
	if (DistrictStateFilter)
	{
		DistrictStateFilter->ClearOptions();
		DistrictStateFilter->AddOption(AllStatesOption);
	}
	if (DistrictOperationalFilter)
	{
		DistrictOperationalFilter->ClearOptions();
		DistrictOperationalFilter->AddOption(AllOperationsOption);
		DistrictOperationalFilter->AddOption(UnlockedOption);
		DistrictOperationalFilter->AddOption(AvailableOption);
		DistrictOperationalFilter->AddOption(OwnedOption);
		DistrictOperationalFilter->AddOption(ManageableOption);
		DistrictOperationalFilter->AddOption(UnderAttackOption);
		DistrictOperationalFilter->AddOption(ContestedOption);
		DistrictOperationalFilter->AddOption(FinancialRiskOption);
		DistrictOperationalFilter->AddOption(ProducingOption);
		DistrictOperationalFilter->AddOption(ProductionBlockedOption);
		DistrictOperationalFilter->AddOption(MissingInputsOption);
		DistrictOperationalFilter->AddOption(StorageFullOption);
	}

	TSet<FString> OwnerNames;
	TSet<FString> StateNames;
	for (const FTerritoryDistrictOperationsView& View : VisibleViews)
	{
		ATerritoryDistrict* District = View.District;
		if (!District)
		{
			continue;
		}

		const FGameplayTag OwnerTag = District->GetOwningFaction();
		const FString OwnerName = UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(OwnerTag).ToString();
		if (!OwnerName.IsEmpty() && !OwnerNames.Contains(OwnerName))
		{
			OwnerNames.Add(OwnerName);
			OwnerFilterTags.Add(OwnerName, OwnerTag);
			if (DistrictOwnerFilter)
			{
				DistrictOwnerFilter->AddOption(OwnerName);
			}
		}

		const FString StateName = GetStateOption(District);
		if (!StateName.IsEmpty() && !StateNames.Contains(StateName))
		{
			StateNames.Add(StateName);
			if (DistrictStateFilter)
			{
				DistrictStateFilter->AddOption(StateName);
			}
		}
	}
	if (SelectedOperationsFilter == ETerritoryOperationsFilter::Locked)
	{
		SelectedOperationsFilter = ETerritoryOperationsFilter::All;
	}

	// CommonUI-backed controls may synchronously broadcast selection changes.
	// Mark initialization complete before restoring the selected options.
	bFiltersInitialized = true;

	if (DistrictOwnerFilter)
	{
		DistrictOwnerFilter->SetSelectedOption(SelectedOwnerFilter.IsEmpty() ? AllOwnersOption : SelectedOwnerFilter);
	}
	if (DistrictStateFilter)
	{
		DistrictStateFilter->SetSelectedOption(SelectedStateFilter.IsEmpty() ? AllStatesOption : SelectedStateFilter);
	}
	if (DistrictOperationalFilter)
	{
		DistrictOperationalFilter->SetSelectedOption(GetOperationsOption(SelectedOperationsFilter));
	}
}

bool UTerritoryJournalWidget::PassesFilters(const FTerritoryDistrictOperationsView& View) const
{
	const ATerritoryDistrict* District = View.District;
	if (!District)
	{
		return false;
	}

	if (!UTerritoryUIBlueprintLibrary::DoesDistrictMatchSearch(View, SearchFilter))
	{
		return false;
	}

	if (!SelectedOwnerFilter.IsEmpty() && SelectedOwnerFilter != AllOwnersOption)
	{
		const FGameplayTag* OwnerTag = OwnerFilterTags.Find(SelectedOwnerFilter);
		if (!OwnerTag || District->GetOwningFaction() != *OwnerTag)
		{
			return false;
		}
	}

	const bool bPassesState = SelectedStateFilter.IsEmpty()
		|| SelectedStateFilter == AllStatesOption
		|| SelectedStateFilter == GetStateOption(District);
	return bPassesState
		&& UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(View, SelectedOperationsFilter);
}

void UTerritoryJournalWidget::RefreshDistrictList()
{
	RefreshLiveEvents();
	if (!bFiltersInitialized)
	{
		RefreshFilterOptions();
	}

	// Include every displayed authority in the revision. Count+filter caching left guard,
	// economy, capture, and assault rows stale whenever the number of districts was stable.
	const TArray<FTerritoryDistrictOperationsView> AllViews =
		UTerritoryUIBlueprintLibrary::GetPlayerVisibleDistrictOperationsViews(
			this, GetOwningPlayer(), ETerritoryOperationsFilter::All);
	RefreshCommandCenterIdentity(AllViews);
	uint32 Revision = GetTypeHash(SelectedOwnerFilter);
	Revision = HashCombineFast(Revision, GetTypeHash(SelectedStateFilter));
	Revision = HashCombineFast(Revision, GetTypeHash(SearchFilter));
	Revision = HashCombineFast(Revision, GetTypeHash(static_cast<uint8>(SelectedOperationsFilter)));
	for (const FTerritoryDistrictOperationsView& View : AllViews)
	{
		Revision = HashCombineFast(Revision,
			static_cast<uint32>(UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(View)));
	}
	if (LastOperationsRevision == static_cast<int32>(Revision))
	{
		return;
	}
	LastOperationsRevision = static_cast<int32>(Revision);
	TerritoryEntryWidgets.Reset();

	if (DistrictList)
	{
		DistrictList->ClearChildren();
	}

	int32 VisibleCount = 0;
	ATerritoryDistrict* FirstOwnedDistrict = nullptr;
	bool bSelectedStillVisible = false;
	FGameplayTag LastDirectoryCity;
	bool bHasDirectoryHeading = false;
	for (const FTerritoryDistrictOperationsView& View : AllViews)
	{
		if (!PassesFilters(View))
		{
			continue;
		}

		if (!FirstOwnedDistrict
			&& UTerritoryUIBlueprintLibrary::IsDistrictCapturedOwned(View))
		{
			FirstOwnedDistrict = View.District;
		}
		bSelectedStillVisible |= SelectedDistrict.Get() == View.District;
		++VisibleCount;

		if (DistrictList)
		{
			if (!bHasDirectoryHeading || View.CityTag != LastDirectoryCity)
			{
				const FText CityName = View.CityDisplayName.IsEmpty()
					? NSLOCTEXT("TerritoryJournal", "IndependentDistricts", "INDEPENDENT DISTRICTS")
					: View.CityDisplayName;
				if (UTextBlock* Heading = CreateHierarchyTextRow(
					FText::Format(NSLOCTEXT("TerritoryJournal", "CityDirectoryHeading", "CITY  /  {0}"), CityName),
					FName(*FString::Printf(TEXT("DirectoryCity_%u"), GetTypeHash(View.CityTag))), true))
				{
					DistrictList->AddChild(Heading);
				}
				LastDirectoryCity = View.CityTag;
				bHasDirectoryHeading = true;
			}
			if (UTerritoryDistrictRowWidget* Row = CreateOperationsRow(View))
			{
				DistrictList->AddChild(Row);
			}
		}
	}

	if (VisibleCount == 0 && DistrictList)
	{
		UTextBlock* EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NoDistrictsMessage"));
		EmptyText->SetText(NSLOCTEXT("TerritoryJournal", "NoDistricts", "No districts match the current filters."));
		StyleGeneratedTerritoryText(EmptyText, 14, FLinearColor(0.65f, 0.69f, 0.68f, 1.f));
		DistrictList->AddChild(EmptyText);
	}
	if (VisibleCount <= 0)
	{
		UpdateSelectedDistrict(nullptr);
	}
	else if (!SelectedDistrict.IsValid() || !bSelectedStillVisible)
	{
		bSelectedTerritoryInfoRequested = FirstOwnedDistrict != nullptr;
		UpdateSelectedDistrict(FirstOwnedDistrict);
	}
	else if (SelectedDistrict.IsValid())
	{
		UpdateSelectedDistrict(SelectedDistrict.Get());
	}

	if (Text_FilterSummary)
	{
		Text_FilterSummary->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "FilterSummary", "{0} visible districts  |  City > District > Place"),
			FText::AsNumber(VisibleCount)));
	}
	RefreshOperationalSummaries(AllViews);
	RefreshEntrySelection();
}

void UTerritoryJournalWidget::RefreshCommandCenterIdentity(
	const TArray<FTerritoryDistrictOperationsView>& Views)
{
	TMap<FString, FText> Cities;
	int32 OwnedDistricts = 0;
	int32 VisiblePlaces = 0;
	int32 HiddenPlaces = 0;
	for (const FTerritoryDistrictOperationsView& View : Views)
	{
		const FString CityKey = View.CityTag.IsValid()
			? View.CityTag.ToString()
			: FString::Printf(TEXT("Independent:%s"), *View.CityDisplayName.ToString());
		Cities.FindOrAdd(CityKey) = View.CityDisplayName.IsEmpty()
			? NSLOCTEXT("TerritoryJournal", "IndependentNetwork", "Independent Territories")
			: View.CityDisplayName;
		OwnedDistricts += View.bOwnedByViewer ? 1 : 0;
		VisiblePlaces += View.KnownProperties;
		HiddenPlaces += View.HiddenProperties;
	}

	if (Text_JournalEyebrow)
	{
		Text_JournalEyebrow->SetText(Cities.Num() > 1
			? NSLOCTEXT("TerritoryJournal", "RegionalNetworkEyebrow", "REGIONAL TERRITORY NETWORK")
			: NSLOCTEXT("TerritoryJournal", "CityNetworkEyebrow", "CITY TERRITORY NETWORK"));
	}
	if (Text_JournalTitle)
	{
		FText Title = NSLOCTEXT("TerritoryJournal", "GenericCommandCenter", "TERRITORY COMMAND CENTER");
		if (Cities.Num() == 1)
		{
			for (const TPair<FString, FText>& Pair : Cities)
			{
				Title = FText::Format(NSLOCTEXT("TerritoryJournal", "NamedCommandCenter",
					"{0} COMMAND CENTER"), Pair.Value);
				break;
			}
		}
		Text_JournalTitle->SetText(Title);
	}
	if (Text_JournalSubtitle)
	{
		Text_JournalSubtitle->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "DynamicNetworkSummary",
				"{0} cities  •  {1} unlocked districts  •  {2} controlled  •  {3} visible places  •  {4} hidden"),
			FText::AsNumber(Cities.Num()), FText::AsNumber(Views.Num()),
			FText::AsNumber(OwnedDistricts), FText::AsNumber(VisiblePlaces),
			FText::AsNumber(HiddenPlaces)));
	}
}

UTerritoryDistrictRowWidget* UTerritoryJournalWidget::CreateOperationsRow(
	const FTerritoryDistrictOperationsView& View)
{
	TSubclassOf<UTerritoryDistrictRowWidget> RowClass = TerritoryEntryWidgetClass;
	if (!RowClass)
	{
		RowClass = DistrictRowWidgetClass;
	}
	if (!RowClass)
	{
		RowClass = UTerritoryDistrictRowWidget::StaticClass();
	}
	UTerritoryDistrictRowWidget* Row = GetOwningPlayer()
		? CreateWidget<UTerritoryDistrictRowWidget>(GetOwningPlayer(), RowClass)
		: CreateWidget<UTerritoryDistrictRowWidget>(this, RowClass);
	if (!Row && RowClass != UTerritoryDistrictRowWidget::StaticClass())
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("Territory Journal could not create entry class %s; using the native District row."),
			*GetNameSafe(RowClass.Get()));
		Row = GetOwningPlayer()
			? CreateWidget<UTerritoryDistrictRowWidget>(
				GetOwningPlayer(), UTerritoryDistrictRowWidget::StaticClass())
			: CreateWidget<UTerritoryDistrictRowWidget>(
				this, UTerritoryDistrictRowWidget::StaticClass());
	}
	if (Row)
	{
		Row->InitializeOperationsView(View);
		const bool bIsSelected = SelectedDistrict.Get() == View.District;
		Row->SetSelected(bIsSelected);
		Row->SetExpanded(bIsSelected
			&& (bSelectedTerritoryInfoRequested || !View.bOwnedByViewer));
		Row->OnDistrictSelected.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleDistrictSelected);
		Row->OnGuardActionRequested.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleGuardActionRequested);
		Row->OnWaypointRequested.AddUniqueDynamic(
			this, &UTerritoryJournalWidget::HandleWaypointRequested);
		Row->OnEspionageRequested.AddUniqueDynamic(
			this, &UTerritoryJournalWidget::HandleEspionageRequested);
		TerritoryEntryWidgets.Add(Row);
	}
	return Row;
}

void UTerritoryJournalWidget::HandleWaypointRequested(ATerritoryDistrict* District)
{
	if (!District) return;
	if (UTerritoryUIBlueprintLibrary::SetTerritoryWaypoint(
		GetOwningPlayer(), District))
	{
		LastOperationsRevision = INDEX_NONE;
		RefreshDistrictList();
	}
}

void UTerritoryJournalWidget::HandleEspionageRequested(
	ATerritoryDistrict* District)
{
	if (!District) return;
	BindManagementComponent();
	if (ManagementComponent.IsValid())
	{
		ManagementComponent->RequestEspionage(District);
	}
}

void UTerritoryJournalWidget::HandleLiveEventWaypointRequested(
	FGameplayTag TerritoryTag)
{
	UWorld* World = GetWorld();
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	ATerritoryVolume* Territory = Registry
		? Registry->GetTerritoryByTag(TerritoryTag) : nullptr;
	if (Territory && UTerritoryUIBlueprintLibrary::SetTerritoryWaypoint(
		GetOwningPlayer(), Territory))
	{
		LastOperationsRevision = INDEX_NONE;
		RefreshDistrictList();
	}
}

void UTerritoryJournalWidget::HandleLiveEventsChanged()
{
	RefreshLiveEvents();
}

void UTerritoryJournalWidget::SetIntelligenceFilter(
	ETerritoryIntelligenceFilter Filter)
{
	SelectedIntelligenceFilter = Filter;
	RefreshLiveEvents();
}

void UTerritoryJournalWidget::HandleIntelligenceAllClicked()
{
	SetIntelligenceFilter(ETerritoryIntelligenceFilter::All);
}

void UTerritoryJournalWidget::HandleIntelligenceConflictClicked()
{
	SetIntelligenceFilter(ETerritoryIntelligenceFilter::Conflict);
}

void UTerritoryJournalWidget::HandleIntelligenceControlClicked()
{
	SetIntelligenceFilter(ETerritoryIntelligenceFilter::Control);
}

void UTerritoryJournalWidget::HandleIntelligenceEconomyClicked()
{
	SetIntelligenceFilter(ETerritoryIntelligenceFilter::Economy);
}

void UTerritoryJournalWidget::HandleIntelligenceCommandClicked()
{
	SetIntelligenceFilter(ETerritoryIntelligenceFilter::Command);
}

void UTerritoryJournalWidget::HandleIntelligenceProductionClicked()
{
	SetIntelligenceFilter(ETerritoryIntelligenceFilter::Production);
}

void UTerritoryJournalWidget::HandleIntelligenceDiplomacyClicked()
{
	SetIntelligenceFilter(ETerritoryIntelligenceFilter::Diplomacy);
}

UTextBlock* UTerritoryJournalWidget::CreateHierarchyTextRow(
	const FText& Text, FName WidgetName, bool bHeading)
{
	if (!WidgetTree)
	{
		return nullptr;
	}
	UNarrativeCommonTextBlock* Row = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), WidgetName);
	Row->SetText(Text);
	StyleGeneratedTerritoryText(Row, bHeading ? 15 : 13,
		bHeading ? FLinearColor(0.96f, 0.72f, 0.38f, 1.f)
			: FLinearColor(0.88f, 0.9f, 0.88f, 1.f),
		bHeading ? ETerritoryGeneratedTextRole::Heading
			: ETerritoryGeneratedTextRole::Body);
	return Row;
}

void UTerritoryJournalWidget::RefreshSelectedHierarchyPanels(
	const FTerritoryDistrictOperationsView& View)
{
	const UEnum* StateEnum = StaticEnum<ETerritoryState>();
	auto GetStateText = [StateEnum](ETerritoryState State)
	{
		return StateEnum
			? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(State))
			: FText::GetEmpty();
	};

	const FText CityName = View.CityDisplayName.IsEmpty()
		? NSLOCTEXT("TerritoryJournal", "IndependentCity", "Independent")
		: View.CityDisplayName;
	if (Text_CommandHierarchy)
	{
		Text_CommandHierarchy->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "SelectedHierarchy", "CITY  {0}   >   DISTRICT  {1}"),
			CityName, View.DisplayName));
	}
	if (Text_CommandOverview)
	{
		Text_CommandOverview->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "SelectedOverview",
				"Owner: {0}\nState: {1}\nPlaces controlled: {2} / {3}\nDiscovered: {4}  |  Hidden: {5}\nControl pressure: {6}%\n{7}"),
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.OwnerFaction),
			GetStateText(View.TerritoryState),
			FText::AsNumber(View.OwnedProperties),
			FText::AsNumber(View.TotalProperties),
			FText::AsNumber(View.KnownProperties),
			FText::AsNumber(View.HiddenProperties),
			FText::AsNumber(FMath::RoundToInt(View.CaptureProgress * 100.f)),
			View.AvailabilityReason));
	}
	if (Text_CommandDiplomacy)
	{
		Text_CommandDiplomacy->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "SelectedDiplomacy",
				"{0}\nWar: {1}  |  Allied: {2}  |  Trade: {3}"),
			View.DiplomacySummary,
			View.bViewerAtWarWithOwner
				? NSLOCTEXT("TerritoryJournal", "Yes", "Yes")
				: NSLOCTEXT("TerritoryJournal", "No", "No"),
			View.bViewerAlliedWithOwner
				? NSLOCTEXT("TerritoryJournal", "YesAllied", "Yes")
				: NSLOCTEXT("TerritoryJournal", "NoAllied", "No"),
			View.bViewerTradesWithOwner
				? NSLOCTEXT("TerritoryJournal", "YesTrade", "Yes")
				: NSLOCTEXT("TerritoryJournal", "NoTrade", "No")));
	}

	if (PlaceHierarchyList)
	{
		PlaceHierarchyList->ClearChildren();
		if (View.VisiblePlaces.IsEmpty())
		{
			if (UTextBlock* Empty = CreateHierarchyTextRow(
				NSLOCTEXT("TerritoryJournal", "NoVisiblePlaces",
					"No unlocked Places are currently visible in this District."),
				TEXT("NoVisiblePlaces")))
			{
				PlaceHierarchyList->AddChild(Empty);
			}
		}
		for (const FTerritoryHierarchyOperationsView& Place : View.VisiblePlaces)
		{
			const FText PlaceText = FText::Format(
				NSLOCTEXT("TerritoryJournal", "PlaceHierarchyRow",
					"PLACE  /  {0}\nOwner {1}  |  {2}\nGuards {3}/{4}/{5}  |  Net {6}"),
				Place.DisplayName,
				UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(Place.OwnerFaction),
				GetStateText(Place.TerritoryState),
				FText::AsNumber(Place.ActiveGuards), FText::AsNumber(Place.DesiredGuards),
				FText::AsNumber(Place.MaximumGuards), FText::AsNumber(Place.NetIncome));
			if (UTextBlock* Row = CreateHierarchyTextRow(PlaceText,
				FName(*FString::Printf(TEXT("Place_%u"), GetTypeHash(Place.TerritoryTag)))))
			{
				PlaceHierarchyList->AddChild(Row);
			}
		}
		if (View.HiddenProperties > 0)
		{
			if (UTextBlock* Hidden = CreateHierarchyTextRow(
				FText::Format(NSLOCTEXT("TerritoryJournal", "HiddenPlacesAggregate",
					"{0} LOCATIONS REMAIN HIDDEN\nNames and objectives will appear only after their story conditions unlock."),
					FText::AsNumber(View.HiddenProperties)),
				TEXT("HiddenPlacesAggregate")))
			{
				PlaceHierarchyList->AddChild(Hidden);
			}
		}
	}

	if (ProductionHierarchyList)
	{
		ProductionHierarchyList->ClearChildren();
		if (View.ProductionSites.IsEmpty())
		{
			if (UTextBlock* Empty = CreateHierarchyTextRow(
				NSLOCTEXT("TerritoryJournal", "NoVisibleProduction",
					"No visible Place has an active production profile."),
				TEXT("NoVisibleProduction")))
			{
				ProductionHierarchyList->AddChild(Empty);
			}
		}
		for (const FTerritoryProductionSiteOperationsView& Site : View.ProductionSites)
		{
			TArray<FString> ResourceLines;
			for (const FTerritoryResourceOperationsView& Resource : Site.Resources)
			{
				ResourceLines.Add(FString::Printf(TEXT("%s %+d/cycle"),
					*Resource.DisplayName.ToString(), Resource.NetPerCycle));
			}
			const FText SiteText = FText::Format(
				NSLOCTEXT("TerritoryJournal", "ProductionHierarchyRow",
					"{0}\n{1}  |  {2}\n{3}"),
				Site.DisplayName,
				UTerritoryUIBlueprintLibrary::GetProductionStatusText(Site.Status),
				Site.StatusReason,
				FText::FromString(ResourceLines.IsEmpty()
					? FString(TEXT("No resource flow."))
					: FString::Join(ResourceLines, TEXT("  |  "))));
			if (UTextBlock* Row = CreateHierarchyTextRow(SiteText,
				FName(*FString::Printf(TEXT("Production_%u"), GetTypeHash(Site.TerritoryTag)))))
			{
				ProductionHierarchyList->AddChild(Row);
			}
		}
	}
}

void UTerritoryJournalWidget::UpdateSelectedDistrict(ATerritoryDistrict* District)
{
	SelectedDistrict = District;
	if (!District)
	{
		bSelectedTerritoryInfoRequested = false;
		SetSelectedTerritoryInfoOpen(false);
		SelectedGarrisonTarget.Reset();
		GarrisonTargetOptions.Empty();
		GarrisonTargetOrder.Empty();
		if (GarrisonTargetSelector) GarrisonTargetSelector->ClearOptions();
		if (GuardTargetSpinBox) GuardTargetSpinBox->SetIsEnabled(false);
		if (PlaceHierarchyList) PlaceHierarchyList->ClearChildren();
		if (ProductionHierarchyList) ProductionHierarchyList->ClearChildren();
		if (Text_CommandHierarchy) Text_CommandHierarchy->SetText(FText::GetEmpty());
		if (Text_CommandOverview) Text_CommandOverview->SetText(FText::GetEmpty());
		if (Text_CommandDiplomacy) Text_CommandDiplomacy->SetText(FText::GetEmpty());
		UpdateGarrisonTargetPreview();
		if (Text_EmptySelection)
		{
			Text_EmptySelection->SetVisibility(ESlateVisibility::Visible);
		}
		if (Text_CommandDistrictName)
		{
			Text_CommandDistrictName->SetText(NSLOCTEXT(
				"TerritoryJournal", "NoCommandSelection", "SELECT A DISTRICT"));
		}
		if (Btn_CommandAddGuard) Btn_CommandAddGuard->SetIsEnabled(false);
		if (Btn_CommandRemoveGuard) Btn_CommandRemoveGuard->SetIsEnabled(false);
		if (Btn_CommandAddFiveGuards) Btn_CommandAddFiveGuards->SetIsEnabled(false);
		if (Btn_CommandRemoveFiveGuards) Btn_CommandRemoveFiveGuards->SetIsEnabled(false);
		return;
	}
	FTerritoryDistrictOperationsView View;
	if (!UTerritoryUIBlueprintLibrary::BuildDistrictOperationsView(
		this, District, GetOwningPlayer(), View))
	{
		return;
	}
	RefreshGarrisonManagementControls(View);
	RefreshSelectedHierarchyPanels(View);

	if (Text_EmptySelection)
	{
		Text_EmptySelection->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Text_SelectedEyebrow)
	{
		Text_SelectedEyebrow->SetText(NSLOCTEXT("TerritoryJournal", "SelectedEyebrow", "LIVE TERRITORY INTELLIGENCE"));
	}
	UTextBlock* SelectedTitle = Text_SelectedTerritoryTitle
		? Text_SelectedTerritoryTitle.Get() : QuestTitle.Get();
	if (SelectedTitle)
	{
		SelectedTitle->SetText(View.DisplayName);
	}

	const UEnum* StateEnum = StaticEnum<ETerritoryState>();
	const FText StateText = StateEnum
		? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(View.TerritoryState))
		: FText::GetEmpty();
	const FLinearColor SelectedAccent =
		(View.bUnderAttack || View.bAttackScheduled || View.bThreatPreviewAvailable)
			? FLinearColor(1.f, 0.25f, 0.16f, 1.f)
			: View.bOwnedByViewer
				? FLinearColor(0.08f, 0.88f, 0.62f, 1.f)
				: View.bAvailableForCapture
					? FLinearColor(1.f, 0.84f, 0.f, 1.f)
					: FLinearColor(0.88f, 0.74f, 0.08f, 1.f);
	const FText ReserveText = View.bReserveCountKnown
		? FText::AsNumber(View.ReserveGuards)
		: NSLOCTEXT("TerritoryJournal", "ReserveUnknown", "server snapshot required");
	const FText CascadeText = FText::Format(
		NSLOCTEXT("TerritoryJournal", "DistrictCascade",
			"{0}/{1} Places controlled  |  {2} discovered  |  {3} hidden  |  {4} manageable garrisons  |  {5} empty posts"),
		FText::AsNumber(View.OwnedProperties), FText::AsNumber(View.TotalProperties),
		FText::AsNumber(View.KnownProperties), FText::AsNumber(View.HiddenProperties),
		FText::AsNumber(View.ManageableGarrisonTargets),
		FText::AsNumber(View.UnguardedGarrisonTargets));
	const FText DetailText = FText::Format(
		NSLOCTEXT("TerritoryJournal", "DistrictDetails",
			"Owner: {0}\nState: {1}\nAvailability: {2}\n"
			"Hierarchy: {17}\n"
			"Garrison: {3} active / {4} assigned / {5} maximum\nReserve: {6}\n"
			"Guard purchase: {7}\nIncome: {8}\nUpkeep: {9}\nNet: {10}\nAvailable funds: {11}\n"
			"Security: quality {12}, fortification {13}, allied support {14}, strategic value {15}\nThreat: {16}"),
		UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.OwnerFaction),
		StateText,
		View.AvailabilityReason,
		FText::AsNumber(View.ActiveGuards),
		FText::AsNumber(View.DesiredGuards),
		FText::AsNumber(View.MaximumGuards),
		ReserveText,
		FText::AsNumber(View.GuardPurchaseCost),
		FText::AsNumber(View.PeriodicIncome),
		FText::AsNumber(View.GuardUpkeep),
		FText::AsNumber(View.NetIncome),
		FText::AsNumber(View.AvailableFunds),
		FText::AsNumber(View.GuardQuality),
		FText::AsNumber(View.Fortification),
		FText::AsNumber(View.AlliedSupport),
		FText::AsNumber(View.StrategicValue),
		View.ThreatSummary,
		CascadeText);
	URichTextBlock* TerritoryDescription = RichText_TerritoryDescription
		? RichText_TerritoryDescription.Get() : RichText_QuestDescription.Get();
	if (TerritoryDescription)
	{
		TerritoryDescription->SetText(DetailText);
	}
	if (Text_CaptureLabel)
	{
		Text_CaptureLabel->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "ControlLabel", "Control {0}%"),
			FText::AsNumber(FMath::RoundToInt(View.CaptureProgress * 100.f))));
	}
	if (CaptureProgressBar)
	{
		CaptureProgressBar->SetPercent(View.CaptureProgress);
	}
	if (RichText_CurrentStateDescription)
	{
		RichText_CurrentStateDescription->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CurrentState", "{0}  |  {1}  |  Attackers {2}  |  Planned force {3}"),
			StateText,
			UTerritoryUIBlueprintLibrary::GetThreatLevelText(View.ThreatLevel),
			View.bAttackerCountKnown
				? FText::AsNumber(View.ActiveAttackers)
				: NSLOCTEXT("TerritoryJournal", "AttackerCountUnknown", "unknown"),
			FText::AsNumber(View.PlannedAttackers)));
	}
	if (Text_OperationalSummary) Text_OperationalSummary->SetText(View.AvailabilityReason);
	if (Text_SecuritySummary)
	{
		Text_SecuritySummary->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "SecuritySummary", "Active {0} / Assigned {1} / Maximum {2} / Reserve {3}"),
			FText::AsNumber(View.ActiveGuards), FText::AsNumber(View.DesiredGuards),
			FText::AsNumber(View.MaximumGuards), ReserveText));
	}
	if (Text_FinanceSummary)
	{
		Text_FinanceSummary->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "FinanceSummary", "Income {0} - Upkeep {1} = Net {2}"),
			FText::AsNumber(View.PeriodicIncome), FText::AsNumber(View.GuardUpkeep),
			FText::AsNumber(View.NetIncome)));
	}
	if (Text_AssaultSummary) Text_AssaultSummary->SetText(View.ThreatSummary);
	SetSelectedTerritoryInfoOpen(bSelectedTerritoryInfoRequested);

	if (Text_CommandDistrictName)
	{
		Text_CommandDistrictName->SetText(View.DisplayName);
		Text_CommandDistrictName->SetColorAndOpacity(FSlateColor(
			FLinearColor(0.95f, 0.98f, 0.97f, 1.f)));
	}
	if (Text_CommandOwnerState)
	{
		Text_CommandOwnerState->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CommandOwnerState", "OWNER  {0}    |    STATE  {1}    |    VIEWER  {2}"),
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.OwnerFaction),
			StateText,
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.ViewerFaction)));
		Text_CommandOwnerState->SetColorAndOpacity(FSlateColor(SelectedAccent));
	}
	if (Text_CommandAvailability)
	{
		Text_CommandAvailability->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CommandAvailability", "{0}\nManagement: {1}"),
			View.AvailabilityReason,
			View.bManageable
				? NSLOCTEXT("TerritoryJournal", "ManagementReady", "Command authority confirmed")
				: View.ManagementFailureReason));
	}
	if (Text_CommandSecurity)
	{
		Text_CommandSecurity->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CommandSecurity",
				"Active {0}  |  Assigned {1}  |  Capacity {2}  |  Reserve {3}\n"
				"Quality {4}  |  Fortification {5}  |  Allied support {6}  |  Strategic value {7}\n"
				"Properties {8}/{9}  |  Manageable garrisons {10}  |  Empty posts {11}  |  District {12}"),
			FText::AsNumber(View.ActiveGuards), FText::AsNumber(View.DesiredGuards),
			FText::AsNumber(View.MaximumGuards), ReserveText,
			FText::AsNumber(View.GuardQuality), FText::AsNumber(View.Fortification),
			FText::AsNumber(View.AlliedSupport), FText::AsNumber(View.StrategicValue),
			FText::AsNumber(View.OwnedProperties), FText::AsNumber(View.TotalProperties),
			FText::AsNumber(View.ManageableGarrisonTargets),
			FText::AsNumber(View.UnguardedGarrisonTargets),
			View.bUnguarded
				? NSLOCTEXT("TerritoryJournal", "DistrictUnguarded", "UNGUARDED")
				: NSLOCTEXT("TerritoryJournal", "DistrictGuarded", "DEFENDED")));
	}
	if (Text_CommandFinance)
	{
		Text_CommandFinance->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CommandFinance",
				"Funds {0}  |  Guard price {1}\nIncome {2}  -  Upkeep {3}  =  Net {4}\nStatus: {5}"),
			FText::AsNumber(View.AvailableFunds), FText::AsNumber(View.GuardPurchaseCost),
			FText::AsNumber(View.PeriodicIncome), FText::AsNumber(View.GuardUpkeep),
			FText::AsNumber(View.NetIncome),
			View.bFinancialRisk
				? NSLOCTEXT("TerritoryJournal", "FinanceAtRisk", "AT RISK")
				: NSLOCTEXT("TerritoryJournal", "FinanceStable", "STABLE")));
	}
	if (Text_CommandThreat)
	{
		Text_CommandThreat->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CommandThreat",
				"{0}\n{1}\nContesting faction: {2}  |  Capture attackers: {3}\nEvaluation: {4}"),
			UTerritoryUIBlueprintLibrary::GetThreatLevelText(View.ThreatLevel),
			View.ThreatSummary,
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.ContestingFaction),
			View.bAttackerCountKnown
				? FText::AsNumber(View.ActiveAttackers)
				: NSLOCTEXT("TerritoryJournal", "CommandAttackersUnknown", "unknown"),
			View.ThreatEvaluationReason));
	}
	if (Text_CommandAssault)
	{
		FText AssaultText;
		if (View.AssaultID.IsValid())
		{
			AssaultText = FText::Format(
				NSLOCTEXT("TerritoryJournal", "CommandAssault",
					"{0}\nTarget {1}  |  Attacker {2}\nPlanned {3}  |  Alive {4}  |  Reserve {5}  |  Killed {6}  |  Withdrawn {7}\n"
					"Launch {8}  |  Estimated success {9}  |  Priority {10}"),
				UTerritoryUIBlueprintLibrary::GetAssaultStateText(View.AssaultState),
				UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.ThreatTargetTerritory),
				UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.AttackingFaction),
				FText::AsNumber(View.PlannedAttackers), FText::AsNumber(View.AliveAttackers),
				FText::AsNumber(View.PendingReserveAttackers), FText::AsNumber(View.KilledAttackers),
				FText::AsNumber(View.WithdrawnAttackers),
				FText::AsPercent(FMath::Clamp(View.LaunchProbability, 0.f, 1.f)),
				FText::AsPercent(FMath::Clamp(View.EstimatedSuccessProbability, 0.f, 1.f)),
				FText::AsNumber(View.AttackPriority));
		}
		else if (View.bThreatPreviewAvailable)
		{
			AssaultText = FText::Format(
				NSLOCTEXT("TerritoryJournal", "ProjectedCommandAssault",
					"PROJECTED — NOT YET SCHEDULED\nTarget {0}  |  Strongest eligible faction {1}\n"
					"Launch {2}  |  Estimated success {3}  |  Defence {4}  |  Power ratio {5}  |  Priority {6}"),
				UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.ThreatTargetTerritory),
				UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.AttackingFaction),
				FText::AsPercent(FMath::Clamp(View.LaunchProbability, 0.f, 1.f)),
				FText::AsPercent(FMath::Clamp(View.EstimatedSuccessProbability, 0.f, 1.f)),
				FText::AsNumber(View.DistrictDefencePower), FText::AsNumber(View.PowerRatio),
				FText::AsNumber(View.AttackPriority));
		}
		else
		{
			AssaultText = NSLOCTEXT("TerritoryJournal", "NoCommandAssault",
				"No scheduled operation and no eligible configured attacker.");
		}
		Text_CommandAssault->SetText(AssaultText);
	}
	if (Text_CommandApproaches)
	{
		TArray<FString> ApproachNames;
		for (const FName Approach : View.SelectedApproaches)
		{
			ApproachNames.Add(Approach.ToString());
		}
		Text_CommandApproaches->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CommandApproaches", "{0}"),
			FText::FromString(ApproachNames.IsEmpty()
				? FString(TEXT("No assault routes selected."))
				: FString::Join(ApproachNames, TEXT("  |  ")))));
	}
	if (Text_CommandCaptureProgress)
	{
		Text_CommandCaptureProgress->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CommandCaptureProgress",
				"DISTRICT CONTROL  {0} / {1} PLACES  •  {2} DISCOVERED  •  {3} HIDDEN"),
			FText::AsNumber(View.OwnedProperties), FText::AsNumber(View.TotalProperties),
			FText::AsNumber(View.KnownProperties), FText::AsNumber(View.HiddenProperties)));
		Text_CommandCaptureProgress->SetColorAndOpacity(FSlateColor(SelectedAccent));
	}
	if (CommandCaptureProgressBar)
	{
		CommandCaptureProgressBar->SetPercent(View.TotalProperties > 0
			? static_cast<float>(View.OwnedProperties) / static_cast<float>(View.TotalProperties)
			: 0.f);
		CommandCaptureProgressBar->SetFillColorAndOpacity(SelectedAccent);
	}

	FTerritoryGarrisonOperationsView SelectedGarrison;
	const bool bHasSelectedGarrison = UTerritoryUIBlueprintLibrary::BuildGarrisonOperationsView(
		this, SelectedGarrisonTarget.Get(), GetOwningPlayer(), SelectedGarrison);
	FText AddFiveFailure;
	FText RemoveFiveFailure;
	AActor* ViewerActor = GetOwningPlayerPawn()
		? static_cast<AActor*>(GetOwningPlayerPawn())
		: static_cast<AActor*>(GetOwningPlayer());
	int32 PreviewCost = 0;
	const bool bCanAddFive = bHasSelectedGarrison && SelectedGarrison.bManageable
		&& SelectedGarrison.DesiredGuards + 5 <= SelectedGarrison.MaximumGuards
		&& SelectedGarrison.Territory->CanSetDesiredGuardCount(ViewerActor,
			SelectedGarrison.DesiredGuards + 5, AddFiveFailure, PreviewCost);
	const bool bCanRemoveFive = bHasSelectedGarrison && SelectedGarrison.bManageable
		&& SelectedGarrison.DesiredGuards >= 5
		&& SelectedGarrison.Territory->CanSetDesiredGuardCount(ViewerActor,
			SelectedGarrison.DesiredGuards - 5, RemoveFiveFailure, PreviewCost);
	if (!bHasSelectedGarrison || !SelectedGarrison.bManageable)
	{
		AddFiveFailure = View.ManagementFailureReason;
		RemoveFiveFailure = View.ManagementFailureReason;
	}
	if (Btn_CommandAddGuard)
	{
		Btn_CommandAddGuard->SetIsEnabled(bHasSelectedGarrison && SelectedGarrison.bCanIncreaseTarget);
		Btn_CommandAddGuard->SetToolTipText(bHasSelectedGarrison && SelectedGarrison.bCanIncreaseTarget
			? NSLOCTEXT("TerritoryJournal", "AddGuardReady", "Raise the selected garrison target by one.")
			: SelectedGarrison.IncreaseFailureReason);
	}
	if (Btn_CommandRemoveGuard)
	{
		Btn_CommandRemoveGuard->SetIsEnabled(bHasSelectedGarrison && SelectedGarrison.bCanDecreaseTarget);
		Btn_CommandRemoveGuard->SetToolTipText(bHasSelectedGarrison && SelectedGarrison.bCanDecreaseTarget
			? NSLOCTEXT("TerritoryJournal", "RemoveGuardReady", "Lower the selected garrison target by one, even if a guard is missing.")
			: SelectedGarrison.DecreaseFailureReason);
	}
	if (Btn_CommandAddFiveGuards)
	{
		Btn_CommandAddFiveGuards->SetIsEnabled(bCanAddFive);
		Btn_CommandAddFiveGuards->SetToolTipText(bCanAddFive
			? NSLOCTEXT("TerritoryJournal", "AddFiveReady", "Purchase and deploy five guards atomically.")
			: AddFiveFailure);
	}
	if (Btn_CommandRemoveFiveGuards)
	{
		Btn_CommandRemoveFiveGuards->SetIsEnabled(bCanRemoveFive);
		Btn_CommandRemoveFiveGuards->SetToolTipText(bCanRemoveFive
			? NSLOCTEXT("TerritoryJournal", "RemoveFiveReady", "Withdraw five active guards atomically.")
			: RemoveFiveFailure);
	}
}

void UTerritoryJournalWidget::RefreshOperationalSummaries(
	const TArray<FTerritoryDistrictOperationsView>& Views)
{
	UPanelWidget* ActivePanel = GetActiveTerritoriesPanel();
	UPanelWidget* CapturedPanel = GetCapturedTerritoriesPanel();
	if (ActivePanel) ActivePanel->ClearChildren();
	if (CapturedPanel) CapturedPanel->ClearChildren();
	if (EarningsList) EarningsList->ClearChildren();
	if (LossReportList) LossReportList->ClearChildren();

	int32 OwnedCount = 0;
	int32 AvailableCount = 0;
	int32 ThreatCount = 0;
	int32 AvailableUnlockedCount = 0;
	int32 AvailableQueueCount = 0;
	int32 ActiveRowsAdded = 0;
	int32 CapturedRowsAdded = 0;
	int32 RiskCount = 0;
	int32 GuardShortfall = 0;
	FGameplayTag ViewerFaction;
	for (const FTerritoryDistrictOperationsView& View : Views)
	{
		if (!ViewerFaction.IsValid()) ViewerFaction = View.ViewerFaction;
		AvailableCount += View.bAvailable ? 1 : 0;
		ThreatCount += (View.bUnderAttack || View.bAttackScheduled
			|| View.bThreatPreviewAvailable) ? 1 : 0;
		if (UTerritoryUIBlueprintLibrary::IsDistrictAvailableUnlocked(View))
		{
			++AvailableUnlockedCount;
		}
		// Operations lists every unlocked non-owned District. A failed quest,
		// diplomacy, or capture condition remains visible as status information;
		// locked District identities never enter this player-facing list.
		if (UTerritoryUIBlueprintLibrary::IsDistrictAvailableUnlocked(View))
		{
			++AvailableQueueCount;
			if (ActivePanel)
			{
				if (UTerritoryDistrictRowWidget* Row = CreateOperationsRow(View))
				{
					ActivePanel->AddChild(Row);
					++ActiveRowsAdded;
				}
			}
		}
		if (UTerritoryUIBlueprintLibrary::IsDistrictCapturedOwned(View))
		{
			++OwnedCount;
			GuardShortfall += FMath::Max(0, View.DesiredGuards - View.ActiveGuards);
			if (CapturedPanel)
			{
				if (UTerritoryDistrictRowWidget* Row = CreateOperationsRow(View))
				{
					CapturedPanel->AddChild(Row);
					++CapturedRowsAdded;
				}
			}
			if (EarningsList)
			{
				if (UTerritoryDistrictRowWidget* Row = CreateOperationsRow(View))
				{
					EarningsList->AddChild(Row);
				}
			}
		}
		if (View.bUnderAttack || View.bAttackScheduled || View.bThreatPreviewAvailable
			|| View.bFinancialRisk)
		{
			++RiskCount;
			if (LossReportList)
			{
				if (UTerritoryDistrictRowWidget* Row = CreateOperationsRow(View))
				{
					LossReportList->AddChild(Row);
				}
			}
		}
	}

	auto AddEmptyMessage = [this](UPanelWidget* Box, const FText& Message, FName WidgetName)
	{
		if (!Box || !WidgetTree)
		{
			return;
		}
		UTextBlock* EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
		EmptyText->SetText(Message);
		StyleGeneratedTerritoryText(EmptyText, 14, FLinearColor(0.65f, 0.69f, 0.68f, 1.f));
		Box->AddChild(EmptyText);
	};
	if (ActiveRowsAdded == 0)
	{
		AddEmptyMessage(ActivePanel,
			AvailableQueueCount == 0
				? NSLOCTEXT("TerritoryJournal", "NoDistrictIntel", "No unlocked District operations are currently visible.")
				: NSLOCTEXT("TerritoryJournal", "DistrictEntriesUnavailable", "District data is available, but its entry widget could not be created."),
			TEXT("NoAvailableUnlockedDistricts"));
	}
	if (CapturedRowsAdded == 0)
	{
		AddEmptyMessage(CapturedPanel,
			OwnedCount == 0
				? NSLOCTEXT("TerritoryJournal", "NoCapturedOwned", "Your faction controls no loaded districts.")
				: NSLOCTEXT("TerritoryJournal", "CapturedEntriesUnavailable", "Captured District data is available, but its entry widget could not be created."),
			TEXT("NoCapturedOwnedDistricts"));
	}
	if (OwnedCount == 0)
	{
		AddEmptyMessage(EarningsList,
			NSLOCTEXT("TerritoryJournal", "NoOwnedEarnings", "Capture a district to establish recurring income."),
			TEXT("NoOwnedEarningsDistricts"));
	}
	if (RiskCount == 0)
	{
		AddEmptyMessage(LossReportList,
			NSLOCTEXT("TerritoryJournal", "NoOperationalRisks", "No district threats or operating risks detected."),
			TEXT("NoOperationalRiskDistricts"));
	}

	const FTerritoryEconomyOperationsView Economy =
		UTerritoryUIBlueprintLibrary::BuildEconomyOperationsView(
			this, GetOwningPlayer(), ViewerFaction, 10);
	TArray<FString> TransactionLines;
	for (const FTerritoryTransaction& Transaction : Economy.RecentTransactions)
	{
		TransactionLines.Add(FString::Printf(TEXT("%+d — %s%s"), Transaction.Amount,
			Transaction.Reason.IsEmpty() ? TEXT("Unspecified transaction") : *Transaction.Reason,
			Transaction.SourceTerritory.IsValid()
				? *FString::Printf(TEXT(" [%s]"), *Transaction.SourceTerritory.ToString()) : TEXT("")));
	}
	const FText TransactionAudit = FText::FromString(TransactionLines.IsEmpty()
		? FString(TEXT("No recent transactions."))
		: FString::Join(TransactionLines, TEXT("\n")));
	UTextBlock* ActiveCountText = Text_ActiveTerritoryCount
		? Text_ActiveTerritoryCount.Get() : Text_ActiveQuestCount.Get();
	if (ActiveCountText)
	{
		ActiveCountText->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "ActiveDistrictCount",
				"ACTIVE TERRITORIES  //  {0}"),
			FText::AsNumber(AvailableQueueCount)));
	}
	UTextBlock* CapturedCountText = Text_CapturedTerritoryCount
		? Text_CapturedTerritoryCount.Get() : Text_FinishedQuestCount.Get();
	if (CapturedCountText)
	{
		CapturedCountText->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CapturedDistrictCount", "CAPTURED TERRITORIES  //  {0}"),
			FText::AsNumber(OwnedCount)));
	}
	if (Text_HeaderStatus)
	{
		Text_HeaderStatus->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "HeaderStatus", "LIVE MAP  //  {0} OPEN  •  {1} HELD  •  {2} THREATS  •  {3} FUNDS"),
			FText::AsNumber(AvailableUnlockedCount), FText::AsNumber(OwnedCount), FText::AsNumber(ThreatCount),
			FText::AsNumber(Economy.AvailableFunds)));
	}
	if (Text_AvailableUnlockedCount)
	{
		Text_AvailableUnlockedCount->SetText(FText::AsNumber(AvailableUnlockedCount));
	}
	if (Text_OwnedDistrictCount)
	{
		Text_OwnedDistrictCount->SetText(FText::AsNumber(OwnedCount));
	}
	if (Text_ThreatenedDistrictCount)
	{
		Text_ThreatenedDistrictCount->SetText(FText::AsNumber(ThreatCount));
	}
	if (Text_RiskDistrictCount)
	{
		Text_RiskDistrictCount->SetText(FText::AsNumber(RiskCount));
	}
	if (Text_TotalWeeklyEarnings)
	{
		Text_TotalWeeklyEarnings->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "GrossIncomeKPI", "GROSS  {0} / CYCLE"),
			FText::AsNumber(Economy.IncomePerTick)));
	}
	if (Text_TotalGuardCost)
	{
		Text_TotalGuardCost->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "GuardUpkeepKPI", "GUARDS  {0} / CYCLE"),
			FText::AsNumber(Economy.CostsPerTick)));
	}
	if (Text_NetProfit)
	{
		Text_NetProfit->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "NetIncomeKPI", "NET  {0} / CYCLE"),
			FText::AsNumber(Economy.NetPerTick)));
	}
	if (Text_TotalEarningsLost)
	{
		Text_TotalEarningsLost->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "OperationalRisk", "Recent debits {0}  |  {1} districts at risk"),
			FText::AsNumber(Economy.RecentDebits), FText::AsNumber(RiskCount)));
	}
	if (RichText_EarningsSummary)
	{
		RichText_EarningsSummary->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "EarningsSummary",
				"Available funds: {0}\nFaction income: {1}\nOperating costs: {2}\nNet per cycle: {3}\nRecent credits: {4}\nOwned districts: {5}\n\nTransaction audit\n{6}"),
			FText::AsNumber(Economy.AvailableFunds), FText::AsNumber(Economy.IncomePerTick),
			FText::AsNumber(Economy.CostsPerTick), FText::AsNumber(Economy.NetPerTick),
			FText::AsNumber(Economy.RecentCredits), FText::AsNumber(OwnedCount), TransactionAudit));
	}
	if (RichText_LossSummary)
	{
		RichText_LossSummary->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "LossSummary",
				"Recent debits: {0}\nAuthoritative faction costs: {1}\nAuthoritative faction net: {2}\nGuard shortfall: {3}\nThreatened districts: {4}\nCurrently available districts: {5}\n\nTransaction audit\n{6}"),
			FText::AsNumber(Economy.RecentDebits), FText::AsNumber(Economy.CostsPerTick),
			FText::AsNumber(Economy.NetPerTick), FText::AsNumber(GuardShortfall),
			FText::AsNumber(ThreatCount), FText::AsNumber(AvailableCount), TransactionAudit));
	}
}

void UTerritoryJournalWidget::SetSelectedDetailTab(int32 TabIndex)
{
	SelectedDetailTab = FMath::Clamp(TabIndex, 0, 6);
	if (CommandDetailSwitcher)
	{
		CommandDetailSwitcher->SetActiveWidgetIndex(SelectedDetailTab);
	}
	const TArray<UNarrativeCommonButtonBase*> Buttons = {
		Btn_OverviewDetailTab,
		Btn_PlacesDetailTab,
		Btn_GarrisonDetailTab,
		Btn_EconomyDetailTab,
		Btn_ProductionDetailTab,
		Btn_ThreatsDetailTab,
		Btn_DiplomacyDetailTab
	};
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		if (Buttons[Index])
		{
			Buttons[Index]->SetIsSelected(Index == SelectedDetailTab);
		}
	}
}

void UTerritoryJournalWidget::SetSelectedTerritoryInfoOpen(bool bOpen)
{
	UWidget* InfoWidget = GetSelectedTerritoryInfoWidget();
	const bool bWasOpen = InfoWidget
		&& InfoWidget->GetVisibility() != ESlateVisibility::Collapsed;
	if (InfoWidget)
	{
		InfoWidget->SetVisibility(bOpen
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (bOpen && !bWasOpen && Btn_OverviewDetailTab)
	{
		// The drawer is a separate focus island that does not exist while collapsed.
		// Move controller/keyboard focus only on the opening edge so live refreshes
		// cannot steal focus from the player's selected detail tab.
		Btn_OverviewDetailTab->SetFocus();
	}
}

void UTerritoryJournalWidget::HandleOverviewDetailTabClicked()
{
	SetSelectedDetailTab(0);
}

void UTerritoryJournalWidget::HandlePlacesDetailTabClicked()
{
	SetSelectedDetailTab(1);
}

void UTerritoryJournalWidget::HandleGarrisonDetailTabClicked()
{
	SetSelectedDetailTab(2);
}

void UTerritoryJournalWidget::HandleEconomyDetailTabClicked()
{
	SetSelectedDetailTab(3);
}

void UTerritoryJournalWidget::HandleProductionDetailTabClicked()
{
	SetSelectedDetailTab(4);
}

void UTerritoryJournalWidget::HandleThreatsDetailTabClicked()
{
	SetSelectedDetailTab(5);
}

void UTerritoryJournalWidget::HandleDiplomacyDetailTabClicked()
{
	SetSelectedDetailTab(6);
}

void UTerritoryJournalWidget::HandleTerritoryTabClicked()
{
	bSelectedTerritoryInfoRequested = SelectedDistrict.IsValid();
	SetSelectedTerritoryInfoOpen(bSelectedTerritoryInfoRequested);
	if (TabSwitcher)
	{
		TabSwitcher->SetActiveWidgetIndex(0);
	}
	if (Btn_TerritoryTab) Btn_TerritoryTab->SetIsSelected(true);
	if (Btn_EarningsTab) Btn_EarningsTab->SetIsSelected(false);
	if (Btn_LossTab) Btn_LossTab->SetIsSelected(false);
}

void UTerritoryJournalWidget::HandleEarningsTabClicked()
{
	bSelectedTerritoryInfoRequested = false;
	SetSelectedTerritoryInfoOpen(false);
	if (TabSwitcher)
	{
		TabSwitcher->SetActiveWidgetIndex(1);
	}
	if (Btn_TerritoryTab) Btn_TerritoryTab->SetIsSelected(false);
	if (Btn_EarningsTab) Btn_EarningsTab->SetIsSelected(true);
	if (Btn_LossTab) Btn_LossTab->SetIsSelected(false);
}

void UTerritoryJournalWidget::HandleLossTabClicked()
{
	bSelectedTerritoryInfoRequested = false;
	SetSelectedTerritoryInfoOpen(false);
	if (TabSwitcher)
	{
		TabSwitcher->SetActiveWidgetIndex(2);
	}
	if (Btn_TerritoryTab) Btn_TerritoryTab->SetIsSelected(false);
	if (Btn_EarningsTab) Btn_EarningsTab->SetIsSelected(false);
	if (Btn_LossTab) Btn_LossTab->SetIsSelected(true);
}

void UTerritoryJournalWidget::HandleCloseSelectedTerritoryClicked()
{
	bSelectedTerritoryInfoRequested = false;
	SetSelectedTerritoryInfoOpen(false);
	UNarrativeCommonButtonBase* ActivePrimaryTab = Btn_TerritoryTab;
	if (TabSwitcher)
	{
		ActivePrimaryTab = TabSwitcher->GetActiveWidgetIndex() == 1
			? Btn_EarningsTab
			: TabSwitcher->GetActiveWidgetIndex() == 2
				? Btn_LossTab
				: Btn_TerritoryTab;
	}
	if (ActivePrimaryTab)
	{
		ActivePrimaryTab->SetFocus();
	}
}

void UTerritoryJournalWidget::HandleSearchChanged(const FText& Text)
{
	SearchFilter = Text.ToString();
	LastOperationsRevision = INDEX_NONE;
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleOwnerFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	(void)SelectionType;
	SelectedOwnerFilter = MoveTemp(SelectedItem);
	LastOperationsRevision = INDEX_NONE;
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleStateFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	(void)SelectionType;
	SelectedStateFilter = MoveTemp(SelectedItem);
	LastOperationsRevision = INDEX_NONE;
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleOperationalFilterChanged(
	FString SelectedItem, ESelectInfo::Type SelectionType)
{
	(void)SelectionType;
	SetOperationsFilter(GetOperationsFilter(SelectedItem));
}

void UTerritoryJournalWidget::HandleGarrisonTargetChanged(
	FString SelectedItem, ESelectInfo::Type SelectionType)
{
	(void)SelectionType;
	if (const TWeakObjectPtr<ATerritoryVolume>* Target = GarrisonTargetOptions.Find(SelectedItem))
	{
		SelectedGarrisonTarget = *Target;
		if (GuardTargetSpinBox && SelectedGarrisonTarget.IsValid())
		{
			const float Maximum = SelectedGarrisonTarget->GetMaxGuardCount();
			GuardTargetSpinBox->SetMaxValue(Maximum);
			GuardTargetSpinBox->SetMaxSliderValue(Maximum);
			GuardTargetSpinBox->SetValue(SelectedGarrisonTarget->GetDesiredGuardCount());
			GuardTargetSpinBox->SetIsEnabled(Maximum > 0.f);
		}
		UpdateGarrisonTargetPreview();
	}
}

void UTerritoryJournalWidget::HandlePreviousGarrisonTargetClicked()
{
	SelectRelativeGarrisonTarget(-1);
}

void UTerritoryJournalWidget::HandleNextGarrisonTargetClicked()
{
	SelectRelativeGarrisonTarget(1);
}

void UTerritoryJournalWidget::HandleGuardTargetSpinChanged(float NewValue)
{
	(void)NewValue;
	UpdateGarrisonTargetPreview();
}

void UTerritoryJournalWidget::HandleApplyGuardTargetClicked()
{
	if (GuardTargetSpinBox)
	{
		SubmitSelectedGuardTarget(FMath::RoundToInt(GuardTargetSpinBox->GetValue()));
	}
}

void UTerritoryJournalWidget::HandleZeroGuardTargetClicked()
{
	SubmitSelectedGuardTarget(0);
}

void UTerritoryJournalWidget::HandleMaxGuardTargetClicked()
{
	if (SelectedGarrisonTarget.IsValid())
	{
		SubmitSelectedGuardTarget(SelectedGarrisonTarget->GetMaxGuardCount());
	}
}

void UTerritoryJournalWidget::HandleSendReinforcementClicked()
{
	if (!SelectedGarrisonTarget.IsValid())
	{
		if (Text_CommandStatus)
		{
			Text_CommandStatus->SetText(NSLOCTEXT("TerritoryJournal", "NoReinforcementTarget",
				"Select a loaded garrison before sending reinforcements."));
		}
		return;
	}
	BindManagementComponent();
	if (ManagementComponent.IsValid())
	{
		ManagementComponent->RequestSendReinforcements(SelectedGarrisonTarget.Get(), 1);
	}
	else if (Text_CommandStatus)
	{
		Text_CommandStatus->SetText(NSLOCTEXT("TerritoryJournal", "NoReinforcementAuthority",
			"Player command authority is unavailable."));
	}
}

void UTerritoryJournalWidget::HandleTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	(void)Territory;
	(void)bWasUnregistered;
	LastOperationsRevision = INDEX_NONE;
	RefreshFilterOptions();
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleTerritoryUnregistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	(void)Territory;
	(void)bWasUnregistered;
	LastOperationsRevision = INDEX_NONE;
	RefreshFilterOptions();
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleDistrictSelected(ATerritoryDistrict* District)
{
	SelectDistrict(District);
}

void UTerritoryJournalWidget::HandleGuardActionRequested(ATerritoryDistrict* District, int32 Delta)
{
	if (!District || Delta == 0)
	{
		return;
	}
	if (SelectedDistrict.Get() != District)
	{
		UpdateSelectedDistrict(District);
	}
	ATerritoryVolume* Target = SelectedGarrisonTarget.Get();
	if (!Target)
	{
		if (Text_CommandStatus) Text_CommandStatus->SetText(NSLOCTEXT(
			"TerritoryJournal", "NoGarrisonForDistrict", "This district has no loaded garrison target."));
		return;
	}
	SubmitSelectedGuardTarget(Target->GetDesiredGuardCount() + Delta);
}

void UTerritoryJournalWidget::HandleCommandAddGuardClicked()
{
	HandleGuardActionRequested(SelectedDistrict.Get(), 1);
}

void UTerritoryJournalWidget::HandleCommandRemoveGuardClicked()
{
	HandleGuardActionRequested(SelectedDistrict.Get(), -1);
}

void UTerritoryJournalWidget::HandleCommandAddFiveGuardsClicked()
{
	HandleGuardActionRequested(SelectedDistrict.Get(), 5);
}

void UTerritoryJournalWidget::HandleCommandRemoveFiveGuardsClicked()
{
	HandleGuardActionRequested(SelectedDistrict.Get(), -5);
}

void UTerritoryJournalWidget::HandleGuardManagementResult(
	ATerritoryVolume* Territory, bool bSuccess, FText Message, int32 RequestId)
{
	(void)bSuccess;
	(void)RequestId;
	if (Text_CommandStatus)
	{
		Text_CommandStatus->SetText(Message);
	}
	RefreshDistrictList();
	if (Territory && (Territory == SelectedDistrict.Get() || Territory == SelectedGarrisonTarget.Get()))
	{
		UpdateSelectedDistrict(SelectedDistrict.Get());
	}
}

FTerritoryDistrictOperationsView UTerritoryJournalWidget::GetSelectedDistrictOperationsView() const
{
	FTerritoryDistrictOperationsView View;
	UTerritoryUIBlueprintLibrary::BuildDistrictOperationsView(
		this, SelectedDistrict.Get(), GetOwningPlayer(), View);
	return View;
}

void UTerritoryJournalWidget::SetOperationsFilter(ETerritoryOperationsFilter Filter)
{
	SelectedOperationsFilter = Filter;
	LastOperationsRevision = INDEX_NONE;
	if (DistrictOperationalFilter)
	{
		const FString Option = GetOperationsOption(Filter);
		if (DistrictOperationalFilter->GetSelectedOption() != Option)
		{
			DistrictOperationalFilter->SetSelectedOption(Option);
		}
	}
	RefreshDistrictList();
}

UWidget* UTerritoryJournalWidget::NativeGetDesiredFocusTarget() const
{
	for (UTerritoryDistrictRowWidget* Entry : TerritoryEntryWidgets)
	{
		if (Entry && Entry->IsVisible())
		{
			return Entry->GetEntryFocusTarget();
		}
	}
	if (Btn_TerritoryTab && Btn_TerritoryTab->GetIsEnabled() && Btn_TerritoryTab->IsVisible())
	{
		return Btn_TerritoryTab;
	}
	return Super::NativeGetDesiredFocusTarget();
}
