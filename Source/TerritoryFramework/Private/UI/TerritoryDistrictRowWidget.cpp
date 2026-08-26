#include "UI/TerritoryDistrictRowWidget.h"

#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "Widgets/NarrativeCommonTextBlock.h"
#include "CommonTextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

namespace
{
	void StyleDistrictRowText(UTextBlock* Text, int32 FontSize, const FLinearColor& Color)
	{
		if (!Text)
		{
			return;
		}
		if (UNarrativeCommonTextBlock* CommonText = Cast<UNarrativeCommonTextBlock>(Text))
		{
			const UTerritoryDeveloperSettings* Settings =
				GetDefault<UTerritoryDeveloperSettings>();
			const TSubclassOf<UCommonTextStyle> Style = Settings
				? (FontSize >= 17
					? Settings->TerritoryHeadingTextStyle.LoadSynchronous()
					: Settings->DefaultTerritoryTextStyle.LoadSynchronous())
				: nullptr;
			if (Style)
			{
				CommonText->SetStyle(Style);
				CommonText->SetAutoWrapText(true);
				CommonText->SetColorAndOpacity(FSlateColor(Color));
			}
		}
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
	}

	void SetRoundedSurface(UBorder* Border, const FLinearColor& Fill,
		const FLinearColor& Outline, float Radius = 10.f, float OutlineWidth = 1.f,
		bool bUseThemePanel = false)
	{
		if (!Border) return;
		FSlateBrush Brush;
		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
		if (bUseThemePanel)
		{
			if (UTexture2D* PanelTexture = Settings
				? Settings->TerritoryPanelTexture.LoadSynchronous() : nullptr)
			{
				Brush.DrawAs = ESlateBrushDrawType::Box;
				Brush.SetResourceObject(PanelTexture);
				Brush.ImageSize = FVector2D(
					PanelTexture->GetSizeX(), PanelTexture->GetSizeY());
				Brush.Margin = FMargin(0.035f, 0.22f);
				Brush.TintColor = FSlateColor(FLinearColor(
					0.72f + Outline.R * 0.28f,
					0.72f + Outline.G * 0.28f,
					0.72f + Outline.B * 0.28f, Fill.A));
				Border->SetBrush(Brush);
				return;
			}
		}
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			Radius, FSlateColor(Outline), OutlineWidth);
		Border->SetBrush(Brush);
	}

	FLinearColor GetDistrictAccent(const FTerritoryDistrictOperationsView& View)
	{
		// Unowned rows deliberately stay neutral. Threat and ownership intelligence
		// belongs to a successful espionage report, not the unlocked directory row.
		if (!View.bOwnedByViewer)
		{
			return View.bUnlocked
				? FLinearColor(1.f, 0.84f, 0.f, 1.f)
				: FLinearColor(0.42f, 0.47f, 0.52f, 1.f);
		}
		if (View.bUnderAttack || View.bAttackScheduled || View.bThreatPreviewAvailable)
		{
			return FLinearColor(1.f, 0.25f, 0.16f, 1.f);
		}
		if (View.bOwnedByViewer)
		{
			return FLinearColor(0.08f, 0.88f, 0.62f, 1.f);
		}
		if (View.bAvailableForCapture)
		{
			return FLinearColor(1.f, 0.84f, 0.f, 1.f);
		}
		if (!View.bUnlocked)
		{
			return FLinearColor(0.42f, 0.47f, 0.52f, 1.f);
		}
		return FLinearColor(0.88f, 0.74f, 0.08f, 1.f);
	}
}

void UTerritoryDistrictRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildNativeLayout();
	// Authored entry widgets may intentionally keep their children private in
	// Blueprint. Resolve the stable widget names as a runtime fallback so the
	// District row never becomes a blank shell when bIsVariable is disabled.
	if (WidgetTree)
	{
		if (!SelectDistrictButton)
		{
			SelectDistrictButton = Cast<UNarrativeCommonButtonBase>(
				WidgetTree->FindWidget(TEXT("SelectDistrictButton")));
		}
		if (!AddGuardButton)
		{
			AddGuardButton = Cast<UNarrativeCommonButtonBase>(
				WidgetTree->FindWidget(TEXT("AddGuardButton")));
		}
		if (!RemoveGuardButton)
		{
			RemoveGuardButton = Cast<UNarrativeCommonButtonBase>(
				WidgetTree->FindWidget(TEXT("RemoveGuardButton")));
		}
		if (!SetWaypointButton)
		{
			SetWaypointButton = Cast<UNarrativeCommonButtonBase>(
				WidgetTree->FindWidget(TEXT("SetWaypointButton")));
		}
		if (!EspionageButton)
		{
			EspionageButton = Cast<UNarrativeCommonButtonBase>(
				WidgetTree->FindWidget(TEXT("EspionageButton")));
		}
		if (!DistrictName)
		{
			DistrictName = Cast<UNarrativeCommonTextBlock>(
				WidgetTree->FindWidget(TEXT("DistrictName")));
		}
		if (!DistrictSummary)
		{
			DistrictSummary = Cast<UNarrativeCommonTextBlock>(
				WidgetTree->FindWidget(TEXT("DistrictSummary")));
		}
		if (!DistrictStatus)
		{
			DistrictStatus = Cast<UNarrativeCommonTextBlock>(
				WidgetTree->FindWidget(TEXT("DistrictStatus")));
		}
		if (!DistrictRowSurface)
		{
			DistrictRowSurface = Cast<UBorder>(
				WidgetTree->FindWidget(TEXT("DistrictRowSurface")));
		}
		if (!DistrictAccentRail)
		{
			DistrictAccentRail = Cast<UBorder>(
				WidgetTree->FindWidget(TEXT("DistrictAccentRail")));
		}
		if (!DistrictStatusSurface)
		{
			DistrictStatusSurface = Cast<UBorder>(
				WidgetTree->FindWidget(TEXT("DistrictStatusSurface")));
		}
		if (!PlaceList)
		{
			PlaceList = Cast<UVerticalBox>(
				WidgetTree->FindWidget(TEXT("PlaceList")));
		}
		if (!PlaceProgressTrack)
		{
			PlaceProgressTrack = Cast<UHorizontalBox>(
				WidgetTree->FindWidget(TEXT("PlaceProgressTrack")));
		}
		if (!OwnedProgressSegment)
		{
			OwnedProgressSegment = Cast<UBorder>(
				WidgetTree->FindWidget(TEXT("OwnedProgressSegment")));
		}
		if (!KnownProgressSegment)
		{
			KnownProgressSegment = Cast<UBorder>(
				WidgetTree->FindWidget(TEXT("KnownProgressSegment")));
		}
		if (!HiddenProgressSegment)
		{
			HiddenProgressSegment = Cast<UBorder>(
				WidgetTree->FindWidget(TEXT("HiddenProgressSegment")));
		}
		if (!PlaceProgressText)
		{
			PlaceProgressText = Cast<UTextBlock>(
				WidgetTree->FindWidget(TEXT("PlaceProgressText")));
		}
		if (!ExpandHintText)
		{
			ExpandHintText = Cast<UTextBlock>(
				WidgetTree->FindWidget(TEXT("ExpandHintText")));
		}
	}
	if (!EspionageButton && WidgetTree)
	{
		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
		const TSubclassOf<UNarrativeCommonButtonBase> ButtonClass = Settings
			? Settings->DefaultNarrativeButtonClass.LoadSynchronous() : nullptr;
		UHorizontalBox* Header = Cast<UHorizontalBox>(
			WidgetTree->FindWidget(TEXT("TerritoryEntryHeader")));
		if (!Header)
		{
			Header = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("DistrictHeader")));
		}
		if (ButtonClass && Header)
		{
			UVerticalBox* ActionStack = WidgetTree->ConstructWidget<UVerticalBox>(
				UVerticalBox::StaticClass(), TEXT("TerritoryEntryActionStack"));
			if (UWidget* WaypointSize = WidgetTree->FindWidget(TEXT("WaypointButtonSize")))
			{
				if (UPanelWidget* Parent = WaypointSize->GetParent())
				{
					Parent->RemoveChild(WaypointSize);
				}
				if (UVerticalBoxSlot* WaypointStackSlot =
					ActionStack->AddChildToVerticalBox(WaypointSize))
				{
					WaypointStackSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 3.f));
				}
			}
			USizeBox* EspionageSize = WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(), TEXT("EspionageButtonSize"));
			EspionageSize->SetMinDesiredWidth(112.f);
			EspionageButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
				ButtonClass, TEXT("EspionageButton"));
			if (const TSubclassOf<UCommonButtonStyle> Style =
				Settings->DefaultTerritoryButtonStyle.LoadSynchronous())
			{
				EspionageButton->SetStyle(Style);
			}
			EspionageSize->SetContent(EspionageButton);
			ActionStack->AddChildToVerticalBox(EspionageSize);
			if (UHorizontalBoxSlot* ActionStackSlot =
				Header->AddChildToHorizontalBox(ActionStack))
			{
				ActionStackSlot->SetPadding(FMargin(0.f, 5.f, 8.f, 5.f));
				ActionStackSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}
	if (SelectDistrictButton)
	{
		SelectDistrictButton->OnClicked().AddUObject(this, &UTerritoryDistrictRowWidget::HandleSelected);
	}
	if (AddGuardButton)
	{
		AddGuardButton->OnClicked().AddUObject(this, &UTerritoryDistrictRowWidget::HandleAddGuard);
		AddGuardButton->SetButtonText(NSLOCTEXT("TerritoryDistrictRow", "AddGuard", "+ GUARD"));
		AddGuardButton->SetVisibility(bShowInlineGuardActions
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (RemoveGuardButton)
	{
		RemoveGuardButton->OnClicked().AddUObject(this, &UTerritoryDistrictRowWidget::HandleRemoveGuard);
		RemoveGuardButton->SetButtonText(NSLOCTEXT("TerritoryDistrictRow", "RemoveGuard", "- GUARD"));
		RemoveGuardButton->SetVisibility(bShowInlineGuardActions
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	StyleDistrictRowText(DistrictName, 15,
		FLinearColor(0.96f, 0.95f, 0.91f, 1.f));
	StyleDistrictRowText(DistrictSummary, 10,
		FLinearColor(0.68f, 0.67f, 0.63f, 1.f));
	StyleDistrictRowText(DistrictStatus, 9,
		FLinearColor(0.31f, 0.82f, 0.63f, 1.f));
	StyleDistrictRowText(PlaceProgressText, 10,
		FLinearColor(0.96f, 0.84f, 0.18f, 1.f));
	StyleDistrictRowText(ExpandHintText, 9,
		FLinearColor(0.72f, 0.70f, 0.64f, 1.f));
	if (SetWaypointButton)
	{
		SetWaypointButton->OnClicked().AddUObject(
			this, &UTerritoryDistrictRowWidget::HandleSetWaypoint);
	}
	if (EspionageButton)
	{
		EspionageButton->SetButtonText(NSLOCTEXT(
			"TerritoryDistrictRow", "Espionage", "ESPIONAGE"));
		EspionageButton->OnClicked().AddUObject(
			this, &UTerritoryDistrictRowWidget::HandleEspionage);
	}
	RefreshRow();
}

void UTerritoryDistrictRowWidget::NativeDestruct()
{
	if (SelectDistrictButton) SelectDistrictButton->OnClicked().RemoveAll(this);
	if (AddGuardButton) AddGuardButton->OnClicked().RemoveAll(this);
	if (RemoveGuardButton) RemoveGuardButton->OnClicked().RemoveAll(this);
	if (SetWaypointButton) SetWaypointButton->OnClicked().RemoveAll(this);
	if (EspionageButton) EspionageButton->OnClicked().RemoveAll(this);
	Super::NativeDestruct();
}

void UTerritoryDistrictRowWidget::BuildNativeLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("DistrictRowSize"));
	RootSize->SetMinDesiredHeight(54.f);
	WidgetTree->RootWidget = RootSize;
	DistrictRowSurface = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("DistrictRowSurface"));
	DistrictRowSurface->SetPadding(FMargin(0.f));
	SetRoundedSurface(DistrictRowSurface,
		FLinearColor(0.014f, 0.014f, 0.014f, 0.98f),
		FLinearColor(0.42f, 0.35f, 0.f, 0.62f), 3.f, 1.f, true);
	RootSize->SetContent(DistrictRowSurface);
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("DistrictRowRoot"));
	DistrictRowSurface->SetContent(Root);

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("DistrictHeader"));
	if (UVerticalBoxSlot* HeaderSlot = Root->AddChildToVerticalBox(Header))
	{
		HeaderSlot->SetPadding(FMargin(0.f));
	}

	USizeBox* AccentSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("DistrictAccentSize"));
	AccentSize->SetWidthOverride(4.f);
	DistrictAccentRail = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("DistrictAccentRail"));
	SetRoundedSurface(DistrictAccentRail,
		FLinearColor(0.88f, 0.74f, 0.08f, 1.f), FLinearColor::Transparent, 2.f, 0.f);
	AccentSize->SetContent(DistrictAccentRail);
	if (UHorizontalBoxSlot* AccentSlot = Header->AddChildToHorizontalBox(AccentSize))
	{
		AccentSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	}

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const TSubclassOf<UNarrativeCommonButtonBase> NarrativeButtonClass = Settings
		? Settings->DefaultNarrativeButtonClass.LoadSynchronous() : nullptr;
	const TSubclassOf<UCommonButtonStyle> TerritoryButtonStyle = Settings
		? Settings->DefaultTerritoryButtonStyle.LoadSynchronous() : nullptr;
	auto ApplyTerritoryStyle = [TerritoryButtonStyle](UNarrativeCommonButtonBase* Button)
	{
		if (Button && TerritoryButtonStyle)
		{
			Button->SetStyle(TerritoryButtonStyle);
		}
	};
	if (!NarrativeButtonClass)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("Territory district row has no valid DefaultNarrativeButtonClass; rendering read-only details."));
	}

	UOverlay* SelectOverlay = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), TEXT("SelectDistrictOverlay"));
	UVerticalBox* Details = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DistrictDetails"));
	DistrictName = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(UNarrativeCommonTextBlock::StaticClass(), TEXT("DistrictName"));
	DistrictSummary = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(UNarrativeCommonTextBlock::StaticClass(), TEXT("DistrictSummary"));
	DistrictStatus = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(UNarrativeCommonTextBlock::StaticClass(), TEXT("DistrictStatus"));
	PlaceProgressText = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("PlaceProgressText"));
	ExpandHintText = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("ExpandHintText"));
	StyleDistrictRowText(DistrictName, 17, FLinearColor(0.96f, 0.95f, 0.91f, 1.f));
	StyleDistrictRowText(DistrictSummary, 12, FLinearColor(0.68f, 0.67f, 0.63f, 1.f));
	StyleDistrictRowText(DistrictStatus, 11, FLinearColor(0.31f, 0.82f, 0.63f, 1.f));
	StyleDistrictRowText(PlaceProgressText, 13, FLinearColor(0.96f, 0.84f, 0.18f, 1.f));
	StyleDistrictRowText(ExpandHintText, 11, FLinearColor(0.72f, 0.70f, 0.64f, 1.f));

	UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("DistrictNameRow"));
	if (UHorizontalBoxSlot* NameSlot = NameRow->AddChildToHorizontalBox(DistrictName))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* ProgressTextSlot = NameRow->AddChildToHorizontalBox(PlaceProgressText))
	{
		ProgressTextSlot->SetHorizontalAlignment(HAlign_Right);
		ProgressTextSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* NameRowSlot = Details->AddChildToVerticalBox(NameRow))
	{
		NameRowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
	}
	if (UVerticalBoxSlot* SummarySlot = Details->AddChildToVerticalBox(DistrictSummary))
	{
		SummarySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
	}

	USizeBox* ProgressHeight = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("PlaceProgressHeight"));
	ProgressHeight->SetHeightOverride(5.f);
	PlaceProgressTrack = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("PlaceProgressTrack"));
	ProgressHeight->SetContent(PlaceProgressTrack);
	auto AddProgressSegment = [this](const TCHAR* Name, const FLinearColor& Color,
		TObjectPtr<UBorder>& OutSegment)
	{
		OutSegment = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(Name));
		SetRoundedSurface(OutSegment, Color, FLinearColor::Transparent, 4.f, 0.f);
		if (UHorizontalBoxSlot* Slot = PlaceProgressTrack->AddChildToHorizontalBox(OutSegment))
		{
			Slot->SetPadding(FMargin(1.f, 0.f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	};
	AddProgressSegment(TEXT("OwnedProgressSegment"),
		FLinearColor(0.16f, 0.72f, 0.55f, 1.f), OwnedProgressSegment);
	AddProgressSegment(TEXT("KnownProgressSegment"),
		FLinearColor(1.f, 0.84f, 0.f, 1.f), KnownProgressSegment);
	AddProgressSegment(TEXT("HiddenProgressSegment"),
		FLinearColor(0.27f, 0.27f, 0.26f, 0.78f), HiddenProgressSegment);
	if (UVerticalBoxSlot* ProgressSlot = Details->AddChildToVerticalBox(ProgressHeight))
	{
		ProgressSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}

	UHorizontalBox* StatusRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("DistrictStatusRow"));
	DistrictStatusSurface = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("DistrictStatusSurface"));
	DistrictStatusSurface->SetPadding(FMargin(7.f, 2.f));
	SetRoundedSurface(DistrictStatusSurface,
		FLinearColor(0.035f, 0.032f, 0.01f, 0.94f),
		FLinearColor(0.88f, 0.74f, 0.08f, 0.62f), 3.f, 1.f);
	DistrictStatusSurface->SetContent(DistrictStatus);
	StatusRow->AddChildToHorizontalBox(DistrictStatusSurface);
	if (UHorizontalBoxSlot* HintSlot = StatusRow->AddChildToHorizontalBox(ExpandHintText))
	{
		HintSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		HintSlot->SetHorizontalAlignment(HAlign_Right);
		HintSlot->SetVerticalAlignment(VAlign_Center);
		HintSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	}
	Details->AddChildToVerticalBox(StatusRow);
	Details->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (NarrativeButtonClass)
	{
		SelectDistrictButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(NarrativeButtonClass, TEXT("SelectDistrictButton"));
		ApplyTerritoryStyle(SelectDistrictButton);
		SelectDistrictButton->SetButtonText(FText::GetEmpty());
		SelectOverlay->AddChild(SelectDistrictButton);
	}
	SelectOverlay->AddChild(Details);
	if (UHorizontalBoxSlot* SelectSlot = Header->AddChildToHorizontalBox(SelectOverlay))
	{
		SelectSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SelectSlot->SetPadding(FMargin(0.f, 10.f, 12.f, 10.f));
	}

	if (NarrativeButtonClass)
	{
		USizeBox* WaypointSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("WaypointButtonSize"));
		WaypointSize->SetMinDesiredWidth(112.f);
		SetWaypointButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
			NarrativeButtonClass, TEXT("SetWaypointButton"));
		ApplyTerritoryStyle(SetWaypointButton);
		WaypointSize->SetContent(SetWaypointButton);
		if (UHorizontalBoxSlot* WaypointSlot =
			Header->AddChildToHorizontalBox(WaypointSize))
		{
			WaypointSlot->SetPadding(FMargin(0.f, 8.f, 10.f, 8.f));
			WaypointSlot->SetVerticalAlignment(VAlign_Center);
		}

		AddGuardButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(NarrativeButtonClass, TEXT("AddGuardButton"));
		ApplyTerritoryStyle(AddGuardButton);
		Header->AddChild(AddGuardButton);

		RemoveGuardButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(NarrativeButtonClass, TEXT("RemoveGuardButton"));
		ApplyTerritoryStyle(RemoveGuardButton);
		Header->AddChild(RemoveGuardButton);
	}

	PlaceList = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("KnownPlaceList"));
	PlaceList->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* PlaceListSlot = Root->AddChildToVerticalBox(PlaceList))
	{
		PlaceListSlot->SetPadding(FMargin(14.f, 0.f, 14.f, 12.f));
	}
}

