// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavPathfinding.h"

#include "SurfaceNavigation/LNPNavRuntime.h"

#include "Algo/Reverse.h"

namespace
{
	/** f가 작은 쪽, 같으면 g가 큰 쪽(목표에 가까운 쪽), 그다음 index가 작은 쪽을 먼저 연다. */
	struct FOpenOrder
	{
		bool operator()(const FLNPNavOpenEntry& A, const FLNPNavOpenEntry& B) const
		{
			if (A.F != B.F)
			{
				return A.F < B.F;
			}
			return A.G != B.G ? A.G > B.G : A.Node < B.Node;
		}
	};

	/** 단순화 한 번이 앞으로 보는 최대 node 수. 직선 검사 비용을 경로 길이에 선형으로 묶는다. */
	constexpr int32 SimplifyLookahead = 64;

	bool IsSameSegment(const FLNPNavGraph& Graph, const int32 A, const int32 B)
	{
		const int32 Slot = Graph.GetSlot(A);
		return Slot == Graph.GetSlot(B) && Graph.GetNode(Slot, A).LayerOrdinal == Graph.GetNode(Slot, B).LayerOrdinal;
	}
}

ELNPNavSearchStatus LNPNavPathfinding::BeginSearch(
	const FLNPNavSnapshot& Nav, const int32 Start, const int32 Goal, const FLNPNavSearchParams& Params,
	FLNPNavSearchScratch& Scratch, FLNPNavSearch& OutSearch)
{
	OutSearch = FLNPNavSearch();
	OutSearch.Params = Params;
	OutSearch.Start = Start;
	OutSearch.Goal = Goal;
	OutSearch.SnapshotGeneration = Nav.SnapshotGeneration;
	OutSearch.ConnectivityGraphVersion = Nav.ConnectivityGraphVersion;
	const FLNPNavGraph& Graph = Nav.Graph;
	const int32 StartSlot = Graph.GetSlot(Start);
	const int32 GoalSlot = Graph.GetSlot(Goal);
	if (StartSlot == INDEX_NONE || GoalSlot == INDEX_NONE || Graph.BlockedNodes[Start] || Graph.BlockedNodes[Goal])
	{
		OutSearch.Status = ELNPNavSearchStatus::Invalid;
		return OutSearch.Status;
	}
	// 끊긴 섬을 향한 요청이 전체 group을 확장하지 않도록 group을 먼저 비교한다.
	if (LNPNavGraph::GetGroup(Nav, Start) != LNPNavGraph::GetGroup(Nav, Goal))
	{
		OutSearch.Status = ELNPNavSearchStatus::Unreachable;
		return OutSearch.Status;
	}

	const int32 NodeCount = Graph.GetNodeCount();
	if (Scratch.Stamp.Num() != NodeCount)
	{
		Scratch.G.SetNumUninitialized(NodeCount);
		Scratch.Parent.SetNumUninitialized(NodeCount);
		Scratch.Stamp.Init(0, NodeCount);
		Scratch.CurrentStamp = 0;
	}
	if (++Scratch.CurrentStamp == 0)
	{
		// uint32 stamp가 한 바퀴 돌았다. 옛 stamp와 겹치지 않게 비운다.
		FMemory::Memzero(Scratch.Stamp.GetData(), Scratch.Stamp.Num() * sizeof(uint32));
		Scratch.CurrentStamp = 1;
	}
	Scratch.Open.Reset();

	OutSearch.Stamp = Scratch.CurrentStamp;
	OutSearch.GoalPoint = Graph.GetWorldPoint(GoalSlot, Goal);
	const FVector3d StartPoint = Graph.GetWorldPoint(StartSlot, Start);
	Scratch.Stamp[Start] = OutSearch.Stamp;
	Scratch.G[Start] = 0.0f;
	Scratch.Parent[Start] = INDEX_NONE;
	const float H = Params.bUseHeuristic ? static_cast<float>(FVector3d::Dist(StartPoint, OutSearch.GoalPoint)) : 0.0f;
	Scratch.Open.HeapPush({H, 0.0f, Start}, FOpenOrder());
	OutSearch.Status = ELNPNavSearchStatus::Running;
	return OutSearch.Status;
}

