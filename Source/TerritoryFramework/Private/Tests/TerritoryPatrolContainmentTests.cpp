#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/NarrativeNPCController.h"
#include "AI/TerritoryPatrolGoal.h"
#include "Components/BoxComponent.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryVolume.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

/**
 * Patrol containment (P0): an authored patrol node outside its Place is either clamped back in or
 * dropped, and a guard that chased something past its leash hands the patrol activity a score high
 * enough to walk it home. Both are pure functions of authored data, so the fixtures stay small.
 */
namespace TerritoryPatrolContainmentTests
{
	/** One Game world with a Place whose bounds box is ready for projection queries. */
	struct FFixture
	{
		UWorld* World = nullptr;
		ATerritoryVolume* Place = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World)
			{
				return;
			}
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

			Place = World->SpawnActor<ATerritoryVolume>();
			if (Place)
			{
				if (UBoxComponent* Bounds = Cast<UBoxComponent>(Place->GetBoundsShape()))
				{
					Bounds->SetBoxExtent(FVector(500.f, 500.f, 200.f), false);
					Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
					Bounds->RecreatePhysicsState();
				}
			}
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
	};

	FTerritoryPatrolNode MakeNode(const FVector& Location)
	{
		FTerritoryPatrolNode Node;
		Node.Location = Location;
		return Node;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPatrolContainmentClampDrop,
	"TerritoryFramework.Patrol.Behavior.ContainmentClampsAndDropsOutsideNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPatrolContainmentClampDrop::RunTest(const FString& Parameters)
{
	using namespace TerritoryPatrolContainmentTests;

	FFixture Fixture;
	if (!TestNotNull(TEXT("Fixture world exists"), Fixture.World)
		|| !TestNotNull(TEXT("Fixture Place exists"), Fixture.Place))
	{
		return false;
	}
	if (!TestNotNull(TEXT("Fixture Place has a bounds shape"), Fixture.Place->GetBoundsShape()))
	{
		return false;
	}

	const FVector Inside(100.f, 0.f, 0.f);
	const FVector Outside(4000.f, 0.f, 0.f);

	FVector Projected(0.f);
	TestTrue(TEXT("Premise: the bounds shape projects an outside point"),
		Fixture.Place->GetBoundsShape()->GetClosestPointOnCollision(Outside, Projected) > 0.f);
	TestTrue(TEXT("Scenario: the inside node is inside"), Fixture.Place->ContainsPoint(Inside));
	TestFalse(TEXT("Scenario: the outside node is outside"), Fixture.Place->ContainsPoint(Outside));

	// ClampToPlace projects the outside node back inside and leaves the inside node alone.
	{
		TArray<FTerritoryPatrolNode> Route = {MakeNode(Inside), MakeNode(Outside)};
		int32 Clamped = 0;
		int32 Dropped = 0;
		ATerritoryGuardCharacter::ApplyPatrolContainmentToRoute(
			Fixture.Place, ETerritoryGuardPatrolContainment::ClampToPlace, Route, &Clamped, &Dropped);
		TestEqual(TEXT("ClampToPlace clamps exactly the outside node"), Clamped, 1);
		TestEqual(TEXT("ClampToPlace drops nothing"), Dropped, 0);
		TestEqual(TEXT("ClampToPlace keeps the route cardinality"), Route.Num(), 2);
		TestTrue(TEXT("The clamped node is inside the Place"), Fixture.Place->ContainsPoint(Route[1].Location));
		TestTrue(TEXT("The inside node is untouched"), Route[0].Location.Equals(Inside, KINDA_SMALL_NUMBER));
	}

	// DropOutsideNodes removes it instead of moving it.
	{
		TArray<FTerritoryPatrolNode> Route = {MakeNode(Inside), MakeNode(Outside)};
		int32 Clamped = 0;
		int32 Dropped = 0;
		ATerritoryGuardCharacter::ApplyPatrolContainmentToRoute(
			Fixture.Place, ETerritoryGuardPatrolContainment::DropOutsideNodes, Route, &Clamped, &Dropped);
		TestEqual(TEXT("DropOutsideNodes drops exactly the outside node"), Dropped, 1);
		TestEqual(TEXT("DropOutsideNodes clamps nothing"), Clamped, 0);
		TestEqual(TEXT("DropOutsideNodes keeps only the contained node"), Route.Num(), 1);
		TestTrue(TEXT("The surviving node is the authored inside node"),
			Route[0].Location.Equals(Inside, KINDA_SMALL_NUMBER));
	}

	// The authored escape hatch and a missing Place both leave the authored data alone.
	{
		TArray<FTerritoryPatrolNode> Route = {MakeNode(Inside), MakeNode(Outside)};
		int32 Clamped = 0;
		int32 Dropped = 0;
		ATerritoryGuardCharacter::ApplyPatrolContainmentToRoute(
			Fixture.Place, ETerritoryGuardPatrolContainment::AllowOutsideNodes, Route, &Clamped, &Dropped);
		TestEqual(TEXT("AllowOutsideNodes keeps every node"), Route.Num(), 2);
		TestTrue(TEXT("AllowOutsideNodes leaves the outside node where its author put it"),
			Route[1].Location.Equals(Outside, KINDA_SMALL_NUMBER));
		TestEqual(TEXT("AllowOutsideNodes reports no work"), Clamped + Dropped, 0);

		ATerritoryGuardCharacter::ApplyPatrolContainmentToRoute(
			nullptr, ETerritoryGuardPatrolContainment::ClampToPlace, Route, &Clamped, &Dropped);
		TestEqual(TEXT("No Place means no containment work"), Clamped + Dropped, 0);
		TestEqual(TEXT("No Place keeps every node"), Route.Num(), 2);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPatrolLeashScore,
	"TerritoryFramework.Patrol.Behavior.LeashRaisesThePatrolGoalScoreOutsideThePost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPatrolLeashScore::RunTest(const FString& Parameters)
{
	using namespace TerritoryPatrolContainmentTests;

	FFixture Fixture;
	if (!TestNotNull(TEXT("Fixture world exists"), Fixture.World))
	{
		return false;
	}

	ATerritoryGuardCharacter* Guard = Fixture.World->SpawnActor<ATerritoryGuardCharacter>();
	if (!TestNotNull(TEXT("Fixture guard exists"), Guard))
	{
		return false;
	}

	ANarrativeNPCController* Controller = Guard->GetNPCController();
	if (!Controller)
	{
		Controller = Fixture.World->SpawnActor<ANarrativeNPCController>();
	}
	if (!TestNotNull(TEXT("Fixture controller exists"), Controller))
	{
		return false;
	}
	Controller->Possess(Guard);

	// A non-zero home is what a spawned-from-post guard has; the world origin is deliberately treated
	// as "no anchor" so a post-less guard is never walked back to the middle of the map.
	Guard->TerritoryHomeTransform = FTransform(FVector(2000.f, 0.f, 0.f));
	Guard->MaximumChaseDistanceFromHome = 500.f;

	UTerritoryPatrolGoal* Goal = NewObject<UTerritoryPatrolGoal>(Guard);
	Goal->TerritoryPatrol = {MakeNode(FVector(200.f, 0.f, 0.f))};
	Goal->OwnerController = Controller;

	Guard->SetActorLocation(FVector(2200.f, 0.f, 0.f));
	TestFalse(TEXT("A guard inside its leash is not leashed"), Guard->IsBeyondTerritoryLeash());
	TestTrue(TEXT("Inside the leash the goal keeps its ordinary score"),
		Goal->GetGoalScore_Implementation() < Goal->LeashReturnScore);

	Guard->SetActorLocation(FVector(9000.f, 0.f, 0.f));
	TestTrue(TEXT("A guard beyond its leash is leashed"), Guard->IsBeyondTerritoryLeash());
	TestEqual(TEXT("Beyond the leash the patrol goal returns its leash score"),
		Goal->GetGoalScore_Implementation(), Goal->LeashReturnScore);

	// 0 disables the leash, and a guard with no anchor at all keeps the vendor behaviour.
	Guard->MaximumChaseDistanceFromHome = 0.f;
	TestFalse(TEXT("A zero leash is disabled"), Guard->IsBeyondTerritoryLeash());
	TestTrue(TEXT("A disabled leash leaves the ordinary score"),
		Goal->GetGoalScore_Implementation() < Goal->LeashReturnScore);

	Guard->MaximumChaseDistanceFromHome = 500.f;
	Guard->TerritoryHomeTransform = FTransform(FVector::ZeroVector);
	TestFalse(TEXT("No post and no replicated home means no leash to measure"), Guard->IsBeyondTerritoryLeash());

	Goal->TerritoryPatrol.Reset();
	TestEqual(TEXT("A route with no nodes scores zero"), Goal->GetGoalScore_Implementation(), 0.f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
