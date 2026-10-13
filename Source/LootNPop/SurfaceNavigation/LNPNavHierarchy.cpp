// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavHierarchy.h"

#include "Algo/BinarySearch.h"
#include "Algo/Reverse.h"
#include "Async/ParallelFor.h"
#include "SurfaceNavigation/LNPNavGraph.h"
#include "SurfaceNavigation/LNPNavOverlay.h"
#include "SurfaceNavigation/LNPNavRuntime.h"

namespace
{
	/** A*와 같은 순서: f가 작은 쪽, 같으면 g가 큰 쪽, 그다음 index가 작은 쪽. */
	struct FHierarchyAStarOrder
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

	/** Cluster 안 Dijkstra: 거리가 작은 쪽, 같으면 node index가 작은 쪽. */
	struct FHierarchyDijkstraOrder
	{
		bool operator()(const FLNPNavOpenEntry& A, const FLNPNavOpenEntry& B) const
		{
			return A.G != B.G ? A.G < B.G : A.Node < B.Node;
		}
	};

	/** 같은 slot 안 node의 Cluster. slot을 아는 호출자가 GetSlot 순회를 건너뛴다. */
	int32 HierarchyClusterInSlot(const FLNPNavSnapshot& Nav, const FLNPNavHierarchy& Hierarchy, const int32 Slot, const int32 Node)
	{
		const FLNPNavGraph& Graph = Nav.Graph;
		const FLNPNavGraphNode& GraphNode = Graph.GetNode(Slot, Node);
		const int32 RuntimeLayer = Nav.SlotLayerBase[Slot] + Graph.SlotGraphs[Slot]->Layers[GraphNode.LayerOrdinal].LocalNavLayerId;
		return Hierarchy.ClusterBaseByRuntimeLayer[RuntimeLayer] + GraphNode.TileId;
	}

	bool HierarchyIsBlocked(const FLNPNavGraph& Graph, const FLNPNavOverlay* Overlay, const int32 Node)
	{
		return Graph.BlockedNodes[Node] || LNPNavOverlay::IsBlocked(Overlay, Node);
	}

	/** 256 cell 지역 Dijkstra 상태. Cluster 안 node는 slot·Layer·Tile이 같아 cell index가 겹치지 않는다. */
	struct FHierarchyClusterDijkstra
	{
		float Dist[256];
		uint64 Settled[4];

		bool IsSettled(const uint8 Cell) const { return (Settled[Cell >> 6] & (uint64(1) << (Cell & 63))) != 0; }
	};

	/**
	 * Source에서 같은 Cluster node로만 편다. TargetMask의 cell이 모두 확정되면 멈춘다. 반환값은 확정 수다.
	 * bReverse면 이웃 → 확정 node 방향(목표로 걷는 방향)으로 비용을 잰다.
	 */
	int32 RunHierarchyClusterDijkstra(const FLNPNavSnapshot& Nav, const int32 Source, const uint64 (&TargetMask)[4], const int32 TargetCount,
		const bool bReverse, const FLNPNavOverlay* Overlay, FHierarchyClusterDijkstra& Out)
	{
		for (float& Value : Out.Dist)
		{
			Value = MAX_flt;
		}
		FMemory::Memzero(Out.Settled, sizeof(Out.Settled));
		const FLNPNavGraph& Graph = Nav.Graph;
		if (HierarchyIsBlocked(Graph, Overlay, Source))
		{
			return 0;
		}
		const int32 Slot = Graph.GetSlot(Source);
		const int32 SlotBegin = Graph.SlotNodeBase[Slot];
		const int32 SlotEnd = Graph.SlotNodeBase[Slot + 1];
		const FLNPNavAssetGraph& Asset = *Graph.SlotGraphs[Slot];
		const FLNPNavGraphNode& SourceNode = Graph.GetNode(Slot, Source);
		const uint16 TileId = SourceNode.TileId;
		const uint8 LayerOrdinal = SourceNode.LayerOrdinal;
		const double WalkableMinDot = Asset.Agent.WalkableMinDot;

		TArray<FLNPNavOpenEntry, TInlineAllocator<64>> Open;
		Out.Dist[SourceNode.LocalCellIndex] = 0.0f;
		Open.HeapPush({0.0f, 0.0f, Source}, FHierarchyDijkstraOrder());
		int32 SettledCount = 0;
		int32 TargetsLeft = TargetCount;
		while (!Open.IsEmpty() && TargetsLeft > 0)
		{
			FLNPNavOpenEntry Entry;
			Open.HeapPop(Entry, FHierarchyDijkstraOrder(), EAllowShrinking::No);
			const FLNPNavGraphNode& Node = Asset.Nodes[Entry.Node - SlotBegin];
			const uint8 Cell = Node.LocalCellIndex;
			if (Out.IsSettled(Cell) || Entry.G > Out.Dist[Cell])
			{
				continue;
			}
			Out.Settled[Cell >> 6] |= uint64(1) << (Cell & 63);
			++SettledCount;
			TargetsLeft -= (TargetMask[Cell >> 6] & (uint64(1) << (Cell & 63))) != 0 ? 1 : 0;
			const FVector3d Point = Graph.GetWorldPoint(Slot, Entry.Node);
			Graph.ForEachNeighbor(Slot, Entry.Node, [&](const int32 To)
			{
				if (To < SlotBegin || To >= SlotEnd)
				{
					return;
				}
				const FLNPNavGraphNode& ToNode = Asset.Nodes[To - SlotBegin];
				if (ToNode.TileId != TileId || ToNode.LayerOrdinal != LayerOrdinal || Out.IsSettled(ToNode.LocalCellIndex))
				{
					return;
				}
				const FVector3d ToPoint = Graph.GetWorldPoint(Slot, To);
				const float NewG = Entry.G + static_cast<float>(bReverse
					? LNPNavGraph::ComputeEdgeCost(ToPoint, Point, WalkableMinDot)
					: LNPNavGraph::ComputeEdgeCost(Point, ToPoint, WalkableMinDot));
				if (NewG >= Out.Dist[ToNode.LocalCellIndex])
				{
					return;
				}
				Out.Dist[ToNode.LocalCellIndex] = NewG;
				Open.HeapPush({NewG, NewG, To}, FHierarchyDijkstraOrder());
			}, Overlay);
		}
		return SettledCount;
	}

	uint8 HierarchyCell(const FLNPNavGraph& Graph, const int32 Node)
	{
		return Graph.GetNode(Graph.GetSlot(Node), Node).LocalCellIndex;
	}

	void HierarchyAddMask(uint64 (&Mask)[4], const uint8 Cell)
	{
		Mask[Cell >> 6] |= uint64(1) << (Cell & 63);
	}