void UTerritoryDistrictRowWidget::InitializeDistrict(ATerritoryDistrict* InDistrict)
{
	District = InDistrict;
	OperationsView = FTerritoryDistrictOperationsView();
	OperationsView.District = InDistrict;
	RefreshRow();
}

void UTerritoryDistrictRowWidget::InitializeOperationsView(
	const FTerritoryDistrictOperationsView& InView)
{
	OperationsView = InView;
	District = InView.District;
	bCanAddGuard = InView.bCanAddGuard;
	bCanRemoveGuard = InView.bCanRemoveGuard;
	if (!InView.bOwnedByViewer && InView.bUnlocked && InView.bHierarchyVisible)
	{
		ActionStatus = NSLOCTEXT("TerritoryDistrictRow", "ReconAvailable",
			"RECONNAISSANCE AVAILABLE");
	}
	else if (InView.bUnderAttack || InView.bAttackScheduled || InView.bThreatPreviewAvailable)
	{
		ActionStatus = InView.ThreatSummary;
	}
	else if (!InView.bUnlocked)
	{
		ActionStatus = InView.LockReason.IsEmpty()
			? NSLOCTEXT("TerritoryDistrictRow", "LockedStatus", "LOCKED — select to inspect requirements")
			: InView.LockReason;
	}
	else if (InView.bAvailableForCapture)
	{
		ActionStatus = NSLOCTEXT("TerritoryDistrictRow", "AvailableStatus", "AVAILABLE FOR CAPTURE");
	}
	else if (InView.bOwnedByViewer)
	{
		ActionStatus = InView.bManageable
			? NSLOCTEXT("TerritoryDistrictRow", "ManageableStatus", "COMMAND AVAILABLE")
			: InView.ManagementFailureReason;
	}
	else
	{
		ActionStatus = InView.AvailabilityReason;
	}
	RefreshRow();
}

