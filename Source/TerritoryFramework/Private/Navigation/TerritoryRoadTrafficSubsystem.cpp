#include "Navigation/TerritoryRoadTrafficSubsystem.h"

#include "Core/TerritoryTypes.h"
#include "Vehicles/Mass/QuestRoadControls.h"
#include "Vehicles/Mass/TrafficLightSettings.h"
#include "Vehicles/Mass/TrafficLightSubsystem.h"
#include "Vehicles/NarrativeVehicleBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionShape.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "ZoneGraphSubsystem.h"
#include "ZoneGraphAnnotationSubsystem.h"

void UTerritoryRoadTrafficSubsystem::Deinitialize()
{
	// Detach bookkeeping before Native callbacks can re-enter this subsystem.
	const auto Leases = MoveTemp(TrafficLeases);
	ArrivalClaims.Empty();
	for (const auto& Pair : Leases)
	{
		const FTrafficLease& Lease = Pair.Value;
		AQuestRoadControls* Controls = Lease.Controls.Get();
		if (!IsValid(Controls) || !GetWorld() || GetWorld()->bIsTearingDown
			|| GetWorld()->GetNetMode() == NM_Client) continue;
		Controls->NewSpawnCount = Lease.OriginalSpawnCount;
		if (!Lease.bWasActiveBeforeTerritory && Controls->IsActive())
		{
			Controls->SetActive(false);
		}
	}
	TrafficLeases.Empty();
	Super::Deinitialize();
}

bool UTerritoryRoadTrafficSubsystem::AcquireMissionTraffic(
	AQuestRoadControls* Controls, const int32 DesiredVehicleCount)
{
	UWorld* World = GetWorld();
	if (!IsValid(Controls) || !World || World->bIsTearingDown
		|| World->GetNetMode() == NM_Client || !Controls->HasAuthority()
		|| Controls->GetWorld() != World) return false;
	if (!World->GetSubsystem<UZoneGraphSubsystem>() || !World->GetSubsystem<UZoneGraphAnnotationSubsystem>()
		|| !World->GetSubsystem<UTrafficLightSubsystem>()) return false;
	for (auto It = TrafficLeases.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}

	for (const TPair<TWeakObjectPtr<AQuestRoadControls>, FTrafficLease>& Pair : TrafficLeases)
	{
		if (Pair.Value.Users > 0 && Pair.Value.Controls.Get() != Controls)
		{
			UE_LOG(LogTerritory, Warning,
				TEXT("[RoadTraffic] %s cannot activate while %s owns Narrative's world traffic lease. Assign the same QuestRoadControls actor to every Territory Road Guide."),
				*GetNameSafe(Controls), *GetNameSafe(Pair.Value.Controls.Get()));
			return false;
		}
	}

	const TWeakObjectPtr<AQuestRoadControls> ControlsKey(Controls);
	FTrafficLease& Lease = TrafficLeases.FindOrAdd(ControlsKey);
	if (Lease.Users == MAX_int32) return false;
	if (Lease.Users++ > 0) return true;

	Lease.Controls = Controls;
	Lease.bWasActiveBeforeTerritory = Controls->IsActive();
	Lease.OriginalSpawnCount = Controls->NewSpawnCount;
	if (!Lease.bWasActiveBeforeTerritory)
	{
		Controls->NewSpawnCount = DesiredVehicleCount == INDEX_NONE
			? Controls->NewSpawnCount : FMath::Clamp(DesiredVehicleCount, 0, 200);
		Controls->SetActive(true);
	}
	if (IsValid(Controls) && Controls->IsActive()) return true;
	TrafficLeases.Remove(ControlsKey);
	return false;
}

void UTerritoryRoadTrafficSubsystem::ReleaseMissionTraffic(AQuestRoadControls* Controls)
{
	if (!IsValid(Controls) || !GetWorld() || GetWorld()->GetNetMode() == NM_Client
		|| Controls->GetWorld() != GetWorld()) return;
	const TWeakObjectPtr<AQuestRoadControls> ControlsKey(Controls);
	FTrafficLease* Lease = TrafficLeases.Find(ControlsKey);
	if (!Lease || Lease->Users <= 0) return;
	if (--Lease->Users > 0) return;

	const FTrafficLease Released = *Lease;
	TrafficLeases.Remove(ControlsKey);
	if (GetWorld()->bIsTearingDown) return;
	Controls->NewSpawnCount = Released.OriginalSpawnCount;
	if (!Released.bWasActiveBeforeTerritory && Controls->IsActive())
	{
		Controls->SetActive(false);
	}
}