	/** Cluster 행렬을 overlay 기준으로 다시 계산한다. 반환값은 확정 수다. */
	int64 ComputeHierarchyClusterRows(const FLNPNavSnapshot& Nav, FLNPNavHierarchy& Hierarchy, const int32 ClusterIndex,
		const FLNPNavOverlay* Overlay)
	{
		FLNPNavHierarchyCluster& Cluster = Hierarchy.Clusters[ClusterIndex];
		Cluster.OverlayTileRevision = LNPNavOverlay::GetTileRevision(Overlay, Cluster.TileKey);
		const int32 Count = Cluster.AbstractCount;
		if (Count == 0)
		{
			return 0;
		}
		const FLNPNavGraph& Graph = Nav.Graph;
		uint64 Mask[4] = {};
		TArray<uint8, TInlineAllocator<64>> Cells;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Cells.Add(HierarchyCell(Graph, Hierarchy.Abstract[Cluster.AbstractBegin + Index]));
			HierarchyAddMask(Mask, Cells.Last());
		}
		int64 Settled = 0;
		FHierarchyClusterDijkstra Search;
		for (int32 From = 0; From < Count; ++From)
		{
			float* Row = Hierarchy.IntraCosts.GetData() + Cluster.CostBegin + From * Count;
			Settled += RunHierarchyClusterDijkstra(Nav, Hierarchy.Abstract[Cluster.AbstractBegin + From], Mask, Count, false, Overlay, Search);
			for (int32 To = 0; To < Count; ++To)
			{
				Row[To] = Search.IsSettled(Cells[To]) ? Search.Dist[Cells[To]] : MAX_flt;
			}
		}
		return Settled;
	}

	bool AreHierarchyGridNeighbors(const FLNPNavGraph& Graph, const int32 A, const int32 B)
	{
		const int32 Slot = Graph.GetSlot(A);
		if (Slot != Graph.GetSlot(B))
		{
			return false;
		}
		const int32 LocalB = B - Graph.SlotNodeBase[Slot];
		for (const int32 Neighbor : Graph.GetNode(Slot, A).Neighbors)
		{
			if (Neighbor == LocalB)
			{
				return true;
			}
		}
		return false;
	}

	int32 FindHierarchyAbstract(const FLNPNavHierarchy& Hierarchy, const int32 ClusterIndex, const int32 Node)
	{
		const FLNPNavHierarchyCluster& Cluster = Hierarchy.Clusters[ClusterIndex];
		const int32 Found = Algo::BinarySearch(TConstArrayView<int32>(Hierarchy.Abstract.GetData() + Cluster.AbstractBegin, Cluster.AbstractCount), Node);
		return Found == INDEX_NONE ? INDEX_NONE : Cluster.AbstractBegin + Found;
	}
}

uint64 FLNPNavHierarchy::GetAllocatedBytes() const
{
	return ClusterBaseByRuntimeLayer.GetAllocatedSize() + Clusters.GetAllocatedSize() + Abstract.GetAllocatedSize()
		+ AbstractCluster.GetAllocatedSize() + InterBegin.GetAllocatedSize() + InterEdges.GetAllocatedSize() + IntraCosts.GetAllocatedSize();
}

