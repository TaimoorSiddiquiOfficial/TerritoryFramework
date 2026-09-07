#include "TerritoryRoadSurfaceExtraction.h"
#include "Algo/Reverse.h"

namespace TerritoryRoadSurfaceExtraction
{
bool Extract(const FGrid& Grid, const float RoadWidth, const float MaximumStep,
	FNetwork& Out, FString& Failure)
{
	Out = {};
	const int64 Count = int64(Grid.Width) * Grid.Height;
	if (Grid.Width < 3 || Grid.Height < 3 || Count > 1000000 || Count != Grid.Road.Num()
		|| Count != Grid.Heights.Num() || !FMath::IsFinite(Grid.Spacing) || Grid.Spacing < 50.f
		|| !FMath::IsFinite(RoadWidth) || RoadWidth <= 0.f || !FMath::IsFinite(MaximumStep) || MaximumStep <= 0.f)
	{
		Failure = TEXT("Invalid or oversized road surface grid");
		return false;
	}
	const int32 DX[] = {0,1,1,1,0,-1,-1,-1};
	const int32 DY[] = {-1,-1,0,1,1,1,0,-1};
	auto Cell = [&](const int32 X, const int32 Y) { return Y * Grid.Width + X; };
	auto Position = [&](const int32 Index)
	{
		return Grid.Origin + FVector((Index % Grid.Width) * Grid.Spacing,
			(Index / Grid.Width) * Grid.Spacing, Grid.Heights[Index]);
	};
	TArray<uint8> Skeleton = Grid.Road;
	// Zhang-Suen thinning preserves connected branches and closed loops. Treat
	// height discontinuities as absent neighbours, so cliffs do not become ramps.
	auto Neighbour = [&](const int32 Index, const int32 Direction) -> int32
	{
		const int32 X = Index % Grid.Width + DX[Direction];
		const int32 Y = Index / Grid.Width + DY[Direction];
		if (X < 0 || Y < 0 || X >= Grid.Width || Y >= Grid.Height) return INDEX_NONE;
		const int32 Other = Cell(X, Y);
		return FMath::Abs(Grid.Heights[Index] - Grid.Heights[Other]) <= MaximumStep ? Other : INDEX_NONE;
	};
	bool bChanged = true;
	int32 Iteration = 0;
	for (; bChanged && Iteration < FMath::Max(Grid.Width, Grid.Height); ++Iteration)
	{
		bChanged = false;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			TArray<int32> Remove;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				if (!Skeleton[Index]) continue;
				int32 P[8];
				int32 Sum = 0, Transitions = 0;
				for (int32 Direction = 0; Direction < 8; ++Direction)
				{
					const int32 Other = Neighbour(Index, Direction);
					P[Direction] = Other != INDEX_NONE && Skeleton[Other] ? 1 : 0;
					Sum += P[Direction];
				}
				for (int32 Direction = 0; Direction < 8; ++Direction)
					Transitions += P[Direction] == 0 && P[(Direction + 1) % 8] == 1;
				if (Sum < 2 || Sum > 6 || Transitions != 1) continue;
				if (Pass == 0 ? (P[0]*P[2]*P[4] == 0 && P[2]*P[4]*P[6] == 0)
					: (P[0]*P[2]*P[6] == 0 && P[0]*P[4]*P[6] == 0)) Remove.Add(Index);
			}
			for (const int32 Index : Remove) Skeleton[Index] = 0;
			bChanged |= !Remove.IsEmpty();
		}
	}
	if (bChanged) { Failure = TEXT("Road thinning exceeded its deterministic iteration limit"); return false; }
	TArray<TArray<int32>> Links;
	Links.SetNum(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!Skeleton[Index]) continue;
		for (int32 Direction = 0; Direction < 8; ++Direction)
		{
			const int32 Other = Neighbour(Index, Direction);
			if (Other == INDEX_NONE || !Skeleton[Other]) continue;
			// Do not add a diagonal shortcut around a connected square corner.
			if ((Direction & 1) != 0)
			{
				const int32 A = Neighbour(Index, (Direction + 7) % 8);
				const int32 B = Neighbour(Index, (Direction + 1) % 8);
				if ((A != INDEX_NONE && Skeleton[A]) || (B != INDEX_NONE && Skeleton[B])) continue;
			}
			Links[Index].Add(Other);
		}
	}
	// Collapse adjacent branch pixels to one junction, keeping long roads between
	// different junctions separate. This avoids tiny competing intersections.
	TArray<int32> NodeFor;
	NodeFor.Init(INDEX_NONE, Count);
	TArray<TArray<int32>> Nodes;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!Skeleton[Index] || Links[Index].Num() == 2 || Links[Index].IsEmpty() || NodeFor[Index] != INDEX_NONE) continue;
		const int32 NodeID = Nodes.AddDefaulted();
		TArray<int32> Queue = {Index};
		NodeFor[Index] = NodeID;
		for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
		{
			const int32 Current = Queue[Cursor];
			Nodes[NodeID].Add(Current);
			if (Links[Index].Num() < 3) continue;
			for (const int32 Other : Links[Current])
				if (Links[Other].Num() >= 3 && NodeFor[Other] == INDEX_NONE)
				{ NodeFor[Other] = NodeID; Queue.Add(Other); }
		}
	}
	// Closed loops have no degree != 2 pixel. Give each unvisited component a
	// deterministic seed node, so a ring is retained instead of silently dropped.
	TArray<uint8> Visited;
	Visited.Init(0, Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!Skeleton[Index] || Visited[Index] || Links[Index].IsEmpty()) continue;
		TArray<int32> Queue = {Index};
		Visited[Index] = 1;
		bool bHasNode = false;
		for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
		{
			bHasNode |= NodeFor[Queue[Cursor]] != INDEX_NONE;
			for (const int32 Other : Links[Queue[Cursor]]) if (!Visited[Other])
			{ Visited[Other] = 1; Queue.Add(Other); }
		}
		if (!bHasNode) { NodeFor[Index] = Nodes.Num(); Nodes.Add({Index}); }
	}
	struct FEdge { int32 A; int32 B; TArray<FVector> Points; };
	TArray<FEdge> Edges;
	TSet<uint64> Used;
	auto Key = [](int32 A, int32 B) { return (uint64(FMath::Min(A,B)) << 32) | uint32(FMath::Max(A,B)); };
	for (int32 NodeID = 0; NodeID < Nodes.Num(); ++NodeID)
	{
		for (const int32 Start : Nodes[NodeID]) for (const int32 Next : Links[Start])
		{
			if (NodeFor[Next] == NodeID || Used.Contains(Key(Start, Next))) continue;
			FEdge Edge{NodeID, INDEX_NONE, {Position(Start)}};
			int32 Previous = Start, Current = Next;
			for (int32 Step = 0; Step < Count; ++Step)
			{
				Used.Add(Key(Previous, Current));
				Edge.Points.Add(Position(Current));
				if (NodeFor[Current] != INDEX_NONE) { Edge.B = NodeFor[Current]; break; }
				int32 Following = INDEX_NONE;
				for (const int32 Other : Links[Current]) if (Other != Previous) { Following = Other; break; }
				if (Following == INDEX_NONE) break;
				Previous = Current; Current = Following;
			}
			if (Edge.B != INDEX_NONE && Edge.Points.Num() >= 2) Edges.Add(MoveTemp(Edge));
		}
	}
	TArray<TArray<FVector>> Mouths;
	Mouths.SetNum(Nodes.Num());
	TArray<int32> Degree;
	Degree.Init(0, Nodes.Num());
	for (const FEdge& Edge : Edges) { ++Degree[Edge.A]; ++Degree[Edge.B]; }
	auto Trim = [](TArray<FVector>& Points, const float Distance)
	{
		float Remaining = Distance;
		while (Points.Num() > 2 && FVector::Distance(Points[0], Points[1]) <= Remaining)
		{ Remaining -= FVector::Distance(Points[0], Points[1]); Points.RemoveAt(0); }
		const float Length = FVector::Distance(Points[0], Points[1]);
		if (Length <= Remaining) return false;
		Points[0] = FMath::Lerp(Points[0], Points[1], Remaining / Length);
		return true;
	};
	for (FEdge& Edge : Edges)
	{
		float Length = 0.f;
		for (int32 I = 1; I < Edge.Points.Num(); ++I) Length += FVector::Distance(Edge.Points[I-1], Edge.Points[I]);
		const float TrimA = Degree[Edge.A] > 2 ? RoadWidth : 0.f;
		const float TrimB = Degree[Edge.B] > 2 ? RoadWidth : 0.f;
		if (Length < TrimA + TrimB + Grid.Spacing * 2.f) continue;
		if (TrimA > 0.f && !Trim(Edge.Points, TrimA)) continue;
		Algo::Reverse(Edge.Points);
		if (TrimB > 0.f && !Trim(Edge.Points, TrimB)) continue;
		Algo::Reverse(Edge.Points);
		// Certify the generated carriageway, not just its centre pixel. Reject a
		// narrow paint stroke instead of generating vehicle lanes over grass/walls.
		bool bFits = true;
		for (int32 I = 0; I < Edge.Points.Num() && bFits; ++I)
		{
			const FVector Direction = (Edge.Points[FMath::Min(I+1, Edge.Points.Num()-1)]
				- Edge.Points[FMath::Max(0, I-1)]).GetSafeNormal2D();
			const FVector Right(-Direction.Y, Direction.X, 0.f);
			for (int32 Side : {-1, 0, 1})
			{
				const FVector Sample = Edge.Points[I] + Right * (Side * RoadWidth * 0.5f);
				const int32 X = FMath::RoundToInt((Sample.X - Grid.Origin.X) / Grid.Spacing);
				const int32 Y = FMath::RoundToInt((Sample.Y - Grid.Origin.Y) / Grid.Spacing);
				if (X < 0 || Y < 0 || X >= Grid.Width || Y >= Grid.Height || !Grid.Road[Cell(X,Y)]
					|| FMath::Abs(Grid.Heights[Cell(X,Y)] + Grid.Origin.Z - Sample.Z) > MaximumStep)
				{ bFits = false; break; }
			}
		}
		if (!bFits) continue;
		Mouths[Edge.A].Add(Edge.Points[0]);
		Mouths[Edge.B].Add(Edge.Points.Last());
		if (Edge.A == Edge.B && Edge.Points.Num() > 4)
		{
			// Native spline shapes connect to other shapes, not their own ends.
			// Two halves preserve continuous routing around an isolated ring.
			const int32 Mid = Edge.Points.Num() / 2;
			TArray<FVector> First;
			First.Append(Edge.Points.GetData(), Mid + 1);
			TArray<FVector> Second;
			Second.Append(Edge.Points.GetData() + Mid, Edge.Points.Num() - Mid);
			Out.Roads.Add(MoveTemp(First));
			Out.Roads.Add(MoveTemp(Second));
		}
		else Out.Roads.Add(MoveTemp(Edge.Points));
	}
	for (int32 Index = 0; Index < Mouths.Num(); ++Index)
	{
		const auto& Points = Mouths[Index];
		if (Points.Num() >= 2 && Degree[Index] > 2) Out.Junctions.Add(Points);
		else if (Points.Num() == 1) ++Out.OpenEnds;
	}
	if (Out.Roads.IsEmpty()) { Failure = TEXT("No connected road is wide enough for the selected Native lane profile"); return false; }
	Failure.Reset();
	return true;
}
}
