#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TerritoryRoadNetworkEditorLibrary.h"
#include "Navigation/TerritoryRoadGuide.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFRoadAuthoringRefresh,
	"TerritoryFramework.Roads.FarOriginConnectionRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFRoadAuthoringRefresh::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
	ATerritoryRoadGuide* A = World->SpawnActor<ATerritoryRoadGuide>();
	ATerritoryRoadGuide* B = World->SpawnActor<ATerritoryRoadGuide>();
	A->SetStraightRoute(FVector(40000, 50000, 0), FVector(46000, 50000, 0));
	B->SetStraightRoute(FVector(46000, 50000, 0), FVector(52000, 50000, 0));
	const auto Built = UTerritoryRoadNetworkEditorLibrary::BuildFromRoadActors(World, {A, B}, {}, TEXT("Road"));
	for (auto Error : Built.Errors) AddError(Error);
	TestTrue(TEXT("Real two-road geometry builds away from the origin"), Built.bSucceeded);
	const auto Inspected = UTerritoryRoadNetworkEditorLibrary::InspectRoadNetwork(World);
	TestEqual(TEXT("Only the two intentional outer mouths remain open"), Inspected.OpenEnds, 2);
	TArray<FVector> Route; FString Failure;
	TestTrue(TEXT("Native routing crosses the shared mouth"), UTerritoryRoadNetworkEditorLibrary::PreviewVehicleRoute(World,
		FVector(41000, 50150, 0), FVector(51000, 50150, 0), Route, Failure));
	AddInfo(Failure);
	// Rebuild the same sources in a different spatial cell, preserving their identity.
	A->SetStraightRoute(FVector(-80000, -50000, 0), FVector(-74000, -50000, 0));
	B->SetStraightRoute(FVector(-74000, -50000, 0), FVector(-68000, -50000, 0));
	const auto Moved = UTerritoryRoadNetworkEditorLibrary::BuildFromRoadActors(World, {A, B}, {}, TEXT("Road"));
	TestTrue(TEXT("Moved generator sources rebuild"), Moved.bSucceeded);
	TestEqual(TEXT("Rebuild updates connection lookup at the new location"), UTerritoryRoadNetworkEditorLibrary::InspectRoadNetwork(World).OpenEnds, 2);
	TestTrue(TEXT("Native route uses the moved geometry"), UTerritoryRoadNetworkEditorLibrary::PreviewVehicleRoute(World,
		FVector(-79000, -49850, 0), FVector(-69000, -49850, 0), Route, Failure));
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif
