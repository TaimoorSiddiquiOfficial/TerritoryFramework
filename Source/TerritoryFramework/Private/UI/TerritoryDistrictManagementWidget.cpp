#include "UI/TerritoryDistrictManagementWidget.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Widgets/NarrativeCommonButtonBase.h"

void UTerritoryDistrictManagementWidget::InitializeManagement(
	ATerritoryDistrictManagementPoint* InManagementPoint)
{
	ManagementPoint = InManagementPoint;
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		ManagedFaction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, PlayerController->GetPawn());
	}
	BindManagementComponent();
	RefreshManagementDisplay();
}

void UTerritoryDistrictManagementWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (AddGuardButton)
	{
		AddGuardButton->OnClicked().AddUObject(this, &UTerritoryDistrictManagementWidget::HandleAddGuardClicked);
		AddGuardButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "AddGuard", "ADD GUARD"));
	}
	if (RemoveGuardButton)
	{
		RemoveGuardButton->OnClicked().AddUObject(this, &UTerritoryDistrictManagementWidget::HandleRemoveGuardClicked);
		RemoveGuardButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "RemoveGuard", "REMOVE GUARD"));
	}
	if (CloseButton)
	{
		CloseButton->OnClicked().AddUObject(this, &UTerritoryDistrictManagementWidget::HandleCloseClicked);
		CloseButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "Close", "CLOSE"));
	}
	BindManagementComponent();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimerHandle, this,
			&UTerritoryDistrictManagementWidget::RefreshManagementDisplay, 0.5f, true);
	}
}

void UTerritoryDistrictManagementWidget::NativeDestruct()
{
	if (AddGuardButton)
	{
		AddGuardButton->OnClicked().RemoveAll(this);
	}
	if (RemoveGuardButton)
	{
		RemoveGuardButton->OnClicked().RemoveAll(this);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
	}
	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnGuardPurchaseResult.RemoveDynamic(
			this, &UTerritoryDistrictManagementWidget::HandleGuardPurchaseResult);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	Super::NativeDestruct();
}

ATerritoryDistrict* UTerritoryDistrictManagementWidget::GetManagedDistrict() const
{
	return ManagementPoint.IsValid() ? ManagementPoint->ResolveDistrict() : nullptr;
}

FGameplayTag UTerritoryDistrictManagementWidget::GetManagedFaction() const
{
	return ManagedFaction;
}

int32 UTerritoryDistrictManagementWidget::GetDistrictIncome() const
{
	const ATerritoryDistrict* District = GetManagedDistrict();
	return District ? District->GetEffectiveIncome() : 0;
}

bool UTerritoryDistrictManagementWidget::CanPurchaseGuard(FText& OutFailureReason) const
{
	const ATerritoryDistrict* District = GetManagedDistrict();
	APlayerController* PlayerController = GetOwningPlayer();
	if (!ManagementComponent.IsValid())
	{
		OutFailureReason = FText::FromString(TEXT("Territory management is not installed on this PlayerController."));
		return false;
	}
	return District && PlayerController
		&& District->CanPurchaseGuards(PlayerController, 1, OutFailureReason);
}

bool UTerritoryDistrictManagementWidget::CanRemoveGuard(FText& OutFailureReason) const
{
	const ATerritoryDistrict* District = GetManagedDistrict();
	APlayerController* PlayerController = GetOwningPlayer();
	if (!ManagementComponent.IsValid())
	{
		OutFailureReason = FText::FromString(TEXT("Territory management is not installed on this PlayerController."));
		return false;
	}
	if (!ManagementPoint.IsValid() || !PlayerController || !ManagementPoint->CanManage(PlayerController->GetPawn(), OutFailureReason))
	{
		return false;
	}
	if (!ManagementPoint->IsInteractorInRange(PlayerController->GetPawn()))
	{
		OutFailureReason = FText::FromString(TEXT("Move closer to the district management point."));
		return false;
	}
	return District && District->CanRemoveGuards(PlayerController, 1, OutFailureReason);
}

