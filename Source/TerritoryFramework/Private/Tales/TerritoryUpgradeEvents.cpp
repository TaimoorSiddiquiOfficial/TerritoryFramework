#include "Tales/TerritoryUpgradeEvents.h"

#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryUpgradeDefinition.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryUpgradeSubsystem.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryTalesUtilities.h"

UTerritoryPlaceCapturedEvent::UTerritoryPlaceCapturedEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// This event reports a capture that already happened, so replaying it while a quest loads
	// would re-deliver a mutation the save already accounts for (NarrativeEvent.h documents the
	// hazard for exactly this kind of event).
	bRefireOnLoad = false;
}

UTerritoryUpgradePlaceEvent::UTerritoryUpgradePlaceEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Same rule as UTerritoryPlaceCapturedEvent: an upgrade is a mutation, not a state readout.
	bRefireOnLoad = false;
}

void UTerritoryPlaceCapturedEvent::SetPayload(ATerritoryVolume* Place, FGameplayTag InPreviousOwner,
	FGameplayTag InNewOwner, int32 InOldUpgradeLevel)
{
	CapturedPlace = Place;
	PreviousOwner = InPreviousOwner;
	NewOwner = InNewOwner;
	OldUpgradeLevel = FMath::Max(0, InOldUpgradeLevel);
}

void UTerritoryPlaceCapturedEvent::ClearPayload()
{
	CapturedPlace = nullptr;
	PreviousOwner = FGameplayTag();
	NewOwner = FGameplayTag();
	OldUpgradeLevel = 0;
}

bool UTerritoryPlaceCapturedEvent::Matches(const ATerritoryVolume* Place) const
{
	if (!IsValid(Place))
	{
		return false;
	}
	if (PlaceTag.IsValid() && Place->GetTerritoryTag() != PlaceTag)
	{
		return false;
	}
	if (NewOwnerFilter.IsValid() && NewOwner != NewOwnerFilter)
	{
		return false;
	}
	return OldUpgradeLevel >= FMath::Max(0, MinOldLevel);
}

void UTerritoryPlaceCapturedEvent::ExecuteEventInContext(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent)
{
	if (!IsValid(CapturedPlace) || !Matches(CapturedPlace))
	{
		return;
	}
 Super::ExecuteEventInContext(Target, Controller, NarrativeComponent);
}

void UTerritoryUpgradePlaceEvent::ExecuteEventInContext(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent)
{
 UWorld* World = TerritoryTales::ResolveWorld(this, Target, nullptr, nullptr);
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	UTerritoryUpgradeSubsystem* Upgrades = World
		? World->GetSubsystem<UTerritoryUpgradeSubsystem>() : nullptr;
	ATerritoryProperty* Place = Registry && PlaceTag.IsValid()
		? Cast<ATerritoryProperty>(Registry->GetTerritoryByTag(PlaceTag)) : nullptr;
	if (!Place || !Upgrades)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[UpgradePlaceEvent] %s could not resolve Place '%s' or the upgrade subsystem."),
			*GetName(), *PlaceTag.ToString());
		return;
	}

	const FGameplayTag RequestingFaction = Target
		? UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Target) : FGameplayTag();
	if (!RequestingFaction.IsValid() && !bAllowForeignOwnership)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[UpgradePlaceEvent] %s has no requesting faction for %s."),
			*GetName(), *PlaceTag.ToString());
		return;
	}

	FString Failure;
	if (!Upgrades->ApplyScriptedUpgrade(Place, TargetLevel, RequestingFaction,
		bAllowForeignOwnership, bFreeUpgrade, Target, Failure))
	{
		UE_LOG(LogTerritory, Warning, TEXT("[UpgradePlaceEvent] %s failed for %s -> level %d: %s"),
			*GetName(), *PlaceTag.ToString(), TargetLevel, *Failure);
		return;
	}
	UE_LOG(LogTerritory, Log, TEXT("[UpgradePlaceEvent] %s upgraded %s to level %d."),
		*GetName(), *PlaceTag.ToString(), TargetLevel);
}
