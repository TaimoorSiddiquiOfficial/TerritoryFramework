#include "UI/TerritoryProductionWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "UI/TerritoryUITheme.h"
#include "Widgets/NarrativeCommonTextBlock.h"

void UTerritoryResourceRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildNativeLayout();
	TerritoryUITheme::ApplyText(ResourceName, TerritoryTypography::Body,
		FLinearColor(0.94f, 0.93f, 0.89f, 1.f),
		ETerritoryTextRole::Heading, false);
	TerritoryUITheme::ApplyText(StoredQuantityText, TerritoryTypography::Metadata,
		FLinearColor(0.64f, 0.68f, 0.67f, 1.f),
		ETerritoryTextRole::Muted, false);
	TerritoryUITheme::ApplyText(ResourceFlowText, TerritoryTypography::Metadata,
		FLinearColor(0.70f, 0.86f, 0.74f, 1.f),
		ETerritoryTextRole::Body, false);
	RefreshResourceDisplay();
}

void UTerritoryResourceRowWidget::InitializeResourceView(
	const FTerritoryResourceOperationsView& InView)
{
	ResourceView = InView;
	RefreshResourceDisplay();
	OnResourceViewChanged(ResourceView);
}

void UTerritoryResourceRowWidget::BuildNativeLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("ResourceRowSize"));
	RootSize->SetMinDesiredHeight(40.f);
	WidgetTree->RootWidget = RootSize;
	UHorizontalBox* Root = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("ResourceRowRoot"));
	RootSize->SetContent(Root);

	USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("ResourceIconSize"));
	IconSize->SetWidthOverride(32.f);
	IconSize->SetHeightOverride(32.f);
	ResourceIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ResourceIcon"));
	IconSize->SetContent(ResourceIcon);
	Root->AddChild(IconSize);

	ResourceName = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("ResourceName"));
	if (UHorizontalBoxSlot* NameSlot = Root->AddChildToHorizontalBox(ResourceName))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetPadding(FMargin(8.f, 0.f));
	}
	StoredQuantityText = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("StoredQuantityText"));
	Root->AddChild(StoredQuantityText);
	ResourceFlowText = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("ResourceFlowText"));
	if (UHorizontalBoxSlot* FlowSlot = Root->AddChildToHorizontalBox(ResourceFlowText))
	{
		FlowSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	}
}

void UTerritoryResourceRowWidget::RefreshResourceDisplay()
{
	if (ResourceIcon)
	{
		ResourceIcon->SetBrushFromTexture(ResourceView.Thumbnail.LoadSynchronous(), true);
	}
	if (ResourceName) ResourceName->SetText(ResourceView.DisplayName);
	if (StoredQuantityText)
	{
		StoredQuantityText->SetText(FText::Format(
			NSLOCTEXT("TerritoryProductionUI", "StoredQuantity", "Stored {0}"),
			FText::AsNumber(ResourceView.StoredQuantity)));
	}
	if (ResourceFlowText)
	{
		ResourceFlowText->SetText(FText::Format(
			NSLOCTEXT("TerritoryProductionUI", "ResourceFlow", "In {0}  Out {1}  Net {2}"),
			FText::AsNumber(ResourceView.InputPerCycle),
			FText::AsNumber(ResourceView.OutputPerCycle),
			FText::AsNumber(ResourceView.NetPerCycle)));
		ResourceFlowText->SetColorAndOpacity(ResourceView.bSufficientForNextCycle
			? FSlateColor(FLinearColor(0.70f, 0.86f, 0.74f))
			: FSlateColor(FLinearColor(0.95f, 0.48f, 0.36f)));
	}
}

void UTerritoryProductionSiteRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildNativeLayout();
	TerritoryUITheme::ApplySurface(
		Cast<UBorder>(WidgetTree
			? WidgetTree->FindWidget(TEXT("ProductionSiteSurface")) : nullptr),
		FLinearColor(0.035f, 0.065f, 0.075f, 0.94f),
		FLinearColor(0.18f, 0.52f, 0.48f, 0.35f), 4.f);
	TerritoryUITheme::ApplyText(ProductionSiteName, TerritoryTypography::CardTitle,
		FLinearColor(0.94f, 0.93f, 0.89f, 1.f),
		ETerritoryTextRole::Heading, false);
	TerritoryUITheme::ApplyText(ProductionStatusText, TerritoryTypography::Metadata,
		FLinearColor(0.70f, 0.86f, 0.74f, 1.f),
		ETerritoryTextRole::Heading, false);
	TerritoryUITheme::ApplyText(ProductionReasonText, TerritoryTypography::Metadata,
		FLinearColor(0.64f, 0.68f, 0.67f, 1.f),
		ETerritoryTextRole::Muted);
	RefreshProductionDisplay();
}

void UTerritoryProductionSiteRowWidget::InitializeProductionSiteView(
	const FTerritoryProductionSiteOperationsView& InView)
{
	ProductionSiteView = InView;
	RefreshProductionDisplay();
	OnProductionSiteViewChanged(ProductionSiteView);
}

void UTerritoryProductionSiteRowWidget::BuildNativeLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;

	UBorder* Surface = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("ProductionSiteSurface"));
	Surface->SetPadding(FMargin(10.f, 8.f));
	Surface->SetBrushColor(FLinearColor(0.035f, 0.065f, 0.075f, 0.96f));
	WidgetTree->RootWidget = Surface;
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("ProductionSiteRoot"));
	Surface->SetContent(Root);

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("ProductionSiteHeader"));
	Root->AddChild(Header);
	ProductionSiteName = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("ProductionSiteName"));
	if (UHorizontalBoxSlot* NameSlot = Header->AddChildToHorizontalBox(ProductionSiteName))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	ProductionStatusText = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("ProductionStatusText"));
	Header->AddChild(ProductionStatusText);
	ProductionReasonText = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("ProductionReasonText"));
	Root->AddChild(ProductionReasonText);
	ResourceRows = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("ResourceRows"));
	if (UVerticalBoxSlot* ResourceSlot = Root->AddChildToVerticalBox(ResourceRows))
	{
		ResourceSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}
}

void UTerritoryProductionSiteRowWidget::RefreshProductionDisplay()
{
	if (ProductionSiteName) ProductionSiteName->SetText(ProductionSiteView.DisplayName);
	if (ProductionStatusText)
	{
		ProductionStatusText->SetText(
			UTerritoryUIBlueprintLibrary::GetProductionStatusText(ProductionSiteView.Status));
		ProductionStatusText->SetColorAndOpacity(ProductionSiteView.bBlocked
			? FSlateColor(FLinearColor(0.95f, 0.48f, 0.36f))
			: FSlateColor(FLinearColor(0.70f, 0.86f, 0.74f)));
	}
	if (ProductionReasonText)
	{
		ProductionReasonText->SetText(ProductionSiteView.StatusReason);
		ProductionReasonText->SetVisibility(ProductionSiteView.StatusReason.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (!ResourceRows) return;

	ResourceRows->ClearChildren();
	TSubclassOf<UTerritoryResourceRowWidget> RowClass = ResourceRowClass;
	if (!RowClass) RowClass = UTerritoryResourceRowWidget::StaticClass();
	for (const FTerritoryResourceOperationsView& Resource : ProductionSiteView.Resources)
	{
		if (UTerritoryResourceRowWidget* Row = CreateWidget<UTerritoryResourceRowWidget>(this, RowClass))
		{
			Row->InitializeResourceView(Resource);
			ResourceRows->AddChild(Row);
		}
	}
}
