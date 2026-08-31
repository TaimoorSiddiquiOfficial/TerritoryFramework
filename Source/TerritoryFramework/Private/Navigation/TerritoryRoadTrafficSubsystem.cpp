#include "Navigation/TerritoryRoadTrafficSubsystem.h"

#include "Core/TerritoryTypes.h"
#include "Vehicles/Mass/QuestRoadControls.h"

void UTerritoryRoadTrafficSubsystem::Deinitialize()
{
	for (TPair<TWeakObjectPtr<AQuestRoadControls>, FTrafficLease>& Pair : TrafficLeases)
	{
		FTrafficLease& Lease = Pair.Value;
		AQuestRoadControls* Controls = Lease.Controls.Get();
		if (!IsValid(Controls)) continue;
		if (!Lease.bWasActiveBeforeTerritory && Controls->IsActive())
		{
			Controls->SetActive(false);
		}
		Controls->NewSpawnCount = Lease.OriginalSpawnCount;
	}
	TrafficLeases.Empty();
	Super::Deinitialize();
}

bool UTerritoryRoadTrafficSubsystem::AcquireMissionTraffic(
	AQuestRoadControls* Controls, const int32 DesiredVehicleCount)
{
	if (!IsValid(Controls)) return false;

	for (const TPair<TWeakObjectPtr<AQuestRoadControls>, FTrafficLease>& Pair : TrafficLeases)
	{
		if (Pair.Value.Users > 0 && Pair.Value.Controls.Get() != Controls)
		{
			UE_LOG(LogTerritory, Warning,
				TEXT("[RoadTraffic] %s cannot activate while %s owns Narrative's world traffic lease. Assign the same QuestRoadControls actor to every Territory Road Guide."),
				*GetNameSafe(Controls), *GetNameSafe(Pair.Value.Controls.Get()));
			return false;
		}
	}

	const TWeakObjectPtr<AQuestRoadControls> ControlsKey(Controls);
	FTrafficLease& Lease = TrafficLeases.FindOrAdd(ControlsKey);
	if (Lease.Users++ > 0) return true;

	Lease.Controls = Controls;
	Lease.bWasActiveBeforeTerritory = Controls->IsActive();
	Lease.OriginalSpawnCount = Controls->NewSpawnCount;
	if (!Lease.bWasActiveBeforeTerritory)
	{
		Controls->NewSpawnCount = DesiredVehicleCount == INDEX_NONE
			? Controls->NewSpawnCount : FMath::Max(0, DesiredVehicleCount);
		Controls->SetActive(true);
	}
	return true;
}

void UTerritoryRoadTrafficSubsystem::ReleaseMissionTraffic(AQuestRoadControls* Controls)
{
	if (!IsValid(Controls)) return;
	const TWeakObjectPtr<AQuestRoadControls> ControlsKey(Controls);
	FTrafficLease* Lease = TrafficLeases.Find(ControlsKey);
	if (!Lease || Lease->Users <= 0) return;
	if (--Lease->Users > 0) return;

	if (!Lease->bWasActiveBeforeTerritory && Controls->IsActive())
	{
		Controls->SetActive(false);
	}
	Controls->NewSpawnCount = Lease->OriginalSpawnCount;
	TrafficLeases.Remove(ControlsKey);
}

int32 UTerritoryRoadTrafficSubsystem::GetMissionTrafficUserCount(
	const AQuestRoadControls* Controls) const
{
	if (!Controls) return 0;
	const FTrafficLease* Lease = TrafficLeases.Find(
		TWeakObjectPtr<AQuestRoadControls>(const_cast<AQuestRoadControls*>(Controls)));
	return Lease ? Lease->Users : 0;
}