bool LNPNavHierarchy::Build(const FLNPNavSnapshot& Nav, const FLNPNavHierarchyParams& Params, FLNPNavHierarchy& OutHierarchy,
	const FLNPNavOverlay* Overlay)
{
	const double StartSeconds = FPlatformTime::Seconds();
	OutHierarchy = FLNPNavHierarchy();
	const FLNPNavGraph& Graph = Nav.Graph;
	if (!Nav.IsValid() || !Graph.IsValid())
	{
		return false;
	}
	OutHierarchy.SnapshotGeneration = Nav.SnapshotGeneration;
	OutHierarchy.ConnectivityGraphVersion = Nav.ConnectivityGraphVersion;
	OutHierarchy.OverlayRevision = LNPNavOverlay::GetRevision(Overlay);
	OutHierarchy.Params = Params;
	FLNPNavHierarchyBuildStats& Stats = OutHierarchy.BuildStats;

	// 1) Cluster = runtime Layer별 Tile. TileId는 Layer 안에서 조밀하다.
	OutHierarchy.ClusterBaseByRuntimeLayer.Init(INDEX_NONE, Nav.SlotLayerBase.Last());
	for (int32 Slot = 0; Slot < 8; ++Slot)
	{
		for (const FLNPNavGraphLayer& Layer : Graph.SlotGraphs[Slot]->Layers)
		{
			const int32 RuntimeLayer = Nav.SlotLayerBase[Slot] + Layer.LocalNavLayerId;
			if (!OutHierarchy.ClusterBaseByRuntimeLayer.IsValidIndex(RuntimeLayer))
			{
				OutHierarchy = FLNPNavHierarchy();
				return false;
			}
			OutHierarchy.ClusterBaseByRuntimeLayer[RuntimeLayer] = OutHierarchy.Clusters.Num();
			const int32 TileCount = Layer.NodeByTileCell.Num() / 256;
			for (int32 Tile = 0; Tile < TileCount; ++Tile)
			{
				FLNPNavHierarchyCluster& Cluster = OutHierarchy.Clusters.AddDefaulted_GetRef();
				Cluster.TileKey = (static_cast<uint32>(RuntimeLayer) << 16) | static_cast<uint32>(Tile);
				for (int32 Cell = 0; Cell < 256; ++Cell)
				{
					Cluster.NodeCount += Layer.NodeByTileCell[Tile * 256 + Cell] != INDEX_NONE ? 1 : 0;
				}
			}
		}
	}
	Stats.Clusters = OutHierarchy.Clusters.Num();

	// 2) Cluster 경계를 넘는 edge. 양방향 edge를 낮은 Cluster 쪽에서 한 번만 센다. Portal은 overlay 없는 정적 graph로 정한다.
	struct FCrossing
	{
		int32 Low = INDEX_NONE;
		int32 High = INDEX_NONE;
		int32 A = INDEX_NONE;
		int32 B = INDEX_NONE;
	};
	TArray<FCrossing> Crossings;
	for (int32 Slot = 0; Slot < 8; ++Slot)
	{
		for (int32 Node = Graph.SlotNodeBase[Slot]; Node < Graph.SlotNodeBase[Slot + 1]; ++Node)
		{
			const int32 From = HierarchyClusterInSlot(Nav, OutHierarchy, Slot, Node);
			Graph.ForEachNeighbor(Slot, Node, [&](const int32 To)
			{
				const int32 ToSlot = To >= Graph.SlotNodeBase[Slot] && To < Graph.SlotNodeBase[Slot + 1] ? Slot : Graph.GetSlot(To);
				const int32 ToCluster = HierarchyClusterInSlot(Nav, OutHierarchy, ToSlot, To);
				if (From < ToCluster)
				{
					Crossings.Add({From, ToCluster, Node, To});
				}
			});
		}
	}
	Crossings.Sort([](const FCrossing& X, const FCrossing& Y)
	{
		if (X.Low != Y.Low) { return X.Low < Y.Low; }
		if (X.High != Y.High) { return X.High < Y.High; }
		return X.A != Y.A ? X.A < Y.A : X.B < Y.B;
	});
	Stats.CrossingEdges = Crossings.Num();

	// 3) 같은 Cluster 쌍에서 양 끝이 각각 같거나 grid 이웃인 edge끼리 span으로 묶고, MaxSpanLength개 경계 node마다 잘라 가운데 edge를 Portal로 둔다.
	TArray<TPair<int32, int32>> Portals;
	const int32 MaxSpan = FMath::Max(1, Params.MaxSpanLength);
	for (int32 GroupBegin = 0; GroupBegin < Crossings.Num();)
	{
		int32 GroupEnd = GroupBegin + 1;
		while (GroupEnd < Crossings.Num() && Crossings[GroupEnd].Low == Crossings[GroupBegin].Low
			&& Crossings[GroupEnd].High == Crossings[GroupBegin].High)
		{
			++GroupEnd;
		}
		const int32 Count = GroupEnd - GroupBegin;
		TArray<int32, TInlineAllocator<64>> Parent;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Parent.Add(Index);
		}
		auto FindRoot = [&Parent](int32 Index)
		{
			while (Parent[Index] != Index)
			{
				Parent[Index] = Parent[Parent[Index]];
				Index = Parent[Index];
			}
			return Index;
		};
		for (int32 X = 0; X < Count; ++X)
		{
			const FCrossing& EX = Crossings[GroupBegin + X];
			for (int32 Y = X + 1; Y < Count; ++Y)
			{
				const FCrossing& EY = Crossings[GroupBegin + Y];
				if ((EX.A == EY.A || AreHierarchyGridNeighbors(Graph, EX.A, EY.A))
					&& (EX.B == EY.B || AreHierarchyGridNeighbors(Graph, EX.B, EY.B)))
				{
					const int32 RX = FindRoot(X);
					const int32 RY = FindRoot(Y);
					Parent[FMath::Max(RX, RY)] = FMath::Min(RX, RY);
				}
			}
		}
		// root는 span의 가장 작은 index다. span edge는 이미 (A, B) 순이다.
		for (int32 Root = 0; Root < Count; ++Root)
		{
			if (FindRoot(Root) != Root)
			{
				continue;
			}
			++Stats.Spans;
			TArray<int32, TInlineAllocator<64>> Members;
			for (int32 Index = Root; Index < Count; ++Index)
			{
				if (FindRoot(Index) == Root)
				{
					Members.Add(GroupBegin + Index);
				}
			}
			for (int32 ChunkBegin = 0; ChunkBegin < Members.Num();)
			{
				int32 ChunkEnd = ChunkBegin;
				int32 Distinct = 0;
				while (ChunkEnd < Members.Num())
				{
					const bool bNewNode = ChunkEnd == ChunkBegin || Crossings[Members[ChunkEnd]].A != Crossings[Members[ChunkEnd - 1]].A;
					if (bNewNode && Distinct == MaxSpan)
					{
						break;
					}
					Distinct += bNewNode ? 1 : 0;
					++ChunkEnd;
				}
				const FCrossing& Representative = Crossings[Members[(ChunkBegin + ChunkEnd - 1) / 2]];
				Portals.Add({Representative.A, Representative.B});
				ChunkBegin = ChunkEnd;
			}
		}
		GroupBegin = GroupEnd;
	}
	Stats.Portals = Portals.Num();

	// 4) abstract node = Portal 양 끝. Cluster별로 node index 오름차순이다.
	TArray<TArray<int32>> ByCluster;
	ByCluster.SetNum(OutHierarchy.Clusters.Num());
	TArray<int32> PortalClusters;
	for (const TPair<int32, int32>& Portal : Portals)
	{
		for (const int32 Node : {Portal.Key, Portal.Value})
		{
			ByCluster[LNPNavHierarchy::GetCluster(Nav, OutHierarchy, Node)].AddUnique(Node);
		}
	}
	int32 CostCount = 0;
	for (int32 ClusterIndex = 0; ClusterIndex < OutHierarchy.Clusters.Num(); ++ClusterIndex)
	{
		TArray<int32>& Nodes = ByCluster[ClusterIndex];
		Nodes.Sort();
		FLNPNavHierarchyCluster& Cluster = OutHierarchy.Clusters[ClusterIndex];
		Cluster.AbstractBegin = OutHierarchy.Abstract.Num();
		Cluster.AbstractCount = Nodes.Num();
		Cluster.CostBegin = CostCount;
		CostCount += Nodes.Num() * Nodes.Num();
		OutHierarchy.Abstract.Append(Nodes);
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			OutHierarchy.AbstractCluster.Add(ClusterIndex);
		}
	}

	// 5) Portal edge(양방향, 보행 방향 비용) CSR.
	const double WalkableMinDot = Graph.SlotGraphs[0]->Agent.WalkableMinDot;
	struct FDirected
	{
		int32 From;
		int32 To;
		float Cost;
	};
	TArray<FDirected> Directed;
	for (const TPair<int32, int32>& Portal : Portals)
	{
		const int32 A = FindHierarchyAbstract(OutHierarchy, LNPNavHierarchy::GetCluster(Nav, OutHierarchy, Portal.Key), Portal.Key);
		const int32 B = FindHierarchyAbstract(OutHierarchy, LNPNavHierarchy::GetCluster(Nav, OutHierarchy, Portal.Value), Portal.Value);
		const FVector3d PA = Graph.GetWorldPoint(Graph.GetSlot(Portal.Key), Portal.Key);
		const FVector3d PB = Graph.GetWorldPoint(Graph.GetSlot(Portal.Value), Portal.Value);
		Directed.Add({A, B, static_cast<float>(LNPNavGraph::ComputeEdgeCost(PA, PB, WalkableMinDot))});
		Directed.Add({B, A, static_cast<float>(LNPNavGraph::ComputeEdgeCost(PB, PA, WalkableMinDot))});
	}
	Directed.Sort([](const FDirected& X, const FDirected& Y) { return X.From != Y.From ? X.From < Y.From : X.To < Y.To; });
	OutHierarchy.InterBegin.Init(0, OutHierarchy.Abstract.Num() + 1);
	for (const FDirected& Edge : Directed)
	{
		++OutHierarchy.InterBegin[Edge.From + 1];
		OutHierarchy.InterEdges.Add({Edge.To, Edge.Cost});
	}
	for (int32 Index = 1; Index < OutHierarchy.InterBegin.Num(); ++Index)
	{
		OutHierarchy.InterBegin[Index] += OutHierarchy.InterBegin[Index - 1];
	}

	// 6) Cluster 내부 비용. Cluster마다 독립이라 병렬 결과가 직렬과 같다.
	OutHierarchy.IntraCosts.SetNumUninitialized(CostCount);
	TArray<int64> Settled;
	Settled.SetNumZeroed(OutHierarchy.Clusters.Num());
	auto ComputeCluster = [&Nav, &OutHierarchy, &Settled, Overlay](const int32 ClusterIndex)
	{
		Settled[ClusterIndex] = ComputeHierarchyClusterRows(Nav, OutHierarchy, ClusterIndex, Overlay);
	};
	if (Params.bParallel)
	{
		ParallelFor(OutHierarchy.Clusters.Num(), ComputeCluster);
	}
	else
	{
		for (int32 ClusterIndex = 0; ClusterIndex < OutHierarchy.Clusters.Num(); ++ClusterIndex)
		{
			ComputeCluster(ClusterIndex);
		}
	}
	for (const int64 Value : Settled)
	{
		Stats.Settled += Value;
	}
	Stats.Seconds = FPlatformTime::Seconds() - StartSeconds;
	return true;
}

