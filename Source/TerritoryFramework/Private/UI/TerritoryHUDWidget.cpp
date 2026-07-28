#include "UI/TerritoryHUDWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryInterfaces.h"
#include "Core/TerritoryVolume.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Widgets/NarrativeGameplayHUD.h"

void UTerritoryHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindToTerritoryAtPlayer();
}

void UTerritoryHUDWidget::RefreshTerritoryDisplay()
{
	if (!GetBoundTerritory())
	{
		BindToTerritoryAtPlayer();
	}

	Super::RefreshTerritoryDisplay();

	ATerritoryVolume* Territory = GetBoundTerritory();
	if (!Territory)
	{
		return;
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
		const int64 GuardUpkeep = static_cast<int64>(Territory->GetGuardCost()) * Territory->GetDesiredGuardCount();
		DistrictDescriptionText->SetText(FText::Format(
			NSLOCTEXT("TerritoryHUD", "DistrictSummary", "Guards {0}/{1}   Income {2}   Upkeep {3}"),
			FText::AsNumber(Territory->GetDesiredGuardCount()),
			FText::AsNumber(Territory->GetMaxGuardCount()),
			FText::AsNumber(Territory->GetPeriodicIncome()),
			FText::AsNumber(GuardUpkeep)));
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
