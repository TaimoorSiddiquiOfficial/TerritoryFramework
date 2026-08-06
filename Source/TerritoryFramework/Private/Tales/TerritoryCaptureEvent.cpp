#include "Tales/TerritoryCaptureEvent.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Core/TerritoryMutationTypes.h"
#include "Tales/TalesComponent.h"
#include "Engine/World.h"

void UTerritoryCaptureEvent::ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TargetTerritoryTag.IsValid()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Capture mutations are server-authoritative — skip on clients to prevent desync.
	if (World->GetNetMode() == NM_Client) return;

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

	// P0-04: Route through ApplyTerritoryMutation — the atomic mutation API.
	// The old path (AttemptCapture + AddCaptureProgress) failed because Narrative
	// events never call RegisterAttacker, so CompleteCapture rejected with
	// ActiveAttackers <= 0 and reset the capture.
	UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>();
	if (!Control)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[TalesCaptureEvent] ControlSubsystem unavailable for %s — capture skipped"),
			*TargetTerritoryTag.ToString());
		return;
	}

	FTerritoryMutationRequest Request;
	Request.Territory = Territory;
	Request.NewOwner = CapturingFaction;
	Request.DesiredState = ETerritoryState::Claimed;

	// Build transition context with TalesComponent for Narrative event/condition evaluation
	Request.TransitionContext.Instigator = Target;
	Request.TransitionContext.TargetPawn = Target;
	Request.TransitionContext.PlayerController = Controller;
	Request.TransitionContext.TalesComponent = NarrativeComponent;
	Request.TransitionContext.RequestingFaction = CapturingFaction;

	if (bForceCapture)
	{
		// Preserve the exact Tales/player context while applying the same explicit
		// bypasses as ForceCapture. Calling the context-free convenience wrapper
		// here previously made PlayerChooses fall back to the authored guard target.
		Request.bBypassConditions = true;
		Request.bBypassDiplomacy = true;
		Request.bBypassLock = true;
	}

	const FTerritoryMutationResponse Response = Control->ApplyTerritoryMutation(Request);
	if (Response.Result != ETerritoryMutationResult::Success)
	{
		UE_LOG(LogTerritory, Warning, TEXT("[TalesCaptureEvent] Mutation rejected for %s: %s (result=%d)"),
			*TargetTerritoryTag.ToString(),
			*Response.Explanation.ToString(),
			static_cast<int32>(Response.Result));
		return;
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
