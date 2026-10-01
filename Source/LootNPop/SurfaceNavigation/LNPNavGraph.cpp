// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavGraph.h"

#include "SurfaceNavigation/LNPNavRuntime.h"
#include "SurfaceNavigation/LNPSupportAtlas.h"

#include "Algo/Find.h"

namespace
{
	constexpr int32 GraphSlotCount = 8;

	int32 FindLayerNode(const FLNPNavGraphLayer& Layer, const int32 I, const int32 J)
	{
		uint16 TileX = 0;
		uint16 TileY = 0;
		uint8 LocalCellIndex = 0;
		if (!LNPNavData::IsValidGridCoord(Layer.Subdivisions, I, J)
			|| !LNPNavData::MakeTileAddress(I, J, TileX, TileY, LocalCellIndex)
			|| TileX >= Layer.TilesPerRow || TileY >= Layer.TilesPerRow)
		{
			return INDEX_NONE;
		}
		const int32 TileId = Layer.TileIdByXY[TileY * Layer.TilesPerRow + TileX];
		return TileId == INDEX_NONE ? INDEX_NONE : Layer.NodeByTileCell[TileId * LNPNavData::MaxCellsPerTile + LocalCellIndex];
	}

	int32 FindLayerOrdinal(const FLNPNavAssetGraph& Graph, const uint16 LocalNavLayerId)
	{
		return Graph.Layers.IndexOfByPredicate([LocalNavLayerId](const FLNPNavGraphLayer& Layer)
		{
			return Layer.LocalNavLayerId == LocalNavLayerId;
		});
	}

	/** asset-local node 주소 → asset-local 조밀 index. */
	int32 FindAssetNode(const FLNPNavAssetGraph& Graph, const FLNPLocalNavNodeRef& Local)
	{
		const int32 Ordinal = FindLayerOrdinal(Graph, Local.LocalNavLayerId);
		if (Ordinal == INDEX_NONE || Local.LocalCellIndex >= LNPNavData::MaxCellsPerTile)
		{
			return INDEX_NONE;
		}
		const FLNPNavGraphLayer& Layer = Graph.Layers[Ordinal];
		const int32 Index = static_cast<int32>(Local.TileId) * LNPNavData::MaxCellsPerTile + Local.LocalCellIndex;
		return Layer.NodeByTileCell.IsValidIndex(Index) ? Layer.NodeByTileCell[Index] : INDEX_NONE;
	}

	/** Nav.Graph가 아직 채워지지 않은 조립 중에도 쓰는 변환. */
	int32 ToGraphNodeIn(const FLNPNavGraph& Graph, const FLNPNavSnapshot& Nav, const FLNPNavNodeRef& Node)
	{
		int32 Slot = INDEX_NONE;
		FLNPLocalNavNodeRef Local;
		if (!Graph.IsValid() || !LNPNavRuntime::ResolveRuntimeNodeRef(Nav, Node, Slot, Local))
		{
			return INDEX_NONE;
		}
		const int32 AssetNode = FindAssetNode(*Graph.SlotGraphs[Slot], Local);
		return AssetNode == INDEX_NONE ? INDEX_NONE : Graph.SlotNodeBase[Slot] + AssetNode;
	}

	uint64 MakeEdgeKey(const int32 From, const int32 To)
	{
		return (static_cast<uint64>(static_cast<uint32>(From)) << 32) | static_cast<uint32>(To);
	}

	uint32 GetNodeGroup(const FLNPNavSnapshot& Nav, const int32 Slot, const uint16 LocalComponent)
	{
		const uint32 Component = LNPNavRuntime::GetRuntimeStaticComponent(Nav, Slot, LocalComponent);
		return Nav.ReachabilityGroupByStaticComponent.IsValidIndex(Component)
			? Nav.ReachabilityGroupByStaticComponent[Component]
			: MAX_uint32;
	}

