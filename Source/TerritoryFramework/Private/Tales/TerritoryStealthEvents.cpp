#include "Tales/TerritoryStealthEvents.h"

#include "Core/TerritoryVolume.h"
#include "Core/TerritoryDisguiseProfile.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryDisguiseSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TerritoryTalesUtilities.h"

namespace
{
	ATerritoryVolume* ResolveEventTerritory(const UObject* Context, UWorld* World,
		const FGameplayTag& TerritoryTag, const APawn* Target)
	{
		if (ATerritoryVolume* Containing = Context
			? Context->GetTypedOuter<ATerritoryVolume>() : nullptr)
		{
			if (!TerritoryTag.IsValid() || Containing->GetTerritoryTag() == TerritoryTag)
			{
				return Containing;
			}
		}
		UTerritoryRegistrySubsystem* Registry = World
			? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
		if (!Registry) return nullptr;
		if (TerritoryTag.IsValid()) return Registry->GetTerritoryByTag(TerritoryTag);
		return Target ? Registry->GetTerritoryAtLocation(Target->GetActorLocation()) : nullptr;
	}

	UTerritoryControlSubsystem* ResolveControl(UWorld* World)
	{
		return World && World->GetNetMode() != NM_Client
			? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
	}
}

UTerritorySetStealthOverrideEvent::UTerritorySetStealthOverrideEvent(
	const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

void UTerritorySetStealthOverrideEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(this, Target, Controller, NarrativeComponent)) return;
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	if (UTerritoryControlSubsystem* Control = ResolveControl(World))
	{
		Control->SetStealthInfiltrationOverride(
			ResolveEventTerritory(this, World, TerritoryToModify, Target),
			bEnableInfiltration, bClearOverride);
	}
}

FString UTerritorySetStealthOverrideEvent::GetGraphDisplayText_Implementation()
{
	return bClearOverride ? TEXT("Clear Territory stealth override")
		: FString::Printf(TEXT("Set Territory stealth infiltration %s"),
			bEnableInfiltration ? TEXT("enabled") : TEXT("disabled"));
}

UTerritoryRevealInfiltratorEvent::UTerritoryRevealInfiltratorEvent(
	const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bRefireOnLoad = false;
	EventFilter = EEventFilter::EF_OnlyPlayers;
}

void UTerritoryRevealInfiltratorEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(this, Target, Controller, NarrativeComponent)
		|| !Target) return;
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	if (UTerritoryControlSubsystem* Control = ResolveControl(World))
	{
		Control->ReportStealthEvidence(
			ResolveEventTerritory(this, World, TerritoryToModify, Target), Target, nullptr,
			ETerritoryStealthEvidence::Scripted, 1.f, Target->GetActorLocation(),
			FVector::ZeroVector, true);
	}
}

FString UTerritoryRevealInfiltratorEvent::GetGraphDisplayText_Implementation()
{
	return TEXT("Reveal the explicit Territory infiltrator");
}

UTerritoryClearExposureEvent::UTerritoryClearExposureEvent(
	const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bRefireOnLoad = false;
	EventFilter = EEventFilter::EF_OnlyPlayers;
}

void UTerritoryClearExposureEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(this, Target, Controller, NarrativeComponent)
		|| !Target) return;
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	if (UTerritoryControlSubsystem* Control = ResolveControl(World))
	{
		Control->ClearInfiltratorExposure(
			ResolveEventTerritory(this, World, TerritoryToModify, Target),
			Target, bResetSuspicion);
	}
}

FString UTerritoryClearExposureEvent::GetGraphDisplayText_Implementation()
{
	return bResetSuspicion ? TEXT("Clear Territory exposure and suspicion")
		: TEXT("Clear Territory exposure but keep suspicion");
}

UTerritoryReportDistractionEvent::UTerritoryReportDistractionEvent(
	const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bRefireOnLoad = false;
	EventFilter = EEventFilter::EF_OnlyPlayers;
}

void UTerritoryReportDistractionEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(this, Target, Controller, NarrativeComponent)
		|| !Target) return;
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	ATerritoryVolume* Territory = ResolveEventTerritory(
		this, World, TerritoryToModify, Target);
	if (UTerritoryControlSubsystem* Control = ResolveControl(World))
	{
		Control->ReportStealthEvidence(Territory, Target, nullptr,
			ETerritoryStealthEvidence::ThrowableDistraction, 0.25f,
			Target->GetActorLocation() + LocationOffset, FVector::ZeroVector, false);
	}
}

