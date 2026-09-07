#include "Tales/TerritoryCaptureEvent.h"
#include "Tales/TerritorySituationCondition.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Core/TerritoryMutationTypes.h"
#include "Tales/TalesComponent.h"
#include "Engine/World.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "Core/TerritoryBlueprintLibrary.h"

UTerritoryCaptureEvent::UTerritoryCaptureEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

void UTerritoryCaptureEvent::ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(this, Target, Controller, NarrativeComponent)) return;
	const FGameplayTag EffectiveTerritory = SituationProfile ? SituationProfile->Territory : TargetTerritoryTag;
	if (!EffectiveTerritory.IsValid()) return;

	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	if (!World) return;

	// Capture mutations are server-authoritative — skip on clients to prevent desync.
	if (World->GetNetMode() == NM_Client) return;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return;

	ATerritoryVolume* Territory = Registry->GetTerritoryByTag(EffectiveTerritory);
	if (!Territory) return;

	if (!Territory->IsAvailableForGameplay() && !bForceCapture)
	{
		UE_LOG(LogTerritory, Warning, TEXT("TerritoryCaptureEvent: %s is locked, skipping (bForceCapture=false)"),
			*EffectiveTerritory.ToString());
		return;
	}

	FGameplayTag ResolvedCapturingFaction = CapturingFaction;
	if (SituationProfile)
	{
		ResolvedCapturingFaction = SituationProfile->ResolveRequestingFaction(Target, Controller, NarrativeComponent);
	}
	else if (CapturingFactionSource == ETerritoryCaptureFactionSource::NarrativeTargetFaction)
	{
		ResolvedCapturingFaction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(
			this, Target);
	}
	else if (CapturingFactionSource == ETerritoryCaptureFactionSource::ControllerPawnFaction)
	{
		ResolvedCapturingFaction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(
			this, Controller ? Controller->GetPawn() : nullptr);
	}

	if (!ResolvedCapturingFaction.IsValid())
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("TerritoryCaptureEvent: no exact capturing faction resolved for %s (source=%d)"),
			*EffectiveTerritory.ToString(), static_cast<int32>(SituationProfile ? SituationProfile->FactionSource : CapturingFactionSource));
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
			*EffectiveTerritory.ToString());
		return;
	}

	FTerritoryMutationRequest Request;
	Request.Territory = Territory;
	Request.NewOwner = ResolvedCapturingFaction;
	Request.DesiredState = ETerritoryState::Claimed;

	// Build transition context with TalesComponent for Narrative event/condition evaluation
	Request.TransitionContext.Instigator = Target;
	Request.TransitionContext.TargetPawn = Target;
	Request.TransitionContext.PlayerController = Controller;
	Request.TransitionContext.TalesComponent = NarrativeComponent;
	Request.TransitionContext.RequestingFaction = ResolvedCapturingFaction;

	if (bForceCapture)
	{
		// Preserve the exact Tales/player context while applying the same explicit
		// bypasses as ForceCapture. Calling the context-free convenience wrapper
		// here previously made PlayerChooses fall back to the authored guard target.
		Request.bBypassConditions = true;
		Request.bBypassDiplomacy = true;
		Request.bBypassLock = true;
		Request.bBypassDefenders = true;
	}

	const FTerritoryMutationResponse Response = Control->ApplyTerritoryMutation(Request);
	if (Response.Result != ETerritoryMutationResult::Success)
	{
		UE_LOG(LogTerritory, Warning, TEXT("[TalesCaptureEvent] Mutation rejected for %s: %s (result=%d)"),
			*EffectiveTerritory.ToString(),
			*Response.Explanation.ToString(),
			static_cast<int32>(Response.Result));
		return;
	}

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebug = Settings && Settings->ShouldDebugTales();

	if (bDebug)
	{
		UE_LOG(LogTerritory, Log, TEXT("TerritoryCaptureEvent: %s captured by %s via event"),
			*EffectiveTerritory.ToString(), *ResolvedCapturingFaction.ToString());
		UE_LOG(LogTerritory, Log, TEXT("[TalesCaptureEvent] ForceCapture %s → %s (force=%s)"),
			*EffectiveTerritory.ToString(), *ResolvedCapturingFaction.ToString(),
			bForceCapture ? TEXT("true") : TEXT("false"));
	}
}
