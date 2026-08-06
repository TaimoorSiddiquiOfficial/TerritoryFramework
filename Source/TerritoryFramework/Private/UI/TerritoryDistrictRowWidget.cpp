#include "UI/TerritoryDistrictRowWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/VerticalBox.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "Widgets/NarrativeCommonTextBlock.h"

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
	}
	if (RemoveGuardButton)
	{
		RemoveGuardButton->OnClicked().AddUObject(this, &UTerritoryDistrictRowWidget::HandleRemoveGuard);
		RemoveGuardButton->SetButtonText(NSLOCTEXT("TerritoryDistrictRow", "RemoveGuard", "- GUARD"));
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

	UHorizontalBox* Root = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DistrictRowRoot"));
	WidgetTree->RootWidget = Root;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const TSubclassOf<UNarrativeCommonButtonBase> NarrativeButtonClass = Settings
		? Settings->DefaultNarrativeButtonClass.LoadSynchronous() : nullptr;
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
	Details->AddChild(DistrictName);
	Details->AddChild(DistrictSummary);
	Details->AddChild(DistrictStatus);
	Details->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (NarrativeButtonClass)
	{
		SelectDistrictButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(NarrativeButtonClass, TEXT("SelectDistrictButton"));
		SelectOverlay->AddChild(SelectDistrictButton);
	}
	SelectOverlay->AddChild(Details);
	Root->AddChild(SelectOverlay);

	if (NarrativeButtonClass)
	{
		AddGuardButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(NarrativeButtonClass, TEXT("AddGuardButton"));
		Root->AddChild(AddGuardButton);

		RemoveGuardButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(NarrativeButtonClass, TEXT("RemoveGuardButton"));
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
	ActionStatus = InView.bUnderAttack || InView.bAttackScheduled || InView.bThreatPreviewAvailable
		? InView.ThreatSummary
		: (!InView.bCanAddGuard && !InView.bCanRemoveGuard
			? (!InView.AddGuardFailureReason.IsEmpty()
				? InView.AddGuardFailureReason
				: InView.RemoveGuardFailureReason)
			: FText::GetEmpty());
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
			NSLOCTEXT("TerritoryDistrictRow", "Summary", "{0}  |  {1}  |  Properties {2}/{3}  |  Guards {4}/{5}/{6}  |  Net {7}  |  {8}"),
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(CurrentDistrict->GetOwningFaction()),
			StateText,
			FText::AsNumber(OperationsView.OwnedProperties),
			FText::AsNumber(OperationsView.TotalProperties),
			FText::AsNumber(Active),
			FText::AsNumber(Desired),
			FText::AsNumber(Maximum),
			FText::AsNumber(Net),
			UTerritoryUIBlueprintLibrary::GetThreatLevelText(OperationsView.ThreatLevel)));
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
