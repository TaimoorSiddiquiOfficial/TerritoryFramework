#include "Interaction/TerritoryDistrictManagementPoint.h"
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
	if (District->GetTerritoryState() == ETerritoryState::Contested) return FLinearColor(1.f, 0.65f, 0.f, 1.f);
	if (District->GetTerritoryState() == ETerritoryState::Locked) return FLinearColor(0.35f, 0.2f, 0.55f, 1.f);
	return District->GetOwningFaction().IsValid() ? FLinearColor(0.1f, 0.75f, 0.35f, 1.f) : FLinearColor::Red;
}

bool UTerritoryDistrictPOIMarker::CanInteract_Implementation(UNarrativeNavigationComponent* Selector) const
{
	if (!ManagementPoint.IsValid() || !Selector) return false;
	const APlayerController* PlayerController = Cast<APlayerController>(Selector->GetOwner());
	FText FailureReason;
	return PlayerController && ManagementPoint->CanManage(PlayerController->GetPawn(), FailureReason);
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
	}
}

void UTerritoryDistrictNavigationMarkerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (DistrictMarker)
	{
		DistrictMarker->ClearManagementPoint();
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
	bReplicates = true;
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
	if (ATerritoryDistrict* District = ResolveDistrict())
	{
		POITag = District->GetTerritoryTag();
		POIDisplayName = District->GetTerritoryDisplayName();
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
	if (Interactor && Interactor->IsLocallyControlled())
	{
		OpenManagementWidget(Cast<APlayerController>(Interactor->GetController()));
	}
}

void ATerritoryDistrictManagementPoint::OpenManagementWidget(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController() || !ManagementWidgetClass) return;
	FText FailureReason;
	if (!CanManage(PlayerController->GetPawn(), FailureReason)) return;
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
