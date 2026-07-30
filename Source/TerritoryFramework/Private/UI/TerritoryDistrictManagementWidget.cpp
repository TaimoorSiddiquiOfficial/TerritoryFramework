#include "UI/TerritoryDistrictManagementWidget.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
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

void UTerritoryDistrictManagementWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshManagementDisplay();
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
	if (!ManagementPoint.IsValid() || !PlayerController
		|| !ManagementPoint->CanManage(PlayerController->GetPawn(), OutFailureReason))
	{
		return false;
	}
	if (!ManagementPoint->IsInteractorInRange(PlayerController->GetPawn()))
	{
		OutFailureReason = FText::FromString(TEXT("Move closer to the district management point."));
		return false;
	}
	return District && District->CanPurchaseGuards(PlayerController->GetPawn(), 1, OutFailureReason);
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
	return District && District->CanRemoveGuards(PlayerController->GetPawn(), 1, OutFailureReason);
}

void UTerritoryDistrictManagementWidget::RefreshManagementDisplay()
{
	ATerritoryDistrict* District = GetManagedDistrict();
	if (!District)
	{
		OperationsView = FTerritoryDistrictOperationsView();
		if (StatusText)
		{
			StatusText->SetText(NSLOCTEXT("TerritoryManagement", "DistrictUnavailable", "District is not loaded or registered."));
		}
		return;
	}

	UTerritoryUIBlueprintLibrary::BuildDistrictOperationsView(
		this, District, GetOwningPlayer(), OperationsView);

	if (DistrictNameText) DistrictNameText->SetText(OperationsView.DisplayName);
	if (OwnerText) OwnerText->SetText(UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(OperationsView.OwnerFaction));
	if (StateText)
	{
		const UEnum* StateEnum = StaticEnum<ETerritoryState>();
		StateText->SetText(StateEnum ? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(OperationsView.TerritoryState)) : FText::GetEmpty());
	}
	if (GuardCountText)
	{
		GuardCountText->SetText(FText::Format(
			NSLOCTEXT("TerritoryManagement", "GuardCounts", "Active {0}  |  Assigned {1}  |  Capacity {2}"),
			FText::AsNumber(OperationsView.ActiveGuards),
			FText::AsNumber(OperationsView.DesiredGuards),
			FText::AsNumber(OperationsView.MaximumGuards)));
	}
	if (GuardCostText)
	{
		GuardCostText->SetText(FText::AsNumber(OperationsView.GuardPurchaseCost));
	}
	if (EarningsText) EarningsText->SetText(FText::AsNumber(OperationsView.PeriodicIncome));
	if (TreasuryText) TreasuryText->SetText(FText::AsNumber(OperationsView.AvailableFunds));
	if (NetIncomeText)
	{
		NetIncomeText->SetText(FText::Format(
			NSLOCTEXT("TerritoryManagement", "NetIncome", "Net per cycle: {0}"),
			FText::AsNumber(OperationsView.NetIncome)));
	}
	if (ReserveGuardText)
	{
		ReserveGuardText->SetText(OperationsView.bReserveCountKnown
			? FText::Format(
				NSLOCTEXT("TerritoryManagement", "ReserveCount", "Reserve: {0}"),
				FText::AsNumber(OperationsView.ReserveGuards))
			: NSLOCTEXT("TerritoryManagement", "ReserveUnknown", "Reserve: server snapshot required"));
	}
	if (ThreatText) ThreatText->SetText(OperationsView.ThreatSummary);
	if (AvailabilityText) AvailabilityText->SetText(OperationsView.AvailabilityReason);

	FText FailureReason;
	const bool bCanPurchase = CanPurchaseGuard(FailureReason);
	if (AddGuardButton) AddGuardButton->SetIsEnabled(bCanPurchase);
	FText RemoveFailureReason;
	const bool bCanRemove = CanRemoveGuard(RemoveFailureReason);
	if (RemoveGuardButton) RemoveGuardButton->SetIsEnabled(bCanRemove);
	if (StatusText)
	{
		const FText DisabledReason = !FailureReason.IsEmpty() ? FailureReason : RemoveFailureReason;
		StatusText->SetText(bCanPurchase || bCanRemove ? FText::GetEmpty() : DisabledReason);
	}
	OnManagementRefreshed();
}

void UTerritoryDistrictManagementWidget::HandleAddGuardClicked()
{
	RequestAddGuards(1);
}

void UTerritoryDistrictManagementWidget::HandleRemoveGuardClicked()
{
	RequestRemoveGuards(1);
}

void UTerritoryDistrictManagementWidget::RequestAddGuards(int32 Count)
{
	FText FailureReason;
	if (Count <= 0 || !CanPurchaseGuard(FailureReason))
	{
		if (StatusText) StatusText->SetText(FailureReason);
		return;
	}
	if (ManagementComponent.IsValid() && ManagementPoint.IsValid())
	{
		if (StatusText) StatusText->SetText(NSLOCTEXT("TerritoryManagement", "AddingGuards", "Submitting guard assignment..."));
		ManagementComponent->RequestPurchaseGuards(ManagementPoint.Get(), Count);
	}
}

void UTerritoryDistrictManagementWidget::RequestRemoveGuards(int32 Count)
{
	FText FailureReason;
	if (Count <= 0 || !CanRemoveGuard(FailureReason))
	{
		if (StatusText) StatusText->SetText(FailureReason);
		return;
	}
	if (ManagementComponent.IsValid() && ManagementPoint.IsValid())
	{
		if (StatusText) StatusText->SetText(NSLOCTEXT("TerritoryManagement", "RemovingGuards", "Submitting guard removal..."));
		ManagementComponent->RequestRemoveGuards(ManagementPoint.Get(), Count);
	}
}

void UTerritoryDistrictManagementWidget::HandleCloseClicked()
{
	CloseTerritoryWidget();
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
		? UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(PlayerController)
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

UWidget* UTerritoryDistrictManagementWidget::NativeGetDesiredFocusTarget() const
{
	if (AddGuardButton && AddGuardButton->GetIsEnabled() && AddGuardButton->IsVisible())
	{
		return AddGuardButton;
	}
	if (RemoveGuardButton && RemoveGuardButton->GetIsEnabled() && RemoveGuardButton->IsVisible())
	{
		return RemoveGuardButton;
	}
	if (CloseButton && CloseButton->GetIsEnabled() && CloseButton->IsVisible())
	{
		return CloseButton;
	}
	return Super::NativeGetDesiredFocusTarget();
}