	/** 한 slot·Layer의 격자 좌표 창을 훑는다. Accept(Node, AssetNode) → 후보 여부. */
	template <typename FAccept>
	void ScanLayer(
		const FLNPNavSnapshot& Nav, const int32 Slot, const int32 LayerOrdinal, const FVector3d& WorldPosition,
		const double Radius, FAccept&& Accept, TArray<FLNPNavGraphCandidate>& OutCandidates,
		const FLNPNavOverlay* Overlay = nullptr)
	{
		const FLNPNavGraph& Graph = Nav.Graph;
		const FLNPNavAssetGraph& Asset = *Graph.SlotGraphs[Slot];
		const FLNPNavGraphLayer& Layer = Asset.Layers[LayerOrdinal];
		if (!(Layer.BaseRadius > 0.0) || Layer.NodeBegin == Layer.NodeEnd)
		{
			return;
		}
		// 이음매 근처 위치는 옥탄트 밖 성분이 조금 음수일 수 있으므로 옥탄트 면으로 자른다(7a ProjectToNode와 같은 규칙).
		const FVector3d Local = Graph.SlotRotations[Slot].UnrotateVector(WorldPosition);
		const FVector3d Clamped(FMath::Max(0.0, Local.X), FMath::Max(0.0, Local.Y), FMath::Max(0.0, Local.Z));
		const double Sum = Clamped.X + Clamped.Y + Clamped.Z;
		if (Sum <= UE_DOUBLE_SMALL_NUMBER)
		{
			return;
		}
		const int32 N = Layer.Subdivisions;
		const double CenterI = Clamped.X / Sum * N;
		const double CenterJ = Clamped.Y / Sum * N;
		// 격자 한 칸의 각간격은 옥탄트 꼭짓점 부근에서 가장 작은 약 1/N rad다. 위치와 Layer 중 작은 반지름으로 각도를 잡아
		// 섬 윗면에서 아래 지각을 찾는 것처럼 반지름이 다른 경우에도 창이 반경을 덮게 한다.
		const double AngleRadius = Radius / FMath::Max(1.0, FMath::Min(Layer.BaseRadius, WorldPosition.Length()));
		const int32 Window = FMath::CeilToInt32(AngleRadius * N) + 1;
		const int32 MinJ = FMath::Max(0, FMath::FloorToInt32(CenterJ) - Window);
		const int32 MaxJ = FMath::Min(N, FMath::CeilToInt32(CenterJ) + Window);
		const int32 MinI = FMath::Max(0, FMath::FloorToInt32(CenterI) - Window);
		const int32 MaxI = FMath::Min(N, FMath::CeilToInt32(CenterI) + Window);
		const double RadiusSquared = FMath::Square(Radius);
		const int32 Base = Graph.SlotNodeBase[Slot];
		for (int32 J = MinJ; J <= MaxJ; ++J)
		{
			for (int32 I = MinI; I <= FMath::Min(MaxI, N - J); ++I)
			{
				const int32 AssetNode = FindLayerNode(Layer, I, J);
				if (AssetNode == INDEX_NONE || Graph.BlockedNodes[Base + AssetNode]
					|| LNPNavOverlay::IsBlocked(Overlay, Base + AssetNode))
				{
					continue;
				}
				const FLNPNavGraphNode& Node = Asset.Nodes[AssetNode];
				const double DistanceSquared = FVector3d::DistSquared(
					Graph.SlotRotations[Slot].RotateVector(FVector3d(Node.LocalPoint)), WorldPosition);
				if (DistanceSquared > RadiusSquared || !Accept(Base + AssetNode, Node))
				{
					continue;
				}
				OutCandidates.Add({Base + AssetNode, GetNodeGroup(Nav, Slot, Node.LocalComponent), FMath::Sqrt(DistanceSquared)});
			}
		}
	}

	void SortCandidates(TArray<FLNPNavGraphCandidate>& Candidates)
	{
		Candidates.Sort([](const FLNPNavGraphCandidate& A, const FLNPNavGraphCandidate& B)
		{
			return A.Distance != B.Distance ? A.Distance < B.Distance : A.Node < B.Node;
		});
	}

	bool IsCurrentHandle(const FLNPNavSnapshot& Nav, const FLNPSurfaceHandle* Handle)
	{
		return Handle != nullptr && Handle->IsValid() && Handle->Generation == Nav.SnapshotGeneration
			&& Handle->OctantSlot < GraphSlotCount;
	}

	/** 삼각 격자 metric `di² + dj² + di·dj`에서 가장 가까운 격자점. 6방향 이웃이 모두 거리 1이다. */
	FIntPoint NearestLatticePoint(const double I, const double J)
	{
		const int32 I0 = FMath::FloorToInt32(I);
		const int32 J0 = FMath::FloorToInt32(J);
		FIntPoint Best(I0, J0);
		double BestMetric = TNumericLimits<double>::Max();
		for (int32 DJ = 0; DJ <= 1; ++DJ)
		{
			for (int32 DI = 0; DI <= 1; ++DI)
			{
				const double A = I - (I0 + DI);
				const double B = J - (J0 + DJ);
				const double Metric = A * A + B * B + A * B;
				if (Metric < BestMetric)
				{
					BestMetric = Metric;
					Best = FIntPoint(I0 + DI, J0 + DJ);
				}
			}
		}
		return Best;
	}
}