bool UTerritoryRoadTrafficSubsystem::ResolveBlockedDeparture(
	TConstArrayView<FVector> Route, FTransform& InOutDeparture) const
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || World->bIsTearingDown || Route.Num() < 2) return false;
	float Length = 0.f;
	for (int32 I = 0; I < Route.Num(); ++I)
	{
		if (Route[I].ContainsNaN()) return false;
		if (I > 0) Length += FVector::Distance(Route[I - 1], Route[I]);
	}
	// Keep a real driving journey: never stage at the objective or invent an off-road pad.
	const float MaxAdvance = FMath::Min(2000.f, Length - 1500.f);
	for (float Advance = 900.f; Advance <= MaxAdvance; Advance += 550.f)
	{
		float Remaining = Advance;
		for (int32 I = 1; I < Route.Num(); ++I)
		{
			const FVector Segment = Route[I] - Route[I - 1];
			const float SegmentLength = Segment.Size();
			if (SegmentLength <= KINDA_SMALL_NUMBER) continue;
			if (Remaining > SegmentLength) { Remaining -= SegmentLength; continue; }
			const FVector Candidate = Route[I - 1] + Segment * (Remaining / SegmentLength);
			if (Candidate.Equals(InOutDeparture.GetLocation(), 100.f)) break;
			bool bOccupied = false;
			for (TActorIterator<ANarrativeVehicleBase> It(World); It; ++It)
			{
				if (!It->IsActorBeingDestroyed() && It->GetComponentsBoundingBox(true)
					.ExpandBy(FVector(350.f, 350.f, 200.f)).IsInsideOrOn(Candidate))
				{ bOccupied = true; break; }
			}
			// Vehicle bounds alone miss walls, props and fallen characters. Leave road
			// clearance beneath the chassis; Native spawning still checks the real car.
			const FQuat Rotation = Segment.GetSafeNormal().Rotation().Quaternion();
			bOccupied |= World->OverlapBlockingTestByChannel(
				Candidate + Rotation.GetUpVector() * 150.f, Rotation, ECC_Vehicle,
				FCollisionShape::MakeBox(FVector(250.f, 140.f, 110.f)));
			if (!bOccupied)
			{
				InOutDeparture.SetLocation(Candidate);
				InOutDeparture.SetRotation(Rotation);
				return true;
			}
			break;
		}
	}
	return false;
}

bool UTerritoryRoadTrafficSubsystem::TrimRouteToDeparture(
	TArray<FVector>& Route, const FVector& Departure, float MaximumDeviation)
{
	if (Route.Num() < 2 || Departure.ContainsNaN() || !FMath::IsFinite(MaximumDeviation)) return false;
	int32 BestSegment = INDEX_NONE;
	float BestDistance = FMath::Square(FMath::Clamp(MaximumDeviation, 1.f, 1000.f));
	FVector Projection;
	for (int32 I = 1; I < Route.Num(); ++I)
	{
		if (Route[I - 1].ContainsNaN() || Route[I].ContainsNaN()) return false;
		const FVector Candidate = FMath::ClosestPointOnSegment(Departure, Route[I - 1], Route[I]);
		const float Distance = FVector::DistSquared(Candidate, Departure);
		if (Distance < BestDistance)
		{ BestDistance = Distance; BestSegment = I; Projection = Candidate; }
	}
	if (BestSegment == INDEX_NONE) return false;
	if (BestSegment == Route.Num() - 1 && Route.Last().Equals(Projection, 1.f)) return false;
	Route.RemoveAt(0, BestSegment, EAllowShrinking::No);
	if (!Route[0].Equals(Projection, 1.f)) Route.Insert(Projection, 0);
	return Route.Num() >= 2;
}