int32 LNPNavHierarchy::GetCluster(const FLNPNavSnapshot& Nav, const FLNPNavHierarchy& Hierarchy, const int32 Node)
{
	const int32 Slot = Nav.Graph.GetSlot(Node);
	return Slot == INDEX_NONE || Hierarchy.ClusterBaseByRuntimeLayer.IsEmpty() ? INDEX_NONE : HierarchyClusterInSlot(Nav, Hierarchy, Slot, Node);
}

FLNPNavHierarchyUpdateStats LNPNavHierarchy::UpdateOverlay(const FLNPNavSnapshot& Nav, FLNPNavHierarchy& Hierarchy,
	const FLNPNavOverlay* Overlay)
{
	const double StartSeconds = FPlatformTime::Seconds();
	FLNPNavHierarchyUpdateStats Stats;
	auto Recompute = [&](const int32 ClusterIndex)
	{
		++Stats.DirtyClusters;
		Stats.DirtyAbstractNodes += Hierarchy.Clusters[ClusterIndex].AbstractCount;
		Stats.Settled += ComputeHierarchyClusterRows(Nav, Hierarchy, ClusterIndex, Overlay);
	};
	// 바뀐 적 있는 Tile만 overlay revision 표에 있다. 표를 순회하므로 Cluster 전체를 훑지 않는다.
	TArray<int32> Dirty;
	if (Overlay)
	{
		for (const TPair<uint32, uint32>& Pair : Overlay->TileRevisions)
		{
			const int32 RuntimeLayer = static_cast<int32>(Pair.Key >> 16);
			if (!Hierarchy.ClusterBaseByRuntimeLayer.IsValidIndex(RuntimeLayer) || Hierarchy.ClusterBaseByRuntimeLayer[RuntimeLayer] == INDEX_NONE)
			{
				continue;
			}
			const int32 ClusterIndex = Hierarchy.ClusterBaseByRuntimeLayer[RuntimeLayer] + static_cast<int32>(Pair.Key & 0xFFFF);
			if (Hierarchy.Clusters.IsValidIndex(ClusterIndex) && Hierarchy.Clusters[ClusterIndex].TileKey == Pair.Key
				&& Hierarchy.Clusters[ClusterIndex].OverlayTileRevision != Pair.Value)
			{
				Dirty.Add(ClusterIndex);
			}
		}
	}
	else
	{
		// overlay를 내렸으면 revision을 남긴 Cluster만 정적 상태로 되돌린다.
		for (int32 ClusterIndex = 0; ClusterIndex < Hierarchy.Clusters.Num(); ++ClusterIndex)
		{
			if (Hierarchy.Clusters[ClusterIndex].OverlayTileRevision != 0)
			{
				Dirty.Add(ClusterIndex);
			}
		}
	}
	// map 순회 순서와 무관하게 같은 순서로 계산한다.
	Dirty.Sort();
	for (const int32 ClusterIndex : Dirty)
	{
		Recompute(ClusterIndex);
	}
	Hierarchy.OverlayRevision = LNPNavOverlay::GetRevision(Overlay);
	Stats.Seconds = FPlatformTime::Seconds() - StartSeconds;
	return Stats;
}

int32 LNPNavHierarchy::BuildLink(const FLNPNavSnapshot& Nav, const FLNPNavHierarchy& Hierarchy, const int32 Node,
	const ELNPNavHierarchyLinkSide Side, const bool bExact, FLNPNavHierarchyLink& OutLink, const FLNPNavOverlay* Overlay, const int32 Target)
{
	OutLink = FLNPNavHierarchyLink();
	OutLink.Node = Node;
	OutLink.bExact = bExact;
	OutLink.Cluster = GetCluster(Nav, Hierarchy, Node);
	if (OutLink.Cluster == INDEX_NONE || !Hierarchy.Clusters.IsValidIndex(OutLink.Cluster))
	{
		OutLink.Cluster = INDEX_NONE;
		return 0;
	}
	const FLNPNavGraph& Graph = Nav.Graph;
	const FLNPNavHierarchyCluster& Cluster = Hierarchy.Clusters[OutLink.Cluster];
	OutLink.Costs.Init(MAX_flt, Cluster.AbstractCount);
	if (HierarchyIsBlocked(Graph, Overlay, Node))
	{
		return 0;
	}
	if (!bExact)
	{
		// chord는 모든 edge 비용의 하한이라 고수준 휴리스틱과 함께 써도 고수준 경로 선택만 근사가 된다. 실제 경로는 corridor A*가 정한다.
		const uint32 Group = LNPNavGraph::GetGroup(Nav, Node);
		const FVector3d Point = Graph.GetWorldPoint(Graph.GetSlot(Node), Node);
		for (int32 Index = 0; Index < Cluster.AbstractCount; ++Index)
		{
			const int32 Abstract = Hierarchy.Abstract[Cluster.AbstractBegin + Index];
			if (!LNPNavOverlay::IsBlocked(Overlay, Abstract) && LNPNavGraph::GetGroup(Nav, Abstract) == Group)
			{
				OutLink.Costs[Index] = static_cast<float>(FVector3d::Dist(Point, Graph.GetWorldPoint(Graph.GetSlot(Abstract), Abstract)));
			}
		}
		return 0;
	}
	uint64 Mask[4] = {};
	TArray<uint8, TInlineAllocator<64>> Cells;
	for (int32 Index = 0; Index < Cluster.AbstractCount; ++Index)
	{
		Cells.Add(HierarchyCell(Graph, Hierarchy.Abstract[Cluster.AbstractBegin + Index]));
		HierarchyAddMask(Mask, Cells.Last());
	}
	int32 TargetCount = Cluster.AbstractCount;
	const bool bTarget = Target != INDEX_NONE && GetCluster(Nav, Hierarchy, Target) == OutLink.Cluster;
	if (bTarget && (Mask[HierarchyCell(Graph, Target) >> 6] & (uint64(1) << (HierarchyCell(Graph, Target) & 63))) == 0)
	{
		HierarchyAddMask(Mask, HierarchyCell(Graph, Target));
		++TargetCount;
	}
	FHierarchyClusterDijkstra Search;
	const int32 Settled = RunHierarchyClusterDijkstra(Nav, Node, Mask, TargetCount, Side == ELNPNavHierarchyLinkSide::Goal, Overlay, Search);
	for (int32 Index = 0; Index < Cluster.AbstractCount; ++Index)
	{
		OutLink.Costs[Index] = Search.IsSettled(Cells[Index]) ? Search.Dist[Cells[Index]] : MAX_flt;
	}
	if (bTarget && Search.IsSettled(HierarchyCell(Graph, Target)))
	{
		OutLink.TargetCost = Search.Dist[HierarchyCell(Graph, Target)];
	}
	return Settled;
}