uint64 FLNPNavAssetGraph::GetAllocatedBytes() const
{
	uint64 Bytes = sizeof(FLNPNavAssetGraph) + Layers.GetAllocatedSize() + Nodes.GetAllocatedSize()
		+ PortalLinks.GetAllocatedSize();
	for (const FLNPNavGraphLayer& Layer : Layers)
	{
		Bytes += Layer.TileIdByXY.GetAllocatedSize() + Layer.NodeByTileCell.GetAllocatedSize()
			+ Layer.LocalComponents.GetAllocatedSize();
	}
	return Bytes;
}

uint64 FLNPNavGraph::GetAllocatedBytes() const
{
	uint64 Bytes = SlotGraphs.GetAllocatedSize() + SlotRotations.GetAllocatedSize() + SlotNodeBase.GetAllocatedSize()
		+ ExtraLinks.GetAllocatedSize() + HasExtraLinks.GetAllocatedSize() + BlockedNodes.GetAllocatedSize()
		+ BlockedEdges.GetAllocatedSize() + HasBlockedEdge.GetAllocatedSize();
	TSet<const FLNPNavAssetGraph*> Counted;
	for (const TSharedPtr<const FLNPNavAssetGraph, ESPMode::ThreadSafe>& Asset : SlotGraphs)
	{
		bool bAlreadyCounted = false;
		Counted.Add(Asset.Get(), &bAlreadyCounted);
		if (!bAlreadyCounted && Asset.IsValid())
		{
			Bytes += Asset->GetAllocatedBytes();
		}
	}
	return Bytes;
}

bool FLNPNavGraph::IsEdgeBlocked(const int32 From, const int32 To) const
{
	return Algo::BinarySearch(BlockedEdges, MakeEdgeKey(From, To)) != INDEX_NONE;
}

bool LNPNavGraph::BuildAssetGraph(
	const FLNPSupportAtlas& Support, const FLNPNavData& Navigation, const FLNPNavTraversalData& Traversal,
	FLNPNavAssetGraph& OutGraph, FString& OutError)
{
	OutGraph = FLNPNavAssetGraph();
	FLNPNavAssetGraph Graph;
	Graph.Agent = Navigation.Agent;
	// 두 번째 pass에서 이웃을 풀 때까지 cell edge mask를 보관한다.
	TArray<uint8> EdgeMasks;
	for (int32 Ordinal = 0; Ordinal < Navigation.Layers.Num(); ++Ordinal)
	{
		const FLNPNavLayer& NavLayer = Navigation.Layers[Ordinal];
		if (!Support.Layers.IsValidIndex(NavLayer.LocalSupportLayerId) || Ordinal > MAX_uint8)
		{
			OutError = FString::Printf(TEXT("Nav Layer %u has no Support Layer %u."),
				NavLayer.LocalNavLayerId, NavLayer.LocalSupportLayerId);
			return false;
		}
		const FLNPSupportAtlasLayer& SupportLayer = Support.Layers[NavLayer.LocalSupportLayerId];
		FLNPNavGraphLayer& Layer = Graph.Layers.AddDefaulted_GetRef();
		Layer.LocalNavLayerId = NavLayer.LocalNavLayerId;
		Layer.Subdivisions = NavLayer.Subdivisions;
		Layer.BaseRadius = SupportLayer.BaseRadius;
		Layer.TilesPerRow = NavLayer.Subdivisions / LNPNavData::TileSide + 1;
		Layer.TileIdByXY.Init(INDEX_NONE, Layer.TilesPerRow * Layer.TilesPerRow);
		Layer.NodeByTileCell.Init(INDEX_NONE, NavLayer.Tiles.Num() * LNPNavData::MaxCellsPerTile);
		Layer.NodeBegin = Graph.Nodes.Num();
		for (int32 TileIndex = 0; TileIndex < NavLayer.Tiles.Num(); ++TileIndex)
		{
			const FLNPNavTile& Tile = NavLayer.Tiles[TileIndex];
			if (Tile.TileId != TileIndex || Tile.TileX >= Layer.TilesPerRow || Tile.TileY >= Layer.TilesPerRow)
			{
				OutError = FString::Printf(TEXT("Nav Layer %u Tile %d has a non-canonical address."), Layer.LocalNavLayerId, TileIndex);
				return false;
			}
			Layer.TileIdByXY[Tile.TileY * Layer.TilesPerRow + Tile.TileX] = TileIndex;
			for (const FLNPNavCell& Cell : Tile.Cells)
			{
				const FIntPoint Coord = LNPNavData::DecodeTileAddress(Tile.TileX, Tile.TileY, Cell.LocalCellIndex);
				const FVector3d Direction = LNPSupportAtlas::GetSampleDirection(NavLayer.Subdivisions, Coord.X, Coord.Y);
				FLNPSupportLayerQuery Query;
				if (!LNPSupportAtlas::QueryLayer(SupportLayer, Direction, Query))
				{
					OutError = FString::Printf(TEXT("Nav Layer %u node (%d,%d) has no Support."), Layer.LocalNavLayerId, Coord.X, Coord.Y);
					return false;
				}
				Layer.NodeByTileCell[TileIndex * LNPNavData::MaxCellsPerTile + Cell.LocalCellIndex] = Graph.Nodes.Num();
				FLNPNavGraphNode& Node = Graph.Nodes.AddDefaulted_GetRef();
				Node.LocalPoint = FVector3f(Direction * Query.Radius);
				Node.I = static_cast<uint16>(Coord.X);
				Node.J = static_cast<uint16>(Coord.Y);
				Node.LocalComponent = Cell.LocalStaticComponentId;
				Node.TileId = Tile.TileId;
				Node.LocalCellIndex = Cell.LocalCellIndex;
				Node.LayerOrdinal = static_cast<uint8>(Ordinal);
				Node.GridDegree = static_cast<uint8>(FMath::CountBits(Cell.EdgeMask & LNPNavData::NeighborMask));
				EdgeMasks.Add(Cell.EdgeMask);
				Layer.LocalComponents.AddUnique(Cell.LocalStaticComponentId);
			}
		}
		Layer.NodeEnd = Graph.Nodes.Num();
		Layer.LocalComponents.Sort();
	}

	for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
	{
		FLNPNavGraphNode& Node = Graph.Nodes[Index];
		const FLNPNavGraphLayer& Layer = Graph.Layers[Node.LayerOrdinal];
		for (uint8 Direction = 0; Direction < 6; ++Direction)
		{
			if ((EdgeMasks[Index] & (1u << Direction)) == 0)
			{
				continue;
			}
			FIntPoint Coord;
			const int32 Neighbor = LNPNavData::TryGetNeighborCoord(
				Layer.Subdivisions, Node.I, Node.J, static_cast<ELNPNavNeighbor>(Direction), Coord)
				? FindLayerNode(Layer, Coord.X, Coord.Y) : INDEX_NONE;
			if (Neighbor == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("Nav Layer %u node (%u,%u) edge %u has no endpoint."),
					Layer.LocalNavLayerId, Node.I, Node.J, Direction);
				return false;
			}
			Node.Neighbors[Direction] = Neighbor;
		}
	}

	for (const FLNPNavPortal& Portal : Traversal.Portals)
	{
		const int32 A = FindAssetNode(Graph, Portal.A);
		const int32 B = FindAssetNode(Graph, Portal.B);
		if (A == INDEX_NONE || B == INDEX_NONE)
		{
			OutError = TEXT("Nav portal endpoint does not resolve to a graph node.");
			return false;
		}
		Graph.PortalLinks.Add({A, B});
		if (EnumHasAnyFlags(Portal.Flags, ELNPNavPortalFlags::Bidirectional))
		{
			Graph.PortalLinks.Add({B, A});
		}
	}
	Graph.PortalLinks.Sort([](const FLNPNavGraphPortalLink& X, const FLNPNavGraphPortalLink& Y)
	{
		return X.From != Y.From ? X.From < Y.From : X.To < Y.To;
	});

	OutGraph = MoveTemp(Graph);
	return true;
}

