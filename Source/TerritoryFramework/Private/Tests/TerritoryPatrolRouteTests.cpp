#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/SplineComponent.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Navigation/TerritoryPatrolRoute.h"

/**
 * Patrol route authoring (P1): a placed ATerritoryPatrolRoute turns spline points into the patrol
 * nodes a guard post walks, and a post that names one by ID prefers it over its authored array.
 * Both halves are pure functions of authored data, so the fixture is one Game world.
 */
namespace TerritoryPatrolRouteTests
{
	/** One Game world that can host a route actor and the guard post that references it. */
	struct FFixture
	{
		UWorld* World = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World)
			{
				return;
			}
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}

		~FFixture()
		{
			if (World)
			{
				World->DestroyWorld(false);
				GEngine->DestroyWorldContext(World);
				World = nullptr;
			}
		}

		ATerritoryPatrolRoute* SpawnRoute(const TCHAR* RouteID, const TArray<FVector>& Points) const
		{
			ATerritoryPatrolRoute* Route = World ? World->SpawnActor<ATerritoryPatrolRoute>() : nullptr;
			if (!Route)
			{
				return nullptr;
			}

			Route->PatrolRouteID = RouteID;

			USplineComponent* Spline = Route->GetRouteSpline();
			if (!Spline)
			{
				return Route;
			}

			Spline->ClearSplinePoints(false);
			for (const FVector& Point : Points)
			{
				Spline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
			}
			Spline->UpdateSpline();

			return Route;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPatrolRouteActorNodes,
	"TerritoryFramework.Patrol.Behavior.PatrolRouteActorBuildsSplineNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPatrolRouteActorNodes::RunTest(const FString& Parameters)
{
	using namespace TerritoryPatrolRouteTests;

	FFixture Fixture;
	if (!TestNotNull(TEXT("A Game world was created"), Fixture.World))
	{
		return false;
	}

	const TArray<FVector> Points = { FVector(0.f, 0.f, 0.f), FVector(500.f, 0.f, 0.f), FVector(500.f, 500.f, 0.f) };
	ATerritoryPatrolRoute* Route = Fixture.SpawnRoute(TEXT("PatrolTest_Route"), Points);
	if (!TestNotNull(TEXT("The patrol route actor was spawned"), Route))
	{
		return false;
	}

	TestEqual(TEXT("The spline point count is what the route reports as its node count"), Route->GetNodeCount(), Points.Num());

#if WITH_EDITOR
	// Spawning the actor in the editor syncs one default row per spline point, which is what keeps a
	// designer's array the same length as the geometry. This test rewrites the spline in C++ after
	// that sync, so it re-syncs deliberately and then empties the array to exercise the pure-spline
	// path; without the re-sync the stale two-row array would (correctly) fail validation.
	Route->SyncPointSettingsToSpline();
	TestEqual(TEXT("The editor sync writes one settings row per spline point"), Route->PointSettings.Num(), Points.Num());
	Route->PointSettings.Reset();
#endif

	// No point setting rows is the pure-spline case: every node takes the documented defaults.
	FText Reason;
	TestTrue(TEXT("A route without point setting rows passes validation"), Route->ValidatePatrolRoute(Reason));
	TestTrue(TEXT("Validation leaves the failure reason empty on success"), Reason.IsEmpty());

	TArray<FTerritoryPatrolNode> Nodes;
	TestTrue(TEXT("Spline points build patrol nodes"), Route->BuildPatrolNodes(Nodes, Reason));
	TestEqual(TEXT("One node per spline point"), Nodes.Num(), Points.Num());
	for (int32 Index = 0; Index < Points.Num() && Nodes.IsValidIndex(Index); ++Index)
	{
		TestTrue(FString::Printf(TEXT("Node %d samples its spline point in world space"), Index),
			Nodes[Index].Location.Equals(Points[Index], 1.0));
		TestEqual(FString::Printf(TEXT("Node %d keeps the documented default wait"), Index), Nodes[Index].WaitTime, 2.f);
	}

	// Authored rows win, and a negative wait is clamped rather than travelling back in time.
	Route->PointSettings.SetNum(3);
	Route->PointSettings[0].WaitTime = 3.f;
	Route->PointSettings[1].WaitTime = 0.f;
	Route->PointSettings[2].WaitTime = -5.f;
	Route->PointSettings[2].Spot.bEnabled = true;
	Route->PointSettings[2].Spot.MaximumVisitSeconds = 45.f;
	Route->PointSettings[1].ActivityTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.Patrol.Spot"), false);

	Nodes.Reset();
	TestTrue(TEXT("Authored point settings build nodes"), Route->BuildPatrolNodes(Nodes, Reason));
	TestEqual(TEXT("Three authored rows still give one node per spline point"), Nodes.Num(), 3);
	if (Nodes.IsValidIndex(0))
	{
		TestEqual(TEXT("The authored wait time is carried onto the node"), Nodes[0].WaitTime, 3.f);
	}
	if (Nodes.IsValidIndex(1))
	{
		TestTrue(TEXT("Reserved activity metadata travels with its node"),
			Nodes[1].ActivityTag == Route->PointSettings[1].ActivityTag);
	}
	if (Nodes.IsValidIndex(2))
	{
		TestEqual(TEXT("A negative wait time is clamped to zero"), Nodes[2].WaitTime, 0.f);
		TestTrue(TEXT("A spot row travels with its node"), Nodes[2].Spot.bEnabled);
		TestEqual(TEXT("The spot keeps its authored visit budget"), Nodes[2].Spot.MaximumVisitSeconds, 45.f);
	}

	// Cardinality is the silent authoring mistake: rows that belong to a different spline.
	// Validation is the strict gate; the build stays lenient and still samples every drawn point, so
	// a designer keeps seeing the spline they drew while the validator reports the disagreement.
	Route->PointSettings.SetNum(2);
	TestFalse(TEXT("A row count that does not match the spline fails validation"), Route->ValidatePatrolRoute(Reason));
	TestTrue(TEXT("The failure explains which counts disagree"), Reason.ToString().Contains(TEXT("point setting rows")));
	Nodes.Reset();
	TestTrue(TEXT("The node build still samples every spline point"), Route->BuildPatrolNodes(Nodes, Reason));
	TestEqual(TEXT("A stale row count cannot shrink the drawn route"), Nodes.Num(), Points.Num());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPatrolRouteReference,
	"TerritoryFramework.Patrol.Behavior.GuardPostPrefersItsPatrolRouteActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPatrolRouteReference::RunTest(const FString& Parameters)
{
	using namespace TerritoryPatrolRouteTests;

	FFixture Fixture;
	if (!TestNotNull(TEXT("A Game world was created"), Fixture.World))
	{
		return false;
	}

	const TArray<FVector> Points = { FVector(0.f, 0.f, 0.f), FVector(400.f, 0.f, 0.f), FVector(400.f, 400.f, 0.f) };
	ATerritoryPatrolRoute* Route = Fixture.SpawnRoute(TEXT("PatrolTest_Post"), Points);
	ATerritoryGuardSpawnPoint* Post = Fixture.World->SpawnActor<ATerritoryGuardSpawnPoint>();
	if (!TestNotNull(TEXT("The patrol route actor was spawned"), Route))
	{
		return false;
	}
	if (!TestNotNull(TEXT("The guard post was spawned"), Post))
	{
		return false;
	}

	TestTrue(TEXT("A post that names no route reports none"), Post->GetEffectivePatrolRouteID().IsNone());
	TestTrue(TEXT("A post that names no route resolves no route actor"), Post->ResolvePatrolRouteActor() == nullptr);
	TestEqual(TEXT("A post that names no route keeps its authored array route"), Post->GetEffectivePatrolRoute().Num(), 0);

	Route->bLoop = false;
	Post->PatrolRouteID = Route->GetPatrolRouteID();

	TestTrue(TEXT("The post reports the authored route ID"),
		Post->GetEffectivePatrolRouteID() == Route->GetPatrolRouteID());
	TestTrue(TEXT("The route actor is resolved by ID, not by position in the level"),
		Post->ResolvePatrolRouteActor() == Route);
	TestEqual(TEXT("The spline route replaces the authored node array"),
		Post->GetEffectivePatrolRoute().Num(), Points.Num());
	TestFalse(TEXT("The route actor's own loop flag wins over the post default"),
		Post->GetEffectiveLoopPatrol());

	// An ID that names nothing has to fall back to the authored array, not keep a stale spline.
	Post->PatrolRouteID = TEXT("PatrolTest_Missing");
	TestTrue(TEXT("An unresolvable route ID resolves no actor"), Post->ResolvePatrolRouteActor() == nullptr);
	TestEqual(TEXT("An unresolvable route ID falls back to the authored array"),
		Post->GetEffectivePatrolRoute().Num(), 0);
	TestTrue(TEXT("An unresolvable route ID restores the post's own loop flag"), Post->GetEffectiveLoopPatrol());

	return true;
}

/**
 * The spline's own closed-loop flag must never be set: the engine's spline visualizer asserts in
 * FSplineComponentVisualizer::UpdateAddSegment as soon as an end point of a closed loop is dragged,
 * which crashed the editor once already. Loop policy lives on the actor instead.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPatrolRouteLoopPolicy,
	"TerritoryFramework.Patrol.Regression.ClosedLoopSplineIsRefusedAndNeverAuthored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPatrolRouteLoopPolicy::RunTest(const FString& Parameters)
{
	using namespace TerritoryPatrolRouteTests;

	FFixture Fixture;
	if (!TestNotNull(TEXT("A Game world was created"), Fixture.World))
	{
		return false;
	}

	const TArray<FVector> Points = {
		FVector(0.f, 0.f, 0.f),
		FVector(600.f, 0.f, 0.f),
		FVector(600.f, 600.f, 0.f)
	};

	ATerritoryPatrolRoute* Route = Fixture.SpawnRoute(TEXT("PatrolTest_Loop"), Points);
	if (!TestNotNull(TEXT("The patrol route actor was spawned"), Route))
	{
		return false;
	}

	TestTrue(TEXT("The route loops by default"), Route->IsLooping());
	TestFalse(TEXT("An authored loop never closes the spline the visualizer edits"),
		Route->GetRouteSpline()->IsClosedLoop());

#if WITH_EDITOR
	Route->SyncPointSettingsToSpline();
	TestFalse(TEXT("The editor sync keeps the spline open"),
		Route->GetRouteSpline()->IsClosedLoop());
	TestTrue(TEXT("The editor sync leaves the actor's own loop flag alone"), Route->IsLooping());
#endif

	// A spline closed by hand (or by a spliced-in asset) is reported, not handed to the visualizer.
	Route->GetRouteSpline()->SetClosedLoop(true, false);

	FText Reason;
	TestFalse(TEXT("A closed-loop spline fails validation"), Route->ValidatePatrolRoute(Reason));
	TestTrue(TEXT("The failure names the closed loop"),
		Reason.ToString().Contains(TEXT("closed loop")));

#if WITH_EDITOR
	Route->SyncPointSettingsToSpline();
	TestFalse(TEXT("The editor sync forces a hand-closed loop back open"),
		Route->GetRouteSpline()->IsClosedLoop());
	TestTrue(TEXT("A route whose loop was forced open validates again"), Route->ValidatePatrolRoute(Reason));
#endif

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
