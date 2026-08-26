#include "UI/TerritoryLiveEventRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "Widgets/NarrativeCommonTextBlock.h"

namespace
{
	void SetLiveEventSurface(UBorder* Border, const FLinearColor& Fill,
		const FLinearColor& Outline)
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
			2.f, FSlateColor(Outline), 1.f);
		Border->SetBrush(Brush);
	}

	void SetLiveEventText(UTextBlock* Text, int32 Size, const FLinearColor& Color)
	{
		if (!Text) return;
		if (UNarrativeCommonTextBlock* NarrativeText =
			Cast<UNarrativeCommonTextBlock>(Text))
		{
			const UTerritoryDeveloperSettings* Settings =
				GetDefault<UTerritoryDeveloperSettings>();
			const TSubclassOf<UCommonTextStyle> Style = Settings
				? Settings->DefaultTerritoryTextStyle.LoadSynchronous() : nullptr;
			if (Style)
			{
				NarrativeText->SetStyle(Style);
			}
		}
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
	}
}

void UTerritoryLiveEventRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// A pure native UUserWidget must have its WidgetTree before RebuildWidget().
	// Constructing the root later in NativeConstruct leaves the Slate wrapper
	// bound to SNullWidget, which produces a valid event count but a 0 x 0 row.
	BuildNativeLayout();
}

void UTerritoryLiveEventRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildNativeLayout();
	if (WaypointButton)
	{
		WaypointButton->OnClicked().AddUObject(
			this, &UTerritoryLiveEventRowWidget::HandleWaypointClicked);
	}
	RefreshEvent();
}

void UTerritoryLiveEventRowWidget::NativeDestruct()
{
	if (WaypointButton) WaypointButton->OnClicked().RemoveAll(this);
	Super::NativeDestruct();
}

void UTerritoryLiveEventRowWidget::InitializeLiveEvent(
	const FTerritoryLiveEvent& InEvent)
{
	LiveEvent = InEvent;
	RefreshEvent();
}

void UTerritoryLiveEventRowWidget::BuildNativeLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;

	EventSurface = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("LiveEventSurface"));
	EventSurface->SetPadding(FMargin(7.f, 5.f));
	WidgetTree->RootWidget = EventSurface;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("LiveEventRow"));
	EventSurface->SetContent(Row);

	StatusDot = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("LiveEventStatusDot"));
	StatusDot->SetText(FText::FromString(TEXT("●")));
	SetLiveEventText(StatusDot, 12, FLinearColor(0.08f, 0.88f, 0.62f, 1.f));
	if (UHorizontalBoxSlot* DotSlot = Row->AddChildToHorizontalBox(StatusDot))
	{
		DotSlot->SetPadding(FMargin(0.f, 1.f, 9.f, 0.f));
		DotSlot->SetVerticalAlignment(VAlign_Top);
	}

	UVerticalBox* Copy = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("LiveEventCopy"));
	HeadlineText = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("LiveEventHeadline"));
	DetailText = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("LiveEventDetail"));
	ImpactText = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("LiveEventImpact"));
	Copy->AddChildToVerticalBox(HeadlineText);
	Copy->AddChildToVerticalBox(DetailText);
	Copy->AddChildToVerticalBox(ImpactText);
	if (UHorizontalBoxSlot* CopySlot = Row->AddChildToHorizontalBox(Copy))
	{
		CopySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CopySlot->SetVerticalAlignment(VAlign_Center);
	}

	const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
	const TSubclassOf<UNarrativeCommonButtonBase> ButtonClass = Settings
		? Settings->DefaultNarrativeButtonClass.LoadSynchronous() : nullptr;
	if (ButtonClass)
	{
		USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("LiveEventWaypointSize"));
		ButtonSize->SetWidthOverride(132.f);
		ButtonSize->SetHeightOverride(42.f);
		WaypointButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
			ButtonClass, TEXT("LiveEventWaypointButton"));
		if (const TSubclassOf<UCommonButtonStyle> Style =
			Settings->DefaultTerritoryButtonStyle.LoadSynchronous())
		{
			WaypointButton->SetStyle(Style);
		}
		WaypointButton->SetMinDimensions(0, 0);
		WaypointButton->SetButtonText(NSLOCTEXT(
			"TerritoryLiveEvents", "SetWaypoint", "TRACK"));
		ButtonSize->SetContent(WaypointButton);
		if (UHorizontalBoxSlot* ButtonSlot = Row->AddChildToHorizontalBox(ButtonSize))
		{
			ButtonSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
			ButtonSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
}

