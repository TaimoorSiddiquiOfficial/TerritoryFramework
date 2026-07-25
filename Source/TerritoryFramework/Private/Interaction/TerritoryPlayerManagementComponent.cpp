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

void UTerritoryPlayerManagementComponent::RequestPurchaseGuards(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count)
{
	if (!ManagementPoint)
	{
		OnGuardPurchaseResult.Broadcast(nullptr, false, FText::FromString(TEXT("District management point is unavailable.")));
		return;
	}

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
		PerformPurchase(ManagementPoint, Count);
	}
	else
	{
		ServerRequestPurchaseGuards(ManagementPoint, Count);
	}
}

void UTerritoryPlayerManagementComponent::ServerRequestPurchaseGuards_Implementation(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count)
{
	PerformPurchase(ManagementPoint, Count);
}

void UTerritoryPlayerManagementComponent::PerformPurchase(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count)
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
		bSuccess = District->TryPurchaseGuards(GetManagedFaction(), Count, Result);
	}

	ClientReceiveGuardPurchaseResult(District, bSuccess, Result);
}

void UTerritoryPlayerManagementComponent::ClientReceiveGuardPurchaseResult_Implementation(
	ATerritoryVolume* Territory, bool bSuccess, const FText& Message)
{
	OnGuardPurchaseResult.Broadcast(Territory, bSuccess, Message);
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
