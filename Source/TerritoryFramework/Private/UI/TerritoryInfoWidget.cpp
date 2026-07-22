#include "UI/TerritoryInfoWidget.h"
#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UTerritoryInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// If a tag was set before construct, resolve and bind
	if (BoundTerritoryTag.IsValid())
	{
		ResolveTerritoryFromTag();
	}
}

void UTerritoryInfoWidget::NativeDestruct()
{
	UnbindDelegates();
	Super::NativeDestruct();
}

void UTerritoryInfoWidget::BindToTerritory(const FGameplayTag& TerritoryTag)
{
	UnbindDelegates();
	BoundTerritoryTag = TerritoryTag;
	ResolveTerritoryFromTag();
}

void UTerritoryInfoWidget::BindToTerritoryAtPlayer()
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC || !PC->GetPawn()) return;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return;

	ATerritoryVolume* Territory = Registry->GetTerritoryAtLocation(PC->GetPawn()->GetActorLocation());
	if (Territory)
	{
		UnbindDelegates();
		BoundTerritory = Territory;
		BoundTerritoryTag = Territory->GetTerritoryTag();
		BindDelegates();
		OnTerritoryBound(Territory);
	}
}

void UTerritoryInfoWidget::UnbindFromTerritory()
{
	UnbindDelegates();
	BoundTerritory = nullptr;
	BoundTerritoryTag = FGameplayTag();
}

ATerritoryVolume* UTerritoryInfoWidget::GetBoundTerritory() const
{
	return BoundTerritory.IsValid() ? BoundTerritory.Get() : nullptr;
}

void UTerritoryInfoWidget::ResolveTerritoryFromTag()
{
	UWorld* World = GetWorld();
	if (!World || !BoundTerritoryTag.IsValid()) return;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return;

	ATerritoryVolume* Territory = Registry->GetTerritoryByTag(BoundTerritoryTag);
	if (Territory)
	{
		BoundTerritory = Territory;
		BindDelegates();
		OnTerritoryBound(Territory);
	}
}

void UTerritoryInfoWidget::BindDelegates()
{
	if (BoundTerritory.IsValid())
	{
		BoundTerritory->OnTerritoryOwnershipChanged.AddDynamic(this, &UTerritoryInfoWidget::HandleControlChanged);
		BoundTerritory->OnTerritoryStateChangedDelegate.AddDynamic(this, &UTerritoryInfoWidget::HandleStateChanged);
	}
}

void UTerritoryInfoWidget::UnbindDelegates()
{
	if (BoundTerritory.IsValid())
	{
		BoundTerritory->OnTerritoryOwnershipChanged.RemoveDynamic(this, &UTerritoryInfoWidget::HandleControlChanged);
		BoundTerritory->OnTerritoryStateChangedDelegate.RemoveDynamic(this, &UTerritoryInfoWidget::HandleStateChanged);
	}
}

void UTerritoryInfoWidget::HandleControlChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	OnTerritoryOwnershipChanged(OldOwner, NewOwner);
}

void UTerritoryInfoWidget::HandleStateChanged(ATerritoryVolume* Territory, ETerritoryState NewState)
{
	OnTerritoryStateChanged(NewState);
}
