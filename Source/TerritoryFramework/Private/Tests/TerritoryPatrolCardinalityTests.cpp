#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

/**
 * A guard post's patrol route can hold any number of nodes, including one, and a post with no
 * route patrols in place when it opts in. This test drives the production predicates and the
 * route the patrol goal actually receives, so it fails if the cardinality rule is relaxed in
 * one place and not the other, and if the opt-in leaks into posts that did not ask for it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPatrolRouteCardinality,
	"TerritoryFramework.Guards.Regression.PatrolRouteCardinalityAndImplicitStop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPatrolRouteCardinality::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Patrol cardinality test world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> ScriptCallbacks(GAllowActorScriptExecutionInEditor, true);

	const FTransform PostTransform(FRotator(0.f, 90.f, 0.f), FVector(1000.f, 2000.f, 30.f));
	ATerritoryGuardSpawnPoint* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>(
		ATerritoryGuardSpawnPoint::StaticClass(), PostTransform);
	ATerritoryGuardCharacter* Guard = World->SpawnActor<ATerritoryGuardCharacter>();
	if (!TestNotNull(TEXT("Territory guard post"), Post)
		|| !TestNotNull(TEXT("Territory guard"), Guard))
	{
		World->DestroyWorld(false);
		return false;
	}

	UTerritoryGuardPostDefinition* Definition = NewObject<UTerritoryGuardPostDefinition>();
	Post->GuardPostDefinition = Definition;
	Guard->OwningTerritorySpawnPoint = Post;

	// ─── No route, no opt-in: the post has no patrol duty at all ───
	TestFalse(TEXT("A post with no route and no opt-in has no authored route"),
		Post->HasPatrolRoute());
	TestFalse(TEXT("A post with no route and no opt-in has no implicit stop"),
		Post->HasImplicitPatrolStop());
	TestFalse(TEXT("A post with no route and no opt-in has no patrol duty"),
		Post->HasAnyPatrolDuty());
	TestFalse(TEXT("A post with no route is not the deployment tie-break winner"),
		Post->HasMultiStopPatrolRoute());
	TestEqual(TEXT("A post with no duty gives its guard no patrol stop"),
		Guard->BuildStaggeredPatrolRoute().Num(), 0);
	TestFalse(TEXT("A post with no duty leaves its guard without patrol duty"),
		Guard->HasAnyTerritoryPatrolDuty());

	// ─── No route, opt-in on the definition: one implicit stop at the post ───
	Definition->bUseSpawnTransformAsPatrolStop = true;
	TestTrue(TEXT("Opting in with no route yields an implicit stop"),
		Post->HasImplicitPatrolStop());
	TestTrue(TEXT("Opting in with no route yields patrol duty"), Post->HasAnyPatrolDuty());
	TestFalse(TEXT("An implicit stop is not an authored route"), Post->HasPatrolRoute());
	TestFalse(TEXT("An implicit stop does not win the deployment tie-break"),
		Post->HasMultiStopPatrolRoute());
	TestTrue(TEXT("An opted-in post gives its guard patrol duty"),
		Guard->HasAnyTerritoryPatrolDuty());
	TestFalse(TEXT("An opted-in post gives its guard no authored route"),
		Guard->HasTerritoryPatrolRoute());

	const FTerritoryPatrolNode ImplicitStop = Post->GetImplicitPatrolStop();
	TestTrue(TEXT("The implicit stop sits at the post's own location"),
		ImplicitStop.Location.Equals(Post->GetActorLocation(), 0.01f));
	TestTrue(TEXT("The implicit stop faces the post's own rotation"),
		ImplicitStop.Rotation.Equals(Post->GetActorRotation(), 0.01f));
	TestEqual(TEXT("The implicit stop carries the node default wait time"),
		ImplicitStop.WaitTime, FTerritoryPatrolNode().WaitTime);
	TestFalse(TEXT("The implicit stop carries no activity tag"),
		ImplicitStop.ActivityTag.IsValid());

	const TArray<FTerritoryPatrolNode> ImplicitRoute = Guard->BuildStaggeredPatrolRoute();
	if (TestEqual(TEXT("The guard materialises exactly one stop"), ImplicitRoute.Num(), 1))
	{
		TestTrue(TEXT("The materialised stop is the post's own transform"),
			ImplicitRoute[0].Location.Equals(Post->GetActorLocation(), 0.01f));
	}

	// ─── One authored node: a real single-stop route ───
	Definition->bUseSpawnTransformAsPatrolStop = false;
	Definition->PatrolRoute.SetNum(1);
	Definition->PatrolRoute[0].Location = FVector(3000.f, 4000.f, 0.f);
	TestTrue(TEXT("One authored node is a route"), Post->HasPatrolRoute());
	TestTrue(TEXT("One authored node is patrol duty"), Post->HasAnyPatrolDuty());
	TestFalse(TEXT("One authored node is not a multi-stop route"),
		Post->HasMultiStopPatrolRoute());
	TestFalse(TEXT("An authored route suppresses the implicit stop"),
		Post->HasImplicitPatrolStop());
	TestTrue(TEXT("A one-node post gives its guard an authored route"),
		Guard->HasTerritoryPatrolRoute());

	const TArray<FTerritoryPatrolNode> SingleRoute = Guard->BuildStaggeredPatrolRoute();
	if (TestEqual(TEXT("A one-node route builds one stop"), SingleRoute.Num(), 1))
	{
		TestTrue(TEXT("The one-node route keeps its authored location"),
			SingleRoute[0].Location.Equals(FVector(3000.f, 4000.f, 0.f), 0.01f));
	}

	// ─── Two authored nodes: a multi-stop walk, still deterministic ───
	Definition->PatrolRoute.SetNum(2);
	Definition->PatrolRoute[0].Location = FVector(3000.f, 4000.f, 0.f);
	Definition->PatrolRoute[1].Location = FVector(5000.f, 6000.f, 0.f);
	TestTrue(TEXT("Two authored nodes are a multi-stop route"),
		Post->HasMultiStopPatrolRoute());

	const TArray<FTerritoryPatrolNode> FirstWalk = Guard->BuildStaggeredPatrolRoute();
	const TArray<FTerritoryPatrolNode> SecondWalk = Guard->BuildStaggeredPatrolRoute();
	// The stagger start comes from the guard's stable identity, so which node it starts on is
	// not fixed here. What must hold is that the same guard asking twice gets the same walk,
	// and that the walk is a rotation of the authored route rather than a lossy rebuild.
	const FVector NodeA(3000.f, 4000.f, 0.f);
	const FVector NodeB(5000.f, 6000.f, 0.f);
	if (TestEqual(TEXT("A two-node route builds two stops"), FirstWalk.Num(), 2))
	{
		TestTrue(TEXT("The staggered walk repeats identically for the same guard"),
			FirstWalk[0].Location.Equals(SecondWalk[0].Location, 0.01f)
			&& FirstWalk[1].Location.Equals(SecondWalk[1].Location, 0.01f));
		TestTrue(TEXT("The staggered walk keeps both authored nodes in some rotation"),
			(FirstWalk[0].Location.Equals(NodeA, 0.01f)
				&& FirstWalk[1].Location.Equals(NodeB, 0.01f))
			|| (FirstWalk[0].Location.Equals(NodeB, 0.01f)
				&& FirstWalk[1].Location.Equals(NodeA, 0.01f)));
	}

	// An authored route wins over the opt-in even when both are set on the definition.
	Definition->bUseSpawnTransformAsPatrolStop = true;
	TestFalse(TEXT("An authored route suppresses the implicit stop even when opted in"),
		Post->HasImplicitPatrolStop());
	TestTrue(TEXT("A multi-stop post stays a multi-stop post when opted in"),
		Post->HasMultiStopPatrolRoute());

	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