namespace
{
	/** 고수준 node(abstract 또는 가상 goal = Abstract.Num())의 g를 낮추면 open에 넣는다. */
	void RelaxHierarchyHigh(const FLNPNavSnapshot& Nav, const FLNPNavHierarchy& Hierarchy, FLNPNavHierarchyScratch& Scratch,
		const FLNPNavHierarchySearch& Search, const int32 Node, const int32 Parent, const float G)
	{
		if (Scratch.HighStamp[Node] == Search.Stamp && G >= Scratch.HighG[Node])
		{
			return;
		}
		Scratch.HighStamp[Node] = Search.Stamp;
		Scratch.HighG[Node] = G;
		Scratch.HighParent[Node] = Parent;
		float H = 0.0f;
		if (Node < Hierarchy.Abstract.Num())
		{
			const int32 LowNode = Hierarchy.Abstract[Node];
			H = static_cast<float>(FVector3d::Dist(Nav.Graph.GetWorldPoint(Nav.Graph.GetSlot(LowNode), LowNode), Search.GoalPoint));
		}
		Scratch.HighOpen.HeapPush({G + H, G, Node}, FHierarchyAStarOrder());
	}

	void MarkHierarchyCluster(FLNPNavHierarchyScratch& Scratch, const FLNPNavHierarchySearch& Search, const int32 Cluster,
		TArray<int32>& OutMarked)
	{
		if (Cluster != INDEX_NONE && Scratch.ClusterStamp[Cluster] != Search.Stamp)
		{
			Scratch.ClusterStamp[Cluster] = Search.Stamp;
			OutMarked.Add(Cluster);
		}
	}

	/** From → To 저수준 A*를 Low scratch에 새로 시작한다. 구간마다 stamp를 새로 받는다. */
	void BeginHierarchyLowSearch(const FLNPNavSnapshot& Nav, FLNPNavHierarchyScratch& Scratch, FLNPNavHierarchySearch& Search,
		const int32 From, const int32 To)
	{
		FLNPNavSearchScratch& Low = Scratch.Low;
		const int32 NodeCount = Nav.Graph.GetNodeCount();
		if (Low.Stamp.Num() != NodeCount)
		{
			Low.G.SetNumUninitialized(NodeCount);
			Low.Parent.SetNumUninitialized(NodeCount);
			Low.Stamp.Init(0, NodeCount);
			Low.CurrentStamp = 0;
		}
		if (++Low.CurrentStamp == 0)
		{
			FMemory::Memzero(Low.Stamp.GetData(), Low.Stamp.Num() * sizeof(uint32));
			Low.CurrentStamp = 1;
		}
		Low.Open.Reset();
		Search.LowStamp = Low.CurrentStamp;
		Search.LowTarget = To;
		Search.LowTargetPoint = Nav.Graph.GetWorldPoint(Nav.Graph.GetSlot(To), To);
		const FVector3d FromPoint = Nav.Graph.GetWorldPoint(Nav.Graph.GetSlot(From), From);
		Low.Stamp[From] = Search.LowStamp;
		Low.G[From] = 0.0f;
		Low.Parent[From] = INDEX_NONE;
		Low.Open.HeapPush({static_cast<float>(FVector3d::Dist(FromPoint, Search.LowTargetPoint)), 0.0f, From}, FHierarchyAStarOrder());
	}

	/** 고수준 경로의 Cluster(+ 이웃)를 corridor로 표시하고 저수준 A*를 시작한다. */
	void BeginHierarchyCorridor(const FLNPNavSnapshot& Nav, const FLNPNavHierarchy& Hierarchy, FLNPNavHierarchyScratch& Scratch,
		FLNPNavHierarchySearch& Search)
	{
		TArray<int32> Marked;
		MarkHierarchyCluster(Scratch, Search, Search.StartCluster, Marked);
		MarkHierarchyCluster(Scratch, Search, Search.GoalCluster, Marked);
		for (int32 Node = Scratch.HighParent[Hierarchy.Abstract.Num()]; Node != INDEX_NONE; Node = Scratch.HighParent[Node])
		{
			MarkHierarchyCluster(Scratch, Search, Hierarchy.AbstractCluster[Node], Marked);
		}
		if (Search.Params.CorridorRing > 0)
		{
			const int32 PathClusters = Marked.Num();
			for (int32 Index = 0; Index < PathClusters; ++Index)
			{
				const FLNPNavHierarchyCluster& Cluster = Hierarchy.Clusters[Marked[Index]];
				for (int32 Abstract = Cluster.AbstractBegin; Abstract < Cluster.AbstractBegin + Cluster.AbstractCount; ++Abstract)
				{
					for (int32 Edge = Hierarchy.InterBegin[Abstract]; Edge < Hierarchy.InterBegin[Abstract + 1]; ++Edge)
					{
						MarkHierarchyCluster(Scratch, Search, Hierarchy.AbstractCluster[Hierarchy.InterEdges[Edge].To], Marked);
					}
				}
			}
		}
		Search.CorridorClusters = Marked.Num();
		BeginHierarchyLowSearch(Nav, Scratch, Search, Search.Start, Search.Goal);
		Search.Phase = ELNPNavHierarchyPhase::Corridor;
	}

