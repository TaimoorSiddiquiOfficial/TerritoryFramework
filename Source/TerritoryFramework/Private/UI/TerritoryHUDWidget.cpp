#include "UI/TerritoryHUDWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryInterfaces.h"
#include "Core/TerritoryVolume.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "NarrativeGameplayTags.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "Styling/SlateBrush.h"

namespace
{
	void ApplyTerritoryHUDTextures(UBorder* Surface, UProgressBar* ProgressBar)
	{
		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
		if (!Settings) return;
		if (Surface)
		{
			if (UTexture2D* PanelTexture =
				Settings->TerritoryPanelTexture.LoadSynchronous())
			{
				FSlateBrush Panel;
				Panel.DrawAs = ESlateBrushDrawType::Box;
				Panel.SetResourceObject(PanelTexture);
				Panel.ImageSize = FVector2D(
					PanelTexture->GetSizeX(), PanelTexture->GetSizeY());
				Panel.Margin = FMargin(0.035f, 0.22f);
				Panel.TintColor = FSlateColor(FLinearColor::White);
				Surface->SetBrush(Panel);
			}
		}
		if (ProgressBar)
		{
			if (UTexture2D* FrameTexture =
				Settings->TerritoryProgressFrameTexture.LoadSynchronous())
			{
				FProgressBarStyle Style = ProgressBar->GetWidgetStyle();
				FSlateBrush Frame;
				Frame.DrawAs = ESlateBrushDrawType::Box;
				Frame.SetResourceObject(FrameTexture);
				Frame.ImageSize = FVector2D(
					FrameTexture->GetSizeX(), FrameTexture->GetSizeY());
				Frame.Margin = FMargin(0.04f, 0.28f);
				Frame.TintColor = FSlateColor(FLinearColor::White);
				Style.BackgroundImage = Frame;
				ProgressBar->SetWidgetStyle(Style);
			}
		}
	}
}

void UTerritoryHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyTerritoryHUDTextures(CaptureSurface, ProgressBar_Capture);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		ManagementComponent = UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(PlayerController);
		if (ManagementComponent.IsValid())
		{
			ManagementComponent->OnAssaultNotification.AddUniqueDynamic(
				this, &UTerritoryHUDWidget::HandleAssaultNotification);
			ManagementComponent->OnCounterHappened.AddUniqueDynamic(
				this, &UTerritoryHUDWidget::HandleCounterHappened);
		}
	}
	BindToTerritoryAtPlayer();
	if (CaptureSignalIn)
	{
		PlayAnimation(CaptureSignalIn);
	}
}

void UTerritoryHUDWidget::NativeDestruct()
{
	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnAssaultNotification.RemoveDynamic(
			this, &UTerritoryHUDWidget::HandleAssaultNotification);
		ManagementComponent->OnCounterHappened.RemoveDynamic(
			this, &UTerritoryHUDWidget::HandleCounterHappened);
	}
	Super::NativeDestruct();
}

