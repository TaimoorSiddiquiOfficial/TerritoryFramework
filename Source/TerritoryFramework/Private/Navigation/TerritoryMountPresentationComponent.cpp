#include "Navigation/TerritoryMountPresentationComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Vehicles/MountComponent.h"

UTerritoryMountPresentationComponent::UTerritoryMountPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.1f;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(false);
	SetAutoActivate(true);
}

void UTerritoryMountPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ObservedRoot = GetOwner()->GetRootComponent();
		if (ObservedRoot.IsValid()) TransformHandle = ObservedRoot->TransformUpdated.AddUObject(
			this, &UTerritoryMountPresentationComponent::OnRootTransformUpdated);
	}
	SetComponentTickEnabled(IsActive() && GetOwner() && !GetOwner()->HasAuthority());
	RefreshMountPresentation();
}

void UTerritoryMountPresentationComponent::Activate(const bool bReset)
{
	Super::Activate(bReset);
	SetComponentTickEnabled(IsActive() && GetOwner() && !GetOwner()->HasAuthority());
	RefreshMountPresentation();
}

void UTerritoryMountPresentationComponent::Deactivate()
{
	Super::Deactivate();
	RestoreCollision();
}

void UTerritoryMountPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ObservedRoot.IsValid()) ObservedRoot->TransformUpdated.Remove(TransformHandle);
	ObservedRoot.Reset();
	RestoreCollision();
	Super::EndPlay(EndPlayReason);
}

void UTerritoryMountPresentationComponent::OnRootTransformUpdated(USceneComponent* Updated,
	EUpdateTransformFlags Flags, ETeleportType Teleport)
{
	RefreshMountPresentation();
}

void UTerritoryMountPresentationComponent::TickComponent(const float DeltaTime,
	const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshMountPresentation();
}

void UTerritoryMountPresentationComponent::RefreshMountPresentation()
{
	ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner());
	if (!IsActive() || !IsValid(Character) || Character->IsActorBeingDestroyed()
		|| !GetWorld() || GetWorld()->bIsTearingDown || Character->HasAuthority()) return;
	// A Blueprint may redundantly add this component to a Territory guard. Only
	// the first component owns the local override; no second collision cache.
	if (Character->FindComponentByClass<UTerritoryMountPresentationComponent>() != this) return;
	AActor* Parent = Character->GetAttachParentActor();
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const bool bMounted = IsValid(Parent) && !Parent->IsActorBeingDestroyed()
		&& Parent->GetWorld() == GetWorld() && Parent->FindComponentByClass<UMountComponent>();
	if (!bMounted)
	{
		RestoreCollision();
		return;
	}
	if (Character->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// A newly owning client now runs Native's mount ability. Its exit warp
		// controls collision timing, so discard our old simulated-proxy cache.
		SuppressedCapsule.Reset();
		return;
	}
	// Native's locally executing mount ability already handles its own character.
	// Simulated passengers/NPCs only receive attachment and movement replication.
	if (!IsValid(Capsule)
		|| !Character->IsAlive() || Character->IsRagdoll(false)) return;
	if (SuppressedCapsule.IsValid() && SuppressedCapsule.Get() != Capsule) RestoreCollision();
	if (Capsule->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		if (!SuppressedCapsule.IsValid())
		{
			SuppressedCapsule = Capsule;
			PreviousCollision = Capsule->GetCollisionEnabled();
		}
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void UTerritoryMountPresentationComponent::RestoreCollision()
{
	UCapsuleComponent* Capsule = SuppressedCapsule.Get();
	SuppressedCapsule.Reset();
	ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner());
	// Do not undo Native death/ragdoll handling, a later collision change, or any
	// server-side state if authority changed during possession or teardown.
	if (IsValid(Capsule) && IsValid(Character) && Character->GetLocalRole() == ROLE_SimulatedProxy
		&& !Character->IsActorBeingDestroyed() && GetWorld() && !GetWorld()->bIsTearingDown
		&& Character->IsAlive() && !Character->IsRagdoll(false)
		&& Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		Capsule->SetCollisionEnabled(PreviousCollision);
	}
}
