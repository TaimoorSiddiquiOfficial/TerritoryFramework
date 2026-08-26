#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "UI/TerritoryDistrictManagementWidget.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Components/SphereComponent.h"
#include "Navigation/NarrativeNavigationComponent.h"
#include "Navigation/NavigatorGameplayTags.h"
#include "NarrativeArsenal.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/World.h"

UTerritoryDistrictPOIMarker::UTerritoryDistrictPOIMarker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsOnPaint = false;
	bDrawBreadcrumbs = false;
	SetZOrder(20);
	FGameplayTagContainer Domains;
	Domains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Minimap);
	Domains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Worldmap);
	SetDefaultDomains(Domains);
}

void UTerritoryDistrictPOIMarker::SetManagementPoint(ATerritoryDistrictManagementPoint* Point)
{
	ManagementPoint = Point;
	ActorOwner = Point;
	MarkerTransform = Point ? Point->GetActorTransform() : FTransform::Identity;
	RefreshPresentationPolicy();
}

void UTerritoryDistrictPOIMarker::ClearManagementPoint()
{
	if (bRegisteredWithNavigation)
	{
		bRegisteredWithNavigation = false;
		if (!GetWorld() || !GetWorld()->bIsTearingDown)
		{
			RemoveMarker();
		}
	}
	SetDomains(FGameplayTagContainer());
	ManagementPoint = nullptr;
	ActorOwner = nullptr;
}

void UTerritoryDistrictPOIMarker::RefreshPresentationPolicy()
{
	ATerritoryDistrict* District = ManagementPoint.IsValid()
		? ManagementPoint->ResolveDistrict() : nullptr;
	const bool bVisible = District
		&& District->GetTerritoryState() == ETerritoryState::Claimed
		&& UTerritoryUIBlueprintLibrary::IsTerritoryVisibleToPlayer(this, District);
	const bool bWorldTearingDown = GetWorld() && GetWorld()->bIsTearingDown;

	FGameplayTagContainer Domains;
	if (bVisible)
	{
		// This marker identifies the physical command post. It remains map/minimap
		// intel and never floods the compass; the tracked child Place owns guidance.
		Domains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Minimap);
		Domains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Worldmap);
	}
	SetDomains(Domains);

	if (bVisible && !bRegisteredWithNavigation && !bWorldTearingDown)
	{
		bRegisteredWithNavigation = true;
		RegisterMarker();
	}
	else if (!bVisible && bRegisteredWithNavigation)
	{
		bRegisteredWithNavigation = false;
		if (!bWorldTearingDown)
		{
			RemoveMarker();
		}
	}
	if (!bWorldTearingDown)
	{
		RefreshMarker();
	}
}

FText UTerritoryDistrictPOIMarker::GetMarkerActionText_Implementation(UNarrativeNavigationComponent* Selector) const
{
	return FText::FromString(TEXT("Manage District"));
}

FText UTerritoryDistrictPOIMarker::GetMarkerDisplayText_Implementation(UNarrativeNavigationComponent* Selector,
	const FGameplayTag& NavigatorType, FText& OutSubtitleText) const
{
	if (ATerritoryDistrict* District = ManagementPoint.IsValid() ? ManagementPoint->ResolveDistrict() : nullptr)
	{
		// Hide text for non-claimed territories — marker is invisible anyway
		if (District->GetTerritoryState() != ETerritoryState::Claimed
			|| !UTerritoryUIBlueprintLibrary::IsTerritoryVisibleToPlayer(this, District))
		{
			OutSubtitleText = FText::GetEmpty();
			return FText::GetEmpty();
		}
		OutSubtitleText = FText::FromString(District->GetOwningFaction().ToString());
		return District->GetTerritoryDisplayName();
	}
	OutSubtitleText = FText::GetEmpty();
	return FText::FromString(TEXT("District Management"));
}

