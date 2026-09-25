#include "Core/TerritoryFloorVolume.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryTypes.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

ATerritoryFloorVolume::ATerritoryFloorVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	// Root, so the volume can be selected and moved in the level viewport.
	FloorBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("FloorBounds"));
	RootComponent = FloorBounds;

	if (UBoxComponent* Box = FloorBounds)
	{
		Box->SetBoxExtent(FVector(800.f, 800.f, 200.f));
		// Floor membership is a pure geometry query through ContainsPoint, so this shape must never
		// participate in physics, overlap events or navigation - the same reasoning that makes
		// ATerritoryVolume::BoundsShape collision-free.
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->SetGenerateOverlapEvents(false);
		Box->SetCanEverAffectNavigation(false);
		Box->SetHiddenInGame(true, true);
		Box->bHiddenInGame = true;
		Box->bVisibleInReflectionCaptures = false;
		Box->ShapeColor = FColor(90, 160, 255);
	}
}

bool ATerritoryFloorVolume::ApplyFloorVolumeDefinition()
{
	if (!PlaceDefinition)
	{
		return false;
	}

	OwnerTerritoryTag = PlaceDefinition->TerritoryTag;
	return OwnerTerritoryTag.IsValid();
}

void ATerritoryFloorVolume::BeginPlay()
{
	Super::BeginPlay();

	if (!FloorBounds)
	{
		UE_LOG(LogTerritory, Error,
			TEXT("FloorVolume %s has no FloorBounds component; this floor claims no space and is not registered."),
			*GetPathName());
		return;
	}

	if (!ApplyFloorVolumeDefinition())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("FloorVolume %s has no Place Definition (or that Place carries no TerritoryTag). "
				 "Floor membership is disabled for this volume."),
			*GetPathName());
		return;
	}

	if (!FloorVolumeGUID.IsValid())
	{
		// A runtime-baked GUID would make the overlap tie-break depend on spawn order, so a missing
		// editor-baked identity is a hard error rather than a lazy fix - the same rule guard posts
		// follow for SpawnPointGUID.
		UE_LOG(LogTerritory, Error,
			TEXT("FloorVolume %s has no editor-baked FloorVolumeGUID; overlapping floors of %s cannot be "
				 "tie-broken deterministically."),
			*GetPathName(), *OwnerTerritoryTag.ToString());
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->RegisterFloorVolume(this);
		}
	}
}

void ATerritoryFloorVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// World Partition stream-out and level teardown both land here. Unregistering is what makes a
	// streamed-out volume resolve to INDEX_NONE instead of leaving a stale pointer behind.
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->UnregisterFloorVolume(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

bool ATerritoryFloorVolume::ContainsPoint(const FVector& WorldPoint) const
{
	if (!FloorBounds) return false;

	// Transform-space containment - does NOT depend on collision geometry. Transforms the world
	// point into the box's local space and compares against the unscaled box extent, so a rotated
	// floor volume is handled correctly. Deliberately identical to ATerritoryVolume::ContainsPoint
	// so the two containment answers cannot drift apart.
	const FTransform& BoxTransform = FloorBounds->GetComponentTransform();
	const FVector LocalPoint = BoxTransform.InverseTransformPosition(WorldPoint);
	const FVector Extent = FloorBounds->GetUnscaledBoxExtent();
	return FMath::Abs(LocalPoint.X) <= Extent.X
		&& FMath::Abs(LocalPoint.Y) <= Extent.Y
		&& FMath::Abs(LocalPoint.Z) <= Extent.Z;
}

FVector ATerritoryFloorVolume::GetFloorBoundsSize() const
{
	return FloorBounds ? FloorBounds->GetUnscaledBoxExtent() * 2.0 : FVector::ZeroVector;
}

double ATerritoryFloorVolume::GetFloorBoundsVolume() const
{
	const FVector Size = GetFloorBoundsSize();
	return FMath::Abs(static_cast<double>(Size.X) * Size.Y * Size.Z);
}

void ATerritoryFloorVolume::EnsurePersistentFloorVolumeGUID()
{
	if (!FloorVolumeGUID.IsValid())
	{
		FloorVolumeGUID = FGuid::NewGuid();
#if WITH_EDITOR
		MarkPackageDirty();
#endif
	}
}

#if WITH_EDITOR
void ATerritoryFloorVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	EnsurePersistentFloorVolumeGUID();
	ApplyFloorVolumeDefinition();
}

void ATerritoryFloorVolume::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	// Normal duplication produces a second volume that would claim the same floor of the same Place
	// with the same identity. Regenerate so the copy is its own region; PIE and world duplication
	// must preserve the editor-authored identity, exactly like ATerritoryGuardSpawnPoint.
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		FloorVolumeGUID = FGuid::NewGuid();
		MarkPackageDirty();
	}
}

void ATerritoryFloorVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyFloorVolumeDefinition();
	if (GetWorld() && !GetWorld()->IsGameWorld()
		&& !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		EnsurePersistentFloorVolumeGUID();
	}
}
#endif
