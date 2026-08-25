#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Widgets/NarrativeGameplayHUD.h"

UTerritoryPlayerManagementComponent::UTerritoryPlayerManagementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTerritoryPlayerManagementComponent::BeginPlay()
{
	Super::BeginPlay();
	BindLiveEventSources();
}

void UTerritoryPlayerManagementComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	UnbindLiveEventSources();
	Super::EndPlay(EndPlayReason);
}

void UTerritoryPlayerManagementComponent::BindLiveEventSources()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController || !PlayerController->IsLocalController()) return;
	UWorld* World = GetWorld();
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Registry) return;

	Registry->OnTerritoryRegistered.AddUniqueDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryRegistered);
	Registry->OnTerritoryUnregistered.AddUniqueDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryUnregistered);
	for (ATerritoryVolume* Territory : Registry->GetAllTerritories())
	{
		BindTerritoryLiveEvents(Territory);
	}
}

void UTerritoryPlayerManagementComponent::UnbindLiveEventSources()
{
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(
				this, &UTerritoryPlayerManagementComponent::HandleTerritoryRegistered);
			Registry->OnTerritoryUnregistered.RemoveDynamic(
				this, &UTerritoryPlayerManagementComponent::HandleTerritoryUnregistered);
			for (ATerritoryVolume* Territory : Registry->GetAllTerritories())
			{
				UnbindTerritoryLiveEvents(Territory);
			}
		}
	}
	ObservedTerritoryStates.Empty();
}

void UTerritoryPlayerManagementComponent::BindTerritoryLiveEvents(
	ATerritoryVolume* Territory)
{
	if (!IsValid(Territory)) return;
	Territory->OnTerritoryOwnershipChanged.RemoveDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryOwnershipChanged);
	Territory->OnTerritoryStateChangedDelegate.RemoveDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryStateChanged);
	Territory->OnTerritoryOwnershipChanged.AddDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryOwnershipChanged);
	Territory->OnTerritoryStateChangedDelegate.AddDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryStateChanged);
	ObservedTerritoryStates.Add(Territory, Territory->GetTerritoryState());
}

void UTerritoryPlayerManagementComponent::UnbindTerritoryLiveEvents(
	ATerritoryVolume* Territory)
{
	if (!IsValid(Territory)) return;
	Territory->OnTerritoryOwnershipChanged.RemoveDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryOwnershipChanged);
	Territory->OnTerritoryStateChangedDelegate.RemoveDynamic(
		this, &UTerritoryPlayerManagementComponent::HandleTerritoryStateChanged);
	ObservedTerritoryStates.Remove(Territory);
}

void UTerritoryPlayerManagementComponent::HandleTerritoryRegistered(
	ATerritoryVolume* Territory, bool bWasUnregistered)
{
	BindTerritoryLiveEvents(Territory);
}

void UTerritoryPlayerManagementComponent::HandleTerritoryUnregistered(
	ATerritoryVolume* Territory, bool bWasUnregistered)
{
	UnbindTerritoryLiveEvents(Territory);
}

FGameplayTag UTerritoryPlayerManagementComponent::ResolveViewerFaction() const
{
	return UTerritoryBlueprintLibrary::GetActorPrimaryFaction(
		this, Cast<APlayerController>(GetOwner()));
}

FText UTerritoryPlayerManagementComponent::ResolveTerritoryName(
	const FGameplayTag& TerritoryTag) const
{
	if (const UWorld* World = GetWorld())
	{
		if (const UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			if (const ATerritoryVolume* Territory =
				Registry->GetTerritoryByTag(TerritoryTag))
			{
				const FText Name = Territory->GetTerritoryDisplayName();
				if (!Name.IsEmpty()) return Name;
			}
		}
	}
	return UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(TerritoryTag);
}

