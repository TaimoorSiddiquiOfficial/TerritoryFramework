#include "UI/TerritoryHUDWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Cinematics/TerritoryCinematicPresentationSubsystem.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryInterfaces.h"
#include "Core/TerritoryVolume.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Engine/World.h"
#include "NarrativeGameplayTags.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "UI/TerritoryUITheme.h"

void UTerritoryHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TerritoryUITheme::ApplySurface(CaptureSurface,
		FLinearColor(0.025f, 0.04f, 0.055f, 0.94f),
		FLinearColor(0.18f, 0.52f, 0.48f, 0.5f), 5.f);
	TerritoryUITheme::ApplyProgress(ProgressBar_Capture, false);
	TerritoryUITheme::ApplyText(Text_DistrictName, TerritoryTypography::CardTitle,
		FLinearColor(0.94f, 0.93f, 0.89f, 1.f),
		ETerritoryTextRole::Heading, false);
	TerritoryUITheme::ApplyText(DistrictOwnerText, TerritoryTypography::Caption,
		FLinearColor(0.64f, 0.68f, 0.67f, 1.f),
		ETerritoryTextRole::Muted, false);
	TerritoryUITheme::ApplyText(Text_CaptureState, TerritoryTypography::Metadata,
		FLinearColor(0.92f, 0.70f, 0.24f, 1.f),
		ETerritoryTextRole::Heading, false);
	TerritoryUITheme::ApplyText(DistrictDescriptionText, TerritoryTypography::Metadata,
		FLinearColor(0.94f, 0.93f, 0.89f, 1.f));
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		CinematicPresentation =
			UTerritoryCinematicPresentationSubsystem::GetForPlayerController(
				PlayerController);
		if (CinematicPresentation.IsValid())
		{
			CinematicPresentation->OnPresentationChanged.AddUniqueDynamic(
				this, &UTerritoryHUDWidget::HandleCinematicPresentationChanged);
		}
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
	// Apply Narrative suppression immediately even if no pawn/territory is bound yet.
	RefreshTerritoryDisplay();
}

void UTerritoryHUDWidget::NativeDestruct()
{
	if (CinematicPresentation.IsValid())
	{
		CinematicPresentation->OnPresentationChanged.RemoveDynamic(
			this, &UTerritoryHUDWidget::HandleCinematicPresentationChanged);
	}
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
	if (IsNarrativePresentationBlockingHUD())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Always re-query territory at player location — handles player moving between territories
	BindToTerritoryAtPlayer();

	ATerritoryVolume* Territory = GetBoundTerritory();
	if (!Territory)
	{
		Super::RefreshTerritoryDisplay();
		if (bCollapseWhenOutsideTerritory)
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	if (!Territory->ShouldShowGameplayHUD())
	{
		// This is a Definition presentation policy, not story availability. Keep
		// the Territory fully playable and visible to POI/menu systems while only
		// removing this passive on-screen card.
		SetVisibility(ESlateVisibility::Collapsed);
		bHasObservedState = false;
		LastObservedContestingFaction = FGameplayTag();
		ActiveCounterAttackAlert = FText::GetEmpty();
		CounterAttackAlertExpiresAtRealTime = 0.0;
		return;
	}

	Super::RefreshTerritoryDisplay();
	if (bCollapseWhenOutsideTerritory && GetVisibility() == ESlateVisibility::Collapsed)
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	const ETerritoryState State = Territory->GetTerritoryState();
	const FText StateText = UTerritoryUIBlueprintLibrary::GetTerritoryStatusText(
		Territory->GetTerritoryAvailability(), State);
	const FLinearColor StateAccent = Territory->IsLocked()
		? FLinearColor(0.42f, 0.47f, 0.52f, 1.f)
		: State == ETerritoryState::Contested
		? FLinearColor(1.f, 0.25f, 0.16f, 1.f)
		: State == ETerritoryState::Claimed
			? FLinearColor(0.08f, 0.88f, 0.62f, 1.f)
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
			NSLOCTEXT("TerritoryHUD", "AttackAlert", "Attack alert: {0} is under attack by {1}."),
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

bool UTerritoryHUDWidget::IsNarrativePresentationBlockingHUD() const
{
	const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
	const UTerritoryCinematicPresentationSubsystem* Presentation =
		CinematicPresentation.IsValid() ? CinematicPresentation.Get()
		: UTerritoryCinematicPresentationSubsystem::GetForPlayerController(
			GetOwningPlayer());
	if (Settings && Settings->bHideTerritoryHUDDuringNarrativeDialogue
		&& Presentation && Presentation->IsNarrativeCinematicActive())
	{
		return true;
	}

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

void UTerritoryHUDWidget::HandleCinematicPresentationChanged(bool bIsActive)
{
	(void)bIsActive;
	RefreshTerritoryDisplay();
}

void UTerritoryHUDWidget::HandleAssaultNotification(const FTerritoryAssaultRecord& Assault)
{
	PresentCounterAttackAlert(FText::Format(
		NSLOCTEXT("TerritoryHUD", "CounterAttackWarning", "Counterattack: {0} is moving on {1}."),
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
