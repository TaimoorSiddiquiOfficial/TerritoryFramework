#include "Navigation/TerritoryPatrolRoute.h"

#include "Components/SplineComponent.h"
#include "Core/TerritoryTypes.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

#define LOCTEXT_NAMESPACE "TerritoryPatrolRoute"

ATerritoryPatrolRoute::ATerritoryPatrolRoute()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	RouteSpline = CreateDefaultSubobject<USplineComponent>(TEXT("RouteSpline"));
	RouteSpline->SetClosedLoop(false);
	RouteSpline->SetMobility(EComponentMobility::Movable);
	RouteSpline->bDrawDebug = true;
	SetRootComponent(RouteSpline);
}

int32 ATerritoryPatrolRoute::GetNodeCount() const
{
	return RouteSpline ? RouteSpline->GetNumberOfSplinePoints() : 0;
}

FTerritoryPatrolPointSettings ATerritoryPatrolRoute::GetPointSettings(const int32 PointIndex) const
{
	if (PointSettings.IsValidIndex(PointIndex))
	{
		return PointSettings[PointIndex];
	}

	return FTerritoryPatrolPointSettings();
}

bool ATerritoryPatrolRoute::BuildPatrolNodes(TArray<FTerritoryPatrolNode>& OutNodes, FText& OutFailureReason) const
{
	OutNodes.Reset();
	OutFailureReason = FText::GetEmpty();

	if (!RouteSpline)
	{
		OutFailureReason = LOCTEXT("MissingSpline", "The patrol route has no spline component.");
		return false;
	}

	const int32 PointCount = RouteSpline->GetNumberOfSplinePoints();
	if (PointCount <= 0)
	{
		OutFailureReason = LOCTEXT("NoSplinePoints",
			"The patrol route spline has no points. Drag spline points in the viewport to author the route.");
		return false;
	}

	OutNodes.Reserve(PointCount);
	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		const FVector WorldLocation = RouteSpline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World);
		const FRotator WorldRotation = RouteSpline->GetRotationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World);

		// A degenerate point would hand the patrol goal a location it can never reach, so the
		// route refuses to build instead of shipping a node the guard walks to forever.
		if (WorldLocation.ContainsNaN() || !FMath::IsFinite(WorldLocation.X) ||
			!FMath::IsFinite(WorldLocation.Y) || !FMath::IsFinite(WorldLocation.Z))
		{
			OutFailureReason = FText::Format(
				LOCTEXT("DegenerateNode", "The patrol route spline point {0} is not a finite world location."),
				FText::AsNumber(PointIndex));
			OutNodes.Reset();
			return false;
		}

		const FTerritoryPatrolPointSettings Settings = GetPointSettings(PointIndex);

		FTerritoryPatrolNode Node;
		Node.Location = WorldLocation;
		Node.Rotation = WorldRotation;
		Node.WaitTime = FMath::Max(0.f, Settings.WaitTime);
		Node.ActivityTag = Settings.ActivityTag;
		Node.Spot = Settings.Spot;
		OutNodes.Add(Node);
	}

	return true;
}

bool ATerritoryPatrolRoute::ValidatePatrolRoute(FText& OutFailureReason) const
{
	OutFailureReason = FText::GetEmpty();

	if (!RouteSpline)
	{
		OutFailureReason = LOCTEXT("MissingSpline", "The patrol route has no spline component.");
		return false;
	}

	// Defence in depth for the crash below: nothing in the plugin sets a closed loop, and the
	// editor sync forces the flag open, but a spliced-in spline or a hand-edited asset should be
	// reported rather than handed to the visualizer that asserts on it.
	if (RouteSpline->IsClosedLoop())
	{
		OutFailureReason = LOCTEXT("ClosedLoopSpline",
			"The patrol route spline is a closed loop. Leave the spline's own Closed Loop flag off and set the actor's Loop Patrol flag instead: the engine's spline visualizer asserts while a closed-loop spline is edited in the viewport.");
		return false;
	}

	const int32 PointCount = RouteSpline->GetNumberOfSplinePoints();
	if (PointCount <= 0)
	{
		OutFailureReason = LOCTEXT("NoSplinePoints",
			"The patrol route spline has no points. Drag spline points in the viewport to author the route.");
		return false;
	}

	if (PointSettings.Num() != 0 && PointSettings.Num() != PointCount)
	{
		OutFailureReason = FText::Format(
			LOCTEXT("PointSettingsCardinality",
				"The patrol route authors {0} point setting rows for {1} spline points. Leave the array empty to use defaults, or author exactly one row per spline point."),
			FText::AsNumber(PointSettings.Num()),
			FText::AsNumber(PointCount));
		return false;
	}

	return true;
}

#if WITH_EDITOR
void ATerritoryPatrolRoute::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	SyncPointSettingsToSpline();
}

void ATerritoryPatrolRoute::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SyncPointSettingsToSpline();
}

void ATerritoryPatrolRoute::SyncPointSettingsToSpline()
{
	if (!RouteSpline)
	{
		return;
	}

	// Spline points are the source of truth for cardinality: growing the array here means a
	// designer adds a point in the viewport and immediately gets a row to describe it, without
	// the array ever drifting out of step with the geometry.
	const int32 PointCount = RouteSpline->GetNumberOfSplinePoints();
	if (PointSettings.Num() != PointCount)
	{
		PointSettings.SetNum(PointCount);
	}

	// Loop policy is ours (bLoop), never the spline's own flag. The engine's spline visualizer
	// asserts on a closed loop the moment an end point is dragged to extend the route
	// (SplineComponentVisualizer::UpdateAddSegment), so the flag is forced open here and the
	// closing arrow below is what shows the authored loop in the viewport.
	if (RouteSpline->IsClosedLoop())
	{
		RouteSpline->SetClosedLoop(false, false);
	}

#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GetWorld())
	{
		if (!World->IsGameWorld())
		{
			for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
			{
				const FVector WorldLocation = RouteSpline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World);
				const FTerritoryPatrolPointSettings Settings = GetPointSettings(PointIndex);

				DrawDebugString(World, WorldLocation + FVector(0.f, 0.f, 120.f),
					FString::Printf(TEXT("[%d] wait %.1fs%s"), PointIndex, Settings.WaitTime,
						Settings.Spot.bEnabled ? TEXT(" +spot") : TEXT("")),
					nullptr, FColor::Cyan, 0.f, false, 1.4f);

				if (PointIndex + 1 < PointCount || bLoop)
				{
					const int32 NextIndex = (PointIndex + 1) % PointCount;
					const FVector NextLocation = RouteSpline->GetLocationAtSplinePoint(NextIndex, ESplineCoordinateSpace::World);
					DrawDebugDirectionalArrow(World, WorldLocation + FVector(0.f, 0.f, 40.f),
						NextLocation + FVector(0.f, 0.f, 40.f), 160.f, FColor::Cyan, false, 0.f, 0, 6.f);
				}
			}
		}
	}
#endif
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
