#include "Core/TerritoryPatrolPoint.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Components/ArrowComponent.h"
#endif

ATerritoryPatrolPoint::ATerritoryPatrolPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	UBillboardComponent* Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	RootComponent = Billboard;
	Billboard->bHiddenInGame = true;
	Billboard->SetHiddenInGame(true, true);

#if WITH_EDITOR
	UArrowComponent* Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->SetupAttachment(RootComponent);
	Arrow->bHiddenInGame = true;
	Arrow->SetHiddenInGame(true, true);
#endif
}

void ATerritoryPatrolPoint::BeginPlay()
{
	Super::BeginPlay();

	if (CachedTerritory.IsValid()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return;

	if (OwnerTerritoryTag.IsValid())
	{
		if (ATerritoryVolume* Vol = Registry->GetTerritoryByTag(OwnerTerritoryTag))
		{
			BindToTerritory(Vol);
		}
	}
}

void ATerritoryPatrolPoint::BindToTerritory(ATerritoryVolume* Territory)
{
	if (!Territory) return;
	CachedTerritory = Territory;
}

ATerritoryVolume* ATerritoryPatrolPoint::GetOwningTerritory() const
{
	return CachedTerritory.Get();
}

FTransform ATerritoryPatrolPoint::GetPatrolTransform() const
{
	return GetActorTransform();
}

FTerritoryPatrolNode ATerritoryPatrolPoint::ToPatrolNode() const
{
	FTerritoryPatrolNode Node;
	Node.Location = GetActorLocation();
	Node.Rotation = GetActorRotation();
	Node.WaitTime = WaitTime;
	Node.ActivityTag = ActivityTag;
	return Node;
}

#if WITH_EDITOR
void ATerritoryPatrolPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}
#endif