void UTerritoryDistrictRowWidget::SetGuardActionState(bool bCanAdd, bool bCanRemove, const FText& Status)
{
	bCanAddGuard = bCanAdd;
	bCanRemoveGuard = bCanRemove;
	ActionStatus = Status;
	if (AddGuardButton)
	{
		AddGuardButton->SetIsEnabled(bCanAdd);
	}
	if (RemoveGuardButton)
	{
		RemoveGuardButton->SetIsEnabled(bCanRemove);
	}
	if (DistrictStatus)
	{
		DistrictStatus->SetText(ActionStatus);
	}
}

void UTerritoryDistrictRowWidget::SetExpanded(bool bInExpanded)
{
	bExpanded = bInExpanded;
	// Refresh the complete visual state so the outline, hint, progress, and
	// anonymous hidden-Place row change together.
	RefreshRow();
}

void UTerritoryDistrictRowWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	if (SelectDistrictButton)
	{
		SelectDistrictButton->SetIsSelected(bSelected);
	}
	RefreshRow();
}

UWidget* UTerritoryDistrictRowWidget::GetEntryFocusTarget() const
{
	if (SelectDistrictButton)
	{
		return SelectDistrictButton.Get();
	}
	return const_cast<UTerritoryDistrictRowWidget*>(this);
}

ATerritoryDistrict* UTerritoryDistrictRowWidget::GetDistrict() const
{
	return District.Get();
}

void UTerritoryDistrictRowWidget::RefreshRow()
{
	ATerritoryDistrict* CurrentDistrict = District.Get();
	if (!CurrentDistrict)
	{
		return;
	}

	const UEnum* StateEnum = StaticEnum<ETerritoryState>();
	const FText StateText = StateEnum
		? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(CurrentDistrict->GetTerritoryState()))
		: FText::GetEmpty();
	if (DistrictName)
	{
		const FText DistrictDisplayName = OperationsView.DisplayName.IsEmpty()
			? CurrentDistrict->GetTerritoryDisplayName()
			: OperationsView.DisplayName;
		DistrictName->SetText(DistrictDisplayName);
	}
	if (DistrictSummary)
	{
		const FText CityName = OperationsView.CityDisplayName.IsEmpty()
			? NSLOCTEXT("TerritoryDistrictRow", "IndependentCity", "Independent")
			: OperationsView.CityDisplayName;
		DistrictSummary->SetText(OperationsView.bOwnedByViewer
			? FText::Format(NSLOCTEXT("TerritoryDistrictRow", "OwnedCompactSummary",
				"{0}  •  {1}  •  {2}"), CityName,
				UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(
					OperationsView.OwnerFaction), StateText)
			: FText::Format(NSLOCTEXT("TerritoryDistrictRow", "UnlockedCompactSummary",
				"{0}  •  UNLOCKED"), CityName));
	}
	if (AddGuardButton)
	{
		AddGuardButton->SetIsEnabled(bCanAddGuard);
	}
	if (RemoveGuardButton)
	{
		RemoveGuardButton->SetIsEnabled(bCanRemoveGuard);
	}
	if (SelectDistrictButton)
	{
		SelectDistrictButton->SetIsSelected(bSelected);
	}
	if (SetWaypointButton)
	{
		const bool bTracked = UTerritoryUIBlueprintLibrary::GetTrackedTerritory(
			GetOwningPlayer()) == CurrentDistrict;
		SetWaypointButton->SetButtonText(bTracked
			? NSLOCTEXT("TerritoryDistrictRow", "WaypointTracked", "TRACKED  ✓")
			: NSLOCTEXT("TerritoryDistrictRow", "SetWaypoint", "SET WAYPOINT"));
		SetWaypointButton->SetIsSelected(bTracked);
		SetWaypointButton->SetIsEnabled(OperationsView.bUnlocked
			&& OperationsView.bHierarchyVisible);
		SetWaypointButton->SetVisibility(OperationsView.bUnlocked
			&& OperationsView.bHierarchyVisible
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (EspionageButton)
	{
		const bool bCanEspionage = !OperationsView.bOwnedByViewer
			&& OperationsView.bUnlocked && OperationsView.bHierarchyVisible;
		EspionageButton->SetIsEnabled(bCanEspionage);
		EspionageButton->SetVisibility(bCanEspionage
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (DistrictStatus)
	{
		DistrictStatus->SetText(ActionStatus);
		const FLinearColor Accent = GetDistrictAccent(OperationsView);
		DistrictStatus->SetColorAndOpacity(FSlateColor(Accent));
		SetRoundedSurface(DistrictStatusSurface,
			FLinearColor(Accent.R * 0.045f, Accent.G * 0.045f, Accent.B * 0.045f, 0.94f),
			FLinearColor(Accent.R, Accent.G, Accent.B, 0.7f), 3.f, 1.f);
		SetRoundedSurface(DistrictAccentRail, Accent, FLinearColor::Transparent, 2.f, 0.f);
		SetRoundedSurface(DistrictRowSurface,
			bSelected
				? FLinearColor(0.035f, 0.032f, 0.012f, 0.99f)
				: FLinearColor(0.014f, 0.014f, 0.014f, 0.98f),
			FLinearColor(Accent.R, Accent.G, Accent.B, bSelected ? 0.72f : 0.30f),
			3.f, 1.f, true);
	}
	RefreshPlaceProgress();
	RebuildPlaceList();
}

void UTerritoryDistrictRowWidget::RefreshPlaceProgress()
{
	const int32 Total = FMath::Max(0, OperationsView.TotalProperties);
	const int32 Known = FMath::Clamp(OperationsView.KnownProperties, 0, Total);
	const int32 Owned = FMath::Clamp(OperationsView.OwnedProperties, 0, Known);
	const int32 KnownUncontrolled = FMath::Max(0, Known - Owned);
	const int32 Hidden = FMath::Max(0, Total - Known);

	if (PlaceProgressText)
	{
		PlaceProgressText->SetText(Total > 0
			? OperationsView.bOwnedByViewer
				? FText::Format(NSLOCTEXT("TerritoryDistrictRow", "PlaceControlProgress",
					"{0} / {1} PLACES"), FText::AsNumber(Owned), FText::AsNumber(Total))
				: FText::Format(NSLOCTEXT("TerritoryDistrictRow", "PlaceDirectoryProgress",
					"{0} KNOWN  •  {1} LOCKED"), FText::AsNumber(Known),
					FText::AsNumber(Hidden))
			: NSLOCTEXT("TerritoryDistrictRow", "NoConfiguredPlaces", "NO PLACES"));
	}
	if (ExpandHintText)
	{
		if (bExpanded)
		{
			ExpandHintText->SetText(NSLOCTEXT(
				"TerritoryDistrictRow", "HideKnownPlaces", "HIDE PLACES"));
		}
		else if (Hidden > 0)
		{
			ExpandHintText->SetText(FText::Format(
				NSLOCTEXT("TerritoryDistrictRow", "KnownAndHiddenPlaces",
					"{0} DISCOVERED  •  {1} HIDDEN  >"),
				FText::AsNumber(Known), FText::AsNumber(Hidden)));
		}
		else
		{
			ExpandHintText->SetText(FText::Format(
				NSLOCTEXT("TerritoryDistrictRow", "KnownPlaces", "{0} DISCOVERED  >"),
				FText::AsNumber(Known)));
		}
	}

	auto SetSegmentWeight = [](UBorder* Segment, int32 Weight)
	{
		if (!Segment)
		{
			return;
		}
		Segment->SetVisibility(Weight > 0
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Segment->Slot))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = static_cast<float>(FMath::Max(1, Weight));
			Slot->SetSize(Size);
		}
	};
	SetSegmentWeight(OwnedProgressSegment, Owned);
	SetSegmentWeight(KnownProgressSegment, KnownUncontrolled);
	SetSegmentWeight(HiddenProgressSegment, Hidden);
	if (PlaceProgressTrack)
	{
		PlaceProgressTrack->SetVisibility(Total > 0 && OperationsView.bOwnedByViewer
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UTerritoryDistrictRowWidget::RebuildPlaceList()
{
	if (!PlaceList)
	{
		return;
	}
	PlaceList->ClearChildren();
	PlaceList->SetVisibility(bExpanded
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!bExpanded || !WidgetTree)
	{
		return;
	}

	const UEnum* StateEnum = StaticEnum<ETerritoryState>();
	for (const FTerritoryHierarchyOperationsView& Place : OperationsView.VisiblePlaces)
	{
		const FText StateText = StateEnum
			? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(Place.TerritoryState))
			: FText::GetEmpty();
		const FLinearColor Accent = Place.bOwnedByViewer
			? FLinearColor(0.16f, 0.72f, 0.55f, 1.f)
			: Place.bAvailableForCapture
				? FLinearColor(1.f, 0.84f, 0.f, 1.f)
				: FLinearColor(0.68f, 0.67f, 0.63f, 1.f);
		UBorder* PlaceSurface = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), FName(*FString::Printf(
				TEXT("KnownPlaceSurface_%u"), GetTypeHash(Place.TerritoryTag))));
		PlaceSurface->SetPadding(FMargin(10.f, 7.f));
		SetRoundedSurface(PlaceSurface,
			FLinearColor(0.025f, 0.025f, 0.025f, 0.98f),
			FLinearColor(Accent.R, Accent.G, Accent.B, 0.28f), 2.f, 1.f);
		UNarrativeCommonTextBlock* PlaceText =
			WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
				UNarrativeCommonTextBlock::StaticClass(),
				FName(*FString::Printf(TEXT("KnownPlaceText_%u"),
					GetTypeHash(Place.TerritoryTag))));
		PlaceText->SetText(OperationsView.bOwnedByViewer
			? FText::Format(NSLOCTEXT("TerritoryDistrictRow", "KnownPlaceRow",
				"{0}\n{1}  •  {2}"), Place.DisplayName,
				UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(Place.OwnerFaction),
				StateText)
			: Place.DisplayName);
		StyleDistrictRowText(PlaceText, 12, Accent);
		PlaceSurface->SetContent(PlaceText);
		if (UVerticalBoxSlot* PlaceSlot = PlaceList->AddChildToVerticalBox(PlaceSurface))
		{
			PlaceSlot->SetPadding(FMargin(0.f, 2.f));
		}
	}

	if (OperationsView.HiddenProperties > 0)
	{
		UBorder* HiddenSurface = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("HiddenPlaceSummarySurface"));
		HiddenSurface->SetPadding(FMargin(10.f, 7.f));
		SetRoundedSurface(HiddenSurface,
			FLinearColor(0.025f, 0.025f, 0.025f, 0.86f),
			FLinearColor(0.32f, 0.32f, 0.31f, 0.28f), 2.f, 1.f);
		UNarrativeCommonTextBlock* HiddenText =
			WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
				UNarrativeCommonTextBlock::StaticClass(), TEXT("HiddenPlaceSummaryText"));
		HiddenText->SetText(FText::Format(
			OperationsView.bOwnedByViewer
				? NSLOCTEXT("TerritoryDistrictRow", "HiddenPlaceSummary",
					"{0} LOCATIONS REMAIN HIDDEN\nNames and objectives stay hidden until the story unlocks them.")
				: NSLOCTEXT("TerritoryDistrictRow", "LockedPlaceCount",
					"{0} PLACES LOCKED BY STORY"),
			FText::AsNumber(OperationsView.HiddenProperties)));
		StyleDistrictRowText(HiddenText, 11, FLinearColor(0.58f, 0.57f, 0.54f, 1.f));
		HiddenSurface->SetContent(HiddenText);
		if (UVerticalBoxSlot* HiddenSlot = PlaceList->AddChildToVerticalBox(HiddenSurface))
		{
			HiddenSlot->SetPadding(FMargin(0.f, 2.f));
		}
	}
}

void UTerritoryDistrictRowWidget::HandleSelected()
{
	if (District.IsValid())
	{
		SetExpanded(!bExpanded);
		OnDistrictSelected.Broadcast(District.Get());
	}
}

void UTerritoryDistrictRowWidget::HandleAddGuard()
{
	if (District.IsValid())
	{
		OnGuardActionRequested.Broadcast(District.Get(), 1);
	}
}

void UTerritoryDistrictRowWidget::HandleRemoveGuard()
{
	if (District.IsValid())
	{
		OnGuardActionRequested.Broadcast(District.Get(), -1);
	}
}

void UTerritoryDistrictRowWidget::HandleSetWaypoint()
{
	if (District.IsValid() && OperationsView.bUnlocked
		&& OperationsView.bHierarchyVisible)
	{
		OnWaypointRequested.Broadcast(District.Get());
	}
}

void UTerritoryDistrictRowWidget::HandleEspionage()
{
	if (District.IsValid() && !OperationsView.bOwnedByViewer
		&& OperationsView.bUnlocked && OperationsView.bHierarchyVisible)
	{
		OnEspionageRequested.Broadcast(District.Get());
	}
}