void UTerritoryLiveEventRowWidget::RefreshEvent()
{
	FLinearColor Active = FLinearColor(0.20f, 0.72f, 0.95f, 1.f);
	switch (LiveEvent.Severity)
	{
	case ETerritoryIntelligenceSeverity::Positive:
		Active = FLinearColor(0.08f, 0.88f, 0.62f, 1.f);
		break;
	case ETerritoryIntelligenceSeverity::Warning:
		Active = FLinearColor(1.f, 0.72f, 0.16f, 1.f);
		break;
	case ETerritoryIntelligenceSeverity::Critical:
		Active = FLinearColor(0.96f, 0.22f, 0.18f, 1.f);
		break;
	default:
		break;
	}
	const FLinearColor Muted = FLinearColor(0.38f, 0.43f, 0.42f, 0.72f);
	const FLinearColor Accent = LiveEvent.bExpired ? Muted : Active;
	SetLiveEventSurface(EventSurface,
		LiveEvent.bExpired
			? FLinearColor(0.022f, 0.022f, 0.022f, 0.72f)
			: FLinearColor(0.014f, 0.014f, 0.014f, 0.97f),
		FLinearColor(Accent.R, Accent.G, Accent.B,
			LiveEvent.bExpired ? 0.18f : 0.36f));
	if (StatusDot)
	{
		StatusDot->SetText(FText::FromString(LiveEvent.bExpired ? TEXT("○") : TEXT("●")));
		StatusDot->SetColorAndOpacity(FSlateColor(Accent));
	}
	if (HeadlineText)
	{
		HeadlineText->SetText(LiveEvent.Headline);
		SetLiveEventText(HeadlineText, 13,
			LiveEvent.bExpired ? Muted : FLinearColor(0.96f, 0.95f, 0.91f, 1.f));
	}
	if (DetailText)
	{
		DetailText->SetText(LiveEvent.Detail);
		DetailText->SetVisibility(LiveEvent.Detail.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		SetLiveEventText(DetailText, 10,
			LiveEvent.bExpired ? Muted : FLinearColor(0.66f, 0.65f, 0.61f, 1.f));
	}
	if (ImpactText)
	{
		TArray<FString> Impacts;
		if (LiveEvent.IncomeDelta != 0)
		{
			Impacts.Add(FString::Printf(TEXT("INCOME %+lld / CYCLE"),
				static_cast<long long>(LiveEvent.IncomeDelta)));
		}
		if (LiveEvent.UpkeepDelta != 0)
		{
			Impacts.Add(FString::Printf(TEXT("UPKEEP %+lld / CYCLE"),
				static_cast<long long>(LiveEvent.UpkeepDelta)));
		}
		if (LiveEvent.CurrencyDelta != 0)
		{
			Impacts.Add(FString::Printf(TEXT("FUNDS %+lld"),
				static_cast<long long>(LiveEvent.CurrencyDelta)));
		}
		TArray<FGameplayTag> CapabilityTags;
		LiveEvent.CommandCapabilities.GetGameplayTagArray(CapabilityTags);
		for (const FGameplayTag& Capability : CapabilityTags)
		{
			Impacts.Add(Capability.ToString());
		}
		ImpactText->SetText(FText::FromString(FString::Join(Impacts, TEXT("  •  "))));
		ImpactText->SetVisibility(Impacts.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		SetLiveEventText(ImpactText, 9, LiveEvent.bExpired ? Muted : Active);
	}
	if (WaypointButton)
	{
		const bool bEnabled = LiveEvent.bCanSetWaypoint
			&& LiveEvent.TerritoryTag.IsValid();
		WaypointButton->SetIsEnabled(bEnabled);
		WaypointButton->SetVisibility(LiveEvent.bCanSetWaypoint
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UTerritoryLiveEventRowWidget::HandleWaypointClicked()
{
	if (LiveEvent.bCanSetWaypoint && LiveEvent.TerritoryTag.IsValid())
	{
		OnWaypointRequested.Broadcast(LiveEvent.TerritoryTag);
	}
}
