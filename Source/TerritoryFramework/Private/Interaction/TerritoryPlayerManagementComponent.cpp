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
	if (!PlayerController->HasAuthority())
	{
		// Runtime replicated components must be authored by the server. Creating a
		// same-named client-only bridge can prevent the authoritative component from
		// resolving correctly when it later replicates.
		return nullptr;
	}

	UTerritoryPlayerManagementComponent* Component =
		NewObject<UTerritoryPlayerManagementComponent>(PlayerController,
			TEXT("TerritoryPlayerManagement"), RF_Transient);
	if (!Component) return nullptr;

	PlayerController->AddInstanceComponent(Component);
	Component->SetIsReplicated(true);
	Component->RegisterComponent();
	PlayerController->ForceNetUpdate();
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
			OnGuardPurchaseResult.Broadcast(ManagementPoint->ResolveDistrict(), false,
				FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
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

void UTerritoryPlayerManagementComponent::RequestPurchaseGuardsForDistrict(
	ATerritoryDistrict* District, int32 Count)
{
	if (!District || Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		OnGuardPurchaseResult.Broadcast(nullptr, false,
			FText::FromString(TEXT("Guard purchase request is invalid.")), ++NextRequestId);
		return;
	}
	const int32 RequestId = ++NextRequestId;
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastPurchaseRequestTime < PurchaseCooldown)
		{
			OnGuardPurchaseResult.Broadcast(District, false,
				FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
			return;
		}
		LastPurchaseRequestTime = Now;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PerformPurchaseForDistrict(District, Count, RequestId);
	}
	else
	{
		ServerRequestPurchaseGuardsForDistrict(District, Count, RequestId);
	}
}

void UTerritoryPlayerManagementComponent::RequestRemoveGuards(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count)
{
	if (!ManagementPoint || Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		OnGuardPurchaseResult.Broadcast(nullptr, false,
			FText::FromString(TEXT("Guard removal request is invalid.")), ++NextRequestId);
		return;
	}
	const int32 RequestId = ++NextRequestId;
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastPurchaseRequestTime < PurchaseCooldown)
		{
			OnGuardPurchaseResult.Broadcast(ManagementPoint->ResolveDistrict(), false,
				FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
			return;
		}
		LastPurchaseRequestTime = Now;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PerformRemove(ManagementPoint, Count, RequestId);
	}
	else
	{
		ServerRequestRemoveGuards(ManagementPoint, Count, RequestId);
	}
}

void UTerritoryPlayerManagementComponent::RequestRemoveGuardsForDistrict(
	ATerritoryDistrict* District, int32 Count)
{
	if (!District || Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		OnGuardPurchaseResult.Broadcast(nullptr, false,
			FText::FromString(TEXT("Guard removal request is invalid.")), ++NextRequestId);
		return;
	}
	const int32 RequestId = ++NextRequestId;
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastPurchaseRequestTime < PurchaseCooldown)
		{
			OnGuardPurchaseResult.Broadcast(District, false,
				FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
			return;
		}
		LastPurchaseRequestTime = Now;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PerformRemoveForDistrict(District, Count, RequestId);
	}
	else
	{
		ServerRequestRemoveGuardsForDistrict(District, Count, RequestId);
	}
}

