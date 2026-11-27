#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "TerritoryPatrolRoute.generated.h"

class USplineComponent;

/**
 * Authoring settings for one spline point of a patrol route actor.
 *
 * The route's geometry lives on the spline, not in this struct, so a designer never types a
 * world location: they drag the point in the viewport and then describe what happens there.
 */
USTRUCT(BlueprintType, meta=(DisplayName="Territory Patrol Point"))
struct TERRITORYFRAMEWORK_API FTerritoryPatrolPointSettings
{
	GENERATED_BODY()

	/**
	 * Seconds the guard waits at this point before moving on.
	 * Use 0 for continuous patrol with no waiting. Typical rest/inspect: 2-5s.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol Point",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="30.0", DisplayName="Wait Time"))
	float WaitTime = 2.f;

	/**
	 * Optional reservable spot at this point, resolved after the move, rotation and wait
	 * complete. Disabled by default, so adding a point changes nothing until a designer opts in.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol Point", meta=(DisplayName="Spot"))
	FTerritoryPatrolSpot Spot;

	/**
	 * Optional Guard.Activity tag naming the activity requested at this point
	 * (e.g. Guard.Activity.Inspect, Guard.Activity.Rest).
	 *
	 * NOT YET CONSUMED, exactly like FTerritoryPatrolNode::ActivityTag: no patrol activity
	 * reads this field today, so setting it does not start an activity.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Patrol Point",
		meta=(Categories="Guard.Activity", DisplayName="Activity Tag (Not Implemented)",
			ToolTip="Reserved metadata only. No patrol activity currently reads this tag, so setting it does not start an activity."))
	FGameplayTag ActivityTag;
};

/**
 * World-space patrol route drawn with an ordinary Unreal spline.
 *
 * This is the authoring-friendly alternative to typing world-space locations into a guard
 * post's PatrolRoute array. Place one actor, drag its spline points in the viewport, and let
 * any number of guard posts share it by referencing the same PatrolRouteID. Posts keep their
 * previous array-based route when they reference no route actor, and a post's own array still
 * beats the Place Definition row, so existing content is unaffected.
 *
 * The route is pure authoring data: ATerritoryGuardSpawnPoint::GetEffectivePatrolRoute()
 * samples it into the same FTerritoryPatrolNode array the guard already consumes, so patrol
 * behaviour, spinning, spot delivery, staggering and the containment policy are unchanged.
 */
UCLASS(BlueprintType, Blueprintable, hidecategories=(Replication, Input))
class TERRITORYFRAMEWORK_API ATerritoryPatrolRoute : public AActor
{
	GENERATED_BODY()

public:
	ATerritoryPatrolRoute();

#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Return this patrol route's stable authored identifier. */
	UFUNCTION(BlueprintPure, Category="Territory|Patrol")
	FName GetPatrolRouteID() const { return PatrolRouteID; }

	/** Return this route's authored spline component. */
	UFUNCTION(BlueprintPure, Category="Territory|Patrol")
	USplineComponent* GetRouteSpline() const { return RouteSpline; }

	/** True when the guard walks back to the first point after the last one. */
	UFUNCTION(BlueprintPure, Category="Territory|Patrol")
	bool IsLooping() const { return bLoop; }

	/** Number of authored spline points. Zero means the route is unusable. */
	UFUNCTION(BlueprintPure, Category="Territory|Patrol")
	int32 GetNodeCount() const;

	/** Settings authored for a spline point, or the struct defaults when nothing is authored. */
	UFUNCTION(BlueprintPure, Category="Territory|Patrol")
	FTerritoryPatrolPointSettings GetPointSettings(int32 PointIndex) const;

	/**
	 * Sample this route into guard patrol nodes, in spline order.
	 * Returns false and fills OutFailureReason when the spline has no usable points.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Patrol")
	bool BuildPatrolNodes(TArray<FTerritoryPatrolNode>& OutNodes, FText& OutFailureReason) const;

	/** Authoring check: spline present, at least one point, point settings cardinality. */
	UFUNCTION(BlueprintCallable, Category="Territory|Patrol|Validation")
	bool ValidatePatrolRoute(FText& OutFailureReason) const;

	/**
	 * Stable level-wide ID referenced by a Place Definition guard-post row. By convention this
	 * is the guard post's own ID, which means a route named after its post needs no extra
	 * authoring on the row.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Patrol",
		meta=(ToolTip="Stable level-wide ID referenced by a Territory guard post. By convention use the guard post ID, for example Blacksmith_WestPatrol."))
	FName PatrolRouteID;

	/** Authored route geometry. Drag the spline points in the viewport instead of typing locations. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Patrol")
	TObjectPtr<USplineComponent> RouteSpline;

	/** Close the loop so the guard returns to the first point after the last one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Patrol")
	bool bLoop = true;

	/**
	 * Per-point authoring, parallel to the spline points. Kept sized to the spline on edit and
	 * validated when it is a deliberate partial override: leave it empty to accept defaults, or
	 * author exactly one entry per spline point.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Patrol", meta=(DisplayName="Point Settings"))
	TArray<FTerritoryPatrolPointSettings> PointSettings;

#if WITH_EDITOR
	/** Resize PointSettings so every spline point has a row, preserving authored rows. */
	void SyncPointSettingsToSpline();
#endif
};
