#include "Interaction/TerritoryCapturePoint.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "Subsystems/TerritoryControlSubsystem.h"

ATerritoryCapturePoint::ATerritoryCapturePoint()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	bReplicates = false;

	CaptureZone = CreateDefaultSubobject<USphereComponent>(TEXT("CaptureZone"));
	SetRootComponent(CaptureZone);
	CaptureZone->InitSphereRadius(CaptureRadius);
	CaptureZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CaptureZone->SetCollisionObjectType(ECC_WorldDynamic);
	CaptureZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	CaptureZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CaptureZone->SetGenerateOverlapEvents(true);
	CaptureZone->ShapeColor = FColor(68, 208, 158);

	CaptureMarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CaptureMarkerMesh"));
	CaptureMarkerMesh->SetupAttachment(CaptureZone);
	CaptureMarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CaptureMarkerMesh->SetGenerateOverlapEvents(false);
	CaptureMarkerMesh->SetCanEverAffectNavigation(false);
}

void ATerritoryCapturePoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPlaceDefinition();
	if (CaptureZone)
	{
		CaptureZone->SetSphereRadius(FMath::Max(100.f, CaptureRadius));
	}
	RefreshCaptureMarkerVisibility();
}

void ATerritoryCapturePoint::BeginPlay()
{
	Super::BeginPlay();
	if (!ApplyPlaceDefinition())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("Capture Point %s has no Place Definition. Legacy Blueprint configuration is disabled."),
			*GetPathName());
		if (CaptureZone) CaptureZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CaptureMarkerMesh) CaptureMarkerMesh->SetVisibility(false, true);
		SetActorTickEnabled(false);
		return;
	}
	if (!CaptureZone) return;
	RefreshCaptureMarkerVisibility();
	if (!TargetTerritoryTag.IsValid())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("Capture Point %s has no Target Territory Tag and cannot capture anything."),
			*GetPathName());
	}
	else if (const ATerritoryVolume* Territory = ResolveTargetTerritory();
		Territory && Territory->UsesStoryCaptureFromBounds())
	{
		if (const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
			Settings && Settings->ShouldDebugInteraction())
		{
			UE_LOG(LogTerritory, Log,
				TEXT("[Interaction] Capture Point %s is disabled because %s uses story capture from its full bounds."),
				*GetName(), *Territory->GetTerritoryTag().ToString());
		}
	}

	CaptureZone->OnComponentBeginOverlap.AddUniqueDynamic(
		this, &ATerritoryCapturePoint::HandleCaptureZoneBeginOverlap);
	CaptureZone->OnComponentEndOverlap.AddUniqueDynamic(
		this, &ATerritoryCapturePoint::HandleCaptureZoneEndOverlap);

	if (HasAuthority())
	{
		TArray<AActor*> ExistingOverlaps;
		CaptureZone->GetOverlappingActors(ExistingOverlaps, APawn::StaticClass());
		for (AActor* Actor : ExistingOverlaps)
		{
			OverlappingParticipants.Add(Actor);
		}
		ReconcileOverlappingParticipants();
	}
}

bool ATerritoryCapturePoint::ApplyPlaceDefinition()
{
	if (!PlaceDefinition) return false;
	TargetTerritoryTag = PlaceDefinition->TerritoryTag;
	CaptureRadius = FMath::Max(100.f,
		PlaceDefinition->CapturePoint.CaptureRadius);
	bCaptureEnabled = PlaceDefinition->CapturePoint.bEnabled
		&& PlaceDefinition->CapturePoint.bAutomaticCapture;
	bHideMarkerWhileCaptureUnavailable =
		PlaceDefinition->CapturePoint.bHideWhileUnavailable;
	if (CaptureZone)
	{
		CaptureZone->SetSphereRadius(CaptureRadius);
	}
	return true;
}

void ATerritoryCapturePoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TArray<TWeakObjectPtr<AActor>> Participants;
	Registrations.GetKeys(Participants);
	for (const TWeakObjectPtr<AActor>& Participant : Participants)
	{
		if (AActor* Actor = Participant.Get())
		{
			UnregisterCaptureParticipant(Actor);
		}
	}
	Registrations.Empty();
	OverlappingParticipants.Empty();
	Super::EndPlay(EndPlayReason);
}

void ATerritoryCapturePoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		ReconcileOverlappingParticipants();
	}
	RefreshCaptureMarkerVisibility();
}

ATerritoryVolume* ATerritoryCapturePoint::ResolveTargetTerritory() const
{
	return TargetTerritoryTag.IsValid()
		? UTerritoryBlueprintLibrary::GetTerritoryByTag(this, TargetTerritoryTag)
		: nullptr;
}

float ATerritoryCapturePoint::GetCaptureProgress() const
{
	const ATerritoryVolume* Territory = ResolveTargetTerritory();
	return Territory ? Territory->GetControlProgress() : 0.f;
}

FGameplayTag ATerritoryCapturePoint::GetContestingFaction() const
{
	const ATerritoryVolume* Territory = ResolveTargetTerritory();
	return Territory ? Territory->GetOwnershipData().ContestingFaction : FGameplayTag();
}

bool ATerritoryCapturePoint::TryRegisterCaptureParticipant(AActor* Participant)
{
	if (!HasAuthority() || !IsAutomaticCaptureFlowActive()
		|| !IsEligiblePlayerParticipant(Participant))
	{
		UnregisterCaptureParticipant(Participant);
		return false;
	}

	ATerritoryVolume* Territory = ResolveTargetTerritory();
	if (!Territory || Territory->GetControlMode() != ETerritoryControlMode::Independent)
	{
		UnregisterCaptureParticipant(Participant);
		return false;
	}

	UNarrativeAbilitySystemComponent* AbilitySystem =
		ResolveParticipantAbilitySystem(Participant);
	if (AbilitySystem && AbilitySystem->IsDead())
	{
		UnregisterCaptureParticipant(Participant);
		return false;
	}

	const FGameplayTag Faction =
		UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Participant);
	if (!Faction.IsValid() || Territory->IsOwnedByFaction(Faction))
	{
		UnregisterCaptureParticipant(Participant);
		return false;
	}

	const TWeakObjectPtr<AActor> ParticipantKey(Participant);
	if (const FParticipantRegistration* Existing = Registrations.Find(ParticipantKey))
	{
		if (Existing->Territory.Get() == Territory && Existing->Faction == Faction)
		{
			if (UTerritoryControlSubsystem* Control =
				GetWorld()->GetSubsystem<UTerritoryControlSubsystem>())
			{
				const bool bRegistered =
					Control->TryRegisterAttacker(Territory, Participant, Faction);
				if (bRegistered)
				{
					return true;
				}
			}
			UnregisterCaptureParticipant(Participant);
			return false;
		}
		UnregisterCaptureParticipant(Participant);
	}

	UTerritoryControlSubsystem* Control =
		GetWorld()->GetSubsystem<UTerritoryControlSubsystem>();
	const bool bRegistered = Control
		&& Control->TryRegisterAttacker(Territory, Participant, Faction);
	if (!bRegistered)
	{
		return false;
	}

	FParticipantRegistration& Registration = Registrations.Add(ParticipantKey);
	Registration.Territory = Territory;
	Registration.Faction = Faction;
	Registration.AbilitySystem = AbilitySystem;
	if (AbilitySystem)
	{
		AbilitySystem->OnDeathStateChanged.AddUniqueDynamic(
			this, &ATerritoryCapturePoint::HandleParticipantDeathStateChanged);
	}
	OnCaptureParticipantChanged.Broadcast(Participant, Territory, true);
	return true;
}