	void FailHierarchySearch(FLNPNavHierarchySearch& Search, const ELNPNavSearchStatus Status, const ELNPNavHierarchyFailure Failure)
	{
		Search.Status = Status;
		Search.Failure = Failure;
		Search.Phase = ELNPNavHierarchyPhase::Done;
	}

	/** 구간을 차례로 잇는다. Portal 구간(다른 Cluster)은 edge 하나로 잇고, 같은 Cluster 구간을 만나면 그 Cluster 안 A*를 시작한다. 다 이으면 Found다. */
	void AdvanceHierarchySegments(const FLNPNavSnapshot& Nav, const FLNPNavHierarchy& Hierarchy, FLNPNavHierarchyScratch& Scratch,
		FLNPNavHierarchySearch& Search)
	{
		const FLNPNavGraph& Graph = Nav.Graph;
		const double WalkableMinDot = Graph.SlotGraphs[0]->Agent.WalkableMinDot;
		while (Search.SegmentIndex + 1 < Scratch.Waypoints.Num())
		{
			const int32 From = Scratch.Waypoints[Search.SegmentIndex];
			const int32 To = Scratch.Waypoints[Search.SegmentIndex + 1];
			const int32 FromCluster = LNPNavHierarchy::GetCluster(Nav, Hierarchy, From);
			if (FromCluster != LNPNavHierarchy::GetCluster(Nav, Hierarchy, To))
			{
				Search.SegmentCost += LNPNavGraph::ComputeEdgeCost(Graph.GetWorldPoint(Graph.GetSlot(From), From),
					Graph.GetWorldPoint(Graph.GetSlot(To), To), WalkableMinDot);
				Scratch.RefinedPath.Add(To);
				++Search.SegmentIndex;
				continue;
			}
			Search.SegmentCluster = FromCluster;
			++Search.SegmentSearches;
			BeginHierarchyLowSearch(Nav, Scratch, Search, From, To);
			Search.Phase = ELNPNavHierarchyPhase::Segment;
			return;
		}
		Search.PathCost = Search.SegmentCost;
		Search.Status = ELNPNavSearchStatus::Found;
		Search.Phase = ELNPNavHierarchyPhase::Done;
	}

	/** 고수준 경로를 시작 → abstract node들 → 목표 node 열로 펴고 첫 구간을 시작한다. */
	void BeginHierarchySegments(const FLNPNavSnapshot& Nav, const FLNPNavHierarchy& Hierarchy, FLNPNavHierarchyScratch& Scratch,
		FLNPNavHierarchySearch& Search)
	{
		TArray<int32, TInlineAllocator<64>> Path;
		for (int32 Node = Scratch.HighParent[Hierarchy.Abstract.Num()]; Node != INDEX_NONE; Node = Scratch.HighParent[Node])
		{
			Path.Add(Hierarchy.Abstract[Node]);
		}
		Scratch.Waypoints.Reset();
		Scratch.Waypoints.Add(Search.Start);
		for (int32 Index = Path.Num() - 1; Index >= 0; --Index)
		{
			if (Path[Index] != Scratch.Waypoints.Last())
			{
				Scratch.Waypoints.Add(Path[Index]);
			}
		}
		if (Search.Goal != Scratch.Waypoints.Last())
		{
			Scratch.Waypoints.Add(Search.Goal);
		}
		Search.CorridorClusters = 1;
		for (int32 Index = 1; Index < Scratch.Waypoints.Num(); ++Index)
		{
			Search.CorridorClusters += LNPNavHierarchy::GetCluster(Nav, Hierarchy, Scratch.Waypoints[Index])
				!= LNPNavHierarchy::GetCluster(Nav, Hierarchy, Scratch.Waypoints[Index - 1]) ? 1 : 0;
		}
		Scratch.RefinedPath.Reset();
		Scratch.RefinedPath.Add(Search.Start);
		Search.SegmentIndex = 0;
		Search.SegmentCost = 0.0;
		AdvanceHierarchySegments(Nav, Hierarchy, Scratch, Search);
	}
}

