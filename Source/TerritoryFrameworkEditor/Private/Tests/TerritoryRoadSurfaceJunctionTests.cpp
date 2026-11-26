#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TerritoryRoadNetworkEditorLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFRoadSurfaceTJunction,
	"TerritoryFramework.Roads.PhysicalTJunctionConnections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFRoadSurfaceTJunction::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
	UPhysicalMaterial* Material = NewObject<UPhysicalMaterial>();
	Material->SurfaceType = SurfaceType9;
	auto Slab = [&](FVector Location, FVector Scale)
	{
		auto* Actor = World->SpawnActor<AStaticMeshActor>();
		Actor->SetActorLocation(Location); Actor->SetActorScale3D(Scale);
		auto* Mesh = Actor->GetStaticMeshComponent();
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Mesh->SetCollisionProfileName(TEXT("BlockAll")); Mesh->SetPhysMaterialOverride(Material);
	};
	// A T is deliberately asymmetric: aiming mouths at their centroid turns both
	// horizontal mouths away from the actual road tangents.
	Slab(FVector(40000,50000,0), FVector(100,12,1));
	Slab(FVector(40000,52300,0), FVector(12,46,1));
	FTerritoryRoadSurfaceBakeSettings Settings;
	Settings.Bounds = FBox(FVector(34400,49000,-100), FVector(45600,55200,500));
	Settings.SampleSpacing = 100; Settings.BakeID = FGuid(960,961,962,963);
	Settings.RoadPhysicalMaterial = Material;
	const auto Built = UTerritoryRoadNetworkEditorLibrary::BakeRoadSurfaces(World, Settings);
	for (const auto& Error : Built.Errors) AddError(Error);
	TestTrue(TEXT("Physical T surface extracts"), Built.bSucceeded);
	TestEqual(TEXT("Three physical branches"), Built.Roads, 3);
	TestEqual(TEXT("One real junction"), Built.Junctions, 1);
	const auto Inspected = UTerritoryRoadNetworkEditorLibrary::InspectRoadNetwork(World);
	TestEqual(TEXT("Only three intentional dead ends; all junction mouths connect"), Inspected.OpenEnds, 3);
	TestEqual(TEXT("All T branches belong to one Native lane group"), Inspected.ConnectedRoadGroups, 1);
	const TArray<FVector> Branches = {FVector(36500,50000,50), FVector(43500,50000,50), FVector(40000,53500,50)};
	for (int32 A=0; A<3; ++A) for (int32 B=0; B<3; ++B) if (A!=B)
	{
		TArray<FVector> Route; FString Failure;
		TestTrue(*FString::Printf(TEXT("Native route crosses T from branch %d to %d"), A, B),
			UTerritoryRoadNetworkEditorLibrary::PreviewVehicleRoute(World, Branches[A], Branches[B], Route, Failure));
		if (!Failure.IsEmpty()) AddInfo(Failure);
	}
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif
