#include "TerritoryRoadNetworkEditorLibrary.h"
#include "TerritoryRoadSurfaceExtraction.h"
#include "Algo/Reverse.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ScopedTransaction.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "ZoneGraphSettings.h"
#include "ZoneGraphSubsystem.h"
#include "ZoneGraphDelegates.h"
#include "ZoneGraphData.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "ZoneShapeComponent.h"
#include "ZoneShapeActor.h"

namespace
{
const FName GeneratedTag(TEXT("Territory.GeneratedRoad.V1"));
struct FShapePlan
{
	FName Source;
	FString Label;
	bool bJunction = false;
	TArray<FZoneShapePoint> Points;
};

const FZoneLaneProfile* Validate(UWorld* World, FName ProfileName, FTerritoryRoadBuildReport& Report)
{
	if (!World || World->IsGameWorld() || !World->PersistentLevel || !World->GetSubsystem<UZoneGraphSubsystem>())
	{
		Report.Errors.Add(TEXT("Load an editor map with ZoneGraph enabled; PIE is not an authoring world."));
		return nullptr;
	}
	const FZoneLaneProfile* Profile = GetDefault<UZoneGraphSettings>()->GetLaneProfiles().FindByPredicate(
		[&](const FZoneLaneProfile& P) { return P.Name == ProfileName; });
	const FZoneGraphTag Road = World->GetSubsystem<UZoneGraphSubsystem>()->GetTagByName(TEXT("Road"));
	if (!Profile || !Road.IsValid() || Profile->Lanes.IsEmpty() || !Profile->ID.IsValid())
	{
		Report.Errors.Add(TEXT("Select an existing Native lane profile and configure the Road tag."));
		return nullptr;
	}
	for (const FZoneLaneDesc& Lane : Profile->Lanes)
	{
		if (Lane.Direction != EZoneLaneDirection::None && (!Lane.Tags.Contains(Road)
			|| Lane.Width < 250.f || !FMath::IsFinite(Lane.Width)))
		{
			Report.Errors.Add(TEXT("Every driving lane must carry the Native Road tag and be at least 250 cm wide."));
			return nullptr;
		}
	}
	return Profile;
}

void MakeJunction(FShapePlan& Plan, const TArray<FVector>& Mouths)
{
	FVector Center = FVector::ZeroVector;
	for (const FVector& Mouth : Mouths) Center += Mouth;
	Center /= Mouths.Num();
	TArray<FVector> Sorted = Mouths;
	Sorted.Sort([&](const FVector& A, const FVector& B)
	{ return FMath::Atan2(A.Y-Center.Y, A.X-Center.X) < FMath::Atan2(B.Y-Center.Y, B.X-Center.X); });
	Plan.bJunction = true;
	for (const FVector& Mouth : Sorted)
	{
		FZoneShapePoint& Point = Plan.Points.Emplace_GetRef(Mouth);
		Point.Type = FZoneShapePointType::LaneProfile;
		Point.SetRotationFromForwardAndUp((Center-Mouth).GetSafeNormal(), FVector::UpVector);
		Point.InnerTurnRadius = 300.f;
		// Asphalt/socket geometry does not author turn restrictions. Native's default
		// connection policy keeps all reachable exits; OneLanePerDestination can
		// discard a branch entirely when overlapping turn candidates are pruned.
		Point.SetLaneConnectionRestrictions(EZoneShapeLaneConnectionRestrictions::None);
	}
}

bool Commit(UWorld* World, const FZoneLaneProfile& Profile, const TArray<FShapePlan>& Plans,
	FTerritoryRoadBuildReport& Report, FName ReplaceGroup = NAME_None)
{
	if (Plans.IsEmpty()) { Report.Errors.Add(TEXT("No road shapes were produced.")); return false; }
	TMap<FName, AZoneShape*> Existing;
	TSet<FName> Sources;
	for (const FShapePlan& Plan : Plans)
	{
		if (Plan.Source.IsNone() || Sources.Contains(Plan.Source) || Plan.Points.Num() < 2)
		{ Report.Errors.Add(TEXT("Duplicate source identity or invalid shape plan.")); return false; }
		Sources.Add(Plan.Source);
		for (const FZoneShapePoint& Point : Plan.Points) if (Point.Position.ContainsNaN())
		{ Report.Errors.Add(TEXT("A generated road point is not finite.")); return false; }
	}
	for (TActorIterator<AZoneShape> It(World); It; ++It)
	{
		if (!It->Tags.Contains(GeneratedTag)) continue;
		for (const FName Tag : It->Tags) if (Sources.Contains(Tag))
		{
			if (Existing.Contains(Tag)) { Report.Errors.Add(TEXT("Duplicate generated source identity; resolve duplicated ZoneShapes before rebuilding.")); return false; }
			Existing.Add(Tag, *It);
		}
	}
	FScopedTransaction Transaction(NSLOCTEXT("TerritoryRoad", "Build", "Build Native road network from existing roads"));
	TArray<AZoneShape*> Created;
	for (const FShapePlan& Plan : Plans)
	{
		if (Existing.Contains(Plan.Source)) continue;
		FActorSpawnParameters Spawn;
		Spawn.OverrideLevel = World->PersistentLevel;
		Spawn.ObjectFlags = RF_Transactional;
		AZoneShape* Shape = World->SpawnActor<AZoneShape>(AZoneShape::StaticClass(), FTransform::Identity, Spawn);
		if (!Shape)
		{
			for (AZoneShape* Actor : Created) Actor->Destroy();
			Transaction.Cancel();
			Report.Errors.Add(TEXT("Could not allocate every ZoneShape; no existing shapes were changed."));
			return false;
		}
		Created.Add(Shape);
		Existing.Add(Plan.Source, Shape);
	}
	const FZoneGraphTag Intersection = World->GetSubsystem<UZoneGraphSubsystem>()->GetTagByName(TEXT("Intersection"));
	for (const FShapePlan& Plan : Plans)
	{
		AZoneShape* Actor = Existing.FindChecked(Plan.Source);
		Actor->Modify();
		Actor->Tags.AddUnique(GeneratedTag);
		Actor->Tags.AddUnique(Plan.Source);
		if (!ReplaceGroup.IsNone()) Actor->Tags.AddUnique(ReplaceGroup);
		Actor->SetActorLabel(Plan.Label);
		Actor->SetFolderPath(TEXT("Territory/Generated Roads"));
		Actor->SetIsSpatiallyLoaded(false);
		const FVector Origin = Plan.Points[0].Position;
		Actor->SetActorTransform(FTransform(FRotator::ZeroRotator, Origin));
		UZoneShapeComponent* Shape = const_cast<UZoneShapeComponent*>(Actor->GetShape());
		Shape->Modify();
		Shape->SetShapeType(Plan.bJunction ? FZoneShapeType::Polygon : FZoneShapeType::Spline);
		Shape->SetCommonLaneProfile(FZoneLaneProfileRef(Profile));
		Shape->SetReverseLaneProfile(false);
		Shape->SetPolygonRoutingType(EZoneShapePolygonRoutingType::Bezier);
		Shape->GetMutableTags() = FZoneGraphTagMask::None;
		if (Plan.bJunction && Plan.Points.Num() > 2 && Intersection.IsValid()) Shape->GetMutableTags().Add(Intersection);
		Shape->GetMutablePoints() = Plan.Points;
		for (FZoneShapePoint& Point : Shape->GetMutablePoints()) Point.Position -= Origin;
		Shape->UpdateShape();
		Actor->MarkPackageDirty();
		if (Plan.bJunction) ++Report.Junctions; else ++Report.Roads;
	}
	if (!ReplaceGroup.IsNone())
	{
		TArray<AZoneShape*> Obsolete;
		for (TActorIterator<AZoneShape> It(World); It; ++It)
		{
			if (It->Tags.Contains(GeneratedTag) && It->Tags.Contains(ReplaceGroup)
				&& !It->Tags.ContainsByPredicate([&](FName Tag) { return Sources.Contains(Tag); })) Obsolete.Add(*It);
		}
		for (AZoneShape* Actor : Obsolete) { Actor->Modify(); World->EditorDestroyActor(Actor, true); }
	}
	UE::ZoneGraphDelegates::OnZoneGraphRequestRebuild.Broadcast();
	World->MarkPackageDirty();
	Report.bSucceeded = Report.Errors.IsEmpty();
	return Report.bSucceeded;
}
}