int32 UTerritoryRoadTrafficSubsystem::GetMissionTrafficUserCount(
	const AQuestRoadControls* Controls) const
{
	if (!Controls) return 0;
	const FTrafficLease* Lease = TrafficLeases.Find(
		TWeakObjectPtr<AQuestRoadControls>(const_cast<AQuestRoadControls*>(Controls)));
	return Lease ? Lease->Users : 0;
}

TArray<FVector> UTerritoryRoadTrafficSubsystem::BuildArrivalCandidates(
	TConstArrayView<FVector> Route, float SearchDistance, float Spacing)
{
	TArray<FVector> Result;
	if (Route.Num() < 2 || !FMath::IsFinite(SearchDistance) || !FMath::IsFinite(Spacing)) return Result;
	for (const FVector& Point : Route) if (Point.ContainsNaN()) return Result;
	SearchDistance = FMath::Clamp(SearchDistance, 0.f, 10000.f);
	Spacing = FMath::Clamp(Spacing, 600.f, 2000.f);
	Result.Add(Route.Last());
	float Travelled = 0.f;
	float Next = Spacing;
	for (int32 Index = Route.Num() - 1; Index > 0; --Index)
	{
		const float Length = FVector::Distance(Route[Index], Route[Index - 1]);
		if (Length < KINDA_SMALL_NUMBER) continue;
		while (Next <= Travelled + Length && Next <= SearchDistance)
		{
			Result.Add(FMath::Lerp(Route[Index], Route[Index - 1], (Next - Travelled) / Length));
			Next += Spacing;
		}
		Travelled += Length;
		if (Travelled >= SearchDistance) break;
	}
	return Result;
}

bool UTerritoryRoadTrafficSubsystem::ReserveArrival(ANarrativeVehicleBase* Vehicle,
	const FGuid& AssaultID, TArray<FVector>& InOutRoute, const FVector& WalkTarget,
	AActor* NavigationAgent, float SearchDistance, float Spacing)
{
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || World->GetNetMode() == NM_Client
		|| !IsValid(Vehicle) || Vehicle->GetWorld() != World || !Vehicle->HasAuthority()
		|| !AssaultID.IsValid() || !IsValid(NavigationAgent) || NavigationAgent->GetWorld() != World
		|| WalkTarget.ContainsNaN()) return false;
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
	if (!Nav) return false;
	for (auto It = ArrivalClaims.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	const float Radius = FMath::Max(400.f, Vehicle->GetComponentsBoundingBox(true).GetExtent().Size2D());
	const TArray<FVector> Candidates = BuildArrivalCandidates(InOutRoute, SearchDistance, Spacing);
	for (const FVector& Candidate : Candidates)
	{
		bool bOccupied = false;
		for (const auto& Entry : ArrivalClaims)
		{
			if (Entry.Key.Get() != Vehicle && FMath::Abs(Entry.Value.Location.Z - Candidate.Z) < 300.f
				&& FVector::DistSquared2D(Entry.Value.Location, Candidate) < FMath::Square(Radius + Entry.Value.Radius))
			{
				bOccupied = true;
				break;
			}
		}
		if (bOccupied) continue;
		for (TActorIterator<ANarrativeVehicleBase> It(World); It; ++It)
		{
			if (*It == Vehicle || It->IsActorBeingDestroyed()) continue;
			const float OtherRadius = FMath::Max(350.f, It->GetComponentsBoundingBox(true).GetExtent().Size2D());
			if (FMath::Abs(It->GetActorLocation().Z - Candidate.Z) < 300.f
				&& FVector::DistSquared2D(It->GetActorLocation(), Candidate) < FMath::Square(Radius + OtherRadius))
			{
				bOccupied = true;
				break;
			}
		}
		if (bOccupied) continue;
		FNavLocation Start;
		if (!Nav->ProjectPointToNavigation(Candidate, Start, FVector(300.f, 300.f, 500.f))) continue;
		const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
			NavigationAgent, Start.Location, WalkTarget, NavigationAgent);
		if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
		// Truncate at the selected point on the original polyline. No lane changes,
		// off-road connector, teleport, or reuse of another squad's vehicle.
		int32 Segment = INDEX_NONE;
		for (int32 Index = InOutRoute.Num() - 1; Index > 0; --Index)
		{
			if (FMath::PointDistToSegmentSquared(Candidate, InOutRoute[Index - 1], InOutRoute[Index]) < 1.f)
			{
				Segment = Index;
				break;
			}
		}
		if (Segment == INDEX_NONE) continue;
		InOutRoute.SetNum(Segment + 1);
		InOutRoute.Last() = Candidate;
		ArrivalClaims.Add(Vehicle, {AssaultID, Candidate, Radius});
		return true;
	}
	return false;
}