void UTerritoryDistrictManagementWidget::RefreshManagementDisplay()
{
	ATerritoryDistrict* District = GetManagedDistrict();
	if (!District) return;

	if (DistrictNameText) DistrictNameText->SetText(District->GetTerritoryDisplayName());
	if (OwnerText) OwnerText->SetText(UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(District->GetOwningFaction()));
	if (StateText)
	{
		const UEnum* StateEnum = StaticEnum<ETerritoryState>();
		StateText->SetText(StateEnum ? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(District->GetTerritoryState())) : FText::GetEmpty());
	}
	if (GuardCountText)
	{
		GuardCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"),
			District->GetSpawnedGuardCount(), District->GetMaxGuardCount())));
	}
	if (GuardCostText)
	{
		GuardCostText->SetText(FText::AsNumber(District->GetGuardPurchaseCost(1)));
	}
	if (EarningsText) EarningsText->SetText(FText::AsNumber(GetDistrictIncome()));
	if (TreasuryText)
	{
		// P2-N17: Null guard on GetWorld
		UWorld* W = GetWorld();
		const UTerritoryEconomySubsystem* Economy = W ? W->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
		TreasuryText->SetText(FText::AsNumber(Economy ? Economy->GetActorCurrency(GetOwningPlayer()) : 0));
	}

	FText FailureReason;
	const bool bCanPurchase = CanPurchaseGuard(FailureReason);
	if (AddGuardButton) AddGuardButton->SetIsEnabled(bCanPurchase);
	FText RemoveFailureReason;
	const bool bCanRemove = CanRemoveGuard(RemoveFailureReason);
	if (RemoveGuardButton) RemoveGuardButton->SetIsEnabled(bCanRemove);
	if (StatusText)
	{
		StatusText->SetText(bCanPurchase || bCanRemove ? FText::GetEmpty() : FailureReason);
	}
	OnManagementRefreshed();
}

void UTerritoryDistrictManagementWidget::HandleAddGuardClicked()
{
	FText FailureReason;
	if (!CanPurchaseGuard(FailureReason))
	{
		if (StatusText) StatusText->SetText(FailureReason);
		return;
	}
	if (ManagementComponent.IsValid() && ManagementPoint.IsValid())
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Purchasing guard...")));
		ManagementComponent->RequestPurchaseGuards(ManagementPoint.Get(), 1);
	}
}

void UTerritoryDistrictManagementWidget::HandleRemoveGuardClicked()
{
	FText FailureReason;
	if (!CanRemoveGuard(FailureReason))
	{
		if (StatusText) StatusText->SetText(FailureReason);
		return;
	}
	if (ManagementComponent.IsValid() && ManagementPoint.IsValid())
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Removing guard...")));
		ManagementComponent->RequestRemoveGuards(ManagementPoint.Get(), 1);
	}
}

void UTerritoryDistrictManagementWidget::HandleCloseClicked()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

void UTerritoryDistrictManagementWidget::HandleGuardPurchaseResult(
	ATerritoryVolume* Territory, bool bSuccess, FText Message, int32 RequestId)
{
	(void)RequestId;
	if (!Territory || Territory == GetManagedDistrict())
	{
		if (StatusText) StatusText->SetText(Message);
		RefreshManagementDisplay();
	}
}

void UTerritoryDistrictManagementWidget::BindManagementComponent()
{
	APlayerController* PlayerController = GetOwningPlayer();
	UTerritoryPlayerManagementComponent* Component = PlayerController
		? PlayerController->FindComponentByClass<UTerritoryPlayerManagementComponent>()
		: nullptr;
	if (ManagementComponent.Get() == Component) return;

	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnGuardPurchaseResult.RemoveDynamic(
			this, &UTerritoryDistrictManagementWidget::HandleGuardPurchaseResult);
	}
	ManagementComponent = Component;
	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnGuardPurchaseResult.AddUniqueDynamic(
			this, &UTerritoryDistrictManagementWidget::HandleGuardPurchaseResult);
	}
}
