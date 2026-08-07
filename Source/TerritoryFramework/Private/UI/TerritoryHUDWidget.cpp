#include "UI/TerritoryHUDWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryInterfaces.h"
#include "Core/TerritoryVolume.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "Engine/World.h"

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
		}
	}
	BindToTerritoryAtPlayer();
}

void UTerritoryHUDWidget::NativeDestruct()
{
	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnAssaultNotification.RemoveDynamic(
			this, &UTerritoryHUDWidget::HandleAssaultNotification);
	}
	Super::NativeDestruct();
}

void UTerritoryHUDWidget::RefreshTerritoryDisplay()
{
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
	}
	if (ProgressBar_Capture)
	{
		ProgressBar_Capture->SetPercent(Territory->GetControlProgress());
	}
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

	if (DistrictDescriptionText && !ActiveCounterAttackAlert.IsEmpty())
	{
		const UWorld* World = GetWorld();
		if (World && World->GetRealTimeSeconds() < CounterAttackAlertExpiresAtRealTime)
		{
			DistrictDescriptionText->SetText(ActiveCounterAttackAlert);
		}
		else
		{
			ActiveCounterAttackAlert = FText::GetEmpty();
			CounterAttackAlertExpiresAtRealTime = 0.0;
		}
	}

	bHasObservedState = true;
	LastObservedState = State;
	LastObservedContestingFaction = ContestingFaction;
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

void UTerritoryHUDWidget::PresentCounterAttackAlert(const FText& AlertText, float Duration)
{
	ActiveCounterAttackAlert = AlertText;
	const UWorld* World = GetWorld();
	CounterAttackAlertExpiresAtRealTime = World
		? World->GetRealTimeSeconds() + FMath::Max(0.f, Duration) : 0.0;
	OnCounterAttackAlert(AlertText, Duration);
}
