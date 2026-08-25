#include "UI/TerritoryHUDWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryInterfaces.h"
#include "Core/TerritoryVolume.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "Engine/World.h"
#include "NarrativeGameplayTags.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Widgets/NarrativeGameplayHUD.h"

void UTerritoryHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
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
	if (DistrictDescriptionText)
	{
		FTerritoryDistrictOperationsView View;
		if (ATerritoryDistrict* District = Cast<ATerritoryDistrict>(Territory);
			District && UTerritoryUIBlueprintLibrary::BuildDistrictOperationsView(
				this, District, GetOwningPlayer(), View))
		{
			DistrictDescriptionText->SetText(FText::Format(
				NSLOCTEXT("TerritoryHUD", "DistrictOperationsSummary", "Active {0} / Assigned {1} / Max {2}   Net {3}   {4}"),
				FText::AsNumber(View.ActiveGuards), FText::AsNumber(View.DesiredGuards),
				FText::AsNumber(View.MaximumGuards), FText::AsNumber(View.NetIncome),
				View.ThreatSummary));
		}
		else
		{
			const int64 GuardUpkeep = static_cast<int64>(Territory->GetGuardCost()) * Territory->GetDesiredGuardCount();
			DistrictDescriptionText->SetText(FText::Format(
				NSLOCTEXT("TerritoryHUD", "TerritorySummary", "Guards {0}/{1}   Income {2}   Upkeep {3}"),
				FText::AsNumber(Territory->GetDesiredGuardCount()),
				FText::AsNumber(Territory->GetMaxGuardCount()),
				FText::AsNumber(Territory->GetPeriodicIncome()),
				FText::AsNumber(GuardUpkeep)));
		}
	}

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
			// The passive card stays compact, but temporarily grows enough to show
			// the counterattack sentence without clipping or covering the screen.
			CardSlot->SetSize(FVector2D(400.f, bAlertVisible ? 230.f : 190.f));
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
		NSLOCTEXT("TerritoryHUD", "CounterAttackWarning", "COUNTERATTACK: {0} is sending {1} attackers to {2}. Reinforce immediately."),
		UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(Assault.AttackingFaction),
		FText::AsNumber(Assault.PlannedForce),
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
