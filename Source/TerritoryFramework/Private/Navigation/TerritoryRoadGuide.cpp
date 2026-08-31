#include "Navigation/TerritoryRoadGuide.h"

#include "Components/SplineComponent.h"
#include "Core/TerritoryTypes.h"
#include "Navigation/TerritoryRoadTrafficSubsystem.h"
#include "Vehicles/Mass/QuestRoadControls.h"
#include "ZoneGraphSettings.h"
#include "ZoneGraphSubsystem.h"
#include "ZoneGraphTypes.h"

ATerritoryRoadGuide::ATerritoryRoadGuide()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	RouteSpline = CreateDefaultSubobject<USplineComponent>(TEXT("Road Mission Spline"));
	SetRootComponent(RouteSpline);
	RouteSpline->SetClosedLoop(false);
}

float ATerritoryRoadGuide::GetSignedLaneOffset(const bool bReverseDirection,
	const ETerritoryRoadLaneSide LaneSide) const
{
	float Offset = 0.f;
	if (LaneSide == ETerritoryRoadLaneSide::Left) Offset = LaneCenterOffset;
	else if (LaneSide == ETerritoryRoadLaneSide::Right) Offset = -LaneCenterOffset;
	return bReverseDirection ? -Offset : Offset;
}

FTransform ATerritoryRoadGuide::GetRouteTransformAtDistance(const float Distance,
	const bool bReverseDirection, const ETerritoryRoadLaneSide LaneSide) const
{
	if (!RouteSpline) return GetActorTransform();
	const float Length = RouteSpline->GetSplineLength();
	const float Clamped = FMath::Clamp(Distance, 0.f, Length);
	FVector Location = RouteSpline->GetLocationAtDistanceAlongSpline(
		Clamped, ESplineCoordinateSpace::World);
	const FVector Right = RouteSpline->GetRightVectorAtDistanceAlongSpline(
		Clamped, ESplineCoordinateSpace::World);
	Location += Right * GetSignedLaneOffset(bReverseDirection, LaneSide);
	FVector Direction = RouteSpline->GetDirectionAtDistanceAlongSpline(
		Clamped, ESplineCoordinateSpace::World);
	if (bReverseDirection) Direction *= -1.f;
	return FTransform(Direction.Rotation(), Location);
}

FTransform ATerritoryRoadGuide::GetRouteStartTransform(
	const bool bReverseDirection, const ETerritoryRoadLaneSide LaneSide) const
{
	return GetRouteTransformAtDistance(bReverseDirection && RouteSpline
		? RouteSpline->GetSplineLength() : 0.f, bReverseDirection, LaneSide);
}

FTransform ATerritoryRoadGuide::GetRouteEndTransform(
	const bool bReverseDirection, const ETerritoryRoadLaneSide LaneSide) const
{
	return GetRouteTransformAtDistance(!bReverseDirection && RouteSpline
		? RouteSpline->GetSplineLength() : 0.f, bReverseDirection, LaneSide);
}

FTransform ATerritoryRoadGuide::GetFinalFightTransform() const
{
	const FVector WorldLocation = GetActorTransform().TransformPosition(FinalFightLocation);
	const FVector Destination = GetRouteEndTransform(false,
		ETerritoryRoadLaneSide::Center).GetLocation();
	FVector Facing = Destination - WorldLocation;
	Facing.Z = 0.f;
	return FTransform(Facing.IsNearlyZero() ? GetActorRotation() : Facing.Rotation(),
		WorldLocation);
}

bool ATerritoryRoadGuide::BuildRoutePoints(const bool bReverseDirection,
	const ETerritoryRoadLaneSide LaneSide, TArray<FVector>& OutRoutePoints,
	FText& OutFailureReason) const
{
	OutRoutePoints.Reset();
	if (!ValidateRoadGuide(OutFailureReason)) return false;

	const float Length = RouteSpline->GetSplineLength();
	const float Spacing = FMath::Clamp(SampleSpacing, 100.f, 2000.f);
	const int32 SegmentCount = FMath::Max(1, FMath::CeilToInt(Length / Spacing));
	OutRoutePoints.Reserve(SegmentCount + 1);
	for (int32 Index = 0; Index <= SegmentCount; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / SegmentCount;
		const float Distance = bReverseDirection ? Length * (1.f - Alpha) : Length * Alpha;
		OutRoutePoints.Add(GetRouteTransformAtDistance(
			Distance, bReverseDirection, LaneSide).GetLocation());
	}
	OutFailureReason = FText::GetEmpty();
	return OutRoutePoints.Num() >= 2;
}

