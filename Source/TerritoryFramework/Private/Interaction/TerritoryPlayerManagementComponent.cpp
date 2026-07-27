#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

UTerritoryPlayerManagementComponent::UTerritoryPlayerManagementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UTerritoryPlayerManagementComponent* UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(
	APlayerController* PlayerController)
{
	if (!PlayerController) return nullptr;
	if (UTerritoryPlayerManagementComponent* Existing =
		PlayerController->FindComponentByClass<UTerritoryPlayerManagementComponent>())
	{
		return Existing;
	}

	UTerritoryPlayerManagementComponent* Component =
		NewObject<UTerritoryPlayerManagementComponent>(PlayerController,
			TEXT("TerritoryPlayerManagement"), RF_Transient);
	if (!Component) return nullptr;

	PlayerController->AddInstanceComponent(Component);
	Component->RegisterComponent();
	return Component;
}

void UTerritoryPlayerManagementComponent::RequestPurchaseGuards(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count)
{
	if (!ManagementPoint || Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		OnGuardPurchaseResult.Broadcast(nullptr, false,
			FText::FromString(TEXT("Guard purchase request is invalid.")), ++NextRequestId);
		return;
	}
	const int32 RequestId = ++NextRequestId;

	// Anti-spam: ignore requests within cooldown window
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastPurchaseRequestTime < PurchaseCooldown)
		{
			return;
		}
		LastPurchaseRequestTime = Now;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		LastPurchaseRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastPurchaseRequestTime;
		PerformPurchase(ManagementPoint, Count, RequestId);
	}
	else
	{
		ServerRequestPurchaseGuards(ManagementPoint, Count, RequestId);
	}
}

void UTerritoryPlayerManagementComponent::ServerRequestPurchaseGuards_Implementation(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId)
{
	if (!GetWorld()) return;
	if (RequestId <= LastServerRequestId)
	{
		return;
	}
	LastServerRequestId = RequestId;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPurchaseRequestTime < PurchaseCooldown)
	{
		return;
	}
	LastPurchaseRequestTime = Now;

	if (Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		ClientReceiveGuardPurchaseResult(nullptr, false,
			FText::FromString(TEXT("Guard purchase request is invalid.")), RequestId);
		return;
	}
	PerformPurchase(ManagementPoint, Count, RequestId);
}

void UTerritoryPlayerManagementComponent::PerformPurchase(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId)
{
	ATerritoryDistrict* District = ManagementPoint ? ManagementPoint->ResolveDistrict() : nullptr;
	APawn* Pawn = GetManagingPawn();
	FText Result;
	bool bSuccess = false;

	if (!ManagementPoint || !District || !Pawn)
	{
		Result = FText::FromString(TEXT("District management context is unavailable."));
	}
	else if (Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		Result = FText::FromString(TEXT("Guard purchase count is invalid."));
	}
	else if (!ManagementPoint->CanManage(Pawn, Result))
	{
		bSuccess = false;
	}
	else if (!ManagementPoint->IsInteractorInRange(Pawn))
	{
		Result = FText::FromString(TEXT("Move closer to the district management point."));
	}
	else
	{
		bSuccess = District->TryPurchaseGuards(Pawn, Count, Result);
	}

	ClientReceiveGuardPurchaseResult(District, bSuccess, Result, RequestId);
}

void UTerritoryPlayerManagementComponent::ClientReceiveGuardPurchaseResult_Implementation(
	ATerritoryVolume* Territory, bool bSuccess, const FText& Message, int32 RequestId)
{
	OnGuardPurchaseResult.Broadcast(Territory, bSuccess, Message, RequestId);
}

APawn* UTerritoryPlayerManagementComponent::GetManagingPawn() const
{
	if (const APlayerController* Controller = Cast<APlayerController>(GetOwner()))
	{
		return Controller->GetPawn();
	}
	return Cast<APawn>(GetOwner());
}

FGameplayTag UTerritoryPlayerManagementComponent::GetManagedFaction() const
{
	return UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, GetManagingPawn());
}
