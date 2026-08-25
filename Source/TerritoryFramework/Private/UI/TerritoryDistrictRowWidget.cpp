#include "UI/TerritoryDistrictRowWidget.h"

#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "Widgets/NarrativeCommonTextBlock.h"

namespace
{
	void StyleDistrictRowText(UTextBlock* Text, int32 FontSize, const FLinearColor& Color)
	{
		if (!Text)
		{
			return;
		}
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
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
	RootSize->SetMinDesiredHeight(104.f);
	WidgetTree->RootWidget = RootSize;
	UBorder* Surface = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("DistrictRowSurface"));
	Surface->SetPadding(FMargin(10.f, 7.f));
	Surface->SetBrushColor(FLinearColor(0.035f, 0.075f, 0.12f, 0.96f));
	RootSize->SetContent(Surface);
	UHorizontalBox* Root = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("DistrictRowRoot"));
	Surface->SetContent(Root);

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
	Details->AddChild(DistrictName);
	Details->AddChild(DistrictSummary);
	Details->AddChild(DistrictStatus);
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
		SelectSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
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
		DistrictName->SetText(OperationsView.DisplayName.IsEmpty()
			? CurrentDistrict->GetTerritoryDisplayName()
			: OperationsView.DisplayName);
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
			NSLOCTEXT("TerritoryDistrictRow", "Summary", "{0}  |  {1}\nProperties {2}/{3}  |  Guards {4}/{5}/{6}  |  Net {7}\nProduction {8} active  |  {9} blocked"),
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