bool LNPNavGraph::BuildRuntimeGraph(
	const TConstArrayView<TSharedPtr<const FLNPNavAssetGraph, ESPMode::ThreadSafe>> SlotGraphs,
	const TConstArrayView<FRotator> SlotRotations, const FLNPNavSnapshot& Nav, FLNPNavGraph& OutGraph, FString& OutError)
{
	OutGraph = FLNPNavGraph();
	if (SlotGraphs.Num() != GraphSlotCount || SlotRotations.Num() != GraphSlotCount || !Nav.IsValid())
	{
		OutError = TEXT("Nav graph requires eight slot graphs, eight rotations, and an assembled Nav snapshot.");
		return false;
	}
	FLNPNavGraph Graph;
	Graph.SlotNodeBase.Add(0);
	for (int32 Slot = 0; Slot < GraphSlotCount; ++Slot)
	{
		if (!SlotGraphs[Slot].IsValid())
		{
			OutError = FString::Printf(TEXT("Nav graph slot %d has no asset graph."), Slot);
			return false;
		}
		Graph.SlotGraphs.Add(SlotGraphs[Slot]);
		Graph.SlotRotations.Add(FQuat4d(SlotRotations[Slot].Quaternion()));
		Graph.SlotNodeBase.Add(Graph.SlotNodeBase.Last() + SlotGraphs[Slot]->Nodes.Num());
	}
	const int32 NodeCount = Graph.GetNodeCount();
	Graph.HasExtraLinks.Init(false, NodeCount);
	Graph.BlockedNodes.Init(false, NodeCount);
	Graph.HasBlockedEdge.Init(false, NodeCount);

	for (const FLNPNavNodeRef& Ref : Nav.BlockedSeamNodes)
	{
		const int32 Node = ToGraphNodeIn(Graph, Nav, Ref);
		if (Node == INDEX_NONE)
		{
			OutError = TEXT("Blocked seam node does not resolve to a graph node.");
			return false;
		}
		Graph.BlockedNodes[Node] = true;
	}
	// 막힌 이음매 edge는 열린 사본 한 방향만 기록돼 있다. 같은 월드 선분이므로 역방향도 막는다.
	for (const FLNPNavEdgeRef& Edge : Nav.BlockedSeamEdges)
	{
		const int32 From = ToGraphNodeIn(Graph, Nav, Edge.From);
		const int32 To = ToGraphNodeIn(Graph, Nav, Edge.To);
		if (From == INDEX_NONE || To == INDEX_NONE)
		{
			OutError = TEXT("Blocked seam edge does not resolve to graph nodes.");
			return false;
		}
		Graph.BlockedEdges.Add(MakeEdgeKey(From, To));
		Graph.BlockedEdges.Add(MakeEdgeKey(To, From));
		Graph.HasBlockedEdge[From] = true;
		Graph.HasBlockedEdge[To] = true;
	}
	Graph.BlockedEdges.Sort();

	for (const FLNPNavSeamLink& Link : Nav.SeamLinks)
	{
		const int32 A = ToGraphNodeIn(Graph, Nav, Link.A);
		const int32 B = ToGraphNodeIn(Graph, Nav, Link.B);
		if (A == INDEX_NONE || B == INDEX_NONE)
		{
			OutError = TEXT("Seam link does not resolve to graph nodes.");
			return false;
		}
		Graph.ExtraLinks.Add({A, B});
		Graph.ExtraLinks.Add({B, A});
	}
	for (int32 Slot = 0; Slot < GraphSlotCount; ++Slot)
	{
		for (const FLNPNavGraphPortalLink& Portal : SlotGraphs[Slot]->PortalLinks)
		{
			Graph.ExtraLinks.Add({Graph.SlotNodeBase[Slot] + Portal.From, Graph.SlotNodeBase[Slot] + Portal.To});
		}
	}
	Graph.ExtraLinks.Sort([](const FLNPNavGraphLink& X, const FLNPNavGraphLink& Y)
	{
		return X.From != Y.From ? X.From < Y.From : X.To < Y.To;
	});
	for (const FLNPNavGraphLink& Link : Graph.ExtraLinks)
	{
		Graph.HasExtraLinks[Link.From] = true;
	}

	OutGraph = MoveTemp(Graph);
	return true;
}

int32 LNPNavGraph::ToGraphNode(const FLNPNavSnapshot& Nav, const FLNPNavNodeRef& Node)
{
	return ToGraphNodeIn(Nav.Graph, Nav, Node);
}

bool LNPNavGraph::ToNodeRef(const FLNPNavSnapshot& Nav, const int32 GraphNode, FLNPNavNodeRef& OutNode)
{
	OutNode = FLNPNavNodeRef();
	const int32 Slot = Nav.Graph.GetSlot(GraphNode);
	if (Slot == INDEX_NONE)
	{
		return false;
	}
	const FLNPNavAssetGraph& Asset = *Nav.Graph.SlotGraphs[Slot];
	const FLNPNavGraphNode& Node = Nav.Graph.GetNode(Slot, GraphNode);
	FLNPLocalNavNodeRef Local;
	Local.LocalNavLayerId = Asset.Layers[Node.LayerOrdinal].LocalNavLayerId;
	Local.TileId = Node.TileId;
	Local.LocalCellIndex = Node.LocalCellIndex;
	return LNPNavRuntime::MakeRuntimeNodeRef(Nav, Slot, Local, OutNode);
}

uint32 LNPNavGraph::GetGroup(const FLNPNavSnapshot& Nav, const int32 GraphNode)
{
	const int32 Slot = Nav.Graph.GetSlot(GraphNode);
	return Slot == INDEX_NONE ? MAX_uint32 : GetNodeGroup(Nav, Slot, Nav.Graph.GetNode(Slot, GraphNode).LocalComponent);
}

