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
	template <typename FAccept, typename FCandidates>
	void ScanLayer(
		const FLNPNavSnapshot& Nav, const int32 Slot, const int32 LayerOrdinal, const FVector3d& WorldPosition,
		const double Radius, FAccept&& Accept, FCandidates& OutCandidates,
		const FLNPNavOverlay* Overlay = nullptr, const bool bNearestOnly = false)
	{
		const FLNPNavGraph& Graph = Nav.Graph;
		const FLNPNavAssetGraph& Asset = *Graph.SlotGraphs[Slot];
		const FLNPNavGraphLayer& Layer = Asset.Layers[LayerOrdinal];
		if (!(Layer.BaseRadius > 0.0) || Layer.NodeBegin == Layer.NodeEnd)
		{
			return;
		}
		const FVector3d Local = Graph.SlotRotations[Slot].UnrotateVector(WorldPosition);
		const int32 N = Layer.Subdivisions;
		FIntPoint Min, Max;
		LNPNavData::GetGridSearchBounds(N, Local, Radius, Min, Max);
		const int32 MinJ = Min.Y;
		const int32 MaxJ = Max.Y;
		const int32 MinI = Min.X;
		const int32 MaxI = Max.X;
		const double RadiusSquared = FMath::Square(Radius);
		const int32 Base = Graph.SlotNodeBase[Slot];
		for (int32 J = MinJ; J <= MaxJ; ++J)
		{
			const int32 RowMaxI = FMath::Min(MaxI, N - J);
			const int32 TileRow = (J / LNPNavData::TileSide) * Layer.TilesPerRow;
			const int32 CellRow = (J % LNPNavData::TileSide) * LNPNavData::TileSide;
			// 희소 Layer의 빈 Tile은 셀 16개를 하나씩 주소 변환하지 않고 건너뛴다. (J, I) 방문 순서는 유지한다.
			for (int32 TileX = MinI / LNPNavData::TileSide; TileX <= RowMaxI / LNPNavData::TileSide; ++TileX)
			{
				const int32 TileId = Layer.TileIdByXY[TileRow + TileX];
				if (TileId == INDEX_NONE)
				{
					continue;
				}
				const int32 TileBeginI = TileX * LNPNavData::TileSide;
				const int32 CellBegin = TileId * LNPNavData::MaxCellsPerTile + CellRow;
				for (int32 I = FMath::Max(MinI, TileBeginI); I <= FMath::Min(RowMaxI, TileBeginI + LNPNavData::TileSide - 1); ++I)
				{
					const int32 AssetNode = Layer.NodeByTileCell[CellBegin + I - TileBeginI];
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
					const FLNPNavGraphCandidate Candidate{Base + AssetNode, LNPNavGraph::GetGroup(Nav, Base + AssetNode), FMath::Sqrt(DistanceSquared)};
					if (!bNearestOnly || OutCandidates.IsEmpty())
					{
						OutCandidates.Add(Candidate);
					}
					else if (Candidate.Distance < OutCandidates[0].Distance
						|| (Candidate.Distance == OutCandidates[0].Distance && Candidate.Node < OutCandidates[0].Node))
					{
						OutCandidates[0] = Candidate;
					}
				}
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
				if (!LNPSupportAtlas::QueryNavLayer(SupportLayer, Direction, Query))
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
	if (!Nav.ReachabilityGroupByNode.IsEmpty())
	{
		return Nav.ReachabilityGroupByNode.IsValidIndex(GraphNode) ? Nav.ReachabilityGroupByNode[GraphNode] : MAX_uint32;
	}
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
	const double Radius, TArray<FLNPNavGraphCandidate>& OutCandidates, const FLNPNavOverlay* Overlay, const bool bNearestOnly)
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
			[](int32, const FLNPNavGraphNode&) { return true; }, OutCandidates, Overlay, bNearestOnly);
		if (!bNearestOnly)
		{
			SortCandidates(OutCandidates);
		}
	}
}