void UTerritoryPlayerManagementComponent::AddLiveEvent(
	ETerritoryLiveEventType Type, const FGameplayTag& TerritoryTag,
	const FText& Headline, const FText& Detail, bool bCanSetWaypoint,
	float ActiveDuration)
{
	UWorld* World = GetWorld();
	if (!World || !Cast<APlayerController>(GetOwner())
		|| !Cast<APlayerController>(GetOwner())->IsLocalController())
	{
		return;
	}
	const double Now = World->GetRealTimeSeconds();
	for (const FTerritoryLiveEvent& Existing : LiveEvents)
	{
		if (Existing.Type == Type && Existing.TerritoryTag == TerritoryTag
			&& Existing.Headline.EqualTo(Headline)
			&& Now - Existing.CreatedRealTime < 0.75)
		{
			return;
		}
	}

	FTerritoryLiveEvent Event;
	Event.EventID = FGuid::NewGuid();
	Event.Type = Type;
	Event.TerritoryTag = TerritoryTag;
	Event.TerritoryName = ResolveTerritoryName(TerritoryTag);
	Event.Headline = Headline;
	Event.Detail = Detail;
	Event.CreatedRealTime = Now;
	Event.ActiveDuration = ActiveDuration >= 0.f
		? ActiveDuration : LiveEventActiveDuration;
	Event.bCanSetWaypoint = bCanSetWaypoint && TerritoryTag.IsValid();
	LiveEvents.Insert(Event, 0);
	if (LiveEvents.Num() > FMath::Max(1, MaxLiveEventHistory))
	{
		LiveEvents.SetNum(FMath::Max(1, MaxLiveEventHistory));
	}

	OnLiveEventAdded.Broadcast(Event);
	OnLiveEventsChanged.Broadcast();
	if (const ANarrativePlayerController* NarrativeController =
		Cast<ANarrativePlayerController>(GetOwner()))
	{
		if (UNarrativeGameplayHUD* HUD =
			NarrativeController->GetNarrativeGameplayHUD())
		{
			HUD->ShowNotification(Headline, FMath::Min(Event.ActiveDuration, 6.f));
		}
	}
}

TArray<FTerritoryLiveEvent>
UTerritoryPlayerManagementComponent::GetLiveEvents(bool bIncludeExpired) const
{
	TArray<FTerritoryLiveEvent> Result;
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetRealTimeSeconds() : 0.0;
	for (FTerritoryLiveEvent Event : LiveEvents)
	{
		Event.bExpired = Event.IsExpiredAt(Now);
		if (Now - Event.CreatedRealTime > ExpiredEventRetentionDuration) continue;
		if (!bIncludeExpired && Event.bExpired) continue;
		Result.Add(MoveTemp(Event));
	}
	return Result;
}

void UTerritoryPlayerManagementComponent::ClearExpiredLiveEvents()
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetRealTimeSeconds() : 0.0;
	const int32 Removed = LiveEvents.RemoveAll(
		[Now](const FTerritoryLiveEvent& Event)
		{
			return Event.IsExpiredAt(Now);
		});
	if (Removed > 0) OnLiveEventsChanged.Broadcast();
}

void UTerritoryPlayerManagementComponent::HandleTerritoryOwnershipChanged(
	ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	if (!Territory || Territory->GetTerritoryState() == ETerritoryState::Locked) return;
	const FGameplayTag ViewerFaction = ResolveViewerFaction();
	const FText Name = Territory->GetTerritoryDisplayName();
	if (ViewerFaction.IsValid() && NewOwner == ViewerFaction)
	{
		AddLiveEvent(ETerritoryLiveEventType::Captured, Territory->GetTerritoryTag(),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CapturedHeadline", "{0} captured"), Name),
			NSLOCTEXT("TerritoryLiveEvents", "CapturedDetail", "Your faction now controls this territory. Track it to navigate, reinforce, or review its Places."),
			true);
	}
	else if (ViewerFaction.IsValid() && OldOwner == ViewerFaction)
	{
		AddLiveEvent(ETerritoryLiveEventType::Lost, Territory->GetTerritoryTag(),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "LostHeadline", "{0} was lost"), Name),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "LostDetail", "Control changed to {0}. Set a waypoint to plan a response."),
				UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(NewOwner)), true);
	}
}

