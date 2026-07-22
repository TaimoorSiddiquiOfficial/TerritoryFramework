#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Engine/World.h"
#include "Components/BillboardComponent.h"
#include "NavigationSystem.h"

#if WITH_EDITOR
#include "Components/ArrowComponent.h"
#endif

ATerritoryGuardSpawnPoint::ATerritoryGuardSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	// Visual root — billboard in editor, invisible in game
	UBillboardComponent* Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	RootComponent = Billboard;
	Billboard->bHiddenInGame = true;
	Billboard->SetHiddenInGame(true, true);

	CurrentReserveCount = 0;
}

void ATerritoryGuardSpawnPoint::BeginPlay()
{
	Super::BeginPlay();
	ResolveOwningTerritory();
	InitializeReserves();
}

#if WITH_EDITOR
void ATerritoryGuardSpawnPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Editor visualization is handled by the visual properties
}
#endif

void ATerritoryGuardSpawnPoint::ResolveOwningTerritory()
{
	UWorld* World = GetWorld();
	if (!World) return;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return;

	// First try by tag
	if (OwnerTerritoryTag.IsValid())
	{
		CachedTerritory = Registry->GetTerritoryByTag(OwnerTerritoryTag);
	}

	// Fallback: find by proximity
	if (!CachedTerritory.IsValid())
	{
		CachedTerritory = Registry->GetTerritoryAtLocation(GetActorLocation());
		if (CachedTerritory.IsValid())
		{
			OwnerTerritoryTag = CachedTerritory->GetTerritoryTag();
		}
	}

	if (CachedTerritory.IsValid())
	{
		UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s resolved to territory %s"),
			*GetActorLabel(), *OwnerTerritoryTag.ToString());
	}
	else
	{
		UE_LOG(LogTerritory, Warning, TEXT("GuardSpawnPoint %s could not find owning territory"),
			*GetActorLabel());
	}
}

void ATerritoryGuardSpawnPoint::InitializeReserves()
{
	CurrentReserveCount = ReserveSlots;
}

FTransform ATerritoryGuardSpawnPoint::GetSpawnTransform() const
{
	FTransform Transform = GetActorTransform();

	// Project spawn location to NavMesh so guards start on walkable ground
	FVector SpawnLoc = Transform.GetLocation();
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation ProjectedLoc;
		if (NavSys->ProjectPointToNavigation(SpawnLoc, ProjectedLoc, FVector(500.f, 500.f, 500.f)))
		{
			Transform.SetLocation(ProjectedLoc.Location);
		}
	}

	return Transform;
}

bool ATerritoryGuardSpawnPoint::HasAvailableSlot() const
{
	return GetActiveGuardCount() < MaxGuards;
}

bool ATerritoryGuardSpawnPoint::HasReserveAvailable() const
{
	return CurrentReserveCount > 0;
}

int32 ATerritoryGuardSpawnPoint::GetActiveGuardCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ATerritoryGuardCharacter>& Ptr : ActiveGuards)
	{
		if (Ptr.IsValid()) ++Count;
	}
	return Count;
}

int32 ATerritoryGuardSpawnPoint::GetReserveCount() const
{
	return CurrentReserveCount;
}

void ATerritoryGuardSpawnPoint::RegisterSpawnedGuard(ATerritoryGuardCharacter* Guard)
{
	if (!Guard) return;
	ActiveGuards.Add(Guard);
}

void ATerritoryGuardSpawnPoint::UnregisterGuard(ATerritoryGuardCharacter* Guard)
{
	if (!Guard) return;

	// CRITICAL FIX: Only process if this spawn point actually tracked this guard.
	// Without this check, a guard dying at SP_1 would also drain reserves at SP_2, SP_3, etc.
	bool bWasTracked = false;
	for (const TWeakObjectPtr<ATerritoryGuardCharacter>& Ptr : ActiveGuards)
	{
		if (Ptr.Get() == Guard)
		{
			bWasTracked = true;
			break;
		}
	}

	if (!bWasTracked) return; // Not our guard — no-op

	ActiveGuards.RemoveAll([Guard](const TWeakObjectPtr<ATerritoryGuardCharacter>& Ptr)
	{
		return !Ptr.IsValid() || Ptr.Get() == Guard;
	});

	// If we have reserves, consume one to signal a replacement should spawn
	if (CurrentReserveCount > 0)
	{
		CurrentReserveCount--;
		UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s: guard died, using reserve (%d remaining)"),
			*GetActorLabel(), CurrentReserveCount);

		// Spawn ONE replacement guard — not a full batch
		if (ATerritoryVolume* Territory = GetOwningTerritory())
		{
			Territory->SpawnSingleGuard(this);
		}
	}
	else
	{
		UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s: guard died, no reserves"),
			*GetActorLabel());
	}
}

const TArray<FTerritoryPatrolNode>& ATerritoryGuardSpawnPoint::GetPatrolRoute() const
{
	return PatrolRoute;
}

ATerritoryVolume* ATerritoryGuardSpawnPoint::GetOwningTerritory() const
{
	return CachedTerritory.IsValid() ? CachedTerritory.Get() : nullptr;
}

bool ATerritoryGuardSpawnPoint::HasPatrolRoute() const
{
	return PatrolRoute.Num() >= 2;
}
