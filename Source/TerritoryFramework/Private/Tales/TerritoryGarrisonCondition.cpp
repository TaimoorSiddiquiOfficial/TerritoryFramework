#include "Tales/TerritoryGarrisonCondition.h"

#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TerritoryTalesUtilities.h"

UTerritoryGarrisonCondition::UTerritoryGarrisonCondition()
{
	ConditionFilter = EConditionFilter::CF_DontTarget;
}

bool UTerritoryGarrisonCondition::CompareValues(int32 ActualValue,
	ETerritoryIntegerComparison Operation, int32 RequiredValue)
{
	switch (Operation)
	{
	case ETerritoryIntegerComparison::Equal: return ActualValue == RequiredValue;
	case ETerritoryIntegerComparison::NotEqual: return ActualValue != RequiredValue;
	case ETerritoryIntegerComparison::AtLeast: return ActualValue >= RequiredValue;
	case ETerritoryIntegerComparison::AtMost: return ActualValue <= RequiredValue;
	case ETerritoryIntegerComparison::GreaterThan: return ActualValue > RequiredValue;
	case ETerritoryIntegerComparison::LessThan: return ActualValue < RequiredValue;
	default: return false;
	}
}

bool UTerritoryGarrisonCondition::CheckCondition_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryToCheck.IsValid())
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[TalesGarrisonCondition] No Territory To Check tag was selected"));
		return false;
	}

	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	ATerritoryVolume* Territory = Registry
		? Registry->GetTerritoryByTag(TerritoryToCheck) : nullptr;
	if (!Territory)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[TalesGarrisonCondition] Territory '%s' is not currently loaded or registered"),
			*TerritoryToCheck.ToString());
		return false;
	}

	int32 ActualValue = 0;
	switch (Metric)
	{
	case ETerritoryGarrisonMetric::ActiveGuards:
		ActualValue = Territory->GetSpawnedGuardCount();
		break;
	case ETerritoryGarrisonMetric::LivingDefenders:
		ActualValue = Territory->GetDefenderCount();
		break;
	case ETerritoryGarrisonMetric::DesiredGuards:
		ActualValue = Territory->GetDesiredGuardCount();
		break;
	case ETerritoryGarrisonMetric::MaximumCapacity:
		ActualValue = Territory->GetMaxGuardCount();
		break;
	case ETerritoryGarrisonMetric::RemainingReserve:
		for (const ATerritoryGuardSpawnPoint* SpawnPoint : Territory->GetGuardSpawnPoints())
		{
			if (SpawnPoint) ActualValue += FMath::Max(0, SpawnPoint->GetReserveCount());
		}
		break;
	case ETerritoryGarrisonMetric::PendingReserveDeployments:
		for (const ATerritoryGuardSpawnPoint* SpawnPoint : Territory->GetGuardSpawnPoints())
		{
			if (SpawnPoint) ActualValue += FMath::Max(0, SpawnPoint->GetPendingReserveCount());
		}
		break;
	case ETerritoryGarrisonMetric::GuardShortfall:
		ActualValue = FMath::Max(0,
			Territory->GetDesiredGuardCount() - Territory->GetSpawnedGuardCount());
		break;
	default:
		return false;
	}

	return CompareValues(ActualValue, Comparison, FMath::Max(0, Value));
}

FString UTerritoryGarrisonCondition::GetGraphDisplayText_Implementation()
{
	const UEnum* MetricEnum = StaticEnum<ETerritoryGarrisonMetric>();
	const UEnum* ComparisonEnum = StaticEnum<ETerritoryIntegerComparison>();
	const FString MetricName = MetricEnum
		? MetricEnum->GetDisplayNameTextByValue(static_cast<int64>(Metric)).ToString()
		: TEXT("Garrison");
	const FString ComparisonName = ComparisonEnum
		? ComparisonEnum->GetDisplayNameTextByValue(static_cast<int64>(Comparison)).ToString()
		: TEXT("Compare");
	return FString::Printf(TEXT("Territory: %s %s %s %d"),
		*TerritoryToCheck.ToString(), *MetricName, *ComparisonName, FMath::Max(0, Value));
}
