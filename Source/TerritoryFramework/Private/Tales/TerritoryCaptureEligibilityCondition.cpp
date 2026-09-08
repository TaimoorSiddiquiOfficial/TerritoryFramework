#include "Tales/TerritoryCaptureEligibilityCondition.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Tales/TerritorySituationCondition.h"

#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryVolume.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TerritoryTalesUtilities.h"

UTerritoryCaptureEligibilityCondition::UTerritoryCaptureEligibilityCondition()
{
	ConditionFilter = EConditionFilter::CF_DontTarget;
}

bool UTerritoryCaptureEligibilityCondition::CheckCondition_Implementation(
	APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	UTerritoryControlSubsystem* Control = World
		? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
	const FGameplayTag EffectiveTerritory = SituationProfile ? SituationProfile->Territory : TerritoryToCheck;
	ATerritoryVolume* Territory = Registry && EffectiveTerritory.IsValid()
		? Registry->GetTerritoryByTag(EffectiveTerritory) : nullptr;
	if (!Territory || !Control
		|| Territory->GetControlMode() != ETerritoryControlMode::Independent
		|| !Territory->IsAvailableForGameplay()
		|| (bRequireStoryCaptureFlow && !Territory->UsesStoryCaptureFromBounds())
		|| (bRequireNoLivingDefenders && Territory->GetDefenderCount() > 0)
		|| (bRequireContestedState
			&& Territory->GetTerritoryState() != ETerritoryState::Contested))
	{
		return false;
	}

	FGameplayTag Faction = ExplicitCapturingFaction;
	if (SituationProfile)
	{
		Faction = SituationProfile->ResolveRequestingFaction(Target, Controller, NarrativeComponent);
	}
	else if (CapturingFactionSource == ETerritoryCaptureFactionSource::NarrativeTargetFaction)
	{
		Faction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Target);
	}
	else if (CapturingFactionSource == ETerritoryCaptureFactionSource::ControllerPawnFaction)
	{
		Faction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(
			this, Controller ? Controller->GetPawn() : nullptr);
	}
	if (!Faction.IsValid()
		|| Control->GetCaptureEligibility(Territory, Faction) != ECaptureResult::Success) return false;
	FTerritoryTransitionContext Context;
	Context.Instigator = Target;
	Context.TargetPawn = Target;
	Context.PlayerController = Controller;
	Context.TalesComponent = NarrativeComponent;
	Context.RequestingFaction = Faction;
	FText Failure;
	return Territory->CheckStateExitConditions(Territory->GetTerritoryState(), Failure, Context)
		&& Territory->CheckStateConditions(ETerritoryState::Claimed, Failure, Context, &Faction);
}

FString UTerritoryCaptureEligibilityCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Can hand over %s%s%s%s"),
		*(SituationProfile ? SituationProfile->Territory : TerritoryToCheck).ToString(),
		bRequireNoLivingDefenders ? TEXT(" after defenders are defeated") : TEXT(""),
		bRequireContestedState ? TEXT(" while contested") : TEXT(""),
		bRequireStoryCaptureFlow ? TEXT(" using story capture") : TEXT(""));
}
