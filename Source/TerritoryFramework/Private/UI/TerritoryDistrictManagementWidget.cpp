#include "UI/TerritoryDistrictManagementWidget.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UTerritoryDistrictManagementWidget::InitializeManagement(
	ATerritoryDistrictManagementPoint* InManagementPoint)
{
	ManagementPoint = InManagementPoint;
	if (ATerritoryDistrict* District = GetManagedDistrict())
	{
		BindToTerritory(District->GetTerritoryTag());
	}
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
		AddGuardButton->OnClicked.AddUniqueDynamic(this, &UTerritoryDistrictManagementWidget::HandleAddGuardClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UTerritoryDistrictManagementWidget::HandleCloseClicked);
	}
	BindManagementComponent();
	// Base class (UTerritoryInfoWidget) already runs a 0.5s refresh timer
	// in its own NativeConstruct via Super::NativeConstruct().
	RefreshManagementDisplay();
}

void UTerritoryDistrictManagementWidget::NativeDestruct()
{
	if (AddGuardButton)
	{
		AddGuardButton->OnClicked.RemoveDynamic(this, &UTerritoryDistrictManagementWidget::HandleAddGuardClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UTerritoryDistrictManagementWidget::HandleCloseClicked);
	}
	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnGuardPurchaseResult.RemoveDynamic(
			this, &UTerritoryDistrictManagementWidget::HandleGuardPurchaseResult);
	}
	// Base class NativeDestruct clears the 0.5s refresh timer.
	Super::NativeDestruct();
}

ATerritoryDistrict* UTerritoryDistrictManagementWidget::GetManagedDistrict() const
{
	return ManagementPoint.IsValid() ? ManagementPoint->ResolveDistrict() : Cast<ATerritoryDistrict>(GetBoundTerritory());
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
	return District && District->CanPurchaseGuards(ManagedFaction, 1, OutFailureReason);
}

void UTerritoryDistrictManagementWidget::RefreshManagementDisplay()
{
	ATerritoryDistrict* District = GetManagedDistrict();
	if (!District) return;

	if (DistrictNameText) DistrictNameText->SetText(District->GetTerritoryDisplayName());
	if (OwnerText) OwnerText->SetText(FText::FromString(District->GetOwningFaction().ToString()));
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
		const UTerritoryEconomySubsystem* Economy = GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>();
		TreasuryText->SetText(FText::AsNumber(Economy ? Economy->GetTreasury(ManagedFaction) : 0));
	}

	FText FailureReason;
	const bool bCanPurchase = CanPurchaseGuard(FailureReason);
	if (AddGuardButton) AddGuardButton->SetIsEnabled(bCanPurchase);
	if (StatusText && !bCanPurchase) StatusText->SetText(FailureReason);
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
	ATerritoryVolume* Territory, bool bSuccess, FText Message)
{
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
