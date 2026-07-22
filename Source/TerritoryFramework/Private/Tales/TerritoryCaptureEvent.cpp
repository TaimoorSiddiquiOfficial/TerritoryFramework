#include "Tales/TerritoryCaptureEvent.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Tales/TalesComponent.h"
#include "Engine/World.h"

void UTerritoryCaptureEvent::ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TargetTerritoryTag.IsValid()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return;

	ATerritoryVolume* Territory = Registry->GetTerritoryByTag(TargetTerritoryTag);
	if (!Territory) return;

	if (Territory->GetTerritoryState() == ETerritoryState::Locked && !bForceCapture)
	{
		UE_LOG(LogTerritory, Warning, TEXT("TerritoryCaptureEvent: %s is locked, skipping (bForceCapture=false)"),
			*TargetTerritoryTag.ToString());
		return;
	}

	if (!CapturingFaction.IsValid())
	{
		UE_LOG(LogTerritory, Warning, TEXT("TerritoryCaptureEvent: CapturingFaction not set for %s"),
			*TargetTerritoryTag.ToString());
		return;
	}

	// Route through the control subsystem — centralized capture authority
	UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>();
	if (Control)
	{
		if (bForceCapture)
		{
			// Forced: bypass all rules (quest/dialogue override)
			Control->ForceCapture(Territory, CapturingFaction);
		}
		else
		{
			// Normal: respect all capture rules (defenders, diplomacy, budget)
			const ECaptureResult Result = Control->AttemptCapture(Territory, CapturingFaction);
			if (Result != ECaptureResult::Success)
			{
				const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
				const bool bDebug = Settings && Settings->ShouldDebugTales();
				if (bDebug)
				{
					UE_LOG(LogTerritory, Log, TEXT("[TalesCaptureEvent] AttemptCapture failed: %s, result=%d"),
						*TargetTerritoryTag.ToString(), static_cast<int32>(Result));
				}
				return; // Capture failed — do not proceed
			}
		}
	}
	else
	{
		// Fallback if control subsystem unavailable (shouldn't happen in normal play)
		Territory->SetOwningFaction(CapturingFaction);
	}

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebug = Settings && Settings->ShouldDebugTales();

	UE_LOG(LogTerritory, Log, TEXT("TerritoryCaptureEvent: %s captured by %s via event"),
		*TargetTerritoryTag.ToString(), *CapturingFaction.ToString());

	if (bDebug)
	{
		UE_LOG(LogTerritory, Log, TEXT("[TalesCaptureEvent] ForceCapture %s → %s (force=%s)"),
			*TargetTerritoryTag.ToString(), *CapturingFaction.ToString(),
			bForceCapture ? TEXT("true") : TEXT("false"));
	}
}
