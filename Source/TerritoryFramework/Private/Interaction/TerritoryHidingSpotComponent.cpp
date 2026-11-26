#include "Interaction/TerritoryHidingSpotComponent.h"

#include "Core/TerritoryVolume.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "TimerManager.h"

UTerritoryHidingSpotComponent::UTerritoryHidingSpotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SetGenerateOverlapEvents(true);
}

void UTerritoryHidingSpotComponent::BeginPlay()
{
	Super::BeginPlay();
	SetSphereRadius(FMath::Max(10.f, EnterRadius), true);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	// A short timer keeps crouch toggles inside the spot responsive without a per-frame tick.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this,
			&UTerritoryHidingSpotComponent::RefreshHiddenState, 0.25f, true);
		RefreshHiddenState();
	}
}

void UTerritoryHidingSpotComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	if (UTerritoryControlSubsystem* Control = GetWorld()
		? GetWorld()->GetSubsystem<UTerritoryControlSubsystem>() : nullptr)
	{
		for (const TPair<TWeakObjectPtr<AActor>, TWeakObjectPtr<ATerritoryVolume>>& Pair : HiddenActors)
		{
			if (AActor* Actor = Pair.Key.Get(); Actor && Pair.Value.IsValid())
			{
				Control->SetTargetHidden(Pair.Value.Get(), Actor, false);
			}
		}
	}
	HiddenActors.Reset();
	Super::EndPlay(EndPlayReason);
}

void UTerritoryHidingSpotComponent::RefreshHiddenState()
{
	UWorld* World = GetWorld();
	if (!World || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>();
	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	ATerritoryVolume* Territory = Registry
		? Registry->GetTerritoryAtLocation(GetComponentLocation()) : nullptr;

	const bool bSpotDisabled = !Control || !Territory
		|| (bRequiresStealthPlace && !Control->IsStealthInfiltrationEnabled(Territory));

	TArray<AActor*> Overlapping;
	if (!bSpotDisabled)
	{
		GetOverlappingActors(Overlapping, ACharacter::StaticClass());
	}

	// Clear pawns that left, stopped crouching, or whose spot became disabled.
	for (auto It = HiddenActors.CreateIterator(); It; ++It)
	{
		AActor* Actor = It->Key.Get();
		const ACharacter* Character = Cast<ACharacter>(Actor);
		const bool bStillHidden = !bSpotDisabled && Actor && Character
			&& Overlapping.Contains(Actor)
			&& (!bRequiresCrouch || Character->bIsCrouched);
		if (bStillHidden)
		{
			continue;
		}
		if (Actor && It->Value.IsValid())
		{
			Control->SetTargetHidden(It->Value.Get(), Actor, false);
		}
		It.RemoveCurrent();
	}

	// Register the pawns that are hidden now (idempotent on the authority).
	for (AActor* Actor : Overlapping)
	{
		const ACharacter* Character = Cast<ACharacter>(Actor);
		if (!Character || (bRequiresCrouch && !Character->bIsCrouched))
		{
			continue;
		}
		Control->SetTargetHidden(Territory, Actor, true);
		HiddenActors.Add(Actor, Territory);
	}
}