bool UTerritoryPlayerManagementComponent::ServerRequestPurchaseGuards_Validate(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId)
{
	return Count > 0 && Count <= MaxGuardPurchaseCount && RequestId > LastServerRequestId;
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
		ClientReceiveGuardPurchaseResult(ManagementPoint ? ManagementPoint->ResolveDistrict() : nullptr,
			false, FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
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

bool UTerritoryPlayerManagementComponent::ServerRequestPurchaseGuardsForDistrict_Validate(
	ATerritoryDistrict* District, int32 Count, int32 RequestId)
{
	return Count > 0 && Count <= MaxGuardPurchaseCount && RequestId > LastServerRequestId;
}

void UTerritoryPlayerManagementComponent::ServerRequestPurchaseGuardsForDistrict_Implementation(
	ATerritoryDistrict* District, int32 Count, int32 RequestId)
{
	if (!GetWorld() || RequestId <= LastServerRequestId)
	{
		return;
	}
	LastServerRequestId = RequestId;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPurchaseRequestTime < PurchaseCooldown)
	{
		ClientReceiveGuardPurchaseResult(District, false,
			FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
		return;
	}
	LastPurchaseRequestTime = Now;
	if (Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		ClientReceiveGuardPurchaseResult(nullptr, false,
			FText::FromString(TEXT("Guard purchase request is invalid.")), RequestId);
		return;
	}
	PerformPurchaseForDistrict(District, Count, RequestId);
}

bool UTerritoryPlayerManagementComponent::ServerRequestRemoveGuards_Validate(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId)
{
	return Count > 0 && Count <= MaxGuardPurchaseCount && RequestId > LastServerRequestId;
}

void UTerritoryPlayerManagementComponent::ServerRequestRemoveGuards_Implementation(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId)
{
	if (!GetWorld() || RequestId <= LastServerRequestId)
	{
		return;
	}
	LastServerRequestId = RequestId;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPurchaseRequestTime < PurchaseCooldown)
	{
		ClientReceiveGuardPurchaseResult(ManagementPoint ? ManagementPoint->ResolveDistrict() : nullptr,
			false, FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
		return;
	}
	LastPurchaseRequestTime = Now;
	if (Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		ClientReceiveGuardPurchaseResult(nullptr, false,
			FText::FromString(TEXT("Guard removal request is invalid.")), RequestId);
		return;
	}
	PerformRemove(ManagementPoint, Count, RequestId);
}

bool UTerritoryPlayerManagementComponent::ServerRequestRemoveGuardsForDistrict_Validate(
	ATerritoryDistrict* District, int32 Count, int32 RequestId)
{
	return Count > 0 && Count <= MaxGuardPurchaseCount && RequestId > LastServerRequestId;
}

void UTerritoryPlayerManagementComponent::ServerRequestRemoveGuardsForDistrict_Implementation(
	ATerritoryDistrict* District, int32 Count, int32 RequestId)
{
	if (!GetWorld() || RequestId <= LastServerRequestId)
	{
		return;
	}
	LastServerRequestId = RequestId;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPurchaseRequestTime < PurchaseCooldown)
	{
		ClientReceiveGuardPurchaseResult(District, false,
			FText::FromString(TEXT("Please wait before making another guard request.")), RequestId);
		return;
	}
	LastPurchaseRequestTime = Now;
	if (Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		ClientReceiveGuardPurchaseResult(nullptr, false,
			FText::FromString(TEXT("Guard removal request is invalid.")), RequestId);
		return;
	}
	PerformRemoveForDistrict(District, Count, RequestId);
}

void UTerritoryPlayerManagementComponent::PerformPurchase(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId)
{
	ATerritoryDistrict* District = ManagementPoint ? ManagementPoint->ResolveDistrict() : nullptr;
	APawn* Pawn = GetManagingPawn();

	if (!ManagementPoint || !District || !Pawn)
	{
		ClientReceiveGuardPurchaseResult(District, false,
			FText::FromString(TEXT("District management context is unavailable.")), RequestId);
		return;
	}
	FText Result;
	if (!ManagementPoint->CanManage(Pawn, Result))
	{
		ClientReceiveGuardPurchaseResult(District, false, Result, RequestId);
		return;
	}
	if (!ManagementPoint->IsInteractorInRange(Pawn))
	{
		ClientReceiveGuardPurchaseResult(District, false,
			FText::FromString(TEXT("Move closer to the district management point.")), RequestId);
		return;
	}
	PerformPurchaseForDistrict(District, Count, RequestId);
}

void UTerritoryPlayerManagementComponent::PerformPurchaseForDistrict(
	ATerritoryDistrict* District, int32 Count, int32 RequestId)
	{
	FText Result;
	bool bSuccess = false;
	APawn* Pawn = GetManagingPawn();
	if (!District || !Pawn)
	{
		Result = FText::FromString(TEXT("District management context is unavailable."));
	}
	else if (Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		Result = FText::FromString(TEXT("Guard purchase count is invalid."));
	}
	else if (!CanManageDistrict(District, Pawn, Result))
	{
		bSuccess = false;
	}
	else
	{
		bSuccess = District->TryPurchaseGuards(Pawn, Count, Result);
	}
	ClientReceiveGuardPurchaseResult(District, bSuccess, Result, RequestId);
}

void UTerritoryPlayerManagementComponent::PerformRemove(
	ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId)
{
	ATerritoryDistrict* District = ManagementPoint ? ManagementPoint->ResolveDistrict() : nullptr;
	APawn* Pawn = GetManagingPawn();
	if (!ManagementPoint || !District || !Pawn)
	{
		ClientReceiveGuardPurchaseResult(District, false,
			FText::FromString(TEXT("District management context is unavailable.")), RequestId);
		return;
	}
	FText Result;
	if (!ManagementPoint->CanManage(Pawn, Result))
	{
		ClientReceiveGuardPurchaseResult(District, false, Result, RequestId);
		return;
	}
	if (!ManagementPoint->IsInteractorInRange(Pawn))
	{
		ClientReceiveGuardPurchaseResult(District, false,
			FText::FromString(TEXT("Move closer to the district management point.")), RequestId);
		return;
	}
	PerformRemoveForDistrict(District, Count, RequestId);
}

void UTerritoryPlayerManagementComponent::PerformRemoveForDistrict(
	ATerritoryDistrict* District, int32 Count, int32 RequestId)
{
	FText Result;
	bool bSuccess = false;
	APawn* Pawn = GetManagingPawn();
	if (!District || !Pawn)
	{
		Result = FText::FromString(TEXT("District management context is unavailable."));
	}
	else if (Count <= 0 || Count > MaxGuardPurchaseCount)
	{
		Result = FText::FromString(TEXT("Guard removal count is invalid."));
	}
	else if (!CanManageDistrict(District, Pawn, Result))
	{
		bSuccess = false;
	}
	else
	{
		bSuccess = District->TryRemoveGuards(Pawn, Count, Result);
	}
	ClientReceiveGuardPurchaseResult(District, bSuccess, Result, RequestId);
}

bool UTerritoryPlayerManagementComponent::CanManageDistrict(
	ATerritoryDistrict* District, APawn* Pawn, FText& OutFailureReason) const
{
	OutFailureReason = FText::GetEmpty();
	if (!District || !Pawn)
	{
		OutFailureReason = FText::FromString(TEXT("District management context is unavailable."));
		return false;
	}
	if (District->GetTerritoryState() != ETerritoryState::Claimed)
	{
		OutFailureReason = FText::FromString(TEXT("Capture this district before managing it."));
		return false;
	}
	const FGameplayTag Faction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Pawn);
	if (!Faction.IsValid() || Faction != District->GetOwningFaction())
	{
		OutFailureReason = FText::FromString(TEXT("Only the owning faction can manage this district."));
		return false;
	}
	return true;
}

void UTerritoryPlayerManagementComponent::ClientReceiveGuardPurchaseResult_Implementation(
	ATerritoryVolume* Territory, bool bSuccess, const FText& Message, int32 RequestId)
{
	OnGuardPurchaseResult.Broadcast(Territory, bSuccess, Message, RequestId);
}

void UTerritoryPlayerManagementComponent::SendAssaultNotification(
	const FTerritoryAssaultRecord& Assault)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ClientReceiveAssaultNotification(Assault);
	}
}

void UTerritoryPlayerManagementComponent::ClientReceiveAssaultNotification_Implementation(
	const FTerritoryAssaultRecord& Assault)
{
	OnAssaultNotification.Broadcast(Assault);
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