FLNPNavGraphCandidate LNPNavGraph::FindNearestNode(
	const FLNPNavSnapshot& Nav, const int32 Slot, const uint16 LocalNavLayerId, const FVector3d& WorldPosition,
	double Radius, const FLNPNavOverlay* Overlay, const int32 HintNode)
{
	const FLNPNavGraph& Graph = Nav.Graph;
	if (!Graph.IsValid() || Slot < 0 || Slot >= GraphSlotCount || !(Radius > 0.0))
	{
		return FLNPNavGraphCandidate();
	}
	const int32 Ordinal = FindLayerOrdinal(*Graph.SlotGraphs[Slot], LocalNavLayerId);
	if (Ordinal == INDEX_NONE)
	{
		return FLNPNavGraphCandidate();
	}
	// 힌트가 이 slot·Layer의 막히지 않은 node면 최근접 거리는 힌트 거리 이하다. 반경을 그만큼 줄여도 최근접과 동률 node가
	// 모두 창 안에 남으므로 결과가 전체 반경 탐색과 같다. 거리식은 ScanLayer와 같고, 1cm 여유는 제곱·제곱근 반올림을 덮는다.
	if (Graph.GetSlot(HintNode) == Slot && !Graph.BlockedNodes[HintNode] && !LNPNavOverlay::IsBlocked(Overlay, HintNode))
	{
		const FLNPNavGraphNode& Hint = Graph.GetNode(Slot, HintNode);
		const double HintDistanceSquared = FVector3d::DistSquared(
			Graph.SlotRotations[Slot].RotateVector(FVector3d(Hint.LocalPoint)), WorldPosition);
		if (Hint.LayerOrdinal == Ordinal && HintDistanceSquared <= FMath::Square(Radius))
		{
			Radius = FMath::Min(Radius, FMath::Sqrt(HintDistanceSquared) + 1.0);
		}
	}
	TArray<FLNPNavGraphCandidate, TInlineAllocator<1>> Nearest;
	ScanLayer(Nav, Slot, Ordinal, WorldPosition, Radius,
		[](int32, const FLNPNavGraphNode&) { return true; }, Nearest, Overlay, true);
	return Nearest.IsEmpty() ? FLNPNavGraphCandidate() : Nearest[0];
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
			const bool bHasStartGroup = !Nav.ReachabilityGroupByNode.IsEmpty() || Asset.Layers[Ordinal].LocalComponents.ContainsByPredicate(
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
						&& FirstStartByGroup.Contains(LNPNavGraph::GetGroup(Nav, Node));
				},
				Approaches, Overlay, true);
		}
	}
	if (!Approaches.IsEmpty())
	{
		// 접근점은 최솟값 하나만 필요하므로 전체 후보를 보관·정렬하지 않는다. 거리·node 동률 규칙은 그대로다.
		const FLNPNavGraphCandidate& Approach = Approaches[0];
		const FLNPNavGraphCandidate& Start = Starts[FirstStartByGroup.FindChecked(Approach.Group)];
		Result.ApproachNode = Approach.Node;
		Result.GoalDistance = Approach.Distance;
		Result.StartNode = Start.Node;
		Result.StartDistance = Start.Distance;
	}
	return Result;
}

