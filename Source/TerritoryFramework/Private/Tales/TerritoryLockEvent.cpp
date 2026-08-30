#include "Tales/TerritoryLockEvent.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Engine/World.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "Tales/TalesComponent.h"

UTerritoryLockEvent::UTerritoryLockEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

void UTerritoryLockEvent::ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(this, Target, Controller, NarrativeComponent)) return;
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
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
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
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
	if (const ATerritoryVolume* SourceTerritory = GetTypedOuter<ATerritoryVolume>();
		SourceTerritory && SourceTerritory->IsOwnershipTransitionActive())
	{
		// Preserve the faction and instigator that caused the source Territory event.
		// This is essential for a captured Place unlocking another Place whose Locked
		// Exit Conditions use the live capturing faction instead of a hardcoded tag.
		Context = SourceTerritory->GetActiveTransitionContext();
	}
	if (!Context.Instigator) Context.Instigator = Target;
	if (!Context.TargetPawn) Context.TargetPawn = Target;
	if (!Context.PlayerController) Context.PlayerController = Controller;
	if (!Context.TalesComponent) Context.TalesComponent = NarrativeComponent;
	if (!Context.RequestingFaction.IsValid())
	{
		Context.RequestingFaction =
			UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Context.TargetPawn);
		if (!Context.RequestingFaction.IsValid())
		{
			Context.RequestingFaction =
				UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Context.PlayerController);
		}
	}
	UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>();
	if (!Control) return;
	ETerritoryUnlockScope EffectiveScope = UnlockScope;
	if (bForceUnlock)
	{
		// Bounded compatibility for already-saved inline Narrative events. The field
		// is hidden/deprecated; resaving the asset should use an explicit Force scope.
		EffectiveScope = UnlockScope == ETerritoryUnlockScope::ExactOnly
			? ETerritoryUnlockScope::ForceExact : ETerritoryUnlockScope::ForceHierarchy;
	}
	const FTerritoryUnlockCascadeResult Result = Control->ApplyUnlockCascade(
		Territory, Context, EffectiveScope);
	if (!Result.bTargetSucceeded)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[UnlockEvent] %s remained locked/blocked (scope=%d, blocked=%d). Check the result rows and local Locked Exit Conditions."),
			*TargetTerritoryTag.ToString(), static_cast<int32>(EffectiveScope), Result.BlockedCount);
		for (const FTerritoryUnlockResultRow& Row : Result.Results)
		{
			UE_LOG(LogTerritory, Warning,
				TEXT("[UnlockEvent]   %s outcome=%d reason=%s"),
				*Row.TerritoryTag.ToString(), static_cast<int32>(Row.Outcome),
				*Row.Reason.ToString());
		}
		return;
	}

	if (const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
		Settings && (Settings->ShouldDebugTales()
			|| Settings->ShouldDebugAvailability()))
	{
		UE_LOG(LogTerritory, Log,
			TEXT("[UnlockEvent] %s applied (scope=%d, unlocked=%d, blocked=%d)"),
			*TargetTerritoryTag.ToString(), static_cast<int32>(EffectiveScope),
			Result.UnlockedCount, Result.BlockedCount);
	}
}

FString UTerritoryUnlockEvent::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Unlock %s (Scope %d)"),
		*TargetTerritoryTag.ToString(), static_cast<int32>(UnlockScope));
}
