#include "Tales/TerritoryOwnershipCondition.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TalesComponent.h"
#include "Engine/World.h"

bool UTerritoryOwnershipCondition::CheckCondition_Implementation(APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryToCheck.IsValid()) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return false;

	ATerritoryVolume* Territory = Registry->GetTerritoryByTag(TerritoryToCheck);
	if (!Territory) return false;

	ETerritoryState State = Territory->GetTerritoryState();

	if (bPassWhenLocked && State == ETerritoryState::Locked)
	{
		return true;
	}

	if (bPassWhenContested && State == ETerritoryState::Contested)
	{
		return true;
	}

	if (bPassWhenUnclaimed && State == ETerritoryState::Unclaimed)
	{
		return true;
	}

	if (RequiredOwner.IsValid())
	{
		return Territory->IsOwnedByFaction(RequiredOwner);
	}

	return State == ETerritoryState::Claimed;
}

FString UTerritoryOwnershipCondition::GetGraphDisplayText_Implementation()
{
	FString Text = FString::Printf(TEXT("Territory: %s"), *TerritoryToCheck.ToString());

	if (RequiredOwner.IsValid())
	{
		Text += FString::Printf(TEXT(" owned by %s"), *RequiredOwner.ToString());
	}

	if (bPassWhenContested) Text += TEXT(" [or contested]");
	if (bPassWhenUnclaimed) Text += TEXT(" [or unclaimed]");
	if (bPassWhenLocked) Text += TEXT(" [or locked]");

	return Text;
}