void UTerritoryRoadTrafficSubsystem::ReleaseArrival(ANarrativeVehicleBase* Vehicle)
{
	if (GetWorld() && GetWorld()->GetNetMode() != NM_Client) ArrivalClaims.Remove(Vehicle);
}

bool UTerritoryRoadTrafficSubsystem::IsSafeRoadOffset(const FVector& Location,
	const FVector& Direction, const float VehicleHalfWidth) const
{
	const UZoneGraphSubsystem* ZoneGraph = GetWorld() ? GetWorld()->GetSubsystem<UZoneGraphSubsystem>() : nullptr;
	if (!ZoneGraph) return false;
	FZoneGraphTagFilter Filter;
	const FZoneGraphTag Road = ZoneGraph->GetTagByName(TEXT("Road"));
	if (!Road.IsValid()) return false;
	Filter.AnyTags.Add(Road);
	FZoneGraphLaneLocation Lane;
	float DistanceSquared = 0.f;
	if (!ZoneGraph->FindNearestLane(FBox::BuildAABB(Location, FVector(500.f, 500.f, 200.f)),
		Filter, Lane, DistanceSquared)) return false;
	float Width = 0.f;
	ZoneGraph->GetLaneWidth(Lane.LaneHandle, Width);
	return FVector::DotProduct(Lane.Direction.GetSafeNormal2D(), Direction.GetSafeNormal2D()) > 0.85f
		&& Width * 0.5f >= VehicleHalfWidth
		&& DistanceSquared <= FMath::Square(Width * 0.5f - VehicleHalfWidth);
}

bool UTerritoryRoadTrafficSubsystem::FindClosedRoadAhead(const FVector& Location,
	TConstArrayView<FVector> Route, int32 RouteIndex, float LookAhead, float& OutDistance) const
{
	const UWorld* World = GetWorld();
	const UZoneGraphSubsystem* Graph = World ? World->GetSubsystem<UZoneGraphSubsystem>() : nullptr;
	const UZoneGraphAnnotationSubsystem* Annotations = World ? World->GetSubsystem<UZoneGraphAnnotationSubsystem>() : nullptr;
	const UTrafficLightSettings* Settings = GetDefault<UTrafficLightSettings>();
	if (!Graph || !Annotations || !Settings->ClosedTag.IsValid()) return false;
	FZoneGraphTagFilter Filter;
	const FZoneGraphTag Road = Graph->GetTagByName(TEXT("Road"));
	if (!Road.IsValid()) return false;
	Filter.AnyTags.Add(Road);
	FVector Previous = Location;
	float Travelled = 0.f;
	for (int32 Index = FMath::Max(0, RouteIndex); Index < Route.Num() && Travelled < LookAhead; ++Index)
	{
		const float Length = FVector::Dist2D(Previous, Route[Index]);
		for (float Along = 0.f; Along < Length && Travelled + Along <= LookAhead; Along += 150.f)
		{
			const FVector Point = FMath::Lerp(Previous, Route[Index], Along / Length);
			FZoneGraphLaneLocation Lane;
			float DistanceSquared = 0.f;
			if (Graph->FindNearestLane(FBox::BuildAABB(Point, FVector(150.f, 150.f, 200.f)), Filter, Lane, DistanceSquared)
				&& Annotations->GetAnnotationTags(Lane.LaneHandle).Contains(Settings->ClosedTag))
			{
				// Cars already inside a closing intersection must clear it.
				if (Travelled + Along < 150.f) return false;
				OutDistance = Travelled + Along;
				return true;
			}
		}
		Travelled += Length;
		Previous = Route[Index];
	}
	return false;
}