bool ATerritoryRoadGuide::ValidateRoadGuide(FText& OutFailureReason) const
{
	if (RoadGuideID.IsNone())
	{
		OutFailureReason = FText::FromString(TEXT("Road Guide ID is empty."));
		return false;
	}
	if (!RouteSpline || RouteSpline->GetNumberOfSplinePoints() < 2
		|| RouteSpline->GetSplineLength() < 200.f)
	{
		OutFailureReason = FText::FromString(
			TEXT("Road Guide needs at least two different spline points."));
		return false;
	}
	if (!bRequireNarrativeZoneGraphCoverage)
	{
		OutFailureReason = FText::GetEmpty();
		return true;
	}

	const UWorld* World = GetWorld();
	const UZoneGraphSubsystem* ZoneGraph = World
		? World->GetSubsystem<UZoneGraphSubsystem>() : nullptr;
	if (!ZoneGraph)
	{
		OutFailureReason = FText::FromString(
			TEXT("Narrative ZoneGraph is unavailable. Enable ZoneGraph and build road data, or disable coverage for an intentional off-road spline."));
		return false;
	}

	const float Length = RouteSpline->GetSplineLength();
	const float Spacing = FMath::Clamp(SampleSpacing * 2.f, 250.f, 2500.f);
	const int32 Samples = FMath::Max(1, FMath::CeilToInt(Length / Spacing));
	const FVector Extent(FMath::Max(100.f, MaximumZoneGraphDistance));
	FZoneGraphTagFilter Filter;
	const UZoneGraphSettings* Settings = GetDefault<UZoneGraphSettings>();
	if (Settings)
	{
		for (const FZoneGraphTagInfo& Info : Settings->GetTagInfos())
		{
			if (Info.Name == TEXT("Road"))
			{
				Filter.AnyTags.Add(Info.Tag);
				break;
			}
		}
	}
	if (Filter.AnyTags == FZoneGraphTagMask::None)
	{
		OutFailureReason = FText::FromString(
			TEXT("ZoneGraph Settings has no Road lane tag."));
		return false;
	}
	for (int32 Index = 0; Index <= Samples; ++Index)
	{
		const float Distance = Length * static_cast<float>(Index) / Samples;
		const FVector Point = RouteSpline->GetLocationAtDistanceAlongSpline(
			Distance, ESplineCoordinateSpace::World);
		FZoneGraphLaneLocation LaneLocation;
		float DistanceSquared = 0.f;
		ZoneGraph->FindNearestLane(FBox::BuildAABB(Point, Extent), Filter,
			LaneLocation, DistanceSquared);
		if (!LaneLocation.IsValid()
			|| DistanceSquared > FMath::Square(MaximumZoneGraphDistance))
		{
			OutFailureReason = FText::FromString(FString::Printf(
				TEXT("Spline sample %d is not covered by a Narrative ZoneGraph road lane."),
				Index));
			return false;
		}
	}
	OutFailureReason = FText::GetEmpty();
	return true;
}

void ATerritoryRoadGuide::BeginMissionTraffic(const int32 DesiredVehicleCount)
{
	if (!HasAuthority()) return;
	AQuestRoadControls* Controls = NarrativeTrafficControls.LoadSynchronous();
	if (!IsValid(Controls)) return;
	if (UTerritoryRoadTrafficSubsystem* Traffic = GetWorld()
		? GetWorld()->GetSubsystem<UTerritoryRoadTrafficSubsystem>() : nullptr)
	{
		const int32 Count = DesiredVehicleCount == INDEX_NONE
			? MissionTrafficVehicleCount : DesiredVehicleCount;
		if (!Traffic->AcquireMissionTraffic(Controls, Count))
		{
			UE_LOG(LogTerritory, Warning,
				TEXT("[RoadTraffic] Road Guide %s continues without mission traffic because its Narrative controller could not acquire the shared lease."),
				*RoadGuideID.ToString());
		}
	}
}

void ATerritoryRoadGuide::EndMissionTraffic()
{
	if (!HasAuthority()) return;
	AQuestRoadControls* Controls = NarrativeTrafficControls.LoadSynchronous();
	if (!IsValid(Controls)) return;
	if (UTerritoryRoadTrafficSubsystem* Traffic = GetWorld()
		? GetWorld()->GetSubsystem<UTerritoryRoadTrafficSubsystem>() : nullptr)
	{
		Traffic->ReleaseMissionTraffic(Controls);
	}
}

#if WITH_EDITOR
void ATerritoryRoadGuide::SetStraightRoute(const FVector& WorldStart,
	const FVector& WorldEnd)
{
	if (!RouteSpline) return;
	SetActorLocation(WorldStart);
	RouteSpline->ClearSplinePoints(false);
	RouteSpline->AddSplinePoint(FVector::ZeroVector,
		ESplineCoordinateSpace::Local, false);
	RouteSpline->AddSplinePoint(GetActorTransform().InverseTransformPosition(WorldEnd),
		ESplineCoordinateSpace::Local, false);
	RouteSpline->SetSplinePointType(0, ESplinePointType::Curve, false);
	RouteSpline->SetSplinePointType(1, ESplinePointType::Curve, false);
	RouteSpline->UpdateSpline();
}
#endif
