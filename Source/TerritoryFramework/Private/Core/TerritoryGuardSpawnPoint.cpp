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

void ATerritoryGuardSpawnPoint::BindToTerritory(ATerritoryVolume* Territory)
{
	if (!Territory) return;

	const FGameplayTag TerritoryTag = Territory->GetTerritoryTag();
	if (OwnerTerritoryTag.IsValid() && OwnerTerritoryTag != TerritoryTag)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("GuardSpawnPoint %s owner tag %s overridden by territory reference %s"),
			*GetName(), *OwnerTerritoryTag.ToString(), *TerritoryTag.ToString());
	}

	CachedTerritory = Territory;
	OwnerTerritoryTag = TerritoryTag;

	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(this, &ATerritoryGuardSpawnPoint::OnTerritoryRegistered);
		}
	}
}

void ATerritoryGuardSpawnPoint::BeginPlay()
{
	Super::BeginPlay();
	ResolveOwningTerritory();
	InitializeReserves();

	// Untagged points keep listening so a more-specific streamed territory can replace
	// an earlier proximity match (for example, Property replacing City).
	if (!OwnerTerritoryTag.IsValid() || !CachedTerritory.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
			{
				Registry->OnTerritoryRegistered.AddDynamic(this, &ATerritoryGuardSpawnPoint::OnTerritoryRegistered);
			}
		}
	}
}

void ATerritoryGuardSpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(this, &ATerritoryGuardSpawnPoint::OnTerritoryRegistered);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ATerritoryGuardSpawnPoint::OnTerritoryRegistered(ATerritoryVolume* Territory, bool bIsNew)
{
	if (!Territory) return;

	if (OwnerTerritoryTag.IsValid())
	{
		if (!CachedTerritory.IsValid() && Territory->GetTerritoryTag() == OwnerTerritoryTag)
		{
			CachedTerritory = Territory;
			UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s late-bound to territory %s"),
				*GetName(), *OwnerTerritoryTag.ToString());

			if (UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
			{
				Registry->OnTerritoryRegistered.RemoveDynamic(this, &ATerritoryGuardSpawnPoint::OnTerritoryRegistered);
			}
		}
		return;
	}

	if (UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		ATerritoryVolume* ResolvedTerritory = Registry->GetTerritoryAtLocation(GetActorLocation());
		if (ResolvedTerritory && CachedTerritory.Get() != ResolvedTerritory)
		{
			CachedTerritory = ResolvedTerritory;
			UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s proximity-bound to territory %s"),
				*GetName(), *ResolvedTerritory->GetTerritoryTag().ToString());
		}
	}
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

	// Explicit tags must not fall back to a different territory while their target streams in.
	if (OwnerTerritoryTag.IsValid())
	{
		CachedTerritory = Registry->GetTerritoryByTag(OwnerTerritoryTag);
	}
	else
	{
		CachedTerritory = Registry->GetTerritoryAtLocation(GetActorLocation());
	}

	if (CachedTerritory.IsValid())
	{
		UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s resolved to territory %s"),
			*GetName(), *CachedTerritory->GetTerritoryTag().ToString());
	}
	else
	{
		UE_LOG(LogTerritory, Warning, TEXT("GuardSpawnPoint %s could not find owning territory"),
			*GetName());
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
			*GetName(), CurrentReserveCount);

		// Spawn ONE replacement guard — not a full batch
		if (ATerritoryVolume* Territory = GetOwningTerritory())
		{
			Territory->SpawnSingleGuard(this);
		}
	}
	else
	{
		UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s: guard died, no reserves"),
			*GetName());
	}
}

TArray<FTerritoryPatrolNode> ATerritoryGuardSpawnPoint::GetPatrolRoute() const
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

TArray<FTransform> ATerritoryGuardSpawnPoint::GetPatrolRouteAsTransforms() const
{
	TArray<FTransform> Transforms;
	Transforms.Reserve(PatrolRoute.Num());
	for (const FTerritoryPatrolNode& Node : PatrolRoute)
	{
		Transforms.Add(FTransform(Node.Rotation, Node.Location));
	}
	return Transforms;
}

TArray<float> ATerritoryGuardSpawnPoint::GetPatrolWaitTimes() const
{
	TArray<float> Times;
	Times.Reserve(PatrolRoute.Num());
	for (const FTerritoryPatrolNode& Node : PatrolRoute)
	{
		Times.Add(Node.WaitTime);
	}
	return Times;
}
