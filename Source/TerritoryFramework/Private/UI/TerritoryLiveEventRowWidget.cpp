#include "UI/TerritoryLiveEventRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "UI/TerritoryUITheme.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "Widgets/NarrativeCommonTextBlock.h"

namespace
{
	void SetLiveEventSurface(UBorder* Border, const FLinearColor& Fill,
		const FLinearColor& Outline)
	{
		TerritoryUITheme::ApplySurface(Border, Fill, Outline, 2.f);
	}

	void SetLiveEventText(UTextBlock* Text, int32 Size, const FLinearColor& Color)
	{
		TerritoryUITheme::ApplyText(Text, Size, Color);
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
	SetLiveEventText(StatusDot, TerritoryTypography::Body,
		FLinearColor(0.08f, 0.88f, 0.62f, 1.f));
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
		TerritoryUITheme::ApplyButton(WaypointButton);
		WaypointButton->SetMinDimensions(0, 0);
		WaypointButton->SetButtonText(NSLOCTEXT(
			"TerritoryLiveEvents", "SetWaypoint", "Track"));
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
		SetLiveEventText(HeadlineText, TerritoryTypography::Body,
			LiveEvent.bExpired ? Muted : FLinearColor(0.96f, 0.95f, 0.91f, 1.f));
	}
	if (DetailText)
	{
		DetailText->SetText(LiveEvent.Detail);
		DetailText->SetVisibility(LiveEvent.Detail.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		SetLiveEventText(DetailText, TerritoryTypography::Metadata,
			LiveEvent.bExpired ? Muted : FLinearColor(0.66f, 0.65f, 0.61f, 1.f));
	}
	if (ImpactText)
	{
		TArray<FString> Impacts;
		if (LiveEvent.IncomeDelta != 0)
		{
			Impacts.Add(FString::Printf(TEXT("Income %+lld / cycle"),
				static_cast<long long>(LiveEvent.IncomeDelta)));
		}
		if (LiveEvent.UpkeepDelta != 0)
		{
			Impacts.Add(FString::Printf(TEXT("Upkeep %+lld / cycle"),
				static_cast<long long>(LiveEvent.UpkeepDelta)));
		}
		if (LiveEvent.CurrencyDelta != 0)
		{
			Impacts.Add(FString::Printf(TEXT("Funds %+lld"),
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
		SetLiveEventText(ImpactText, TerritoryTypography::Caption,
			LiveEvent.bExpired ? Muted : Active);
	}
	if (WaypointButton)
	{
		const UTerritoryRegistrySubsystem* Registry = GetWorld()
			? GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
		ATerritoryVolume* Territory = Registry && LiveEvent.TerritoryTag.IsValid()
			? Registry->GetTerritoryByTag(LiveEvent.TerritoryTag) : nullptr;
		const bool bEnabled = LiveEvent.bCanSetWaypoint && Territory
			&& UTerritoryUIBlueprintLibrary::ResolveTerritoryWaypointTarget(
				GetOwningPlayer(), Territory) != nullptr;
		const bool bTracked = bEnabled
			&& UTerritoryUIBlueprintLibrary::IsTerritoryWaypointTracked(
				GetOwningPlayer(), Territory);
		WaypointButton->SetButtonText(bTracked
			? NSLOCTEXT("TerritoryLiveEvents", "WaypointTracked", "Tracked  ✓")
			: NSLOCTEXT("TerritoryLiveEvents", "SetWaypoint", "Track"));
		TerritoryUITheme::SetTabSelected(WaypointButton, bTracked);
		WaypointButton->SetIsEnabled(bEnabled);
		WaypointButton->SetVisibility(bEnabled
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