double LNPNavGraph::ComputeEdgeCost(const FVector3d& From, const FVector3d& To, const double WalkableMinDot)
{
	const FVector3d Delta = To - From;
	const double Chord = Delta.Length();
	if (Chord <= UE_DOUBLE_KINDA_SMALL_NUMBER)
	{
		return Chord;
	}
	// 내부형 구에서 지역 Up은 중심 방향이다.
	const FVector3d Up = -(From + To).GetSafeNormal();
	const double Rise = FMath::Abs(FVector3d::DotProduct(Delta, Up));
	const double Horizontal = FMath::Sqrt(FMath::Max(0.0, Chord * Chord - Rise * Rise));
	const double MaxSlope = FMath::Acos(FMath::Clamp(WalkableMinDot, 0.0, 1.0));
	const double SlopeAlpha = MaxSlope > 0.0 ? FMath::Clamp(FMath::Atan2(Rise, Horizontal) / MaxSlope, 0.0, 1.0) : 0.0;
	return Chord * (1.0 + (MaxSlopeCostMultiplier - 1.0) * SlopeAlpha);
}

void LNPNavGraph::CollectNodesNear(
	const FLNPNavSnapshot& Nav, const int32 Slot, const uint16 LocalNavLayerId, const FVector3d& WorldPosition,
	const double Radius, TArray<FLNPNavGraphCandidate>& OutCandidates, const FLNPNavOverlay* Overlay)
{
	OutCandidates.Reset();
	if (!Nav.Graph.IsValid() || Slot < 0 || Slot >= GraphSlotCount || !(Radius > 0.0))
	{
		return;
	}
	const int32 Ordinal = FindLayerOrdinal(*Nav.Graph.SlotGraphs[Slot], LocalNavLayerId);
	if (Ordinal != INDEX_NONE)
	{
		ScanLayer(Nav, Slot, Ordinal, WorldPosition, Radius,
			[](int32, const FLNPNavGraphNode&) { return true; }, OutCandidates, Overlay);
		SortCandidates(OutCandidates);
	}
}

