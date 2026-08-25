#include "UI/TerritoryDistrictRowWidget.h"

#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
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
				return;
			}
		}
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
	}

	void SetRoundedSurface(UBorder* Border, const FLinearColor& Fill,
		const FLinearColor& Outline, float Radius = 10.f, float OutlineWidth = 1.f)
	{
		if (!Border) return;
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			Radius, FSlateColor(Outline), OutlineWidth);
		Border->SetBrush(Brush);
	}

	FLinearColor GetDistrictAccent(const FTerritoryDistrictOperationsView& View)
	{
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
			return FLinearColor(1.f, 0.66f, 0.18f, 1.f);
		}
		if (!View.bUnlocked)
		{
			return FLinearColor(0.42f, 0.47f, 0.52f, 1.f);
		}
		return FLinearColor(0.18f, 0.66f, 0.82f, 1.f);
	}
}

void UTerritoryDistrictRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildNativeLayout();
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
	RefreshRow();
}

void UTerritoryDistrictRowWidget::NativeDestruct()
{
	if (SelectDistrictButton) SelectDistrictButton->OnClicked().RemoveAll(this);
	if (AddGuardButton) AddGuardButton->OnClicked().RemoveAll(this);
	if (RemoveGuardButton) RemoveGuardButton->OnClicked().RemoveAll(this);
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
	RootSize->SetMinDesiredHeight(96.f);
	WidgetTree->RootWidget = RootSize;
	DistrictRowSurface = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("DistrictRowSurface"));
	DistrictRowSurface->SetPadding(FMargin(0.f));
	SetRoundedSurface(DistrictRowSurface,
		FLinearColor(0.018f, 0.045f, 0.07f, 0.94f),
		FLinearColor(0.12f, 0.24f, 0.31f, 0.85f));
	RootSize->SetContent(DistrictRowSurface);
	UHorizontalBox* Root = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("DistrictRowRoot"));
	DistrictRowSurface->SetContent(Root);

	USizeBox* AccentSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("DistrictAccentSize"));
	AccentSize->SetWidthOverride(5.f);
	DistrictAccentRail = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("DistrictAccentRail"));
	SetRoundedSurface(DistrictAccentRail,
		FLinearColor(0.18f, 0.66f, 0.82f, 1.f), FLinearColor::Transparent, 10.f, 0.f);
	AccentSize->SetContent(DistrictAccentRail);
	if (UHorizontalBoxSlot* AccentSlot = Root->AddChildToHorizontalBox(AccentSize))
	{
		AccentSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
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

	UOverlay* SelectOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SelectDistrictOverlay"));
	UVerticalBox* Details = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DistrictDetails"));
	DistrictName = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(UNarrativeCommonTextBlock::StaticClass(), TEXT("DistrictName"));
	DistrictSummary = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(UNarrativeCommonTextBlock::StaticClass(), TEXT("DistrictSummary"));
	DistrictStatus = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(UNarrativeCommonTextBlock::StaticClass(), TEXT("DistrictStatus"));
	StyleDistrictRowText(DistrictName, 18, FLinearColor(0.95f, 0.96f, 0.95f, 1.f));
	StyleDistrictRowText(DistrictSummary, 13, FLinearColor(0.65f, 0.69f, 0.68f, 1.f));
	StyleDistrictRowText(DistrictStatus, 13, FLinearColor(0.31f, 0.82f, 0.63f, 1.f));
	if (UVerticalBoxSlot* NameSlot = Details->AddChildToVerticalBox(DistrictName))
	{
		NameSlot->SetPadding(FMargin(0.f, 1.f, 0.f, 3.f));
	}
	if (UVerticalBoxSlot* SummarySlot = Details->AddChildToVerticalBox(DistrictSummary))
	{
		SummarySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 7.f));
	}
	DistrictStatusSurface = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("DistrictStatusSurface"));
	DistrictStatusSurface->SetPadding(FMargin(8.f, 3.f));
	SetRoundedSurface(DistrictStatusSurface,
		FLinearColor(0.04f, 0.12f, 0.14f, 0.9f),
		FLinearColor(0.18f, 0.66f, 0.82f, 0.65f), 12.f, 1.f);
	DistrictStatusSurface->SetContent(DistrictStatus);
	Details->AddChild(DistrictStatusSurface);
	Details->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (NarrativeButtonClass)
	{
		SelectDistrictButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(NarrativeButtonClass, TEXT("SelectDistrictButton"));
		ApplyTerritoryStyle(SelectDistrictButton);
		SelectOverlay->AddChild(SelectDistrictButton);
	}
	SelectOverlay->AddChild(Details);
	if (UHorizontalBoxSlot* SelectSlot = Root->AddChildToHorizontalBox(SelectOverlay))
	{
		SelectSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SelectSlot->SetPadding(FMargin(0.f, 10.f, 10.f, 10.f));
	}

	if (NarrativeButtonClass)
	{
		AddGuardButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(NarrativeButtonClass, TEXT("AddGuardButton"));
		ApplyTerritoryStyle(AddGuardButton);
		Root->AddChild(AddGuardButton);

		RemoveGuardButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(NarrativeButtonClass, TEXT("RemoveGuardButton"));
		ApplyTerritoryStyle(RemoveGuardButton);
		Root->AddChild(RemoveGuardButton);
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
	if (InView.bUnderAttack || InView.bAttackScheduled || InView.bThreatPreviewAvailable)
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
		DistrictName->SetText(OperationsView.CityDisplayName.IsEmpty()
			? DistrictDisplayName
			: FText::Format(NSLOCTEXT("TerritoryDistrictRow", "CityDistrictName", "{0}  //  {1}"),
				OperationsView.CityDisplayName, DistrictDisplayName));
	}
	if (DistrictSummary)
	{
		const int32 Active = OperationsView.District ? OperationsView.ActiveGuards : CurrentDistrict->GetSpawnedGuardCount();
		const int32 Desired = OperationsView.District ? OperationsView.DesiredGuards : CurrentDistrict->GetDesiredGuardCount();
		const int32 Maximum = OperationsView.District ? OperationsView.MaximumGuards : CurrentDistrict->GetMaxGuardCount();
		const int64 Net = OperationsView.District ? OperationsView.NetIncome
			: CurrentDistrict->GetEffectiveIncome()
				- (static_cast<int64>(CurrentDistrict->GetGuardCost()) * CurrentDistrict->GetDesiredGuardCount());
		DistrictSummary->SetText(FText::Format(
			NSLOCTEXT("TerritoryDistrictRow", "Summary", "{0}  •  {1}\nPOSTS {2}/{3}    GUARDS {4}/{5}/{6}    NET {7}\nPRODUCTION {8} ONLINE    {9} BLOCKED"),
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(CurrentDistrict->GetOwningFaction()),
			StateText,
			FText::AsNumber(OperationsView.OwnedProperties),
			FText::AsNumber(OperationsView.TotalProperties),
			FText::AsNumber(Active),
			FText::AsNumber(Desired),
			FText::AsNumber(Maximum),
			FText::AsNumber(Net),
			FText::AsNumber(OperationsView.ProducingSiteCount),
			FText::AsNumber(OperationsView.BlockedProductionSiteCount)));
	}
	if (AddGuardButton)
	{
		AddGuardButton->SetIsEnabled(bCanAddGuard);
	}
	if (RemoveGuardButton)
	{
		RemoveGuardButton->SetIsEnabled(bCanRemoveGuard);
	}
	if (DistrictStatus)
	{
		DistrictStatus->SetText(ActionStatus);
		const FLinearColor Accent = GetDistrictAccent(OperationsView);
		DistrictStatus->SetColorAndOpacity(FSlateColor(Accent));
		SetRoundedSurface(DistrictStatusSurface,
			FLinearColor(Accent.R * 0.08f, Accent.G * 0.08f, Accent.B * 0.08f, 0.92f),
			FLinearColor(Accent.R, Accent.G, Accent.B, 0.7f), 12.f, 1.f);
		SetRoundedSurface(DistrictAccentRail, Accent, FLinearColor::Transparent, 10.f, 0.f);
		SetRoundedSurface(DistrictRowSurface,
			FLinearColor(0.018f, 0.045f, 0.07f, 0.94f),
			FLinearColor(Accent.R, Accent.G, Accent.B, 0.32f), 10.f, 1.f);
	}
}

void UTerritoryDistrictRowWidget::HandleSelected()
{
	if (District.IsValid())
	{
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
