#include "Tales/TerritoryCaptureEligibilityCondition.h"

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
	ATerritoryVolume* Territory = Registry && TerritoryToCheck.IsValid()
		? Registry->GetTerritoryByTag(TerritoryToCheck) : nullptr;
	if (!Territory || !Control
		|| Territory->GetControlMode() != ETerritoryControlMode::Independent
		|| !Territory->IsAvailableForGameplay()
		|| (bRequireNoLivingDefenders && Territory->GetDefenderCount() > 0)
		|| (bRequireContestedState
			&& Territory->GetTerritoryState() != ETerritoryState::Contested))
	{
		return false;
	}

	FGameplayTag Faction = ExplicitCapturingFaction;
	if (CapturingFactionSource == ETerritoryCaptureFactionSource::NarrativeTargetFaction)
	{
		Faction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Target);
	}
	else if (CapturingFactionSource == ETerritoryCaptureFactionSource::ControllerPawnFaction)
	{
		Faction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(
			this, Controller ? Controller->GetPawn() : nullptr);
	}
	return Faction.IsValid()
		&& Control->GetCaptureEligibility(Territory, Faction) == ECaptureResult::Success;
}

FString UTerritoryCaptureEligibilityCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Can hand over %s%s%s"),
		*TerritoryToCheck.ToString(),
		bRequireNoLivingDefenders ? TEXT(" after defenders are defeated") : TEXT(""),
		bRequireContestedState ? TEXT(" while contested") : TEXT(""));
}