FLNPNavEndpoints LNPNavGraph::ResolveEndpoints(const FLNPNavSnapshot& Nav, const FLNPNavEndpointQuery& Query,
	const FLNPNavOverlay* Overlay)
{
	FLNPNavEndpoints Result;
	if (!Nav.Graph.IsValid())
	{
		return Result;
	}
	const bool bStartCurrent = IsCurrentHandle(Nav, Query.StartSurface);
	const bool bGoalCurrent = IsCurrentHandle(Nav, Query.GoalSurface);
	if ((Query.StartSurface != nullptr && Query.StartSurface->IsValid() && !bStartCurrent)
		|| (Query.GoalSurface != nullptr && Query.GoalSurface->IsValid() && !bGoalCurrent))
	{
		Result.Status = ELNPNavEndpointStatus::Stale;
		return Result;
	}
	if (!bStartCurrent)
	{
		return Result;
	}

	TArray<FLNPNavGraphCandidate> Starts;
	CollectNodesNear(Nav, Query.StartSurface->OctantSlot, Query.StartSurface->LocalLayerId,
		Query.StartPosition, Query.SnapRadius, Starts, Overlay);
	if (Starts.IsEmpty())
	{
		return Result;
	}
	TArray<FLNPNavGraphCandidate> Goals;
	if (bGoalCurrent)
	{
		CollectNodesNear(Nav, Query.GoalSurface->OctantSlot, Query.GoalSurface->LocalLayerId,
			Query.GoalPosition, Query.SnapRadius, Goals, Overlay);
	}

	Result.StartNode = Starts[0].Node;
	Result.StartDistance = Starts[0].Distance;
	if (!Goals.IsEmpty())
	{
		if (Starts[0].Group == Goals[0].Group)
		{
			Result.Status = ELNPNavEndpointStatus::Reachable;
			Result.GoalNode = Goals[0].Node;
			Result.GoalDistance = Goals[0].Distance;
			return Result;
		}
		// 가장 가까운 node끼리 group이 다르면 주변에서 공통 group 짝을 스냅 거리 합 최소로 고른다(D-062).
		// 후보는 거리 오름차순이므로 group마다 첫 후보가 그 group의 최근접이다.
		TMap<uint32, int32> FirstStartByGroup;
		for (int32 Index = 0; Index < Starts.Num(); ++Index)
		{
			FirstStartByGroup.FindOrAdd(Starts[Index].Group, Index);
		}
		TSet<uint32> SeenGoalGroups;
		int32 BestStart = INDEX_NONE;
		int32 BestGoal = INDEX_NONE;
		double BestSum = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index < Goals.Num(); ++Index)
		{
			bool bSeen = false;
			SeenGoalGroups.Add(Goals[Index].Group, &bSeen);
			const int32* StartIndex = bSeen ? nullptr : FirstStartByGroup.Find(Goals[Index].Group);
			if (StartIndex == nullptr)
			{
				continue;
			}
			const double Sum = Starts[*StartIndex].Distance + Goals[Index].Distance;
			const bool bBetter = Sum < BestSum
				|| (Sum == BestSum && (Starts[*StartIndex].Node < Starts[BestStart].Node
					|| (Starts[*StartIndex].Node == Starts[BestStart].Node && Goals[Index].Node < Goals[BestGoal].Node)));
			if (bBetter)
			{
				BestSum = Sum;
				BestStart = *StartIndex;
				BestGoal = Index;
			}
		}
		if (BestStart != INDEX_NONE)
		{
			Result.Status = ELNPNavEndpointStatus::Reachable;
			Result.bReselected = true;
			Result.StartNode = Starts[BestStart].Node;
			Result.StartDistance = Starts[BestStart].Distance;
			Result.GoalNode = Goals[BestGoal].Node;
			Result.GoalDistance = Goals[BestGoal].Distance;
			return Result;
		}
		Result.Status = ELNPNavEndpointStatus::Unreachable;
	}
	if (!(Query.ApproachRadius > 0.0))
	{
		return Result;
	}

	// D-063 접근점: 시작 후보가 속한 group의 내부 node 중 목표에 가장 가까운 것. 목표와 다른 slot·Layer에 있을 수 있으므로
	// 시작 group을 가진 모든 slot·Layer를 목표 주변에서 훑는다.
	TMap<uint32, int32> FirstStartByGroup;
	for (int32 Index = 0; Index < Starts.Num(); ++Index)
	{
		FirstStartByGroup.FindOrAdd(Starts[Index].Group, Index);
	}
	TArray<FLNPNavGraphCandidate> Approaches;
	const FVector3d GoalDirection = Query.GoalPosition.GetSafeNormal();
	const double AngleRadius = Query.ApproachRadius / FMath::Max(1.0, Query.GoalPosition.Length() * 0.5);
	for (int32 Slot = 0; Slot < GraphSlotCount; ++Slot)
	{
		const FVector3d Local = Nav.Graph.SlotRotations[Slot].UnrotateVector(GoalDirection);
		const FVector3d Clamped(FMath::Max(0.0, Local.X), FMath::Max(0.0, Local.Y), FMath::Max(0.0, Local.Z));
		if (Clamped.IsNearlyZero()
			|| FVector3d::DotProduct(Clamped.GetSafeNormal(), Local) < FMath::Cos(FMath::Min(AngleRadius, UE_DOUBLE_PI)))
		{
			continue;
		}
		const FLNPNavAssetGraph& Asset = *Nav.Graph.SlotGraphs[Slot];
		for (int32 Ordinal = 0; Ordinal < Asset.Layers.Num(); ++Ordinal)
		{
			const bool bHasStartGroup = Asset.Layers[Ordinal].LocalComponents.ContainsByPredicate(
				[&Nav, Slot, &FirstStartByGroup](const uint16 Component)
				{
					return FirstStartByGroup.Contains(GetNodeGroup(Nav, Slot, Component));
				});
			if (!bHasStartGroup)
			{
				continue;
			}
			ScanLayer(Nav, Slot, Ordinal, Query.GoalPosition, Query.ApproachRadius,
				[&Nav, Slot, &FirstStartByGroup](const int32 Node, const FLNPNavGraphNode& GraphNode)
				{
					// 가장자리 바로 위를 피하려고 6방향 edge가 모두 열린 내부 node만 받는다.
					return GraphNode.GridDegree == 6 && !Nav.Graph.HasBlockedEdge[Node]
						&& FirstStartByGroup.Contains(GetNodeGroup(Nav, Slot, GraphNode.LocalComponent));
				},
				Approaches, Overlay);
		}
	}
	if (!Approaches.IsEmpty())
	{
		SortCandidates(Approaches);
		const FLNPNavGraphCandidate& Approach = Approaches[0];
		const FLNPNavGraphCandidate& Start = Starts[FirstStartByGroup.FindChecked(Approach.Group)];
		Result.ApproachNode = Approach.Node;
		Result.GoalDistance = Approach.Distance;
		Result.StartNode = Start.Node;
		Result.StartDistance = Start.Distance;
	}
	return Result;
}

