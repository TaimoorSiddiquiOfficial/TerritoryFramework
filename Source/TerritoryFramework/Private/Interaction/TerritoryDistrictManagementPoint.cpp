#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "UI/TerritoryDistrictManagementWidget.h"
#include "Components/SphereComponent.h"
#include "Navigation/NarrativeNavigationComponent.h"
#include "Navigation/NavigatorGameplayTags.h"
#include "NarrativeArsenal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"

UTerritoryDistrictPOIMarker::UTerritoryDistrictPOIMarker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsOnPaint = false;
	bDrawBreadcrumbs = false;
	SetZOrder(20);
	FGameplayTagContainer Domains;
	Domains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Compass);
	Domains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Minimap);
	Domains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Worldmap);
	SetDefaultDomains(Domains);
}

void UTerritoryDistrictPOIMarker::SetManagementPoint(ATerritoryDistrictManagementPoint* Point)
{
	ManagementPoint = Point;
	ActorOwner = Point;
	MarkerTransform = Point ? Point->GetActorTransform() : FTransform::Identity;
}

void UTerritoryDistrictPOIMarker::ClearManagementPoint()
{
	ManagementPoint = nullptr;
	ActorOwner = nullptr;
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
		if (District->GetTerritoryState() != ETerritoryState::Claimed)
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
	if (!District) return FLinearColor::Gray;

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
	FText FailureReason;
	return PlayerController
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
		DistrictMarker->SetManagementPoint(Point);
		MarkerObject = DistrictMarker;
		RegisterMarker();

		// Subscribe to territory state changes for dynamic marker refresh.
		// Without this, the POI marker color/text stays stale after a capture.
		if (ATerritoryDistrict* District = Point->ResolveDistrict())
		{
			District->OnTerritoryStateChangedDelegate.AddDynamic(this, &UTerritoryDistrictNavigationMarkerComponent::OnTerritoryStateChanged);
			District->OnTerritoryOwnershipChanged.AddDynamic(this, &UTerritoryDistrictNavigationMarkerComponent::OnTerritoryOwnershipChanged);
		}
	}
}

void UTerritoryDistrictNavigationMarkerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (DistrictMarker)
	{
		DistrictMarker->ClearManagementPoint();
	}
	// Unsubscribe from territory delegates
	if (ATerritoryDistrictManagementPoint* Point = Cast<ATerritoryDistrictManagementPoint>(GetOwner()))
	{
		if (ATerritoryDistrict* District = Point->ResolveDistrict())
		{
			District->OnTerritoryStateChangedDelegate.RemoveDynamic(this, &UTerritoryDistrictNavigationMarkerComponent::OnTerritoryStateChanged);
			District->OnTerritoryOwnershipChanged.RemoveDynamic(this, &UTerritoryDistrictNavigationMarkerComponent::OnTerritoryOwnershipChanged);
		}
	}
	RemoveMarker();
	Super::EndPlay(EndPlayReason);
	DistrictMarker = nullptr;
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
	if (HasAuthority())
	{
		FGameModeEvents::OnGameModePostLoginEvent().AddUObject(
			this, &ATerritoryDistrictManagementPoint::OnPlayerPostLogin);
		if (UWorld* World = GetWorld())
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(It->Get());
			}
		}
	}
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

void ATerritoryDistrictManagementPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		FGameModeEvents::OnGameModePostLoginEvent().RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ATerritoryDistrictManagementPoint::OnPlayerPostLogin(
	AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (GameMode && GameMode->GetWorld() == GetWorld())
	{
		UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(NewPlayer);
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
	if (UTerritoryDistrictManagementWidget* Widget = CreateWidget<UTerritoryDistrictManagementWidget>(
		PlayerController, ManagementWidgetClass))
	{
		Widget->InitializeManagement(this);
		Widget->AddToViewport(50);
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(true);
	}
}