ELNPNavSearchStatus LNPNavHierarchy::BeginQuery(const FLNPNavSnapshot& Nav, const FLNPNavHierarchy& Hierarchy,
	const FLNPNavHierarchyLink& StartLink, const FLNPNavHierarchyLink& GoalLink, const FLNPNavHierarchyQueryParams& Params,
	FLNPNavHierarchyScratch& Scratch, FLNPNavHierarchySearch& OutSearch, const FLNPNavOverlay* Overlay)
{
	OutSearch = FLNPNavHierarchySearch();
	OutSearch.Params = Params;
	OutSearch.Start = StartLink.Node;
	OutSearch.Goal = GoalLink.Node;
	OutSearch.StartCluster = StartLink.Cluster;
	OutSearch.GoalCluster = GoalLink.Cluster;
	OutSearch.SnapshotGeneration = Nav.SnapshotGeneration;
	OutSearch.ConnectivityGraphVersion = Nav.ConnectivityGraphVersion;
	OutSearch.OverlayRevision = LNPNavOverlay::GetRevision(Overlay);
	const FLNPNavGraph& Graph = Nav.Graph;
	const bool bValid = Hierarchy.SnapshotGeneration == Nav.SnapshotGeneration && Hierarchy.ConnectivityGraphVersion == Nav.ConnectivityGraphVersion
		&& Hierarchy.OverlayRevision == OutSearch.OverlayRevision
		&& Hierarchy.Clusters.IsValidIndex(StartLink.Cluster) && Hierarchy.Clusters.IsValidIndex(GoalLink.Cluster)
		&& GetCluster(Nav, Hierarchy, StartLink.Node) == StartLink.Cluster && GetCluster(Nav, Hierarchy, GoalLink.Node) == GoalLink.Cluster
		&& StartLink.Costs.Num() == Hierarchy.Clusters[StartLink.Cluster].AbstractCount
		&& GoalLink.Costs.Num() == Hierarchy.Clusters[GoalLink.Cluster].AbstractCount
		&& !HierarchyIsBlocked(Graph, Overlay, StartLink.Node) && !HierarchyIsBlocked(Graph, Overlay, GoalLink.Node);
	if (!bValid)
	{
		FailHierarchySearch(OutSearch, ELNPNavSearchStatus::Invalid, ELNPNavHierarchyFailure::Invalid);
		return OutSearch.Status;
	}
	if (LNPNavGraph::GetGroup(Nav, StartLink.Node) != LNPNavGraph::GetGroup(Nav, GoalLink.Node))
	{
		FailHierarchySearch(OutSearch, ELNPNavSearchStatus::Unreachable, ELNPNavHierarchyFailure::None);
		return OutSearch.Status;
	}

	const int32 HighCount = Hierarchy.Abstract.Num() + 1;
	if (Scratch.HighStamp.Num() != HighCount || Scratch.ClusterStamp.Num() != Hierarchy.Clusters.Num())
	{
		Scratch.HighG.SetNumUninitialized(HighCount);
		Scratch.HighParent.SetNumUninitialized(HighCount);
		Scratch.HighStamp.Init(0, HighCount);
		Scratch.ClusterStamp.Init(0, Hierarchy.Clusters.Num());
		Scratch.CurrentStamp = 0;
	}
	if (++Scratch.CurrentStamp == 0)
	{
		FMemory::Memzero(Scratch.HighStamp.GetData(), Scratch.HighStamp.Num() * sizeof(uint32));
		FMemory::Memzero(Scratch.ClusterStamp.GetData(), Scratch.ClusterStamp.Num() * sizeof(uint32));
		Scratch.CurrentStamp = 1;
	}
	Scratch.HighOpen.Reset();
	OutSearch.Stamp = Scratch.CurrentStamp;
	OutSearch.GoalPoint = Graph.GetWorldPoint(Graph.GetSlot(GoalLink.Node), GoalLink.Node);
	OutSearch.GoalCosts = GoalLink.Costs;

	const FLNPNavHierarchyCluster& StartCluster = Hierarchy.Clusters[StartLink.Cluster];
	for (int32 Index = 0; Index < StartCluster.AbstractCount; ++Index)
	{
		const int32 Abstract = StartCluster.AbstractBegin + Index;
		if (StartLink.Costs[Index] < MAX_flt && !LNPNavOverlay::IsBlocked(Overlay, Hierarchy.Abstract[Abstract]))
		{
			RelaxHierarchyHigh(Nav, Hierarchy, Scratch, OutSearch, Abstract, INDEX_NONE, StartLink.Costs[Index]);
		}
	}
	if (StartLink.Cluster == GoalLink.Cluster)
	{
		// 같은 Cluster 직접 비용. exact는 Cluster 안 최단, chord는 하한이다.
		const float Direct = StartLink.bExact ? StartLink.TargetCost : static_cast<float>(FVector3d::Dist(
			Graph.GetWorldPoint(Graph.GetSlot(StartLink.Node), StartLink.Node), OutSearch.GoalPoint));
		if (Direct < MAX_flt)
		{
			RelaxHierarchyHigh(Nav, Hierarchy, Scratch, OutSearch, Hierarchy.Abstract.Num(), INDEX_NONE, Direct);
		}
	}
	OutSearch.Status = ELNPNavSearchStatus::Running;
	OutSearch.Phase = ELNPNavHierarchyPhase::HighLevel;
	return OutSearch.Status;
}

