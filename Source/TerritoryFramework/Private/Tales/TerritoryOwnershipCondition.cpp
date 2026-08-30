#include "Tales/TerritoryOwnershipCondition.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "Engine/World.h"

bool UTerritoryOwnershipCondition::CheckCondition_Implementation(APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryToCheck.IsValid())
	{
		UE_LOG(LogTerritory, Warning, TEXT("[TalesOwnershipCondition] No TerritoryToCheck tag set"));
		return false;
	}

	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	if (!World) return false;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return false;

	ATerritoryVolume* Territory = Registry->GetTerritoryByTag(TerritoryToCheck);
	if (!Territory)
	{
		// Warning-level so designers catch unregistered territory references during testing.
		// Territories may not be registered yet during early quest evaluation (save/load, streaming).
		UE_LOG(LogTerritory, Warning, TEXT("[TalesOwnershipCondition] Territory '%s' not found in registry — condition returns false"),
			*TerritoryToCheck.ToString());
		return false;
	}

	ETerritoryState State = Territory->GetTerritoryState();

	// State-based flags are checked first — if any matches, condition passes
	// (these are OR conditions for special states, independent of RequiredOwner)
	if (bPassWhenLocked && Territory->IsLocked()) return true;
	if (bPassWhenContested && State == ETerritoryState::Contested) return true;
	if (bPassWhenUnclaimed && State == ETerritoryState::Unclaimed) return true;

	// If RequiredOwner is set, check that the territory is owned by that faction.
	// This is an AND with the default "Claimed" state — the owner must match.
	if (RequiredOwner.IsValid())
	{
		return Territory->IsOwnedByFaction(RequiredOwner);
	}

	// Empty RequiredOwner is dynamic, not a hidden Heroes/Bandits hardcode. State
	// conditions receive the pawn/controller carried by the capture or quest event.
	// When that actor has a Narrative faction, require the checked Territory to be
	// owned by the same faction. World-only recovery calls retain the safe legacy
	// behavior below and accept any valid Claimed owner.
	FGameplayTag ContextFaction =
		UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Target);
	if (!ContextFaction.IsValid() && Controller)
	{
		ContextFaction =
			UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Controller);
		if (!ContextFaction.IsValid())
		{
			ContextFaction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(
				this, Controller->GetPawn());
		}
	}
	if (ContextFaction.IsValid())
	{
		return Territory->IsOwnedByFaction(ContextFaction);
	}

	// Default: pass if the territory is in the Claimed state
	return State == ETerritoryState::Claimed;
}

FString UTerritoryOwnershipCondition::GetGraphDisplayText_Implementation()
{
	FString Text = FString::Printf(TEXT("Territory: %s"), *TerritoryToCheck.ToString());

	if (RequiredOwner.IsValid())
	{
		Text += FString::Printf(TEXT(" owned by %s"), *RequiredOwner.ToString());
	}
	else
	{
		Text += TEXT(" owned by Narrative target faction (or any owner without context)");
	}

	if (bPassWhenContested) Text += TEXT(" [or contested]");
	if (bPassWhenUnclaimed) Text += TEXT(" [or unclaimed]");
	if (bPassWhenLocked) Text += TEXT(" [or locked]");

	return Text;
}