void UTerritoryHUDWidget::RefreshTerritoryDisplay()
{
	// The capture card is passive gameplay information. Narrative Menu and Modal
	// layers own full-screen interaction, so they always suppress it.
	if (IsNarrativeMenuBlockingHUD())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Always re-query territory at player location — handles player moving between territories
	BindToTerritoryAtPlayer();

	Super::RefreshTerritoryDisplay();

	ATerritoryVolume* Territory = GetBoundTerritory();
	if (!Territory)
	{
		if (bCollapseWhenOutsideTerritory)
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	if (bCollapseWhenOutsideTerritory && GetVisibility() == ESlateVisibility::Collapsed)
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	const ETerritoryState State = Territory->GetTerritoryState();
	const UEnum* StateEnum = StaticEnum<ETerritoryState>();
	const FText StateText = StateEnum
		? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(State))
		: FText::GetEmpty();
	const FLinearColor StateAccent = State == ETerritoryState::Contested
		? FLinearColor(1.f, 0.25f, 0.16f, 1.f)
		: State == ETerritoryState::Claimed
			? FLinearColor(0.08f, 0.88f, 0.62f, 1.f)
			: State == ETerritoryState::Locked
				? FLinearColor(0.42f, 0.47f, 0.52f, 1.f)
				: FLinearColor(1.f, 0.84f, 0.f, 1.f);

	if (Text_DistrictName)
	{
		Text_DistrictName->SetText(Territory->GetTerritoryDisplayName());
	}
	if (DistrictOwnerText)
	{
		DistrictOwnerText->SetText(UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(Territory->GetOwningFaction()));
	}
	if (Text_CaptureState)
	{
		Text_CaptureState->SetText(StateText);
		Text_CaptureState->SetColorAndOpacity(FSlateColor(StateAccent));
	}
	if (ProgressBar_Capture)
	{
		ProgressBar_Capture->SetPercent(Territory->GetControlProgress());
		ProgressBar_Capture->SetFillColorAndOpacity(StateAccent);
	}
	if (CaptureAccentRail) CaptureAccentRail->SetBrushColor(StateAccent);
	if (CaptureDivider) CaptureDivider->SetBrushColor(StateAccent);
	const FGameplayTag ContestingFaction =
		Territory->GetOwnershipData().ContestingFaction;
	if (bHasObservedState
		&& State == ETerritoryState::Contested
		&& (LastObservedState != ETerritoryState::Contested
			|| LastObservedContestingFaction != ContestingFaction))
	{
		const FText DistrictName = Territory->GetTerritoryDisplayName();
		PresentCounterAttackAlert(FText::Format(
			NSLOCTEXT("TerritoryHUD", "AttackAlert", "ATTACK ALERT: {0} is under attack by {1}."),
			DistrictName,
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(ContestingFaction)), 6.f);
	}

	bool bAlertVisible = false;
	if (DistrictDescriptionText && !ActiveCounterAttackAlert.IsEmpty())
	{
		const UWorld* World = GetWorld();
		if (World && World->GetRealTimeSeconds() < CounterAttackAlertExpiresAtRealTime)
		{
			DistrictDescriptionText->SetText(ActiveCounterAttackAlert);
			DistrictDescriptionText->SetColorAndOpacity(FSlateColor(
				FLinearColor(1.f, 0.58f, 0.32f, 1.f)));
			bAlertVisible = true;
		}
		else
		{
			ActiveCounterAttackAlert = FText::GetEmpty();
			CounterAttackAlertExpiresAtRealTime = 0.0;
		}
	}
	if (DistrictDescriptionText)
	{
		DistrictDescriptionText->SetVisibility(bAlertVisible
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (CaptureSurface)
	{
		if (UCanvasPanelSlot* CardSlot = Cast<UCanvasPanelSlot>(CaptureSurface->Slot))
		{
			// HUD owns immediate location/capture feedback only. Strategic guard,
			// finance, and intelligence details stay in the Command Center.
			CardSlot->SetSize(FVector2D(360.f, bAlertVisible ? 162.f : 124.f));
		}
	}

	bHasObservedState = true;
	LastObservedState = State;
	LastObservedContestingFaction = ContestingFaction;
}

bool UTerritoryHUDWidget::IsNarrativeMenuBlockingHUD() const
{
	const ANarrativePlayerController* NarrativeController =
		Cast<ANarrativePlayerController>(GetOwningPlayer());
	UNarrativeGameplayHUD* HUD = NarrativeController
		? NarrativeController->GetNarrativeGameplayHUD() : nullptr;
	if (!HUD) return false;
	const FNarrativeGameplayTags& Tags = FNarrativeGameplayTags::Get();
	for (const FGameplayTag LayerTag : { Tags.UI_Layer_Menu, Tags.UI_Layer_Modal })
	{
		const UCommonActivatableWidgetContainerBase* Layer =
			HUD->GetLayerContainer(LayerTag);
		if (Layer && IsValid(Layer->GetActiveWidget())) return true;
	}
	return false;
}

void UTerritoryHUDWidget::HandleAssaultNotification(const FTerritoryAssaultRecord& Assault)
{
	PresentCounterAttackAlert(FText::Format(
		NSLOCTEXT("TerritoryHUD", "CounterAttackWarning", "COUNTERATTACK: {0} is moving on {1}."),
		UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(Assault.AttackingFaction),
		UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(Assault.TargetTerritory)), 8.f);
	RefreshTerritoryDisplay();
}

void UTerritoryHUDWidget::HandleCounterHappened(
	const FTerritoryCounterAttackStateEvent& Event)
{
	OnCounterHappened(Event);
	RefreshTerritoryDisplay();
}

void UTerritoryHUDWidget::PresentCounterAttackAlert(const FText& AlertText, float Duration)
{
	ActiveCounterAttackAlert = AlertText;
	const UWorld* World = GetWorld();
	CounterAttackAlertExpiresAtRealTime = World
		? World->GetRealTimeSeconds() + FMath::Max(0.f, Duration) : 0.0;
	if (CaptureSignalIn)
	{
		PlayAnimation(CaptureSignalIn, 0.f, 1,
			EUMGSequencePlayMode::Forward, 1.35f, false);
	}
	OnCounterAttackAlert(AlertText, Duration);
}
