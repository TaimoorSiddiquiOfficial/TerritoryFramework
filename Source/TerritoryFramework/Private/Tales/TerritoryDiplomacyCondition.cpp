#include "Tales/TerritoryDiplomacyCondition.h"

#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Tales/TerritoryTalesUtilities.h"

UTerritoryDiplomacyCondition::UTerritoryDiplomacyCondition()
{
	ConditionFilter = EConditionFilter::CF_DontTarget;
}

bool UTerritoryDiplomacyCondition::CheckCondition_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!FactionA.IsValid() || !FactionB.IsValid() || FactionA == FactionB)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[TalesDiplomacyCondition] Choose two different valid Narrative faction tags"));
		return false;
	}

	const UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	const UTerritoryDiplomacySubsystem* Diplomacy = World
		? World->GetSubsystem<UTerritoryDiplomacySubsystem>() : nullptr;
	if (!Diplomacy)
	{
		if (const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
			Settings && Settings->ShouldDebugTales()
			&& Settings->IsDebugLevelEnabled(6))
		{
			UE_LOG(LogTerritory, Log,
				TEXT("[TalesDiplomacyCondition] Territory diplomacy is not available"));
		}
		return false;
	}

	return Diplomacy->GetDiplomacyState(FactionA, FactionB) == RequiredState;
}

FString UTerritoryDiplomacyCondition::GetGraphDisplayText_Implementation()
{
	const UEnum* StateEnum = StaticEnum<EDiplomacyState>();
	const FString StateName = StateEnum
		? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(RequiredState)).ToString()
		: TEXT("Unknown");
	return FString::Printf(TEXT("Diplomacy: %s and %s are %s"),
		*FactionA.ToString(), *FactionB.ToString(), *StateName);
}
