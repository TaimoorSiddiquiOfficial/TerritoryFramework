#include "Navigation/TerritoryNavigationMarkerComponent.h"
#include "Navigation/TerritoryMapMarker.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"

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
	if (UTerritoryRegistrySubsystem* Registry =
		GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		Registry->OnTerritoryRegistered.AddUniqueDynamic(
			this, &UTerritoryNavigationMarkerComponent::OnRegistryTerritoryChanged);
		Registry->OnTerritoryUnregistered.AddUniqueDynamic(
			this, &UTerritoryNavigationMarkerComponent::OnRegistryTerritoryChanged);
	}
	{
		// Don't bind component-level delegates — the TerritoryMapMarker already binds
		// to these same delegates in SetTerritoryVolume and handles RefreshMarker directly.
		// Binding here would cause double-refresh per state change.

		// Create the territory map marker
		TerritoryMapMarker = NewObject<UTerritoryMapMarker>(this);
		if (TerritoryMapMarker)
		{
			// Assign to parent's MarkerObject so NavigationMarkerComponent manages it
			MarkerObject = TerritoryMapMarker;
			TerritoryMapMarker->SetTerritoryVolume(CachedTerritory.Get());
		}
	}
	RefreshAncestorAvailabilityBindings();
}

void UTerritoryNavigationMarkerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(
				this, &UTerritoryNavigationMarkerComponent::OnRegistryTerritoryChanged);
			Registry->OnTerritoryUnregistered.RemoveDynamic(
				this, &UTerritoryNavigationMarkerComponent::OnRegistryTerritoryChanged);
		}
	}
	if (TerritoryMapMarker)
	{
		TerritoryMapMarker->ClearTerritoryBinding();
	}
	for (const TWeakObjectPtr<ATerritoryVolume>& AncestorPtr : BoundAvailabilityAncestors)
	{
		if (ATerritoryVolume* Ancestor = AncestorPtr.Get())
		{
			Ancestor->OnTerritoryAvailabilityChanged.RemoveDynamic(
				this, &UTerritoryNavigationMarkerComponent::OnAncestorAvailabilityChanged);
		}
	}
	BoundAvailabilityAncestors.Empty();

	// ClearTerritoryBinding performs the one normal runtime unregister. During
	// world teardown it deliberately emits no Narrative UI callbacks because the
	// owning player/HUD may already be gone. Nulling prevents the base component
	// from issuing a duplicate removal in either path.
	MarkerObject = nullptr;
	Super::EndPlay(EndPlayReason);

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
		TerritoryMapMarker->RefreshTerritoryPresentation();
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

void UTerritoryNavigationMarkerComponent::OnRegistryTerritoryChanged(
	ATerritoryVolume* Territory, bool bWasUnregistered)
{
	if (!CachedTerritory.IsValid() || !Territory) return;
	RefreshAncestorAvailabilityBindings();
	const FGameplayTag ChangedTag = Territory->GetTerritoryTag();
	if (Territory == CachedTerritory.Get())
	{
		RefreshTerritoryMarker();
		return;
	}

	TSet<FGameplayTag> Visited;
	FGameplayTag ParentTag = CachedTerritory->GetParentTerritoryTag();
	while (ParentTag.IsValid() && !Visited.Contains(ParentTag))
	{
		if (ParentTag == ChangedTag)
		{
			RefreshTerritoryMarker();
			return;
		}
		Visited.Add(ParentTag);
		const UTerritoryRegistrySubsystem* Registry =
			GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
		const ATerritoryVolume* Parent = Registry
			? Registry->GetTerritoryByTag(ParentTag) : nullptr;
		if (!Parent) return;
		ParentTag = Parent->GetParentTerritoryTag();
	}
}

void UTerritoryNavigationMarkerComponent::OnAncestorAvailabilityChanged(
	ATerritoryVolume* Territory, ETerritoryAvailability NewAvailability)
{
	(void)Territory;
	(void)NewAvailability;
	RefreshTerritoryMarker();
}

void UTerritoryNavigationMarkerComponent::RefreshAncestorAvailabilityBindings()
{
	for (const TWeakObjectPtr<ATerritoryVolume>& AncestorPtr : BoundAvailabilityAncestors)
	{
		if (ATerritoryVolume* Ancestor = AncestorPtr.Get())
		{
			Ancestor->OnTerritoryAvailabilityChanged.RemoveDynamic(
				this, &UTerritoryNavigationMarkerComponent::OnAncestorAvailabilityChanged);
		}
	}
	BoundAvailabilityAncestors.Empty();

	if (!CachedTerritory.IsValid() || !GetWorld()) return;
	UTerritoryRegistrySubsystem* Registry =
		GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return;

	TSet<FGameplayTag> Visited;
	FGameplayTag ParentTag = CachedTerritory->GetParentTerritoryTag();
	while (ParentTag.IsValid() && !Visited.Contains(ParentTag))
	{
		Visited.Add(ParentTag);
		ATerritoryVolume* Parent = Registry->GetTerritoryByTag(ParentTag);
		if (!Parent) break;
		Parent->OnTerritoryAvailabilityChanged.AddUniqueDynamic(
			this, &UTerritoryNavigationMarkerComponent::OnAncestorAvailabilityChanged);
		BoundAvailabilityAncestors.Add(Parent);
		ParentTag = Parent->GetParentTerritoryTag();
	}
}