FLNPNavEndpoints LNPNavGraph::ResolveEndpointsCached(const FLNPNavSnapshot& Nav, const FLNPNavEndpointQuery& Query,
	FStartCache& StartCache, const FLNPNavOverlay* Overlay)
{
	FLNPNavEndpoints Result;
	if (!Nav.Graph.IsValid())
	{
		return Result;
	}
	// 판정 순서와 Stale·시작 없음의 반환은 ResolveEndpoints와 같다.
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
	TArray<FLNPNavGraphCandidate> Nearest;
	if (!StartCache.bResolved)
	{
		CollectNodesNear(Nav, Query.StartSurface->OctantSlot, Query.StartSurface->LocalLayerId,
			Query.StartPosition, Query.SnapRadius, Nearest, Overlay, true);
		StartCache.bResolved = true;
		StartCache.Nearest = Nearest.IsEmpty() ? FLNPNavGraphCandidate() : Nearest[0];
	}
	if (StartCache.Nearest.Node == INDEX_NONE)
	{
		return Result;
	}
	if (bGoalCurrent)
	{
		CollectNodesNear(Nav, Query.GoalSurface->OctantSlot, Query.GoalSurface->LocalLayerId,
			Query.GoalPosition, Query.SnapRadius, Nearest, Overlay, true);
		if (!Nearest.IsEmpty() && Nearest[0].Group == StartCache.Nearest.Group)
		{
			Result.Status = ELNPNavEndpointStatus::Reachable;
			Result.StartNode = StartCache.Nearest.Node;
			Result.StartDistance = StartCache.Nearest.Distance;
			Result.GoalNode = Nearest[0].Node;
			Result.GoalDistance = Nearest[0].Distance;
			return Result;
		}
	}
	// 공통 group 재선택(D-062)·접근점(D-063)은 전체 후보 목록이 필요하다.
	return ResolveEndpoints(Nav, Query, Overlay);
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

namespace
{
	/** 격자 삼각형의 세 꼭짓점 node가 있고 막히지 않았으며 세 변이 모두 막히지 않은 이웃 edge인지 본다(D-090). */
	bool IsTriangleOpen(const FLNPNavGraph& Graph, const FLNPNavAssetGraph& Asset, const FLNPNavGraphLayer& Layer,
		const int32 Base, const FIntPoint (&Corners)[3], const FLNPNavOverlay* Overlay)
	{
		int32 Nodes[3];
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			Nodes[Corner] = FindLayerNode(Layer, Corners[Corner].X, Corners[Corner].Y);
			if (Nodes[Corner] == INDEX_NONE || Graph.BlockedNodes[Base + Nodes[Corner]]
				|| LNPNavOverlay::IsBlocked(Overlay, Base + Nodes[Corner]))
			{
				return false;
			}
		}
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const int32 From = Nodes[Corner];
			const int32 To = Nodes[(Corner + 1) % 3];
			if (Algo::Find(Asset.Nodes[From].Neighbors, To) == nullptr
				|| (Graph.HasBlockedEdge[Base + From] && Graph.IsEdgeBlocked(Base + From, Base + To))
				|| (Graph.HasBlockedEdge[Base + To] && Graph.IsEdgeBlocked(Base + To, Base + From)))
			{
				return false;
			}
		}
		return true;
	}

	/** 실수 격자 좌표를 담은 삼각형의 세 꼭짓점. 반환값은 같은 삼각형을 가리는 키(i0, j0, 위·아래)다. */
	FIntVector GetGridTriangle(const FVector2d& Grid, FIntPoint (&OutCorners)[3])
	{
		const int32 I0 = FMath::FloorToInt32(Grid.X);
		const int32 J0 = FMath::FloorToInt32(Grid.Y);
		const bool bUpper = (Grid.X - I0) + (Grid.Y - J0) > 1.0;
		OutCorners[0] = bUpper ? FIntPoint(I0 + 1, J0 + 1) : FIntPoint(I0, J0);
		OutCorners[1] = FIntPoint(I0 + 1, J0);
		OutCorners[2] = FIntPoint(I0, J0 + 1);
		return FIntVector(I0, J0, bUpper ? 1 : 0);
	}

	/** 한 격자선 족(i, j 또는 i+j가 정수인 선)과 선분의 다음 교점. 선분 매개변수 t가 커지는 순서로 낸다. */
	struct FGridLineCrossings
	{
		double Value = 0.0;
		double Delta = 0.0;
		int32 Next = 0;
		int32 Direction = 0;

		FGridLineCrossings(const double InValue, const double InDelta)
			: Value(InValue), Delta(InDelta)
		{
			if (InDelta > UE_DOUBLE_SMALL_NUMBER)
			{
				Direction = 1;
				Next = FMath::FloorToInt32(InValue) + 1;
			}
			else if (InDelta < -UE_DOUBLE_SMALL_NUMBER)
			{
				Direction = -1;
				Next = FMath::CeilToInt32(InValue) - 1;
			}
		}

		double NextT() const { return Direction == 0 ? TNumericLimits<double>::Max() : (Next - Value) / Delta; }
	};
}

