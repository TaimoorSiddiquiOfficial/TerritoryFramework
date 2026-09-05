#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritorySpatialIndex.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFSpatialBoundedEnumeration,
	"TerritoryFramework.Registry.Regression.BoundedSpatialEnumeration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpatialBoundedEnumeration::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Spatial world exists"), World)) return false;
	ATerritoryProperty* Territory = World->SpawnActor<ATerritoryProperty>();
	UBoxComponent* Box = Territory->FindComponentByClass<UBoxComponent>();
	if (!TestNotNull(TEXT("Territory box exists"), Box))
	{
		World->DestroyWorld(false);
		return false;
	}
	FTerritorySpatialIndex Index;
	Index.Initialize(2000.f);
	Box->SetBoxExtent(FVector(20000.0));
	Index.Insert(Territory);
	TestTrue(TEXT("Large volume storage is bounded to 4096 grid cells"), Index.GetCellCount() <= 4096);
	TestTrue(TEXT("Large volume remains discoverable by precise point query"), Index.QueryPoint(FVector::ZeroVector).Contains(Territory));
	TestTrue(TEXT("Large volume remains discoverable by box query"),
		Index.QueryBox(FBox(FVector(-100.0), FVector(100.0))).Contains(Territory));
	Index.Remove(Territory);
	TestEqual(TEXT("Removal clears large volume lookup"), Index.QueryPoint(FVector::ZeroVector).Num(), 0);
	Box->SetBoxExtent(FVector(100.0));
	Index.Insert(Territory);
	const int32 OriginalEntries = Index.GetTotalCellEntries();
	Territory->SetActorLocation(FVector(10000.0));
	Index.Insert(Territory);
	TestEqual(TEXT("Reinsertion replaces old occupied cells"), Index.GetTotalCellEntries(), OriginalEntries);
	TestEqual(TEXT("Moved volume no longer contains the old location"), Index.QueryPoint(FVector::ZeroVector).Num(), 0);
	TestTrue(TEXT("Moved volume is found at its current location"), Index.QueryPoint(FVector(10000.0)).Contains(Territory));
	TestTrue(TEXT("An enormous query scans registered bounds instead of enumerating empty cells"),
		Index.QueryBox(FBox(FVector(-1.e20), FVector(1.e20))).Contains(Territory));
	TestEqual(TEXT("Invalid box produces no spatial matches"), Index.QueryBox(FBox(ForceInit)).Num(), 0);
	const double NotFinite = std::numeric_limits<double>::quiet_NaN();
	TestEqual(TEXT("Nonfinite point cannot be converted to an integer cell"), Index.QueryPoint(FVector(NotFinite)).Num(), 0);
	Index.Initialize(std::numeric_limits<float>::quiet_NaN());
	Index.Insert(Territory);
	TestTrue(TEXT("Invalid cell-size authoring falls back to a usable grid"), Index.QueryPoint(FVector(10000.0)).Contains(Territory));
	Index.Clear();
	TestEqual(TEXT("Clearing removes every lookup"), Index.QueryPoint(FVector(10000.0)).Num(), 0);
	World->DestroyWorld(false);
	return true;
}


#endif