FLinearColor UTerritoryDistrictPOIMarker::GetMarkerColor_Implementation(UNarrativeNavigationComponent* Selector,
	const FGameplayTag& NavigatorType) const
{
	const ATerritoryDistrict* District = ManagementPoint.IsValid() ? ManagementPoint->ResolveDistrict() : nullptr;
	if (!District || !UTerritoryUIBlueprintLibrary::IsTerritoryVisibleToPlayer(
		this, const_cast<ATerritoryDistrict*>(District)))
	{
		return FLinearColor(0.f, 0.f, 0.f, 0.f);
	}

	const ETerritoryState State = District->GetTerritoryState();

	// Only show the management POI for captured districts. Unclaimed, contested,
	// and locked districts have no management to offer — hide the marker entirely.
	if (State != ETerritoryState::Claimed)
	{
		return FLinearColor(0.f, 0.f, 0.f, 0.f);
	}

	// Viewer-relative color: friendly owner = green, enemy = red.
	const FGameplayTag Owner = District->GetOwningFaction();
	const bool bIsFriendly = Selector
		&& UTerritoryBlueprintLibrary::IsActorInFaction(this, Selector->GetOwner(), Owner);
	return bIsFriendly ? FLinearColor(0.1f, 0.75f, 0.35f, 1.f) : FLinearColor(0.85f, 0.15f, 0.15f, 1.f);
}

bool UTerritoryDistrictPOIMarker::CanInteract_Implementation(UNarrativeNavigationComponent* Selector) const
{
	if (!ManagementPoint.IsValid() || !Selector) return false;
	const APlayerController* PlayerController = Cast<APlayerController>(Selector->GetOwner());
	ATerritoryDistrict* District = ManagementPoint->ResolveDistrict();
	FText FailureReason;
	return PlayerController
		&& District
		&& UTerritoryUIBlueprintLibrary::IsTerritoryVisibleToPlayer(
			this, District)
		&& ManagementPoint->CanManage(PlayerController->GetPawn(), FailureReason)
		&& ManagementPoint->IsInteractorInRange(PlayerController->GetPawn());
}

void UTerritoryDistrictPOIMarker::OnSelect_Implementation(UNarrativeNavigationComponent* Selector)
{
	Super::OnSelect_Implementation(Selector);
	if (ManagementPoint.IsValid() && Selector)
	{
		ManagementPoint->OpenManagementWidget(Cast<APlayerController>(Selector->GetOwner()));
	}
}

UTerritoryDistrictNavigationMarkerComponent::UTerritoryDistrictNavigationMarkerComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTerritoryDistrictNavigationMarkerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (ATerritoryDistrictManagementPoint* Point = Cast<ATerritoryDistrictManagementPoint>(GetOwner()))
	{
		DistrictMarker = NewObject<UTerritoryDistrictPOIMarker>(this);
		MarkerObject = DistrictMarker;
		DistrictMarker->SetManagementPoint(Point);
		if (UTerritoryRegistrySubsystem* Registry =
			GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.AddUniqueDynamic(
				this, &UTerritoryDistrictNavigationMarkerComponent::OnRegistryTerritoryChanged);
			Registry->OnTerritoryUnregistered.AddUniqueDynamic(
				this, &UTerritoryDistrictNavigationMarkerComponent::OnRegistryTerritoryChanged);
		}
		BindToDistrictIfAvailable();
		RefreshMarkerPolicy();
		GetWorld()->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(
				this, &UTerritoryDistrictNavigationMarkerComponent::RefreshMarkerPolicy));
	}
}

void UTerritoryDistrictNavigationMarkerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(
				this, &UTerritoryDistrictNavigationMarkerComponent::OnRegistryTerritoryChanged);
			Registry->OnTerritoryUnregistered.RemoveDynamic(
				this, &UTerritoryDistrictNavigationMarkerComponent::OnRegistryTerritoryChanged);
		}
	}
	UnbindFromDistrict();
	if (DistrictMarker)
	{
		DistrictMarker->ClearManagementPoint();
	}
	MarkerObject = nullptr;
	Super::EndPlay(EndPlayReason);
	DistrictMarker = nullptr;
}

void UTerritoryDistrictNavigationMarkerComponent::BindToDistrictIfAvailable()
{
	ATerritoryDistrictManagementPoint* Point =
		Cast<ATerritoryDistrictManagementPoint>(GetOwner());
	ATerritoryDistrict* District = Point ? Point->ResolveDistrict() : nullptr;
	if (BoundDistrict.Get() == District) return;
	UnbindFromDistrict();
	if (!District) return;
	BoundDistrict = District;
	District->OnTerritoryStateChangedDelegate.AddUniqueDynamic(
		this, &UTerritoryDistrictNavigationMarkerComponent::OnTerritoryStateChanged);
	District->OnTerritoryOwnershipChanged.AddUniqueDynamic(
		this, &UTerritoryDistrictNavigationMarkerComponent::OnTerritoryOwnershipChanged);
}

void UTerritoryDistrictNavigationMarkerComponent::UnbindFromDistrict()
{
	if (BoundDistrict.IsValid())
	{
		BoundDistrict->OnTerritoryStateChangedDelegate.RemoveDynamic(
			this, &UTerritoryDistrictNavigationMarkerComponent::OnTerritoryStateChanged);
		BoundDistrict->OnTerritoryOwnershipChanged.RemoveDynamic(
			this, &UTerritoryDistrictNavigationMarkerComponent::OnTerritoryOwnershipChanged);
	}
	BoundDistrict = nullptr;
}

void UTerritoryDistrictNavigationMarkerComponent::RefreshMarkerPolicy()
{
	BindToDistrictIfAvailable();
	if (DistrictMarker) DistrictMarker->RefreshPresentationPolicy();
}

void UTerritoryDistrictNavigationMarkerComponent::OnTerritoryStateChanged(
	ATerritoryVolume* Territory, ETerritoryState NewState)
{
	RefreshMarkerPolicy();
}

void UTerritoryDistrictNavigationMarkerComponent::OnTerritoryOwnershipChanged(
	ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	RefreshMarkerPolicy();
}

void UTerritoryDistrictNavigationMarkerComponent::OnRegistryTerritoryChanged(
	ATerritoryVolume* Territory, bool bWasUnregistered)
{
	const ATerritoryDistrictManagementPoint* Point =
		Cast<ATerritoryDistrictManagementPoint>(GetOwner());
	if (!Point || !Territory || Territory->GetTerritoryTag() != Point->DistrictTag) return;
	if (bWasUnregistered && BoundDistrict.Get() == Territory)
	{
		UnbindFromDistrict();
	}
	RefreshMarkerPolicy();
}

FText UTerritoryDistrictInteractableComponent::GetInteractableNameText_Implementation(
	APawn* Interactor, UNarrativeInteractionComponent* InteractionComp) const
{
	if (const ATerritoryDistrictManagementPoint* Point = Cast<ATerritoryDistrictManagementPoint>(GetOwner()))
	{
		if (const ATerritoryDistrict* District = Point->ResolveDistrict())
		{
			return District->GetTerritoryDisplayName();
		}
	}
	return FText::FromString(TEXT("District Management"));
}

FText UTerritoryDistrictInteractableComponent::GetInteractableActionText_Implementation(
	APawn* Interactor, UNarrativeInteractionComponent* InteractionComp) const
{
	return FText::FromString(TEXT("Manage"));
}

bool UTerritoryDistrictInteractableComponent::CanInteract_Implementation(
	APawn* Interactor, UNarrativeInteractionComponent* InteractionComp, FText& OutErrorText)
{
	const ATerritoryDistrictManagementPoint* Point = Cast<ATerritoryDistrictManagementPoint>(GetOwner());
	return Point && Point->CanManage(Interactor, OutErrorText) && Point->IsInteractorInRange(Interactor);
}

void UTerritoryDistrictInteractableComponent::OnInteract_Implementation(
	APawn* Interactor, UNarrativeInteractionComponent* InteractionComp)
{
	Super::OnInteract_Implementation(Interactor, InteractionComp);
	if (ATerritoryDistrictManagementPoint* Point = Cast<ATerritoryDistrictManagementPoint>(GetOwner()))
	{
		Point->HandleInteraction(Interactor);
	}
}

ATerritoryDistrictManagementPoint::ATerritoryDistrictManagementPoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateMapMarker = false;
	bSupportsFastTravel = false;
	POIDisplayName = FText::FromString(TEXT("District Management"));
	ManagementLayerTag = FGameplayTag::RequestGameplayTag(FName(TEXT("UI.Layer.Menu")), false);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(80.f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(TraceChannel_NarrativeInteraction, ECR_Block);

	InteractableComponent = CreateDefaultSubobject<UTerritoryDistrictInteractableComponent>(TEXT("DistrictInteractable"));
	DistrictMarkerComponent = CreateDefaultSubobject<UTerritoryDistrictNavigationMarkerComponent>(TEXT("DistrictMarker"));
}

void ATerritoryDistrictManagementPoint::BeginPlay()
{
	Super::BeginPlay();
	if (ATerritoryDistrict* District = ResolveDistrict())
	{
		POITag = District->GetTerritoryTag();
		POIDisplayName = District->GetTerritoryDisplayName();
	}
	if (InteractionSphere)
	{
		InteractionSphere->SetSphereRadius(ManagementDistance);
	}
	if (InteractableComponent)
	{
		InteractableComponent->InteractionDistance = ManagementDistance;
	}
}

ATerritoryDistrict* ATerritoryDistrictManagementPoint::ResolveDistrict() const
{
	return Cast<ATerritoryDistrict>(UTerritoryBlueprintLibrary::GetTerritoryByTag(this, DistrictTag));
}

bool ATerritoryDistrictManagementPoint::CanManage(APawn* Interactor, FText& OutFailureReason) const
{
	OutFailureReason = FText::GetEmpty();
	ATerritoryDistrict* District = ResolveDistrict();
	if (!District || !Interactor)
	{
		OutFailureReason = FText::FromString(TEXT("District management is unavailable."));
		return false;
	}
	if (District->GetTerritoryState() != ETerritoryState::Claimed)
	{
		OutFailureReason = FText::FromString(TEXT("Capture this district before managing it."));
		return false;
	}
	const FGameplayTag InteractorFaction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Interactor);
	if (!InteractorFaction.IsValid() || District->GetOwningFaction() != InteractorFaction)
	{
		OutFailureReason = FText::FromString(TEXT("Only the owning faction can manage this district."));
		return false;
	}
	return true;
}

bool ATerritoryDistrictManagementPoint::IsInteractorInRange(APawn* Interactor) const
{
	return Interactor && FVector::DistSquared(Interactor->GetActorLocation(), GetActorLocation())
		<= FMath::Square(ManagementDistance + 100.f);
}

void ATerritoryDistrictManagementPoint::HandleInteraction(APawn* Interactor)
{
	if (!Interactor) return;

	if (APlayerController* PC = Cast<APlayerController>(Interactor->GetController()))
	{
		OpenManagementWidget(PC);
	}
}

void ATerritoryDistrictManagementPoint::OpenManagementWidget(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController() || !ManagementWidgetClass) return;
	FText FailureReason;
	if (!CanManage(PlayerController->GetPawn(), FailureReason)) return;
	UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(PlayerController);

	if (UTerritoryDistrictManagementWidget* Widget = Cast<UTerritoryDistrictManagementWidget>(
		UTerritoryUIBlueprintLibrary::OpenTerritoryMenu(
			PlayerController, ManagementWidgetClass, ManagementLayerTag)))
	{
		Widget->InitializeManagement(this);
	}
	else
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("Cannot open district management: Narrative HUD or layer %s is unavailable."),
			*ManagementLayerTag.ToString());
	}
}