bool LNPNavGraph::IsSegmentWalkable(const FLNPNavSnapshot& Nav, const int32 FromNode, const FVector3d& FromPosition,
	const int32 ToNode, const FVector3d& ToPosition, const FLNPNavOverlay* Overlay)
{
	const FLNPNavGraph& Graph = Nav.Graph;
	const int32 Slot = Graph.GetSlot(FromNode);
	if (Slot == INDEX_NONE || Graph.GetSlot(ToNode) != Slot)
	{
		return false;
	}
	const FLNPNavAssetGraph& Asset = *Graph.SlotGraphs[Slot];
	const int32 Base = Graph.SlotNodeBase[Slot];
	const int32 LayerOrdinal = Asset.Nodes[FromNode - Base].LayerOrdinal;
	if (Asset.Nodes[ToNode - Base].LayerOrdinal != LayerOrdinal)
	{
		return false;
	}
	const FLNPNavGraphLayer& Layer = Asset.Layers[LayerOrdinal];
	FVector2d A, B;
	if (!LNPNavData::ProjectToGrid(Layer.Subdivisions, Graph.SlotRotations[Slot].UnrotateVector(FromPosition), A)
		|| !LNPNavData::ProjectToGrid(Layer.Subdivisions, Graph.SlotRotations[Slot].UnrotateVector(ToPosition), B))
	{
		return false;
	}

	// 세 격자선 족과의 교점을 t 오름차순으로 병합한다. 이웃한 두 교점 사이 구간은 삼각형 하나 안에 있으므로 그 중점의 삼각형을 본다.
	// 꼭짓점을 정확히 지나면 두 족의 교점이 겹쳐 길이 0 구간이 생기며, 그 구간은 건너뛴다(앞뒤 삼각형이 그 꼭짓점을 이미 본다).
	const FVector2d Delta = B - A;
	FGridLineCrossings Families[3] = {
		FGridLineCrossings(A.X, Delta.X), FGridLineCrossings(A.Y, Delta.Y), FGridLineCrossings(A.X + A.Y, Delta.X + Delta.Y)};
	constexpr double EmptyInterval = 1e-9;
	// node 지면점처럼 끝점이 꼭짓점이면 투영 오차로 꼭짓점 바로 옆에 격자 길이 1e-9 수준의 구간이 생기고, 그 중점은 선분이 지나지 않는
	// 꼭짓점 반대편 삼각형(구멍 쪽)에 떨어질 수 있다(단위6: node↔node 단순화가 구멍 옆 node에서 모두 거부됨). 이보다 짧은 구간은 건너뛴다.
	// node 지면점의 격자 좌표 오차는 1e-5 수준이라 그보다 넉넉히 1e-3 격자(약 0.2cm)를 쓴다. 길이 0 선분(제자리 판정)은 그 점의 삼각형을 그대로 본다.
	constexpr double MinGridInterval = 1e-3;
	const double GridLength = Delta.Size();
	// 격자선 위를 따라가는 구간(edge 줄)은 중점이 선 위라 오차가 고른 한쪽 삼각형만 보게 된다. 그 선 양쪽 중 하나라도 트였으면
	// 그 삼각형의 변인 edge(베이커 sweep 반지름50)를 따라가는 것이므로 통과다.
	constexpr double OnLineTolerance = 1e-3;
	auto IsIntervalOpen = [&](const FVector2d& Mid, FIntPoint (&Corners)[3])
	{
		if (IsTriangleOpen(Graph, Asset, Layer, Base, Corners, Overlay))
		{
			return true;
		}
		const FVector2d Nudges[3] = {FVector2d(1.0, 0.0), FVector2d(0.0, 1.0), FVector2d(1.0, 0.0)};
		const double Values[3] = {Mid.X, Mid.Y, Mid.X + Mid.Y};
		for (int32 Family = 0; Family < 3; ++Family)
		{
			if (FMath::Abs(Values[Family] - FMath::RoundToDouble(Values[Family])) > OnLineTolerance)
			{
				continue;
			}
			for (const double Sign : {-1.0, 1.0})
			{
				FIntPoint Side[3];
				GetGridTriangle(Mid + Nudges[Family] * (Sign * OnLineTolerance * 2.0), Side);
				if (IsTriangleOpen(Graph, Asset, Layer, Base, Side, Overlay))
				{
					return true;
				}
			}
		}
		return false;
	};
	const int32 MaxIntervals = FMath::CeilToInt32(FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) + FMath::Abs(Delta.X + Delta.Y)) + 4;
	FIntVector LastTriangle(MIN_int32, MIN_int32, 0);
	double Previous = 0.0;
	for (int32 Interval = 0; Interval < MaxIntervals; ++Interval)
	{
		const double Next = FMath::Min3(Families[0].NextT(), Families[1].NextT(), Families[2].NextT());
		const double End = FMath::Min(Next, 1.0);
		if (End - Previous > EmptyInterval && ((End - Previous) * GridLength > MinGridInterval || GridLength <= MinGridInterval))
		{
			FIntPoint Corners[3];
			const FVector2d Mid = A + Delta * (0.5 * (Previous + End));
			const FIntVector Triangle = GetGridTriangle(Mid, Corners);
			if (Triangle != LastTriangle)
			{
				if (!IsIntervalOpen(Mid, Corners))
				{
					return false;
				}
				LastTriangle = Triangle;
			}
		}
		if (Next >= 1.0)
		{
			return true;
		}
		for (FGridLineCrossings& Family : Families)
		{
			if (Family.NextT() <= Next)
			{
				Family.Next += Family.Direction;
			}
		}
		Previous = Next;
	}
	// 교점 수는 MaxIntervals를 넘을 수 없다. 수치 오차로 넘으면 보수적으로 막힘이다.
	return false;
}