namespace
{
	/**
	 * 직선 보행 검사 본체. 통과한 cell의 전역 index를 순서대로 OnNode에 넘긴다(From 포함, 중복 없음).
	 * 옥탄트 면으로의 중심 투영은 대원을 직선으로 보낸다. 격자 좌표 (i, j)는 그 면의 affine 좌표이므로
	 * 두 node 사이 대원호는 (i, j) 공간의 선분이다.
	 */
	template <typename FOnNode>
	bool WalkDirect(const FLNPNavSnapshot& Nav, const int32 From, const int32 To, FOnNode&& OnNode,
		const FLNPNavOverlay* Overlay)
	{
		const FLNPNavGraph& Graph = Nav.Graph;
		const int32 Slot = Graph.GetSlot(From);
		if (Slot == INDEX_NONE || Graph.GetSlot(To) != Slot || Graph.BlockedNodes[From] || Graph.BlockedNodes[To]
			|| LNPNavOverlay::IsBlocked(Overlay, From) || LNPNavOverlay::IsBlocked(Overlay, To))
		{
			return false;
		}
		const FLNPNavAssetGraph& Asset = *Graph.SlotGraphs[Slot];
		const int32 Base = Graph.SlotNodeBase[Slot];
		const FLNPNavGraphNode& A = Asset.Nodes[From - Base];
		const FLNPNavGraphNode& B = Asset.Nodes[To - Base];
		if (A.LayerOrdinal != B.LayerOrdinal)
		{
			return false;
		}
		OnNode(From);
		if (From == To)
		{
			return true;
		}
		const FLNPNavGraphLayer& Layer = Asset.Layers[A.LayerOrdinal];
		const int32 DI = static_cast<int32>(B.I) - A.I;
		const int32 DJ = static_cast<int32>(B.J) - A.J;
		const int32 HexDistance = FMath::Max3(FMath::Abs(DI), FMath::Abs(DJ), FMath::Abs(DI + DJ));
		const int32 Samples = HexDistance * 4;
		int32 Previous = From - Base;
		for (int32 Sample = 1; Sample <= Samples; ++Sample)
		{
			const double Alpha = static_cast<double>(Sample) / Samples;
			const FIntPoint Cell = NearestLatticePoint(A.I + DI * Alpha, A.J + DJ * Alpha);
			const int32 Current = FindLayerNode(Layer, Cell.X, Cell.Y);
			if (Current == INDEX_NONE || Graph.BlockedNodes[Base + Current]
				|| LNPNavOverlay::IsBlocked(Overlay, Base + Current))
			{
				return false;
			}
			if (Current == Previous)
			{
				continue;
			}
			// 연속 샘플은 격자 간격의 1/4 이내라 같은 cell이거나 6방향 이웃이다. 그 밖(세 cell 경계점)은 보수적으로 막힘이다.
			const FLNPNavGraphNode& PreviousNode = Asset.Nodes[Previous];
			const bool bNeighbor = Algo::Find(PreviousNode.Neighbors, Current) != nullptr;
			if (!bNeighbor || (Graph.HasBlockedEdge[Base + Previous] && Graph.IsEdgeBlocked(Base + Previous, Base + Current)))
			{
				return false;
			}
			OnNode(Base + Current);
			Previous = Current;
		}
		return Previous == To - Base;
	}
}

bool LNPNavGraph::IsDirectWalkable(const FLNPNavSnapshot& Nav, const int32 From, const int32 To,
	const FLNPNavOverlay* Overlay)
{
	return WalkDirect(Nav, From, To, [](int32) {}, Overlay);
}

bool LNPNavGraph::CollectDirectWalkNodes(const FLNPNavSnapshot& Nav, const int32 From, const int32 To,
	TArray<int32>& OutNodes, const FLNPNavOverlay* Overlay)
{
	OutNodes.Reset();
	return WalkDirect(Nav, From, To, [&OutNodes](const int32 Node) { OutNodes.Add(Node); }, Overlay);
}

uint32 LNPNavGraph::GetTileKey(const FLNPNavSnapshot& Nav, const int32 GraphNode)
{
	const int32 Slot = Nav.Graph.GetSlot(GraphNode);
	if (Slot == INDEX_NONE)
	{
		return MAX_uint32;
	}
	const FLNPNavGraphNode& Node = Nav.Graph.GetNode(Slot, GraphNode);
	const uint32 RuntimeLayer = Nav.SlotLayerBase[Slot] + Nav.Graph.SlotGraphs[Slot]->Layers[Node.LayerOrdinal].LocalNavLayerId;
	return (RuntimeLayer << 16) | Node.TileId;
}
