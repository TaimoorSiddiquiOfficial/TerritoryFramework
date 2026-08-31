#include "Navigation/TerritoryRoadTrafficActors.h"

#include "Components/BoxComponent.h"

ATerritoryRoadTrafficControls::ATerritoryRoadTrafficControls()
{
	bAutoActivate = false;
	MissionTrafficBounds = CreateDefaultSubobject<UBoxComponent>(
		TEXT("Mission Traffic Bounds"));
	MissionTrafficBounds->SetupAttachment(GetRootComponent());
	MissionTrafficBounds->SetBoxExtent(FVector(5000.f, 5000.f, 1500.f));
	MissionTrafficBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATerritoryRoadTrafficControls::SetMissionTrafficWorldBounds(
	const FBox& WorldBounds)
{
	if (!MissionTrafficBounds || !WorldBounds.IsValid) return;
	const FVector Centre = WorldBounds.GetCenter();
	const FVector Extent = WorldBounds.GetExtent().ComponentMax(
		FVector(1000.f, 1000.f, 500.f));
	SetActorLocation(Centre);
	MissionTrafficBounds->SetRelativeLocation(FVector::ZeroVector);
	MissionTrafficBounds->SetBoxExtent(Extent);
	MissionTrafficBounds->MarkRenderStateDirty();
	MarkPackageDirty();
}
