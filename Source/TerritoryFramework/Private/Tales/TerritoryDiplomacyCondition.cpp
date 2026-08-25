#include "Tales/TerritoryDiplomacyCondition.h"

#include "Core/TerritoryTypes.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"

UTerritoryDiplomacyCondition::UTerritoryDiplomacyCondition()
{
	ConditionFilter = EConditionFilter::CF_DontTarget;
}

bool UTerritoryDiplomacyCondition::CheckCondition_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	(void)Target;
	(void)Controller;
	(void)NarrativeComponent;

	if (!FactionA.IsValid() || !FactionB.IsValid() || FactionA == FactionB)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[TalesDiplomacyCondition] Choose two different valid Narrative faction tags"));
		return false;
	}

	const UWorld* World = GetWorld();
	const UTerritoryDiplomacySubsystem* Diplomacy = World
		? World->GetSubsystem<UTerritoryDiplomacySubsystem>() : nullptr;
	if (!Diplomacy)
	{
		UE_LOG(LogTerritory, Verbose,
			TEXT("[TalesDiplomacyCondition] Territory diplomacy is not available"));
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