void ATerritoryCapturePoint::UnregisterCaptureParticipant(AActor* Participant)
{
	if (!Participant) return;
	const TWeakObjectPtr<AActor> ParticipantKey(Participant);
	const FParticipantRegistration* Existing = Registrations.Find(ParticipantKey);
	if (!Existing) return;

	const FParticipantRegistration Registration = *Existing;
	Registrations.Remove(ParticipantKey);
	if (UNarrativeAbilitySystemComponent* AbilitySystem = Registration.AbilitySystem.Get())
	{
		AbilitySystem->OnDeathStateChanged.RemoveDynamic(
			this, &ATerritoryCapturePoint::HandleParticipantDeathStateChanged);
	}
	if (HasAuthority())
	{
		if (ATerritoryVolume* Territory = Registration.Territory.Get())
		{
			UWorld* World = GetWorld();
			if (UTerritoryControlSubsystem* Control = World
				? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr)
			{
				Control->UnregisterAttacker(Territory, Participant, Registration.Faction);
			}
		}
	}
	OnCaptureParticipantChanged.Broadcast(Participant, Registration.Territory.Get(), false);
}

bool ATerritoryCapturePoint::IsCaptureParticipantRegistered(const AActor* Participant) const
{
	return Participant && Registrations.Contains(
		TWeakObjectPtr<AActor>(const_cast<AActor*>(Participant)));
}

bool ATerritoryCapturePoint::IsAutomaticCaptureFlowActive() const
{
	const ATerritoryVolume* Territory = ResolveTargetTerritory();
	return bCaptureEnabled && Territory
		&& Territory->GetControlMode() == ETerritoryControlMode::Independent
		&& !Territory->UsesStoryCaptureFromBounds();
}

void ATerritoryCapturePoint::HandleCaptureZoneBeginOverlap(
	UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || !OtherActor) return;
	OverlappingParticipants.Add(OtherActor);
	TryRegisterCaptureParticipant(OtherActor);
}

void ATerritoryCapturePoint::HandleCaptureZoneEndOverlap(
	UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	if (!HasAuthority() || !OtherActor) return;
	OverlappingParticipants.Remove(OtherActor);
	UnregisterCaptureParticipant(OtherActor);
}

void ATerritoryCapturePoint::HandleParticipantDeathStateChanged(
	AActor* Participant, UNarrativeAbilitySystemComponent* AbilitySystem, bool bIsDead)
{
	if (!HasAuthority() || !Participant) return;
	if (bIsDead)
	{
		UnregisterCaptureParticipant(Participant);
	}
	else if (OverlappingParticipants.Contains(Participant))
	{
		TryRegisterCaptureParticipant(Participant);
	}
}

void ATerritoryCapturePoint::ReconcileOverlappingParticipants()
{
	if (!IsAutomaticCaptureFlowActive())
	{
		TArray<TWeakObjectPtr<AActor>> Registered;
		Registrations.GetKeys(Registered);
		for (const TWeakObjectPtr<AActor>& Participant : Registered)
		{
			if (AActor* Actor = Participant.Get()) UnregisterCaptureParticipant(Actor);
		}
		return;
	}

	for (auto It = OverlappingParticipants.CreateIterator(); It; ++It)
	{
		AActor* Participant = It->Get();
		if (!Participant || !CaptureZone || !CaptureZone->IsOverlappingActor(Participant))
		{
			if (Participant) UnregisterCaptureParticipant(Participant);
			It.RemoveCurrent();
			continue;
		}
		TryRegisterCaptureParticipant(Participant);
	}
}

UNarrativeAbilitySystemComponent* ATerritoryCapturePoint::ResolveParticipantAbilitySystem(
	AActor* Participant) const
{
	return FTerritoryNarrativeProAdapter::ResolveAbilitySystem(Participant);
}

bool ATerritoryCapturePoint::IsEligiblePlayerParticipant(AActor* Participant) const
{
	const APawn* Pawn = Cast<APawn>(Participant);
	return Pawn && Pawn->IsPlayerControlled();
}

void ATerritoryCapturePoint::RefreshCaptureMarkerVisibility()
{
	if (!CaptureMarkerMesh) return;
	bool bVisible = IsAutomaticCaptureFlowActive();
	if (bVisible && bHideMarkerWhileCaptureUnavailable)
	{
		const ATerritoryVolume* Territory = ResolveTargetTerritory();
		bVisible = Territory
			&& Territory->GetControlMode() == ETerritoryControlMode::Independent
			&& Territory->IsAvailableForGameplay();
	}
	CaptureMarkerMesh->SetVisibility(bVisible, true);
	CaptureMarkerMesh->SetHiddenInGame(!bVisible, true);
}
