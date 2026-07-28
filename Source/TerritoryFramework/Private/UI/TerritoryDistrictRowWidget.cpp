#include "UI/TerritoryDistrictRowWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/VerticalBox.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "Widgets/NarrativeCommonTextBlock.h"

void UTerritoryDistrictRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildNativeLayout();
	RefreshRow();
}

void UTerritoryDistrictRowWidget::BuildNativeLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UHorizontalBox* Root = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DistrictRowRoot"));
	WidgetTree->RootWidget = Root;

	const TSubclassOf<UNarrativeCommonButtonBase> NarrativeButtonClass = LoadClass<UNarrativeCommonButtonBase>(
		nullptr,
		TEXT("/NarrativePro/Pro/Core/UI/Widgets/Base/WBP_NarrativeButton_Text.WBP_NarrativeButton_Text_C"));
	if (!NarrativeButtonClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Territory district row could not load Narrative button class."));
		return;
	}

	SelectButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(NarrativeButtonClass, TEXT("SelectDistrictButton"));
	UOverlay* SelectOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SelectDistrictOverlay"));
	UVerticalBox* Details = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DistrictDetails"));
	NameText = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(UNarrativeCommonTextBlock::StaticClass(), TEXT("DistrictName"));
	SummaryText = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(UNarrativeCommonTextBlock::StaticClass(), TEXT("DistrictSummary"));
	StatusText = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(UNarrativeCommonTextBlock::StaticClass(), TEXT("DistrictStatus"));
	Details->AddChild(NameText);
	Details->AddChild(SummaryText);
	Details->AddChild(StatusText);
	Details->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SelectOverlay->AddChild(SelectButton);
	SelectOverlay->AddChild(Details);
	Root->AddChild(SelectOverlay);

	AddGuardButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(NarrativeButtonClass, TEXT("AddGuardButton"));
	AddGuardButton->SetButtonText(NSLOCTEXT("TerritoryDistrictRow", "AddGuard", "+ GUARD"));
	Root->AddChild(AddGuardButton);

	RemoveGuardButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(NarrativeButtonClass, TEXT("RemoveGuardButton"));
	RemoveGuardButton->SetButtonText(NSLOCTEXT("TerritoryDistrictRow", "RemoveGuard", "- GUARD"));
	Root->AddChild(RemoveGuardButton);

	SelectButton->OnClicked().AddUObject(this, &UTerritoryDistrictRowWidget::HandleSelected);
	AddGuardButton->OnClicked().AddUObject(this, &UTerritoryDistrictRowWidget::HandleAddGuard);
	RemoveGuardButton->OnClicked().AddUObject(this, &UTerritoryDistrictRowWidget::HandleRemoveGuard);
}

void UTerritoryDistrictRowWidget::InitializeDistrict(ATerritoryDistrict* InDistrict)
{
	District = InDistrict;
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
	if (StatusText)
	{
		StatusText->SetText(ActionStatus);
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
	if (NameText)
	{
		NameText->SetText(CurrentDistrict->GetTerritoryDisplayName());
	}
	if (SummaryText)
	{
		SummaryText->SetText(FText::Format(
			NSLOCTEXT("TerritoryDistrictRow", "Summary", "{0}  |  {1}  |  Guards {2}/{3}  |  Net {4}"),
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(CurrentDistrict->GetOwningFaction()),
			StateText,
			FText::AsNumber(CurrentDistrict->GetDesiredGuardCount()),
			FText::AsNumber(CurrentDistrict->GetMaxGuardCount()),
			FText::AsNumber(CurrentDistrict->GetEffectiveIncome()
				- (static_cast<int64>(CurrentDistrict->GetGuardCost()) * CurrentDistrict->GetDesiredGuardCount()))));
	}
	if (AddGuardButton)
	{
		AddGuardButton->SetIsEnabled(bCanAddGuard);
	}
	if (RemoveGuardButton)
	{
		RemoveGuardButton->SetIsEnabled(bCanRemoveGuard);
	}
	if (StatusText)
	{
		StatusText->SetText(ActionStatus);
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