ELNPNavSearchStatus LNPNavHierarchy::StepQuery(const FLNPNavSnapshot& Nav, const FLNPNavHierarchy& Hierarchy,
	FLNPNavHierarchyScratch& Scratch, FLNPNavHierarchySearch& Search, const int32 Budget, const FLNPNavOverlay* Overlay)
{
	if (Search.Status != ELNPNavSearchStatus::Running)
	{
		return Search.Status;
	}
	if (Nav.SnapshotGeneration != Search.SnapshotGeneration || Nav.ConnectivityGraphVersion != Search.ConnectivityGraphVersion
		|| LNPNavOverlay::GetRevision(Overlay) != Search.OverlayRevision || Hierarchy.OverlayRevision != Search.OverlayRevision
		|| Scratch.CurrentStamp != Search.Stamp || ((Search.Phase == ELNPNavHierarchyPhase::Corridor || Search.Phase == ELNPNavHierarchyPhase::Segment)
			&& Scratch.Low.CurrentStamp != Search.LowStamp))
	{
		FailHierarchySearch(Search, ELNPNavSearchStatus::Invalid, ELNPNavHierarchyFailure::Invalid);
		return Search.Status;
	}
	const FLNPNavGraph& Graph = Nav.Graph;
	const double WalkableMinDot = Graph.SlotGraphs[0]->Agent.WalkableMinDot;
	const int32 VirtualGoal = Hierarchy.Abstract.Num();
	const int32 HighCost = FMath::Max(1, Search.Params.HighLevelExpansionCost);
	int32 Remaining = Budget;
	while (Remaining > 0 && Search.Phase == ELNPNavHierarchyPhase::HighLevel)
	{
		if (Scratch.HighOpen.IsEmpty())
		{
			FailHierarchySearch(Search, ELNPNavSearchStatus::NoPath, ELNPNavHierarchyFailure::HighLevel);
			return Search.Status;
		}
		const FLNPNavOpenEntry Top = Scratch.HighOpen.HeapTop();
		if (Top.G > Scratch.HighG[Top.Node])
		{
			Scratch.HighOpen.HeapPopDiscard(FHierarchyAStarOrder(), EAllowShrinking::No);
			continue;
		}
		if (Top.Node == VirtualGoal)
		{
			Scratch.HighOpen.HeapPopDiscard(FHierarchyAStarOrder(), EAllowShrinking::No);
			Search.HighLevelCost = Top.G;
			if (Search.Params.Refine == ELNPNavHierarchyRefine::Segment)
			{
				BeginHierarchySegments(Nav, Hierarchy, Scratch, Search);
			}
			else
			{
				BeginHierarchyCorridor(Nav, Hierarchy, Scratch, Search);
			}
			break;
		}
		// 고수준 확장 하나는 HighCost 단위다. 예산이 모자라면 모아 두고 다음 호출에서 확장한다(확장 순서는 그대로).
		if (Remaining + Search.HighLevelCredit < HighCost)
		{
			Search.HighLevelCredit += Remaining;
			Search.Units += Remaining;
			Remaining = 0;
			break;
		}
		const int32 Charge = HighCost - Search.HighLevelCredit;
		Search.HighLevelCredit = 0;
		Remaining -= Charge;
		Search.Units += Charge;
		++Search.HighLevelExpansions;
		Scratch.HighOpen.HeapPopDiscard(FHierarchyAStarOrder(), EAllowShrinking::No);

		const int32 Node = Top.Node;
		const int32 ClusterIndex = Hierarchy.AbstractCluster[Node];
		const FLNPNavHierarchyCluster& Cluster = Hierarchy.Clusters[ClusterIndex];
		const int32 Local = Node - Cluster.AbstractBegin;
		const float* Row = Hierarchy.IntraCosts.GetData() + Cluster.CostBegin + Local * Cluster.AbstractCount;
		for (int32 To = 0; To < Cluster.AbstractCount; ++To)
		{
			if (To != Local && Row[To] < MAX_flt && !LNPNavOverlay::IsBlocked(Overlay, Hierarchy.Abstract[Cluster.AbstractBegin + To]))
			{
				RelaxHierarchyHigh(Nav, Hierarchy, Scratch, Search, Cluster.AbstractBegin + To, Node, Top.G + Row[To]);
			}
		}
		for (int32 Edge = Hierarchy.InterBegin[Node]; Edge < Hierarchy.InterBegin[Node + 1]; ++Edge)
		{
			const FLNPNavHierarchyEdge& Inter = Hierarchy.InterEdges[Edge];
			if (!LNPNavOverlay::IsBlocked(Overlay, Hierarchy.Abstract[Inter.To]))
			{
				RelaxHierarchyHigh(Nav, Hierarchy, Scratch, Search, Inter.To, Node, Top.G + Inter.Cost);
			}
		}
		if (ClusterIndex == Search.GoalCluster && Search.GoalCosts[Local] < MAX_flt)
		{
			RelaxHierarchyHigh(Nav, Hierarchy, Scratch, Search, VirtualGoal, Node, Top.G + Search.GoalCosts[Local]);
		}
	}

	FLNPNavSearchScratch& Low = Scratch.Low;
	TArray<int32, TInlineAllocator<64>> SegmentNodes;
	while (Remaining > 0 && (Search.Phase == ELNPNavHierarchyPhase::Corridor || Search.Phase == ELNPNavHierarchyPhase::Segment))
	{
		const bool bSegment = Search.Phase == ELNPNavHierarchyPhase::Segment;
		if (Low.Open.IsEmpty())
		{
			if (bSegment)
			{
				// chord 시작 link가 Cluster 안에서 못 가는 abstract node를 골랐거나 Cluster 안 우회가 없다. 같은 고수준 경로의 corridor로 다시 푼다.
				Search.bRefineFallback = true;
				BeginHierarchyCorridor(Nav, Hierarchy, Scratch, Search);
				continue;
			}
			FailHierarchySearch(Search, ELNPNavSearchStatus::NoPath, ELNPNavHierarchyFailure::Corridor);
			return Search.Status;
		}
		FLNPNavOpenEntry Entry;
		Low.Open.HeapPop(Entry, FHierarchyAStarOrder(), EAllowShrinking::No);
		if (Entry.G > Low.G[Entry.Node])
		{
			continue;
		}
		if (Entry.Node == Search.LowTarget)
		{
			if (!bSegment)
			{
				Search.PathCost = Entry.G;
				Search.Status = ELNPNavSearchStatus::Found;
				Search.Phase = ELNPNavHierarchyPhase::Done;
				return Search.Status;
			}
			SegmentNodes.Reset();
			for (int32 Node = Entry.Node; Low.Parent[Node] != INDEX_NONE; Node = Low.Parent[Node])
			{
				SegmentNodes.Add(Node);
			}
			for (int32 Index = SegmentNodes.Num() - 1; Index >= 0; --Index)
			{
				Scratch.RefinedPath.Add(SegmentNodes[Index]);
			}
			Search.SegmentCost += Entry.G;
			++Search.SegmentIndex;
			AdvanceHierarchySegments(Nav, Hierarchy, Scratch, Search);
			if (Search.Status != ELNPNavSearchStatus::Running)
			{
				return Search.Status;
			}
			continue;
		}
		if (Search.CorridorExpansions >= Search.Params.MaxCorridorExpansions)
		{
			FailHierarchySearch(Search, ELNPNavSearchStatus::NoPath, ELNPNavHierarchyFailure::Corridor);
			return Search.Status;
		}
		++Search.CorridorExpansions;
		++Search.Units;
		--Remaining;
		const uint32 Stamp = Search.LowStamp;
		const int32 Slot = Graph.GetSlot(Entry.Node);
		const FVector3d Point = Graph.GetWorldPoint(Slot, Entry.Node);
		const int32 SlotBegin = Graph.SlotNodeBase[Slot];
		const int32 SlotEnd = Graph.SlotNodeBase[Slot + 1];
		Graph.ForEachNeighbor(Slot, Entry.Node, [&](const int32 To)
		{
			const int32 ToSlot = To >= SlotBegin && To < SlotEnd ? Slot : Graph.GetSlot(To);
			const int32 ToCluster = HierarchyClusterInSlot(Nav, Hierarchy, ToSlot, To);
			if (bSegment ? ToCluster != Search.SegmentCluster : Scratch.ClusterStamp[ToCluster] != Search.Stamp)
			{
				return;
			}
			const FVector3d ToPoint = Graph.GetWorldPoint(ToSlot, To);
			const float NewG = Entry.G + static_cast<float>(LNPNavGraph::ComputeEdgeCost(Point, ToPoint, WalkableMinDot));
			if (Low.Stamp[To] == Stamp && NewG >= Low.G[To])
			{
				return;
			}
			Low.Stamp[To] = Stamp;
			Low.G[To] = NewG;
			Low.Parent[To] = Entry.Node;
			Low.Open.HeapPush({NewG + static_cast<float>(FVector3d::Dist(ToPoint, Search.LowTargetPoint)), NewG, To}, FHierarchyAStarOrder());
		}, Overlay);
	}
	return Search.Status;
}

bool LNPNavHierarchy::ExtractNodePath(const FLNPNavHierarchyScratch& Scratch, const FLNPNavHierarchySearch& Search, TArray<int32>& OutNodes)
{
	OutNodes.Reset();
	if (Search.Status != ELNPNavSearchStatus::Found)
	{
		return false;
	}
	if (Search.Params.Refine == ELNPNavHierarchyRefine::Segment && !Search.bRefineFallback)
	{
		OutNodes = Scratch.RefinedPath;
		return !OutNodes.IsEmpty() && OutNodes[0] == Search.Start && OutNodes.Last() == Search.Goal;
	}
	if (Scratch.Low.CurrentStamp != Search.LowStamp)
	{
		return false;
	}
	for (int32 Node = Search.Goal; Node != INDEX_NONE; Node = Scratch.Low.Parent[Node])
	{
		OutNodes.Add(Node);
		if (OutNodes.Num() > Scratch.Low.Parent.Num())
		{
			OutNodes.Reset();
			return false;
		}
	}
	Algo::Reverse(OutNodes);
	return OutNodes[0] == Search.Start;
}
