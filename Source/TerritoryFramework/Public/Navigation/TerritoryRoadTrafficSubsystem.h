#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TerritoryRoadTrafficSubsystem.generated.h"

class AQuestRoadControls;
class ANarrativeVehicleBase;

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

	/** Return how many active operations currently hold a request on mission traffic. */
	UFUNCTION(BlueprintPure, Category="Territory|Road|Traffic")
	int32 GetMissionTrafficUserCount(const AQuestRoadControls* Controls) const;

	/** Server-only transient parking claim. Trims an existing route; never invents a shortcut. */
	bool ReserveArrival(ANarrativeVehicleBase* Vehicle, const FGuid& AssaultID,
		TArray<FVector>& InOutRoute, const FVector& WalkTarget,
		AActor* NavigationAgent, float SearchDistance, float Spacing);
	void ReleaseArrival(ANarrativeVehicleBase* Vehicle);

	/** Bounded alternative on the same route when a car or physical obstacle blocks departure. */
	bool ResolveBlockedDeparture(TConstArrayView<FVector> Route, FTransform& InOutDeparture) const;
	/** Remove the occupied entrance from the driving route after selecting a departure. */
	static bool TrimRouteToDeparture(TArray<FVector>& Route, const FVector& Departure,
		float MaximumDeviation = 200.f);

	/** True only on a same-direction Native road lane wide enough for the vehicle. */
	bool IsSafeRoadOffset(const FVector& Location, const FVector& Direction,
		float VehicleHalfWidth) const;

	/** Native traffic-light annotations own stop/go. Distance is measured along the mission route. */
	bool FindClosedRoadAhead(const FVector& Location, TConstArrayView<FVector> Route,
		int32 RouteIndex, float LookAhead, float& OutDistance) const;

	static TArray<FVector> BuildArrivalCandidates(TConstArrayView<FVector> Route,
		float SearchDistance, float Spacing);

private:
	struct FTrafficLease
	{
		TWeakObjectPtr<AQuestRoadControls> Controls;
		int32 Users = 0;
		bool bWasActiveBeforeTerritory = false;
		int32 OriginalSpawnCount = 0;
	};

	TMap<TWeakObjectPtr<AQuestRoadControls>, FTrafficLease> TrafficLeases;
	struct FArrivalClaim
	{
		FGuid AssaultID;
		FVector Location;
		float Radius = 400.f;
	};
	TMap<TWeakObjectPtr<ANarrativeVehicleBase>, FArrivalClaim> ArrivalClaims;
};
