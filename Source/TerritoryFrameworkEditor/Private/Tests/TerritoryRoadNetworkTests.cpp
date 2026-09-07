#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TerritoryRoadSurfaceExtraction.h"
#include "TerritoryRoadNetworkEditorLibrary.h"
#include "Navigation/TerritoryRoadGuide.h"
#include "Navigation/TerritoryRoadTrafficSubsystem.h"
#include "Components/SplineComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Vehicles/Mass/QuestRoadControls.h"
#include "ZoneShapeActor.h"
#include "ZoneShapeComponent.h"
#include "ZoneGraphData.h"
#include "ZoneGraphSettings.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Weapons/WeaponVisual.h"
#include "Engine/StaticMeshSocket.h"
#include "Vehicles/NarrativeVehicleBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFRoadSurfaceTopology,
	"TerritoryFramework.Roads.SurfaceTopologyAndDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFRoadSurfaceTopology::RunTest(const FString& Parameters)
{
	using namespace TerritoryRoadSurfaceExtraction;
	FGrid Grid;
	Grid.Width = Grid.Height = 81;
	Grid.Spacing = 100.f;
	Grid.Road.Init(0, Grid.Width * Grid.Height);
	Grid.Heights.Init(0.f, Grid.Road.Num());
	for (int32 Y = 3; Y < 78; ++Y) for (int32 X = 3; X < 78; ++X)
		if (FMath::Abs(X-40) <= 5 || FMath::Abs(Y-40) <= 5) Grid.Road[Y*Grid.Width+X] = 1;
	FNetwork Network, Again;
	FString Failure;
	TestTrue(TEXT("Surface cross extracts"), Extract(Grid, 600.f, 60.f, Network, Failure));
	AddInfo(Failure);
	TestEqual(TEXT("Cross has four road branches"), Network.Roads.Num(), 4);
	TestEqual(TEXT("Cross has one junction"), Network.Junctions.Num(), 1);
	TestEqual(TEXT("Cross has four dead ends"), Network.OpenEnds, 4);
	TestTrue(TEXT("Same sampled collision rebuilds"), Extract(Grid, 600.f, 60.f, Again, Failure));
	TestTrue(TEXT("No random road or node ordering"), Again.Roads == Network.Roads && Again.Junctions == Network.Junctions);
	for (int32 Y = 0; Y < 81; ++Y) for (int32 X = 0; X < 81; ++X)
	{
		const float Radius = FVector2D(X-40,Y-40).Size();
		Grid.Road[Y*81+X] = Radius >= 21.f && Radius <= 35.f;
	}
	TestTrue(TEXT("An isolated ring is retained"), Extract(Grid, 600.f, 60.f, Again, Failure));
	TestEqual(TEXT("A ring is split into two Native shapes that can connect"), Again.Roads.Num(), 2);
	for (int32 Y = 0; Y < 81; ++Y) for (int32 X = 0; X < 81; ++X)
		Grid.Road[Y*81+X] = X >= 5 && X <= 75 && FMath::Abs(Y-40) <= 1;
	TestFalse(TEXT("A narrow road paint stroke cannot support a car lane profile"), Extract(Grid, 600.f, 60.f, Again, Failure));
	Grid.Road.SetNum(1);
	TestFalse(TEXT("Malformed grids fail without out-of-bounds reads"), Extract(Grid, 600.f, 60.f, Again, Failure));
	TestTrue(TEXT("A failed extraction clears prior results"), Again.Roads.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFRoadPhysicalCollisionBake,
	"TerritoryFramework.Roads.PhysicalCollisionBakeAndFailurePreservesLanes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFRoadPhysicalCollisionBake::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
	AStaticMeshActor* Road = World->SpawnActor<AStaticMeshActor>();
	UStaticMeshComponent* Mesh = Road->GetStaticMeshComponent();
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Road->SetActorScale3D(FVector(70.f, 14.f, 1.f));
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	UPhysicalMaterial* Material = NewObject<UPhysicalMaterial>();
	Material->SurfaceType = SurfaceType2;
	Mesh->SetPhysMaterialOverride(Material);
	FTerritoryRoadSurfaceBakeSettings Settings;
	Settings.Bounds = FBox(FVector(-4000,-1000,-100), FVector(4000,1000,500));
	Settings.SampleSpacing = 100.f;
	Settings.BakeID = FGuid(320,321,322,323);
	Settings.RoadPhysicalMaterial = Material;
	const auto Built = UTerritoryRoadNetworkEditorLibrary::BakeRoadSurfaces(World, Settings);
	for (const FString& Error : Built.Errors) AddError(Error);
	TestTrue(TEXT("Actual collision-body physical surface builds a Native road"), Built.bSucceeded);
	TestTrue(TEXT("Collision queries found road samples"), Built.RoadSamples > 100);
	const auto Before = UTerritoryRoadNetworkEditorLibrary::InspectRoadNetwork(World);
	TestTrue(TEXT("Surface road produces actual Native lane data"), Before.NativeRoadLanes >= 2);
	Mesh->SetPhysMaterialOverride(nullptr);
	const auto Rejected = UTerritoryRoadNetworkEditorLibrary::BakeRoadSurfaces(World, Settings);
	TestFalse(TEXT("Default surfaces cannot become roads"), Rejected.bSucceeded);
	const auto After = UTerritoryRoadNetworkEditorLibrary::InspectRoadNetwork(World);
	TestEqual(TEXT("A failed rebake preserves the prior Native lanes"), After.NativeRoadLanes, Before.NativeRoadLanes);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFRoadArrivalGeometry,
	"TerritoryFramework.Roads.ArrivalCandidatesFollowRouteAndKeepRight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFRoadArrivalGeometry::RunTest(const FString& Parameters)
{
	const TArray<FVector> Route = {FVector(0,0,0), FVector(2000,0,0), FVector(2000,2000,0)};
	const auto Candidates = UTerritoryRoadTrafficSubsystem::BuildArrivalCandidates(Route, 3500.f, 1000.f);
	TestEqual(TEXT("Bounded candidate count"), Candidates.Num(), 4);
	if (Candidates.Num() == 4)
	{
		TestEqual(TEXT("First claim uses authored endpoint"), Candidates[0], Route.Last());
		TestEqual(TEXT("Second squad stops back along its lane"), Candidates[1], FVector(2000,1000,0));
		TestEqual(TEXT("Candidates follow bends instead of cutting across city blocks"), Candidates[3], FVector(1000,0,0));
	}
	TestTrue(TEXT("Invalid route offers no parking"), UTerritoryRoadTrafficSubsystem::BuildArrivalCandidates({}, 5000.f, 1000.f).IsEmpty());
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ATerritoryRoadGuide* Guide = World->SpawnActor<ATerritoryRoadGuide>();
	Guide->SetStraightRoute(FVector::ZeroVector, FVector(5000,0,0));
	Guide->LaneCenterOffset = 150.f;
	TestEqual(TEXT("Right lane is +Y while driving +X"), Guide->GetRouteStartTransform(false, ETerritoryRoadLaneSide::Right).GetLocation().Y, 150.0);
	TestEqual(TEXT("Right lane is -Y while driving -X"), Guide->GetRouteStartTransform(true, ETerritoryRoadLaneSide::Right).GetLocation().Y, -150.0);
	UTerritoryRoadTrafficSubsystem* Traffic = World->GetSubsystem<UTerritoryRoadTrafficSubsystem>();
	AQuestRoadControls* Controls = World->SpawnActor<AQuestRoadControls>();
	Controls->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("A client controller cannot mutate Native traffic"), Traffic->AcquireMissionTraffic(Controls, 5));
	TestEqual(TEXT("Rejected lease leaves no users"), Traffic->GetMissionTrafficUserCount(Controls), 0);
	UWorld* Other = UWorld::CreateWorld(EWorldType::Game, false);
	AQuestRoadControls* Foreign = Other->SpawnActor<AQuestRoadControls>();
	TestFalse(TEXT("Another world's controller cannot acquire this world"), Traffic->AcquireMissionTraffic(Foreign, 5));
	TestFalse(TEXT("Side avoidance requires a real same-direction Native road"), Traffic->IsSafeRoadOffset(FVector::ZeroVector, FVector::ForwardVector, 125.f));
	World->DestroyWorld(false);
	Other->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFRoadGeneratorBake,
	"TerritoryFramework.Roads.NativeGeneratorBakeRebuildAndRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFRoadGeneratorBake::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
	AActor* Source = World->SpawnActor<AActor>();
	USplineComponent* Curve = NewObject<USplineComponent>(Source);
	Source->SetRootComponent(Curve);
	Source->AddInstanceComponent(Curve);
	Curve->RegisterComponent();
	Curve->ClearSplinePoints(false);
	Curve->AddSplinePoint(FVector(0,0,0), ESplineCoordinateSpace::World, false);
	Curve->AddSplinePoint(FVector(6000,0,0), ESplineCoordinateSpace::World, false);
	Curve->UpdateSpline();
	const auto First = UTerritoryRoadNetworkEditorLibrary::BuildFromRoadActors(World, {Source}, {}, TEXT("Road"));
	for (const FString& Error : First.Errors) AddError(Error);
	TestTrue(TEXT("Existing generator curve bakes to Native road"), First.bSucceeded);
	const auto Second = UTerritoryRoadNetworkEditorLibrary::BuildFromRoadActors(World, {Source}, {}, TEXT("Road"));
	TestTrue(TEXT("Same source GUID rebuilds"), Second.bSucceeded);
	int32 Shapes = 0;
	AZoneShape* Baked = nullptr;
	for (TActorIterator<AZoneShape> It(World); It; ++It) { ++Shapes; Baked = *It; }
	TestEqual(TEXT("Rebuild does not duplicate road authority"), Shapes, 1);
	if (Baked)
	{
		TestFalse(TEXT("World Partition cannot stream generated authoring data out mid-bake"), Baked->GetIsSpatiallyLoaded());
		UZoneShapeComponent* Shape = const_cast<UZoneShapeComponent*>(Baked->GetShape());
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		FObjectAndNameAsStringProxyArchive Save(Writer, false);
		Shape->Serialize(Save);
		const FVector SavedEnd = Shape->GetPoints().Last().Position;
		Shape->GetMutablePoints().Last().Position = FVector(55,66,77);
		FMemoryReader Reader(Bytes);
		FObjectAndNameAsStringProxyArchive Load(Reader, true);
		Shape->Serialize(Load);
		TestEqual(TEXT("Native shape serialization preserves its geometry"), Shape->GetPoints().Last().Position, SavedEnd);
	}
	TArray<FVector> Forward, Reverse;
	FString Failure;
	TestTrue(TEXT("Native route chooses keep-right forward lane"), UTerritoryCounterAttackSubsystem::BuildNarrativeVehicleRoute(World,
		FVector(500,0,0), FVector(5500,0,0), Forward, &Failure));
	AddInfo(Failure);
	TestTrue(TEXT("Reverse journey chooses reverse-direction lane instead of driving backwards"), UTerritoryCounterAttackSubsystem::BuildNarrativeVehicleRoute(World,
		FVector(5500,0,0), FVector(500,0,0), Reverse, &Failure));
	if (Forward.Num() > 2) TestTrue(TEXT("Forward samples use positive Y lane"), Forward[1].Y > 0.f);
	if (Reverse.Num() > 2) TestTrue(TEXT("Reverse samples use negative Y lane"), Reverse[1].Y < 0.f);
	const auto Rejected = UTerritoryRoadNetworkEditorLibrary::BuildFromRoadActors(World, {Source,Source}, {}, TEXT("Road"));
	TestFalse(TEXT("Duplicated source list fails before mutation"), Rejected.bSucceeded);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFRoadIntermediateLaneChange,
	"TerritoryFramework.Roads.IntermediateTurnLaneRequiresForwardLaneChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFRoadIntermediateLaneChange::RunTest(const FString& Parameters)
{
	if (!GetDefault<UZoneGraphSettings>()->GetLaneProfiles().ContainsByPredicate(
		[](const FZoneLaneProfile& P) { return P.Name == TEXT("CityRoad4L"); }))
	{ AddInfo(TEXT("Optional project CityRoad4L fixture is absent.")); return true; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
	TArray<AActor*> Sources;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		AActor* Source = World->SpawnActor<AActor>();
		USplineComponent* Curve = NewObject<USplineComponent>(Source);
		Source->SetRootComponent(Curve);
		Source->AddInstanceComponent(Curve);
		Curve->RegisterComponent();
		Curve->ClearSplinePoints(false);
		Curve->AddSplinePoint(FVector(Index*6000.f,0,0), ESplineCoordinateSpace::World, false);
		Curve->AddSplinePoint(FVector((Index+1)*6000.f,0,0), ESplineCoordinateSpace::World, false);
		Curve->UpdateSpline();
		Sources.Add(Source);
	}
	const auto Built = UTerritoryRoadNetworkEditorLibrary::BuildFromRoadActors(World, Sources, {}, TEXT("CityRoad4L"));
	TestTrue(TEXT("Three four-lane Native roads build"), Built.bSucceeded);
	AZoneGraphData* Data = nullptr;
	for (TActorIterator<AZoneGraphData> It(World); It; ++It) Data = *It;
	if (TestNotNull(TEXT("Native lane data exists"), Data))
	{
		FZoneGraphStorage& Storage = Data->GetStorageMutable();
		TArray<int32> ZoneOrder;
		for (int32 I = 0; I < Storage.Zones.Num(); ++I) ZoneOrder.Add(I);
		ZoneOrder.Sort([&](int32 A, int32 B) { return Storage.Zones[A].Bounds.GetCenter().X < Storage.Zones[B].Bounds.GetCenter().X; });
		for (int32 Segment = 0; Segment < FMath::Min(2, ZoneOrder.Num()); ++Segment)
		{
			const FZoneData& Zone = Storage.Zones[ZoneOrder[Segment]];
			TArray<int32> Forward;
			for (int32 I = Zone.LanesBegin; I < Zone.LanesEnd; ++I)
			{
				const FZoneLaneData& Lane = Storage.Lanes[I];
				if (Storage.LanePoints[Lane.PointsEnd-1].X > Storage.LanePoints[Lane.PointsBegin].X) Forward.Add(I);
			}
			Forward.Sort([&](int32 A, int32 B) { return Storage.LanePoints[Storage.Lanes[A].PointsBegin].Y > Storage.LanePoints[Storage.Lanes[B].PointsBegin].Y; });
			if (!TestEqual(TEXT("Fixture has two forward lanes"), Forward.Num(), 2)) continue;
			// First road can leave only from the outer lane; the next can leave
			// only from the inner lane. Starting on a different lane cannot solve it.
			const FZoneLaneData& BlockedExit = Storage.Lanes[Forward[Segment == 0 ? 1 : 0]];
			for (int32 Link = BlockedExit.LinksBegin; Link < BlockedExit.LinksEnd; ++Link)
				if (Storage.LaneLinks[Link].Type == EZoneLaneLinkType::Outgoing) Storage.LaneLinks[Link].Type = EZoneLaneLinkType::None;
		}
		TArray<FVector> Route;
		FString Failure;
		TestTrue(TEXT("A later turn lane is reachable through Native same-direction adjacency"),
			UTerritoryCounterAttackSubsystem::BuildNarrativeVehicleRoute(World, FVector(500,600,0), FVector(17500,600,0), Route, &Failure));
		AddInfo(Failure);
		bool bTransition = false;
		for (int32 Index = 1; Index < Route.Num(); ++Index)
		{
			TestTrue(TEXT("No jump back to the adjacent lane's start"), Route[Index].X + 1.f >= Route[Index-1].X);
			TestTrue(TEXT("No opposing carriageway samples"), Route[Index].Y > 0.f);
			bTransition |= Route[Index].X > 6000.f && Route[Index].X < 12000.f && Route[Index].Y > 210.f && Route[Index].Y < 590.f;
		}
		TestTrue(TEXT("Lane change is sampled gradually on the middle road"), bTransition);
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFRoadJunctionAllTurns,
	"TerritoryFramework.Roads.GeneratedJunctionRetainsEveryExit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFRoadJunctionAllTurns::RunTest(const FString& Parameters)
{
	if (!GetDefault<UZoneGraphSettings>()->GetLaneProfiles().ContainsByPredicate(
		[](const FZoneLaneProfile& P) { return P.Name == TEXT("CityRoad4L"); }))
	{ AddInfo(TEXT("Optional project CityRoad4L fixture is absent.")); return true; }
	for (const int32 MouthCount : {3,4})
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
		GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
		UStaticMesh* SocketMesh = NewObject<UStaticMesh>();
		TArray<AActor*> Roads;
		TArray<FVector> Ends;
		for (int32 Index = 0; Index < MouthCount; ++Index)
		{
			const FVector Out = FRotator(0,Index*90.f,0).Vector();
			UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(SocketMesh);
			Socket->SocketName = FName(*FString::Printf(TEXT("Socket%d"),Index+1));
			Socket->RelativeLocation = Out * 1300.f;
			Socket->RelativeRotation = Out.Rotation();
			SocketMesh->Sockets.Add(Socket);
			AActor* Road = World->SpawnActor<AActor>();
			USplineComponent* Curve = NewObject<USplineComponent>(Road);
			Road->SetRootComponent(Curve);
			Road->AddInstanceComponent(Curve);
			Curve->RegisterComponent();
			Curve->ClearSplinePoints(false);
			Curve->AddSplinePoint(Out*1300.f, ESplineCoordinateSpace::World, false);
			Curve->AddSplinePoint(Out*7300.f, ESplineCoordinateSpace::World, false);
			Curve->UpdateSpline();
			Roads.Add(Road);
			Ends.Add(Out*6000.f);
		}
		AStaticMeshActor* Junction = World->SpawnActor<AStaticMeshActor>();
		Junction->GetStaticMeshComponent()->SetStaticMesh(SocketMesh);
		const auto Built = UTerritoryRoadNetworkEditorLibrary::BuildFromRoadActors(World, Roads, {Junction}, TEXT("CityRoad4L"));
		TestTrue(TEXT("Native socket junction builds"), Built.bSucceeded);
		for (int32 From = 0; From < MouthCount; ++From) for (int32 To = 0; To < MouthCount; ++To)
		{
			if (From == To) continue;
			TArray<FVector> Route;
			FString Failure;
			TestTrue(*FString::Printf(TEXT("%d-mouth junction permits journey %d -> %d"), MouthCount, From, To),
				UTerritoryCounterAttackSubsystem::BuildNarrativeVehicleRoute(World, Ends[From], Ends[To], Route, &Failure));
		}
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFRoadSteeringObstacle,
	"TerritoryFramework.Roads.SteeringCorridorDetectsAdjacentObstacle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFRoadSteeringObstacle::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	UClass* VehicleClass = LoadClass<ANarrativeVehicleBase>(nullptr,
		TEXT("/NarrativePro/Pro/Core/BP/Vehicles/Demo/vehicle03_Car/BPV_Sedan.BPV_Sedan_C"));
	if (TestNotNull(TEXT("Actual Narrative vehicle loads"), VehicleClass))
	{
		ANarrativeVehicleBase* Vehicle = World->SpawnActor<ANarrativeVehicleBase>(VehicleClass);
		AActor* Owner = World->SpawnActor<AActor>();
		UTerritoryAssaultParticipantComponent* Participant = NewObject<UTerritoryAssaultParticipantComponent>(Owner);
		Participant->RegisterComponent();
		Participant->NarrativeIngressVehicle = Vehicle;
		AStaticMeshActor* Obstacle = World->SpawnActor<AStaticMeshActor>();
		Obstacle->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Obstacle->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Obstacle->SetActorScale3D(FVector(2,2,2));
		Obstacle->SetActorLocation(FVector(900,550,100));
		float Distance = 0.f;
		TestFalse(TEXT("The current lane is clear"), Participant->QueryVehicleObstacleDistance(FVector::ZeroVector, Distance));
		TestTrue(TEXT("The intended diagonal merge detects the adjacent obstacle"),
			Participant->QueryVehicleObstacleDistance(FVector::ZeroVector, Distance, FVector(750,550,0)));
		Obstacle->SetActorEnableCollision(false);
		TestFalse(TEXT("A cleared merge becomes traversable"),
			Participant->QueryVehicleObstacleDistance(FVector::ZeroVector, Distance, FVector(750,550,0)));
		auto* Occupant = World->SpawnActor<ATerritoryAssaultCharacter>();
		Occupant->AssaultParticipant->NarrativeIngressVehicle = Vehicle;
		auto* Sword = World->SpawnActor<AWeaponVisual>();
		Sword->SetOwner(Occupant);
		Sword->AttachToComponent(Occupant->GetMesh(), FAttachmentTransformRules::KeepWorldTransform);
		auto* MeleeCollider = NewObject<UBoxComponent>(Sword, TEXT("MeleeCollider"));
		MeleeCollider->SetupAttachment(Sword->GetRootComponent());
		MeleeCollider->SetBoxExtent(FVector(30.f));
		MeleeCollider->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		MeleeCollider->RegisterComponent();
		Sword->SetActorLocation(FVector(175.f, 0.f, 90.f));
		TestFalse(TEXT("A mounted occupant's separate sword collider cannot hold its own car's brake"),
			Participant->QueryVehicleObstacleDistance(FVector::ZeroVector, Distance));
		Obstacle->SetActorLocation(FVector(600.f, 0.f, 100.f));
		Obstacle->SetActorEnableCollision(true);
		TestTrue(TEXT("Ignoring the occupant's sword still detects a real obstacle behind it"),
			Participant->QueryVehicleObstacleDistance(FVector::ZeroVector, Distance));
		Obstacle->SetActorEnableCollision(false);
		Occupant->SetActorEnableCollision(false);
		Occupant->AssaultParticipant->NarrativeIngressVehicle.Reset();
		TestTrue(TEXT("A departed occupant's weapon is once again a real road obstacle"),
			Participant->QueryVehicleObstacleDistance(FVector::ZeroVector, Distance));
		Occupant->AssaultParticipant->NarrativeIngressVehicle = World->SpawnActor<ANarrativeVehicleBase>(VehicleClass,
			FVector(0.f, 5000.f, 0.f), FRotator::ZeroRotator);
		TestTrue(TEXT("Another squad's weapon is never excluded by the first vehicle"),
			Participant->QueryVehicleObstacleDistance(FVector::ZeroVector, Distance));
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFRoadBlockedDeparture,
	"TerritoryFramework.Roads.BlockedEntranceUsesBoundedRouteDeparture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFRoadBlockedDeparture::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	UClass* VehicleClass = LoadClass<ANarrativeVehicleBase>(nullptr,
		TEXT("/NarrativePro/Pro/Core/BP/Vehicles/Demo/vehicle03_Car/BPV_Sedan.BPV_Sedan_C"));
	auto* Traffic = World->GetSubsystem<UTerritoryRoadTrafficSubsystem>();
	if (TestNotNull(TEXT("Actual Narrative car loads"), VehicleClass) && Traffic)
	{
		auto* Car = World->SpawnActor<ANarrativeVehicleBase>(VehicleClass);
		TestNotNull(TEXT("Parked car exists"), Car);
		TArray<FVector> Route = {FVector::ZeroVector, FVector(500,0,0), FVector(1000,0,0), FVector(3000,0,0), FVector(5000,0,0)};
		FTransform Departure = FTransform::Identity;
		TestTrue(TEXT("A parked entrance does not indefinitely block the second squad"), Traffic->ResolveBlockedDeparture(Route, Departure));
		TestTrue(TEXT("Departure stays within twenty meters of the authored entrance"), Departure.GetLocation().X >= 900.f && Departure.GetLocation().X <= 2000.f);
		TestEqual(TEXT("Alternative stays on the same lane"), Departure.GetLocation().Y, 0.0);
		const FVector End = Route.Last();
		TestTrue(TEXT("Driving path can start at the new departure"), Traffic->TrimRouteToDeparture(Route, Departure.GetLocation()));
		TestTrue(TEXT("Driver never rewinds to the occupied entrance"), Route[0].Equals(Departure.GetLocation(), 1.f));
		TestTrue(TEXT("Original destination is preserved"), Route.Last().Equals(End));
		const TArray<FVector> BeforeFailure = Route;
		TestFalse(TEXT("Off-road spawn cannot trim the authored route"), Traffic->TrimRouteToDeparture(Route, FVector(0,5000,0)));
		TestTrue(TEXT("Failed trimming preserves the route"), Route == BeforeFailure);
		TestFalse(TEXT("A departure at the destination has no driving journey"), Traffic->TrimRouteToDeparture(Route, End));
		TestTrue(TEXT("Destination rejection also preserves the route"), Route == BeforeFailure);
		FTransform Unchanged = FTransform::Identity;
		TestFalse(TEXT("Short route cannot turn a reinforcement into a destination teleport"), Traffic->ResolveBlockedDeparture({FVector::ZeroVector,FVector(1800,0,0)}, Unchanged));
		TestTrue(TEXT("Failed search preserves the spawn transform"), Unchanged.Equals(FTransform::Identity));
		FTransform Repeated = FTransform::Identity;
		const TArray<FVector> OriginalRoute = {FVector::ZeroVector,FVector(5000,0,0)};
		Traffic->ResolveBlockedDeparture(OriginalRoute, Repeated);
		TestTrue(TEXT("Same geometry/occupancy yields the same departure after reconstruction"), Repeated.Equals(Departure));
		AStaticMeshActor* Road = World->SpawnActor<AStaticMeshActor>();
		AStaticMeshActor* Obstacle = World->SpawnActor<AStaticMeshActor>();
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		for (AStaticMeshActor* MeshActor : {Road, Obstacle})
		{
			MeshActor->GetStaticMeshComponent()->SetStaticMesh(Cube);
			MeshActor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		}
		Road->SetActorScale3D(FVector(50.f, 20.f, 0.2f));
		Road->SetActorLocation(FVector(2500.f, 0.f, -10.f));
		Obstacle->SetActorScale3D(FVector(2.f));
		Obstacle->SetActorLocation(Departure.GetLocation() + FVector(0.f, 0.f, 150.f));
		FTransform PhysicalFallback = FTransform::Identity;
		TestTrue(TEXT("A physical prop at the first fallback selects another clear road position"),
			Traffic->ResolveBlockedDeparture(OriginalRoute, PhysicalFallback));
		TestTrue(TEXT("Physical clearance skips the occupied candidate"),
			PhysicalFallback.GetLocation().X > Departure.GetLocation().X);
		Obstacle->SetActorEnableCollision(false);
		PhysicalFallback = FTransform::Identity;
		TestTrue(TEXT("Road surface itself does not block a vehicle departure"),
			Traffic->ResolveBlockedDeparture(OriginalRoute, PhysicalFallback));
		TestTrue(TEXT("Removing the physical obstacle restores the original deterministic choice"), PhysicalFallback.Equals(Departure));
		AActor* Wall = World->SpawnActor<AActor>();
		UBoxComponent* WallCollision = NewObject<UBoxComponent>(Wall);
		Wall->SetRootComponent(WallCollision);
		WallCollision->SetBoxExtent(FVector(2500.f, 200.f, 150.f));
		WallCollision->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->SetActorLocation(FVector(1500.f, 0.f, 150.f));
		WallCollision->RegisterComponent();
		const FTransform BeforeBlocked = PhysicalFallback;
		TestFalse(TEXT("A solid wall across every fallback is rejected without spawning through it"),
			Traffic->ResolveBlockedDeparture(OriginalRoute, PhysicalFallback));
		TestTrue(TEXT("Fully blocked physical geometry preserves the caller's transform"), PhysicalFallback.Equals(BeforeBlocked));
		Wall->Destroy();
		Car->SetActorScale3D(FVector(20.f));
		TestFalse(TEXT("Fully occupied search area stays blocked instead of overlapping a car"), Traffic->ResolveBlockedDeparture(OriginalRoute, Repeated));
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
