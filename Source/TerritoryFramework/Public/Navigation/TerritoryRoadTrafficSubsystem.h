#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TerritoryRoadTrafficSubsystem.generated.h"

class AQuestRoadControls;

/**
 * Owns the shared Narrative QuestRoadControls lease for Territory road missions.
 *
 * Narrative's Mass spawn-point generator resolves one QuestRoadControls actor for
 * the world. Keeping the lease here (instead of on each Road Guide) prevents one
 * mission from restoring or disabling traffic while another mission still uses it.
 */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryRoadTrafficSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Returns false when another Narrative road controller already owns the world lease. */
	bool AcquireMissionTraffic(AQuestRoadControls* Controls, int32 DesiredVehicleCount);
	void ReleaseMissionTraffic(AQuestRoadControls* Controls);

	UFUNCTION(BlueprintPure, Category="Territory|Road|Traffic")
	int32 GetMissionTrafficUserCount(const AQuestRoadControls* Controls) const;

private:
	struct FTrafficLease
	{
		TWeakObjectPtr<AQuestRoadControls> Controls;
		int32 Users = 0;
		bool bWasActiveBeforeTerritory = false;
		int32 OriginalSpawnCount = 0;
	};

	TMap<TWeakObjectPtr<AQuestRoadControls>, FTrafficLease> TrafficLeases;
};
