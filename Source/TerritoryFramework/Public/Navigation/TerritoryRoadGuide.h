#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Navigation/TerritoryRoadTypes.h"
#include "TerritoryRoadGuide.generated.h"

class AQuestRoadControls;
class USplineComponent;

/**
 * Runtime road mission guide drawn with an ordinary Unreal spline.
 *
 * The spline is a mission route, not a replacement for Narrative Pro's road system.
 * Put it over a Narrative Road/BigRoad ZoneGraph shape so Mass traffic, intersections,
 * scripted pursuit vehicles, impact damage, and Territory reinforcements agree on the
 * same physical street. Both travel directions are produced from this one spline.
 */
UCLASS(BlueprintType, Blueprintable, hidecategories=(Replication, Input))
class TERRITORYFRAMEWORK_API ATerritoryRoadGuide : public AActor
{
	GENERATED_BODY()

public:
	ATerritoryRoadGuide();

	UFUNCTION(BlueprintPure, Category="Territory|Road")
	FName GetRoadGuideID() const { return RoadGuideID; }

	UFUNCTION(BlueprintPure, Category="Territory|Road")
	USplineComponent* GetRouteSpline() const { return RouteSpline; }

	/** Samples the guide in the requested travel direction and offsets to that direction's left/right lane. */
	UFUNCTION(BlueprintCallable, Category="Territory|Road")
	bool BuildRoutePoints(bool bReverseDirection, ETerritoryRoadLaneSide LaneSide,
		TArray<FVector>& OutRoutePoints, FText& OutFailureReason) const;

	UFUNCTION(BlueprintPure, Category="Territory|Road")
	FTransform GetRouteStartTransform(bool bReverseDirection,
		ETerritoryRoadLaneSide LaneSide) const;

	UFUNCTION(BlueprintPure, Category="Territory|Road")
	FTransform GetRouteEndTransform(bool bReverseDirection,
		ETerritoryRoadLaneSide LaneSide) const;

	UFUNCTION(BlueprintPure, Category="Territory|Road")
	FTransform GetFinalFightTransform() const;

	/** Validate the spline and, when requested, its coverage by Narrative's ZoneGraph road lanes. */
	UFUNCTION(BlueprintCallable, Category="Territory|Road|Validation")
	bool ValidateRoadGuide(FText& OutFailureReason) const;

	/** Reference-counted mission traffic control. Safe when two Territory missions share this road. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Road|Traffic")
	void BeginMissionTraffic(int32 DesiredVehicleCount = -1);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Road|Traffic")
	void EndMissionTraffic();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Road",
		meta=(ToolTip="Stable level-wide ID referenced by a Territory approach. By convention use the Approach ID, for example Blacksmith_WestRoad."))
	FName RoadGuideID;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Road")
	TObjectPtr<USplineComponent> RouteSpline;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Road",
		meta=(ClampMin="100.0", ToolTip="Distance between drive points sampled from the spline. Smaller values follow sharp bends more closely."))
	float SampleSpacing = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Road",
		meta=(ClampMin="0.0", ToolTip="Distance from the spline centre to each directional lane. Narrative's default Road profile uses two 300 cm lanes, so 150 cm is a useful starting point."))
	float LaneCenterOffset = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Road|Narrative",
		meta=(ToolTip="Reject this helper when samples are not close to Narrative's ZoneGraph. Keep enabled for traffic, reinforcement, and pursuit roads; disable only for a deliberate off-road mission spline."))
	bool bRequireNarrativeZoneGraphCoverage = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Road|Narrative",
		meta=(ClampMin="100.0", ToolTip="Maximum distance from each sampled mission point to a Narrative ZoneGraph lane."))
	float MaximumZoneGraphDistance = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Road|Mission",
		meta=(MakeEditWidget, ToolTip="Local-space location used after a chased vehicle is disabled or abandoned. The target dismounts and Narrative navigation moves it here for the final fight."))
	FVector FinalFightLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Road|Traffic",
		meta=(ToolTip="Optional Narrative Quest Road Controls actor. Territory only activates/deactivates it; Narrative remains responsible for Mass traffic spawning, lane occupancy, obstacle avoidance, and intersection rules."))
	TSoftObjectPtr<AQuestRoadControls> NarrativeTrafficControls;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Road|Traffic",
		meta=(ClampMin="0", ClampMax="200", ToolTip="Requested Narrative Mass vehicle count while a mission uses this road. A mission option may override it."))
	int32 MissionTrafficVehicleCount = 5;

#if WITH_EDITOR
	/** Editor helper used by Territory's setup library to create a readable two-point route. */
	void SetStraightRoute(const FVector& WorldStart, const FVector& WorldEnd);
#endif

private:
	FTransform GetRouteTransformAtDistance(float Distance, bool bReverseDirection,
		ETerritoryRoadLaneSide LaneSide) const;
	float GetSignedLaneOffset(bool bReverseDirection,
		ETerritoryRoadLaneSide LaneSide) const;
};
