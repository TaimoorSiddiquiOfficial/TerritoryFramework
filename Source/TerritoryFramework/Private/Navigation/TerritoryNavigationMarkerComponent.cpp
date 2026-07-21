#include "Navigation/TerritoryNavigationMarkerComponent.h"
#include "Navigation/TerritoryMapMarker.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"

UTerritoryNavigationMarkerComponent::UTerritoryNavigationMarkerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTerritoryNavigationMarkerComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedTerritory = Cast<ATerritoryVolume>(GetOwner());
	if (CachedTerritory.IsValid())
	{
		CachedTerritory->OnTerritoryControlChanged.AddDynamic(this,
			&UTerritoryNavigationMarkerComponent::OnTerritoryControlChanged);
		CachedTerritory->OnTerritoryStateChanged.AddDynamic(this,
			&UTerritoryNavigationMarkerComponent::OnTerritoryStateChanged);

		// Create the territory map marker
		TerritoryMapMarker = NewObject<UTerritoryMapMarker>(this);
		if (TerritoryMapMarker)
		{
			TerritoryMapMarker->SetTerritoryVolume(CachedTerritory.Get());

			// Assign to parent's MarkerObject so NavigationMarkerComponent manages it
			MarkerObject = TerritoryMapMarker;

			// Register with navigation subsystem
			RegisterMarker();
		}
	}
}

void UTerritoryNavigationMarkerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CachedTerritory.IsValid())
	{
		CachedTerritory->OnTerritoryControlChanged.RemoveDynamic(this,
			&UTerritoryNavigationMarkerComponent::OnTerritoryControlChanged);
		CachedTerritory->OnTerritoryStateChanged.RemoveDynamic(this,
			&UTerritoryNavigationMarkerComponent::OnTerritoryStateChanged);
	}

	TerritoryMapMarker = nullptr;
	CachedTerritory = nullptr;

	Super::EndPlay(EndPlayReason);
}

UTerritoryMapMarker* UTerritoryNavigationMarkerComponent::GetTerritoryMapMarker() const
{
	return TerritoryMapMarker;
}

ATerritoryVolume* UTerritoryNavigationMarkerComponent::GetOwningTerritory() const
{
	return CachedTerritory.IsValid() ? CachedTerritory.Get() : nullptr;
}

void UTerritoryNavigationMarkerComponent::RefreshTerritoryMarker()
{
	if (TerritoryMapMarker)
	{
		TerritoryMapMarker->RefreshMarker();
	}
}

void UTerritoryNavigationMarkerComponent::OnTerritoryControlChanged(
	ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	RefreshTerritoryMarker();
}

void UTerritoryNavigationMarkerComponent::OnTerritoryStateChanged(
	ATerritoryVolume* Territory, ETerritoryState NewState)
{
	RefreshTerritoryMarker();
}
