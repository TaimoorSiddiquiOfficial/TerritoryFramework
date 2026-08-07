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

void UTerritoryPlayerManagementComponent::RequestSetGuardTarget(
	ATerritoryDistrictManagementPoint* ManagementPoint, ATerritoryVolume* Territory,
	int32 NewDesiredGuardCount)
{
	const int32 RequestId = ++NextRequestId;
	if (!ManagementPoint || !Territory || NewDesiredGuardCount < 0
		|| NewDesiredGuardCount > MaxGuardTargetCount)
	{
		OnGuardPurchaseResult.Broadcast(Territory, false,
			FText::FromString(TEXT("Garrison staffing target request is invalid.")), RequestId);
		return;
	}
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastPurchaseRequestTime < PurchaseCooldown)
		{
			OnGuardPurchaseResult.Broadcast(Territory, false,
				FText::FromString(TEXT("Please wait before changing another garrison target.")), RequestId);
			return;
		}
		LastPurchaseRequestTime = Now;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PerformSetGuardTargetAtManagementPoint(ManagementPoint, Territory,
			NewDesiredGuardCount, RequestId);
	}
	else
	{
		ServerRequestSetGuardTarget(ManagementPoint, Territory, NewDesiredGuardCount, RequestId);
	}
}

void UTerritoryPlayerManagementComponent::RequestSetGuardTargetForTerritory(
	ATerritoryVolume* Territory, int32 NewDesiredGuardCount)
{
	const int32 RequestId = ++NextRequestId;
	if (!Territory || NewDesiredGuardCount < 0 || NewDesiredGuardCount > MaxGuardTargetCount)
	{
		OnGuardPurchaseResult.Broadcast(Territory, false,
			FText::FromString(TEXT("Garrison staffing target request is invalid.")), RequestId);
		return;
	}
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastPurchaseRequestTime < PurchaseCooldown)
		{
			OnGuardPurchaseResult.Broadcast(Territory, false,
				FText::FromString(TEXT("Please wait before changing another garrison target.")), RequestId);
			return;
		}
		LastPurchaseRequestTime = Now;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PerformSetGuardTarget(Territory, NewDesiredGuardCount, RequestId);
	}
	else
	{
		ServerRequestSetGuardTargetForTerritory(Territory, NewDesiredGuardCount, RequestId);
	}
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

bool UTerritoryPlayerManagementComponent::ServerRequestSetGuardTarget_Validate(
	ATerritoryDistrictManagementPoint* ManagementPoint, ATerritoryVolume* Territory,
	int32 NewDesiredGuardCount, int32 RequestId)
{
	return ManagementPoint && Territory && NewDesiredGuardCount >= 0
		&& NewDesiredGuardCount <= MaxGuardTargetCount && RequestId > LastServerRequestId;
}

void UTerritoryPlayerManagementComponent::ServerRequestSetGuardTarget_Implementation(
	ATerritoryDistrictManagementPoint* ManagementPoint, ATerritoryVolume* Territory,
	int32 NewDesiredGuardCount, int32 RequestId)
{
	if (!GetWorld() || RequestId <= LastServerRequestId) return;
	LastServerRequestId = RequestId;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPurchaseRequestTime < PurchaseCooldown)
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			FText::FromString(TEXT("Please wait before changing another garrison target.")), RequestId);
		return;
	}
	LastPurchaseRequestTime = Now;
	PerformSetGuardTargetAtManagementPoint(ManagementPoint, Territory,
		NewDesiredGuardCount, RequestId);
}

bool UTerritoryPlayerManagementComponent::ServerRequestSetGuardTargetForTerritory_Validate(
	ATerritoryVolume* Territory, int32 NewDesiredGuardCount, int32 RequestId)
{
	return Territory && NewDesiredGuardCount >= 0
		&& NewDesiredGuardCount <= MaxGuardTargetCount && RequestId > LastServerRequestId;
}