bool LNPNavGraph::IsInNavPocket(const FLNPNavSnapshot& Nav, const int32 NearestNode, const FVector3d& Position,
	const FLNPNavOverlay* Overlay)
{
	const FLNPNavGraph& Graph = Nav.Graph;
	const int32 Slot = Graph.GetSlot(NearestNode);
	if (Slot == INDEX_NONE)
	{
		return false;
	}
	const FLNPNavAssetGraph& Asset = *Graph.SlotGraphs[Slot];
	const int32 Base = Graph.SlotNodeBase[Slot];
	const FLNPNavGraphLayer& Layer = Asset.Layers[Asset.Nodes[NearestNode - Base].LayerOrdinal];
	FVector2d Grid;
	if (!LNPNavData::ProjectToGrid(Layer.Subdivisions, Graph.SlotRotations[Slot].UnrotateVector(Position), Grid))
	{
		return false;
	}
	FIntPoint Corners[3];
	GetGridTriangle(Grid, Corners);
	if (IsTriangleOpen(Graph, Asset, Layer, Base, Corners, Overlay))
	{
		return false;
	}
	// 그 자리 삼각형의 외접원 반지름(가장 긴 변/√3). 꼭짓점은 격자 방향을 Position 반경에 둔 점이다.
	const double Radius = Position.Length();
	FVector3d Points[3];
	for (int32 Corner = 0; Corner < 3; ++Corner)
	{
		const FVector3d Direction(Corners[Corner].X, Corners[Corner].Y, Layer.Subdivisions - Corners[Corner].X - Corners[Corner].Y);
		Points[Corner] = Graph.SlotRotations[Slot].RotateVector(Direction.GetSafeNormal()) * Radius;
	}
	const double LongestEdge = FMath::Sqrt(FMath::Max3(FVector3d::DistSquared(Points[0], Points[1]),
		FVector3d::DistSquared(Points[1], Points[2]), FVector3d::DistSquared(Points[2], Points[0])));
	const double Threshold = LongestEdge / UE_DOUBLE_SQRT_3 + NavPocketMargin;
	return FVector3d::DistSquared(Position, Graph.GetWorldPoint(Slot, NearestNode)) > FMath::Square(Threshold);
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