void UTerritoryPlayerManagementComponent::HandleTerritoryStateChanged(
	ATerritoryVolume* Territory, ETerritoryState NewState)
{
	if (!Territory) return;
	const ETerritoryState PreviousState = ObservedTerritoryStates.FindRef(Territory);
	ObservedTerritoryStates.Add(Territory, NewState);
	const FText Name = Territory->GetTerritoryDisplayName();
	if (PreviousState == ETerritoryState::Locked && NewState != ETerritoryState::Locked)
	{
		AddLiveEvent(ETerritoryLiveEventType::Unlocked, Territory->GetTerritoryTag(),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "UnlockedHeadline", "{0} unlocked"), Name),
			NSLOCTEXT("TerritoryLiveEvents", "UnlockedDetail", "New Territory intel is available. Open Operations or set a waypoint."), true);
	}
	else if (NewState == ETerritoryState::Contested)
	{
		AddLiveEvent(ETerritoryLiveEventType::Contested, Territory->GetTerritoryTag(),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "ContestedHeadline", "{0} is contested"), Name),
			NSLOCTEXT("TerritoryLiveEvents", "ContestedDetail", "Physical attackers are applying capture pressure. Track the Territory to respond."), true);
	}
	else if (PreviousState == ETerritoryState::Contested
		&& NewState == ETerritoryState::Claimed)
	{
		AddLiveEvent(ETerritoryLiveEventType::Secured, Territory->GetTerritoryTag(),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "SecuredHeadline", "{0} secured"), Name),
			NSLOCTEXT("TerritoryLiveEvents", "SecuredDetail", "The contest ended and control is stable."), true);
	}
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
	return ManagementPoint != nullptr && Count > 0 && Count <= MaxGuardPurchaseCount && RequestId > LastServerRequestId;
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
	return District != nullptr && Count > 0 && Count <= MaxGuardPurchaseCount && RequestId > LastServerRequestId;
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
	return ManagementPoint != nullptr && Count > 0 && Count <= MaxGuardPurchaseCount && RequestId > LastServerRequestId;
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
	return District != nullptr && Count > 0 && Count <= MaxGuardPurchaseCount && RequestId > LastServerRequestId;
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
	AddLiveEvent(ETerritoryLiveEventType::CounterAttackWarning,
		Assault.TargetTerritory,
		FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterWarningHeadline",
			"Counterattack forming near {0}"), ResolveTerritoryName(Assault.TargetTerritory)),
		FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterWarningDetail",
			"{0} is preparing {1} finite attackers. The assault waits for the configured activation rules."),
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(Assault.AttackingFaction),
			FText::AsNumber(Assault.PlannedForce)), true, 45.f);
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
	const FGameplayTag Target = Event.Assault.TargetTerritory;
	const FText Name = ResolveTerritoryName(Target);
	switch (Event.NewState)
	{
	case ETerritoryAssaultState::ScheduledWarning:
	case ETerritoryAssaultState::WaitingForPlayerProximity:
		AddLiveEvent(ETerritoryLiveEventType::CounterAttackWarning, Target,
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterScheduledHeadline",
				"Attack warning: {0}"), Name),
			NSLOCTEXT("TerritoryLiveEvents", "CounterScheduledDetail",
				"A hostile response is scheduled. Track the District to inspect its route and defence."), true, 45.f);
		break;
	case ETerritoryAssaultState::Active:
		AddLiveEvent(ETerritoryLiveEventType::CounterAttackActive, Target,
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterActiveHeadline",
				"Counterattack active at {0}"), Name),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterActiveDetail",
				"{0} attackers remain alive and {1} remain in reserve."),
				FText::AsNumber(Event.Assault.AliveForce),
				FText::AsNumber(Event.Assault.PendingReserveForce)), true, 60.f);
		break;
	case ETerritoryAssaultState::Defeated:
		AddLiveEvent(ETerritoryLiveEventType::CounterAttackDefeated, Target,
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterDefeatedHeadline",
				"Counterattack defeated at {0}"), Name),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterDefeatedDetail",
				"The finite force was removed after {0} casualties and {1} withdrawals."),
				FText::AsNumber(Event.Assault.KilledForce),
				FText::AsNumber(Event.Assault.WithdrawnForce)), true);
		break;
	case ETerritoryAssaultState::Succeeded:
		AddLiveEvent(ETerritoryLiveEventType::CounterAttackSucceeded, Target,
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterSucceededHeadline",
				"Enemy force took {0}"), Name),
			NSLOCTEXT("TerritoryLiveEvents", "CounterSucceededDetail",
				"The physical capture completed. Track the Territory to organize a counter-offensive."), true, 60.f);
		break;
	case ETerritoryAssaultState::Cancelled:
		AddLiveEvent(ETerritoryLiveEventType::CounterAttackCancelled, Target,
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterCancelledHeadline",
				"Attack on {0} cancelled"), Name),
			FText::Format(NSLOCTEXT("TerritoryLiveEvents", "CounterCancelledDetail",
				"The response ended before capture. Resolution: {0}."),
				FText::FromString(UEnum::GetValueAsString(Event.Resolution))), true);
		break;
	default:
		break;
	}
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