void UTerritoryPlayerManagementComponent::ServerRequestSetGuardTargetForTerritory_Implementation(
	ATerritoryVolume* Territory, int32 NewDesiredGuardCount, int32 RequestId)
{
	if (!GetWorld() || RequestId <= LastServerRequestId) return;
	LastServerRequestId = RequestId;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPurchaseRequestTime < PurchaseCooldown)
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			FText::FromString(TEXT("Please wait before changing another garrison target.")), RequestId);
		return;
	}
	LastPurchaseRequestTime = Now;
	PerformSetGuardTarget(Territory, NewDesiredGuardCount, RequestId);
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

void UTerritoryPlayerManagementComponent::PerformSetGuardTargetAtManagementPoint(
	ATerritoryDistrictManagementPoint* ManagementPoint, ATerritoryVolume* Territory,
	int32 NewDesiredGuardCount, int32 RequestId)
{
	ATerritoryDistrict* District = ManagementPoint ? ManagementPoint->ResolveDistrict() : nullptr;
	APawn* Pawn = GetManagingPawn();
	FText FailureReason;
	if (!ManagementPoint || !District || !Territory || !Pawn)
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			FText::FromString(TEXT("District management context is unavailable.")), RequestId);
		return;
	}
	if (!ManagementPoint->CanManage(Pawn, FailureReason)
		|| !CanManageTerritory(Territory, Pawn, FailureReason))
	{
		ClientReceiveGuardPurchaseResult(Territory, false, FailureReason, RequestId);
		return;
	}
	if (!ManagementPoint->IsInteractorInRange(Pawn))
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			FText::FromString(TEXT("Move closer to the district management point.")), RequestId);
		return;
	}
	if (!IsTerritoryManagedByDistrict(District, Territory))
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			FText::FromString(TEXT("That garrison does not belong to this district.")), RequestId);
		return;
	}
	PerformSetGuardTarget(Territory, NewDesiredGuardCount, RequestId);
}

void UTerritoryPlayerManagementComponent::PerformSetGuardTarget(
	ATerritoryVolume* Territory, int32 NewDesiredGuardCount, int32 RequestId)
{
	APawn* Pawn = GetManagingPawn();
	FText FailureReason;
	if (!CanManageTerritory(Territory, Pawn, FailureReason))
	{
		ClientReceiveGuardPurchaseResult(Territory, false, FailureReason, RequestId);
		return;
	}
	if (NewDesiredGuardCount < 0 || NewDesiredGuardCount > Territory->GetMaxGuardCount())
	{
		ClientReceiveGuardPurchaseResult(Territory, false,
			FText::FromString(TEXT("The staffing target exceeds this garrison's capacity.")), RequestId);
		return;
	}

	const FTerritoryGarrisonMutationResult Result =
		Territory->TrySetDesiredGuardCount(Pawn, NewDesiredGuardCount);
	ClientReceiveGuardPurchaseResult(Territory, Result.bSuccess, Result.Message, RequestId);
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

bool UTerritoryPlayerManagementComponent::CanManageTerritory(
	ATerritoryVolume* Territory, APawn* Pawn, FText& OutFailureReason) const
{
	OutFailureReason = FText::GetEmpty();
	if (!Territory || !Pawn)
	{
		OutFailureReason = FText::FromString(TEXT("Garrison management context is unavailable."));
		return false;
	}
	if (Territory->GetTerritoryState() != ETerritoryState::Claimed)
	{
		OutFailureReason = FText::FromString(TEXT("Capture this territory before managing its garrison."));
		return false;
	}
	const FGameplayTag Faction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Pawn);
	if (!Faction.IsValid() || Faction != Territory->GetOwningFaction())
	{
		OutFailureReason = FText::FromString(TEXT("Only the owning faction can manage this garrison."));
		return false;
	}
	return true;
}

bool UTerritoryPlayerManagementComponent::IsTerritoryManagedByDistrict(
	ATerritoryDistrict* District, ATerritoryVolume* Territory) const
{
	if (!District || !Territory) return false;
	if (District == Territory) return true;
	return District->GetProperties().Contains(Territory);
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

void UTerritoryPlayerManagementComponent::SendCounterHappened(
	const FTerritoryCounterAttackStateEvent& Event)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ClientReceiveCounterHappened(Event);
	}
}

void UTerritoryPlayerManagementComponent::ClientReceiveCounterHappened_Implementation(
	const FTerritoryCounterAttackStateEvent& Event)
{
	OnCounterHappened.Broadcast(Event);
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
