#include "UI/TerritoryHUDWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryInterfaces.h"
#include "Core/TerritoryVolume.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "UnrealFramework/NarrativePlayerController.h"
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
		ITerritoryOwnershipInterface::Execute_GetContestingFaction(Territory);
	if (bHasObservedState
		&& State == ETerritoryState::Contested
		&& (LastObservedState != ETerritoryState::Contested
			|| LastObservedContestingFaction != ContestingFaction))
	{
		if (ANarrativePlayerController* PlayerController = Cast<ANarrativePlayerController>(GetOwningPlayer()))
		{
			if (UNarrativeGameplayHUD* HUD = PlayerController->GetNarrativeGameplayHUD())
			{
				const FText DistrictName = Territory->GetTerritoryDisplayName();
				HUD->ShowMajorNotification(
					FText::Format(NSLOCTEXT("TerritoryHUD", "AttackAlertTitle", "Attack alert: {0}"), DistrictName),
					FText::Format(NSLOCTEXT("TerritoryHUD", "AttackAlertBody", "{0} is under attack."), DistrictName),
					6.f,
					true);
			}
		}
	}

	bHasObservedState = true;
	LastObservedState = State;
	LastObservedContestingFaction = ContestingFaction;
}

void UTerritoryHUDWidget::HandleAssaultNotification(const FTerritoryAssaultRecord& Assault)
{
	if (ANarrativePlayerController* PlayerController = Cast<ANarrativePlayerController>(GetOwningPlayer()))
	{
		if (UNarrativeGameplayHUD* HUD = PlayerController->GetNarrativeGameplayHUD())
		{
			HUD->ShowMajorNotification(
				FText::Format(
					NSLOCTEXT("TerritoryHUD", "CounterAttackWarningTitle", "Counterattack warning: {0}"),
					UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(Assault.TargetTerritory)),
				FText::Format(
					NSLOCTEXT("TerritoryHUD", "CounterAttackWarningBody", "{0} is mobilizing {1} attackers. Reinforce the district before they arrive."),
					UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(Assault.AttackingFaction),
					FText::AsNumber(Assault.PlannedForce)),
				8.f,
				true);
		}
	}
	RefreshTerritoryDisplay();
}