FTerritoryRoadBuildReport UTerritoryRoadNetworkEditorLibrary::BuildFromRoadActors(UWorld* World,
	const TArray<AActor*>& RoadActors, const TArray<AActor*>& JunctionActors,
	FName LaneProfileName, float EndpointSnapDistance)
{
	FTerritoryRoadBuildReport Report;
	const FZoneLaneProfile* Profile = Validate(World, LaneProfileName, Report);
	if (!Profile) return Report;
	if (!FMath::IsFinite(EndpointSnapDistance) || EndpointSnapDistance < 0.f || EndpointSnapDistance > 1000.f)
	{ Report.Errors.Add(TEXT("Endpoint snap distance must be between 0 and 1000 cm.")); return Report; }
	TArray<FShapePlan> Plans;
	TArray<FVector> SocketLocations;
	TArray<FVector> SocketDirections;
	for (AActor* Actor : JunctionActors)
	{
		if (!IsValid(Actor) || Actor->GetWorld() != World || !Actor->GetActorGuid().IsValid())
		{ Report.Errors.Add(TEXT("Every junction requires a loaded actor and an editor GUID.")); continue; }
		TInlineComponentArray<UStaticMeshComponent*> Meshes(Actor);
		TArray<FVector> Mouths;
		TMap<FVector, FVector> Directions;
		for (const UStaticMeshComponent* Mesh : Meshes)
		{
			for (const FName Socket : Mesh->GetAllSocketNames())
			{
				const FString Name = Socket.ToString();
				if (!Name.StartsWith(TEXT("Socket")) || !Name.Mid(6).IsNumeric()) continue;
				const FVector Location = Mesh->GetSocketLocation(Socket);
				if (Mouths.ContainsByPredicate([&](const FVector& P) { return P.Equals(Location, 1.f); })) continue;
				Mouths.Add(Location);
				const FVector Outward = Mesh->GetSocketRotation(Socket).Vector();
				Directions.Add(Location, -Outward);
				SocketLocations.Add(Location);
				SocketDirections.Add(Outward);
			}
		}
		if (Mouths.Num() < 2) { Report.Errors.Add(Actor->GetActorLabel() + TEXT(": no numbered road-mouth sockets")); continue; }
		FShapePlan Plan;
		Plan.Source = FName(*FString::Printf(TEXT("Territory.RoadSource.%s"), *Actor->GetActorGuid().ToString(EGuidFormats::Digits)));
		Plan.Label = TEXT("Traffic - ") + Actor->GetActorLabel();
		MakeJunction(Plan, Mouths);
		for (FZoneShapePoint& Point : Plan.Points)
			Point.SetRotationFromForwardAndUp(Directions.FindChecked(Point.Position), FVector::UpVector);
		Plans.Add(MoveTemp(Plan));
	}
	const int32 FirstRoad = Plans.Num();
	for (AActor* Actor : RoadActors)
	{
		if (!IsValid(Actor) || Actor->GetWorld() != World || !Actor->GetActorGuid().IsValid())
		{ Report.Errors.Add(TEXT("Every road requires a loaded actor and an editor GUID.")); continue; }
		TInlineComponentArray<USplineComponent*> Splines(Actor);
		if (Splines.Num() != 1 || Splines[0]->GetNumberOfSplinePoints() < 2 || Splines[0]->IsClosedLoop())
		{ Report.Errors.Add(Actor->GetActorLabel() + TEXT(": expected one open generator curve with at least two points")); continue; }
		const USplineComponent* Spline = Splines[0];
		FShapePlan Plan;
		Plan.Source = FName(*FString::Printf(TEXT("Territory.RoadSource.%s"), *Actor->GetActorGuid().ToString(EGuidFormats::Digits)));
		Plan.Label = TEXT("Traffic - ") + Actor->GetActorLabel();
		for (int32 Index = 0; Index < Spline->GetNumberOfSplinePoints(); ++Index)
		{
			FZoneShapePoint& Point = Plan.Points.Emplace_GetRef(Spline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World));
			Point.Type = Spline->GetSplinePointType(Index) == ESplinePointType::Linear ? FZoneShapePointType::Sharp : FZoneShapePointType::Bezier;
			Point.TangentLength = Spline->GetTangentAtSplinePoint(Index, ESplineCoordinateSpace::World).Length() / 3.f;
			Point.SetRotationFromForwardAndUp(Spline->GetDirectionAtSplinePoint(Index, ESplineCoordinateSpace::World),
				Spline->GetUpVectorAtSplinePoint(Index, ESplineCoordinateSpace::World));
		}
		Plans.Add(MoveTemp(Plan));
	}
	if (!Report.Errors.IsEmpty()) return Report;
	struct FEnd { int32 Road; int32 Point; bool bConnected = false; };
	TArray<FEnd> Ends;
	for (int32 R = FirstRoad; R < Plans.Num(); ++R)
	{ Ends.Add({R, 0}); Ends.Add({R, Plans[R].Points.Num()-1}); }
	for (FEnd& End : Ends)
	{
		FZoneShapePoint& Point = Plans[End.Road].Points[End.Point];
		float BestDistance = FMath::Square(EndpointSnapDistance);
		int32 Best = INDEX_NONE;
		for (int32 S = 0; S < SocketLocations.Num(); ++S)
		{
			const float Distance = FVector::DistSquared(Point.Position, SocketLocations[S]);
			const FVector Out = Point.Rotation.Vector() * (End.Point == 0 ? -1.f : 1.f);
			if (Distance <= BestDistance && FVector::DotProduct(Out, SocketDirections[S]) < -0.8f)
			{ Best = S; BestDistance = Distance; }
		}
		if (Best != INDEX_NONE)
		{
			Point.Position = SocketLocations[Best];
			Point.SetRotationFromForwardAndUp(SocketDirections[Best] * (End.Point == 0 ? 1.f : -1.f), FVector::UpVector);
			End.bConnected = true;
		}
	}
	for (int32 A = 0; A < Ends.Num(); ++A)
	{
		if (Ends[A].bConnected) continue;
		FZoneShapePoint& PA = Plans[Ends[A].Road].Points[Ends[A].Point];
		for (int32 B = A+1; B < Ends.Num(); ++B)
		{
			if (Ends[B].bConnected || Ends[A].Road == Ends[B].Road) continue;
			FZoneShapePoint& PB = Plans[Ends[B].Road].Points[Ends[B].Point];
			const FVector OutA = PA.Rotation.Vector() * (Ends[A].Point == 0 ? -1.f : 1.f);
			const FVector OutB = PB.Rotation.Vector() * (Ends[B].Point == 0 ? -1.f : 1.f);
			if (FVector::DistSquared(PA.Position, PB.Position) <= FMath::Square(EndpointSnapDistance)
				&& FVector::DotProduct(OutA, OutB) < -0.9f)
			{
				PA.Position = PB.Position = (PA.Position + PB.Position) * 0.5f;
				Ends[A].bConnected = Ends[B].bConnected = true;
				break;
			}
		}
		if (!Ends[A].bConnected) ++Report.OpenEnds;
	}
	if (Report.OpenEnds) Report.Warnings.Add(FString::Printf(TEXT("%d generator ends have no matching road or junction mouth; inspect intentional dead ends and unloaded sources."), Report.OpenEnds));
	Commit(World, *Profile, Plans, Report);
	return Report;
}

FTerritoryRoadBuildReport UTerritoryRoadNetworkEditorLibrary::InspectRoadNetwork(UWorld* World)
{
	FTerritoryRoadBuildReport Report;
	const UZoneGraphSubsystem* Graph = World ? World->GetSubsystem<UZoneGraphSubsystem>() : nullptr;
	if (!Graph) { Report.Errors.Add(TEXT("No Native ZoneGraph subsystem in this world.")); return Report; }
	const FZoneGraphTag RoadTag = Graph->GetTagByName(TEXT("Road"));
	if (!RoadTag.IsValid()) { Report.Errors.Add(TEXT("No Native Road tag.")); return Report; }
	for (TActorIterator<AZoneGraphData> It(World); It; ++It)
	{
		const FZoneGraphStorage& Storage = It->GetStorage();
		TSet<int32> Visited;
		for (int32 Index = 0; Index < Storage.Lanes.Num(); ++Index)
		{
			if (!Storage.Lanes[Index].Tags.Contains(RoadTag)) continue;
			++Report.NativeRoadLanes;
			if (Visited.Contains(Index)) continue;
			++Report.ConnectedRoadGroups;
			TArray<int32> Queue = {Index};
			Visited.Add(Index);
			for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
			{
				const FZoneLaneData& Lane = Storage.Lanes[Queue[Cursor]];
				for (int32 LinkIndex = Lane.LinksBegin; LinkIndex < Lane.LinksEnd; ++LinkIndex)
				{
					const int32 Other = Storage.LaneLinks[LinkIndex].DestLaneIndex;
					if (Storage.Lanes.IsValidIndex(Other) && Storage.Lanes[Other].Tags.Contains(RoadTag) && !Visited.Contains(Other))
					{ Visited.Add(Other); Queue.Add(Other); }
				}
			}
		}
	}
	for (TActorIterator<AZoneShape> It(World); It; ++It)
	{
		const UZoneShapeComponent* Shape = It->GetShape();
		if (!Shape || !It->Tags.Contains(GeneratedTag)) continue;
		if (Shape->GetShapeType() == FZoneShapeType::Spline) ++Report.Roads; else ++Report.Junctions;
		const auto Connections = Shape->GetConnectedShapes();
		const auto Connectors = Shape->GetShapeConnectors();
		for (int32 Index = 0; Index < Connectors.Num(); ++Index)
		{
			if (Connections.IsValidIndex(Index) && Connections[Index].ShapeComponent.IsValid()) continue;
			++Report.OpenEnds;
			Report.Warnings.Add(FString::Printf(TEXT("%s mouth %d at %s has no Native shape connection"),
				*It->GetName(), Index, *Shape->GetComponentTransform().TransformPosition(Connectors[Index].Position).ToCompactString()));
		}
	}
	Report.bSucceeded = Report.NativeRoadLanes > 0;
	if (!Report.bSucceeded) Report.Errors.Add(TEXT("No baked Native road lanes."));
	return Report;
}

bool UTerritoryRoadNetworkEditorLibrary::PreviewVehicleRoute(UWorld* World, FVector Start, FVector End,
	TArray<FVector>& OutRoute, FString& OutFailure)
{
	return UTerritoryCounterAttackSubsystem::BuildNarrativeVehicleRoute(World, Start, End, OutRoute, &OutFailure);
}

FString UTerritoryRoadNetworkEditorLibrary::ExportRoadLaneDiagnostics(UWorld* World)
{
	if (!World) return TEXT("[]");
	const UZoneGraphSubsystem* Graph = World->GetSubsystem<UZoneGraphSubsystem>();
	if (!Graph) return TEXT("[]");
	const FZoneGraphTag Road = Graph->GetTagByName(TEXT("Road"));
	if (!Road.IsValid()) return TEXT("[]");
	TArray<FString> Rows;
	for (TActorIterator<AZoneGraphData> It(World); It; ++It)
	{
		const FZoneGraphStorage& Storage = It->GetStorage();
		for (int32 Index = 0; Index < Storage.Lanes.Num(); ++Index)
		{
			const FZoneLaneData& Lane = Storage.Lanes[Index];
			if (!Lane.Tags.Contains(Road)) continue;
			TArray<FString> Points, Links;
			for (int32 P = Lane.PointsBegin; P < Lane.PointsEnd; ++P)
			{
				const FVector& V = Storage.LanePoints[P];
				Points.Add(FString::Printf(TEXT("[%.3f,%.3f,%.3f]"), V.X, V.Y, V.Z));
			}
			for (int32 L = Lane.LinksBegin; L < Lane.LinksEnd; ++L)
			{
				const FZoneLaneLinkData& Link = Storage.LaneLinks[L];
				Links.Add(FString::Printf(TEXT("[%d,%d,%d]"), Link.DestLaneIndex, int32(Link.Type), Link.Flags));
			}
			Rows.Add(FString::Printf(TEXT("{\"data\":%d,\"lane\":%d,\"zone\":%d,\"width\":%.3f,\"tags\":%u,\"points\":[%s],\"links\":[%s]}"),
				Storage.DataHandle.Index, Index, Lane.ZoneIndex, Lane.Width, Lane.Tags.GetValue(), *FString::Join(Points, TEXT(",")), *FString::Join(Links, TEXT(","))));
		}
	}
	return TEXT("[") + FString::Join(Rows, TEXT(",")) + TEXT("]");
}

FTerritoryRoadBuildReport UTerritoryRoadNetworkEditorLibrary::BakeRoadSurfaces(UWorld* World,
	const FTerritoryRoadSurfaceBakeSettings& Settings)
{
	FTerritoryRoadBuildReport Report;
	const FZoneLaneProfile* Profile = Validate(World, Settings.LaneProfileName, Report);
	if (!Profile) return Report;
	if (!Settings.Bounds.IsValid || Settings.Bounds.Min.ContainsNaN() || Settings.Bounds.Max.ContainsNaN()
		|| !Settings.BakeID.IsValid() || !Settings.RoadPhysicalMaterial
		|| Settings.RoadPhysicalMaterial->SurfaceType == SurfaceType_Default
		|| !FMath::IsFinite(Settings.SampleSpacing) || Settings.SampleSpacing < 50.f || Settings.SampleSpacing > 500.f
		|| !FMath::IsFinite(Settings.MaximumSlopeDegrees) || Settings.MaximumSlopeDegrees < 1.f || Settings.MaximumSlopeDegrees > 30.f)
	{ Report.Errors.Add(TEXT("Use finite bounds, a stable Bake ID, a non-default Road physical surface, 50–500 cm spacing and 1–30 degree slope.")); return Report; }
	const FVector Size = Settings.Bounds.GetSize();
	if (Size.X / Settings.SampleSpacing > 10000 || Size.Y / Settings.SampleSpacing > 10000 || Size.Z <= 0.f)
	{ Report.Errors.Add(TEXT("Road bake bounds exceed the supported grid size.")); return Report; }
	TerritoryRoadSurfaceExtraction::FGrid Grid;
	Grid.Width = FMath::FloorToInt(Size.X / Settings.SampleSpacing) + 1;
	Grid.Height = FMath::FloorToInt(Size.Y / Settings.SampleSpacing) + 1;
	Grid.Spacing = Settings.SampleSpacing;
	Grid.Origin = FVector(Settings.Bounds.Min.X, Settings.Bounds.Min.Y, 0.f);
	const int64 Count = int64(Grid.Width) * Grid.Height;
	if (Grid.Width < 3 || Grid.Height < 3 || Count > FMath::Clamp(Settings.MaximumSamples, 100, 1000000))
	{ Report.Errors.Add(TEXT("Bake area exceeds Maximum Samples. Use a smaller loaded area or coarser spacing.")); return Report; }
	if (UWorldPartition* Partition = World->GetWorldPartition())
	{
		int32 Unloaded = 0;
		FWorldPartitionHelpers::ForEachActorDescInstance(Partition, AActor::StaticClass(),
			[&](const FWorldPartitionActorDescInstance* Desc)
			{ if (!Desc->IsLoaded() && Desc->GetEditorBounds().Intersect(Settings.Bounds)) ++Unloaded; return true; });
		if (Unloaded)
		{ Report.Errors.Add(FString::Printf(TEXT("Load the bake region first: %d intersecting World Partition actors are unloaded. Existing lanes were preserved."), Unloaded)); return Report; }
	}
	Grid.Road.Init(0, Count);
	Grid.Heights.Init(0.f, Count);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(TerritoryRoadSurfaceBake), true);
	Query.bReturnPhysicalMaterial = true;
	const float MinimumNormalZ = FMath::Cos(FMath::DegreesToRadians(Settings.MaximumSlopeDegrees));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector XY = Grid.Origin + FVector((Index % Grid.Width)*Grid.Spacing, (Index / Grid.Width)*Grid.Spacing, 0.f);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, XY + FVector(0,0,Settings.Bounds.Max.Z),
			XY + FVector(0,0,Settings.Bounds.Min.Z), ECC_Visibility, Query) || Hit.ImpactNormal.Z < MinimumNormalZ) continue;
		const UPhysicalMaterial* Material = Hit.PhysMaterial.Get();
		bool bRoad = Material && Material->SurfaceType == Settings.RoadPhysicalMaterial->SurfaceType;
		// A mesh may deliberately classify its whole collision body as Road while
		// complex triangle materials retain their Narrative footstep material.
		if (!bRoad && Cast<UStaticMeshComponent>(Hit.GetComponent()))
		{
			const FBodyInstance* Body = Hit.GetComponent()->GetBodyInstance();
			const UPhysicalMaterial* Simple = Body ? Body->GetSimplePhysicalMaterial() : nullptr;
			bRoad = Simple && Simple->SurfaceType == Settings.RoadPhysicalMaterial->SurfaceType;
		}
		if (bRoad) { Grid.Road[Index] = 1; Grid.Heights[Index] = Hit.ImpactPoint.Z; ++Report.RoadSamples; }
	}
	TerritoryRoadSurfaceExtraction::FNetwork Network;
	FString Failure;
	if (!TerritoryRoadSurfaceExtraction::Extract(Grid, Profile->GetLanesTotalWidth(),
		Grid.Spacing * FMath::Tan(FMath::DegreesToRadians(Settings.MaximumSlopeDegrees)), Network, Failure))
	{ Report.Errors.Add(Failure); return Report; }
	TArray<FShapePlan> Plans;
	const FString Group = FString::Printf(TEXT("Territory.SurfaceBake.%s"), *Settings.BakeID.ToString(EGuidFormats::Digits));
	for (int32 Index = 0; Index < Network.Roads.Num(); ++Index)
	{
		FShapePlan Plan;
		Plan.Source = FName(*FString::Printf(TEXT("%s.Road.%d"), *Group, Index));
		Plan.Label = FString::Printf(TEXT("Surface Road %d"), Index);
		for (const FVector& Position : Network.Roads[Index])
		{
			FZoneShapePoint& Point = Plan.Points.Emplace_GetRef(Position);
			Point.Type = FZoneShapePointType::AutoBezier;
		}
		Plans.Add(MoveTemp(Plan));
	}
	for (int32 Index = 0; Index < Network.Junctions.Num(); ++Index)
	{
		FShapePlan Plan;
		Plan.Source = FName(*FString::Printf(TEXT("%s.Junction.%d"), *Group, Index));
		Plan.Label = FString::Printf(TEXT("Surface Junction %d"), Index);
		MakeJunction(Plan, Network.Junctions[Index]);
		Plans.Add(MoveTemp(Plan));
	}
	Report.OpenEnds = Network.OpenEnds;
	Report.Warnings.Add(TEXT("Surface extraction samples the top collision surface in this loaded height band. Inspect dead ends, lane clearance and junction turns before release; bake stacked roads in separate bands."));
	Commit(World, *Profile, Plans, Report, FName(*Group));
	return Report;
}
