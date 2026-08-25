#include "Tales/TerritoryLockEvent.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Engine/World.h"
#include "Tales/TerritoryTalesUtilities.h"

UTerritoryLockEvent::UTerritoryLockEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

void UTerritoryLockEvent::ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(this, Target, Controller, NarrativeComponent)) return;
	UWorld* World = GetWorld();
	if (!World || !TargetTerritoryTag.IsValid()) return;

	// Lock/unlock mutations are server-authoritative
	if (World->GetNetMode() == NM_Client) return;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return;

	ATerritoryVolume* Territory = Registry->GetTerritoryByTag(TargetTerritoryTag);
	if (!Territory)
	{
		UE_LOG(LogTerritory, Warning, TEXT("[LockEvent] Territory %s not found"),
			*TargetTerritoryTag.ToString());
		return;
	}

	FTerritoryTransitionContext Context;
	Context.Instigator = Target;
	Context.TargetPawn = Target;
	Context.PlayerController = Controller;
	Context.TalesComponent = NarrativeComponent;
	Territory->LockTerritoryWithContext(LockReason, Context);
}

FString UTerritoryLockEvent::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Lock %s"), *TargetTerritoryTag.ToString());
}

// ─── Unlock Event ───

UTerritoryUnlockEvent::UTerritoryUnlockEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

void UTerritoryUnlockEvent::ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(this, Target, Controller, NarrativeComponent)) return;
	UWorld* World = GetWorld();
	if (!World || !TargetTerritoryTag.IsValid()) return;

	// Lock/unlock mutations are server-authoritative
	if (World->GetNetMode() == NM_Client) return;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return;

	ATerritoryVolume* Territory = Registry->GetTerritoryByTag(TargetTerritoryTag);
	if (!Territory)
	{
		UE_LOG(LogTerritory, Warning, TEXT("[UnlockEvent] Territory %s not found"),
			*TargetTerritoryTag.ToString());
		return;
	}

	FTerritoryTransitionContext Context;
	Context.Instigator = Target;
	Context.TargetPawn = Target;
	Context.PlayerController = Controller;
	Context.TalesComponent = NarrativeComponent;
	Territory->TryUnlockWithContext(Context, bForceUnlock);
}

FString UTerritoryUnlockEvent::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Unlock %s%s"), *TargetTerritoryTag.ToString(), bForceUnlock ? TEXT(" (Force)") : TEXT(""));
}