FString UTerritoryReportDistractionEvent::GetGraphDisplayText_Implementation()
{
	return TEXT("Report an anonymous Territory distraction");
}

UTerritoryActivateDisguiseEvent::UTerritoryActivateDisguiseEvent(
	const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bRefireOnLoad = false;
	EventFilter = EEventFilter::EF_OnlyPlayers;
}

void UTerritoryActivateDisguiseEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(this, Target, Controller, NarrativeComponent)
		|| !Target || !DisguiseProfile) return;
	UWorld* World = TerritoryTales::ResolveWorld(this, Target, Controller, NarrativeComponent);
	if (UTerritoryDisguiseSubsystem* Disguises = ResolveControl(World)
		? World->GetSubsystem<UTerritoryDisguiseSubsystem>() : nullptr)
	{
		Disguises->ActivateDisguise(Target, DisguiseProfile, this);
	}
}

FString UTerritoryActivateDisguiseEvent::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Activate disguise: %s"), *GetNameSafe(DisguiseProfile));
}

UTerritoryRemoveDisguiseEvent::UTerritoryRemoveDisguiseEvent(
	const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bRefireOnLoad = false;
	EventFilter = EEventFilter::EF_OnlyPlayers;
}

void UTerritoryRemoveDisguiseEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(this, Target, Controller, NarrativeComponent)
		|| !Target) return;
	UWorld* World = TerritoryTales::ResolveWorld(this, Target, Controller, NarrativeComponent);
	if (UTerritoryDisguiseSubsystem* Disguises = ResolveControl(World)
		? World->GetSubsystem<UTerritoryDisguiseSubsystem>() : nullptr)
	{
		Disguises->RemoveDisguise(Target, nullptr);
	}
}

FString UTerritoryRemoveDisguiseEvent::GetGraphDisplayText_Implementation()
{
	return TEXT("Remove the player's Territory disguise");
}

UTerritorySetDisguiseCoverEvent::UTerritorySetDisguiseCoverEvent(
	const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bRefireOnLoad = false;
	EventFilter = EEventFilter::EF_OnlyPlayers;
}

void UTerritorySetDisguiseCoverEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(this, Target, Controller, NarrativeComponent)
		|| !Target) return;
	UWorld* World = TerritoryTales::ResolveWorld(this, Target, Controller, NarrativeComponent);
	UTerritoryDisguiseSubsystem* Disguises = ResolveControl(World)
		? World->GetSubsystem<UTerritoryDisguiseSubsystem>() : nullptr;
	ATerritoryVolume* Territory = ResolveEventTerritory(
		this, World, TerritoryContext, Target);
	if (!Disguises) return;
	if (Action == ETerritoryDisguiseEventAction::Compromise)
	{
		Disguises->CompromiseDisguise(Target, ObserverFaction, Territory);
	}
	else
	{
		Disguises->RestoreDisguise(Target, ObserverFaction);
	}
}

FString UTerritorySetDisguiseCoverEvent::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("%s Territory disguise for %s"),
		Action == ETerritoryDisguiseEventAction::Compromise
			? TEXT("Compromise") : TEXT("Restore"),
		ObserverFaction.IsValid() ? *ObserverFaction.ToString() : TEXT("all factions"));
}

UTerritoryDisguiseIdentityCheckEvent::UTerritoryDisguiseIdentityCheckEvent(
	const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bRefireOnLoad = false;
	EventFilter = EEventFilter::EF_OnlyPlayers;
}

void UTerritoryDisguiseIdentityCheckEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(this, Target, Controller, NarrativeComponent)
		|| !Target) return;
	UWorld* World = TerritoryTales::ResolveWorld(this, Target, Controller, NarrativeComponent);
	UTerritoryDisguiseSubsystem* Disguises = ResolveControl(World)
		? World->GetSubsystem<UTerritoryDisguiseSubsystem>() : nullptr;
	ATerritoryVolume* Territory = ResolveEventTerritory(
		this, World, TerritoryToCheck, Target);
	FText Reason;
	if (Disguises && Territory)
	{
		Disguises->PerformIdentityCheck(
			Target, Territory, ObserverFaction, Reason);
	}
}

FString UTerritoryDisguiseIdentityCheckEvent::GetGraphDisplayText_Implementation()
{
	return TerritoryToCheck.IsValid()
		? FString::Printf(TEXT("Check disguise at %s"), *TerritoryToCheck.ToString())
		: TEXT("Check disguise at the player's current Territory");
}
