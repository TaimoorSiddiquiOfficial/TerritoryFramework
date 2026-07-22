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
	if (!CachedTerritory.IsValid())
	{
		UE_LOG(LogTerritory, Warning, TEXT("[NavMarker] Owner is not an ATerritoryVolume — marker component inactive on %s"),
			*GetOwner()->GetName());
		return;
	}
	{
		CachedTerritory->OnTerritoryOwnershipChanged.AddDynamic(this,
			&UTerritoryNavigationMarkerComponent::OnTerritoryControlChanged);
		CachedTerritory->OnTerritoryStateChangedDelegate.AddDynamic(this,
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
	// CRITICAL FIX: Cleanup order matters.
	// 1. Unbind territory delegates first
	// 2. Clear territory binding on the marker
	// 3. Remove marker from navigation subsystem
	// 4. Call Super::EndPlay (parent uses MarkerObject to remove marker)
	// 5. Null references LAST (parent needs MarkerObject during its EndPlay)

	if (CachedTerritory.IsValid())
	{
		CachedTerritory->OnTerritoryOwnershipChanged.RemoveDynamic(this,
			&UTerritoryNavigationMarkerComponent::OnTerritoryControlChanged);
		CachedTerritory->OnTerritoryStateChangedDelegate.RemoveDynamic(this,
			&UTerritoryNavigationMarkerComponent::OnTerritoryStateChanged);
	}

	if (TerritoryMapMarker)
	{
		TerritoryMapMarker->ClearTerritoryBinding();
	}

	// RemoveMarker while MarkerObject is still valid
	RemoveMarker();

	// Call Super BEFORE nulling MarkerObject — parent EndPlay uses MarkerObject
	Super::EndPlay(EndPlayReason);

	// Null references AFTER Super::EndPlay
	TerritoryMapMarker = nullptr;
	CachedTerritory = nullptr;
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