ELNPNavSearchStatus LNPNavPathfinding::StepSearch(
	const FLNPNavSnapshot& Nav, FLNPNavSearchScratch& Scratch, FLNPNavSearch& Search, const int32 ExpansionBudget)
{
	if (Search.Status != ELNPNavSearchStatus::Running)
	{
		return Search.Status;
	}
	if (Nav.SnapshotGeneration != Search.SnapshotGeneration || Nav.ConnectivityGraphVersion != Search.ConnectivityGraphVersion
		|| Scratch.CurrentStamp != Search.Stamp || Scratch.Stamp.Num() != Nav.Graph.GetNodeCount())
	{
		Search.Status = ELNPNavSearchStatus::Invalid;
		return Search.Status;
	}

	const FLNPNavGraph& Graph = Nav.Graph;
	// 7a 조립이 모든 slot의 agent profile이 같음을 검증했다.
	const double WalkableMinDot = Graph.SlotGraphs[0]->Agent.WalkableMinDot;
	const uint32 Stamp = Search.Stamp;
	int32 Budget = ExpansionBudget;
	while (Budget > 0 && !Scratch.Open.IsEmpty())
	{
		FLNPNavOpenEntry Entry;
		Scratch.Open.HeapPop(Entry, FOpenOrder(), EAllowShrinking::No);
		// 더 짧은 g로 다시 넣은 node의 옛 항목이다.
		if (Entry.G > Scratch.G[Entry.Node])
		{
			continue;
		}
		if (Entry.Node == Search.Goal)
		{
			Search.PathCost = Entry.G;
			Search.Status = ELNPNavSearchStatus::Found;
			return Search.Status;
		}
		if (Search.Expansions >= Search.Params.MaxExpansions)
		{
			Search.Status = ELNPNavSearchStatus::NoPath;
			return Search.Status;
		}
		++Search.Expansions;
		--Budget;

		const int32 Slot = Graph.GetSlot(Entry.Node);
		const FVector3d Point = Graph.GetWorldPoint(Slot, Entry.Node);
		const int32 SlotBegin = Graph.SlotNodeBase[Slot];
		const int32 SlotEnd = Graph.SlotNodeBase[Slot + 1];
		Graph.ForEachNeighbor(Slot, Entry.Node, [&](const int32 To)
		{
			const int32 ToSlot = To >= SlotBegin && To < SlotEnd ? Slot : Graph.GetSlot(To);
			const FVector3d ToPoint = Graph.GetWorldPoint(ToSlot, To);
			const float NewG = Entry.G + static_cast<float>(LNPNavGraph::ComputeEdgeCost(Point, ToPoint, WalkableMinDot));
			if (Scratch.Stamp[To] == Stamp && NewG >= Scratch.G[To])
			{
				return;
			}
			Scratch.Stamp[To] = Stamp;
			Scratch.G[To] = NewG;
			Scratch.Parent[To] = Entry.Node;
			const float H = Search.Params.bUseHeuristic
				? static_cast<float>(FVector3d::Dist(ToPoint, Search.GoalPoint)) : 0.0f;
			Scratch.Open.HeapPush({NewG + H, NewG, To}, FOpenOrder());
		});
	}
	if (Scratch.Open.IsEmpty())
	{
		Search.Status = ELNPNavSearchStatus::NoPath;
	}
	return Search.Status;
}

bool LNPNavPathfinding::ExtractNodePath(
	const FLNPNavSearchScratch& Scratch, const FLNPNavSearch& Search, TArray<int32>& OutNodes)
{
	OutNodes.Reset();
	if (Search.Status != ELNPNavSearchStatus::Found || Scratch.CurrentStamp != Search.Stamp)
	{
		return false;
	}
	for (int32 Node = Search.Goal; Node != INDEX_NONE; Node = Scratch.Parent[Node])
	{
		OutNodes.Add(Node);
		if (OutNodes.Num() > Scratch.Parent.Num())
		{
			OutNodes.Reset();
			return false;
		}
	}
	Algo::Reverse(OutNodes);
	return OutNodes[0] == Search.Start;
}

void LNPNavPathfinding::SimplifyPath(
	const FLNPNavSnapshot& Nav, const TConstArrayView<int32> Nodes, TArray<int32>& OutWaypoints)
{
	OutWaypoints.Reset();
	if (Nodes.IsEmpty())
	{
		return;
	}
	const FLNPNavGraph& Graph = Nav.Graph;
	OutWaypoints.Add(Nodes[0]);
	int32 Anchor = 0;
	const int32 Last = Nodes.Num() - 1;
	while (Anchor < Last)
	{
		// 바로 다음 node는 edge로 이어져 있으므로 항상 갈 수 있다. 그 너머는 같은 구간 안에서 직선 검사가 처음 실패할 때까지 민다.
		int32 Best = Anchor + 1;
		for (int32 Candidate = Anchor + 2; Candidate <= FMath::Min(Last, Anchor + SimplifyLookahead); ++Candidate)
		{
			if (!IsSameSegment(Graph, Nodes[Anchor], Nodes[Candidate])
				|| !LNPNavGraph::IsDirectWalkable(Nav, Nodes[Anchor], Nodes[Candidate]))
			{
				break;
			}
			Best = Candidate;
		}
		OutWaypoints.Add(Nodes[Best]);
		Anchor = Best;
	}
}
