// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Algo/BinarySearch.h"
#include "SurfaceNavigation/LNPNavData.h"
#include "SurfaceNavigation/LNPSurfaceTypes.h"

struct FLNPNavSnapshot;
struct FLNPSupportAtlas;

/**
 * A* 확장 루프용 조밀 node 하나. 같은 SurfaceData asset을 쓰는 slot이 공유하므로 좌표는 slot local이다.
 * 위치·법선을 codec에 저장하지 않는 대신 load 때 Support Layer 보간으로 한 번 복원한다(`phases/Phase07b_PathExecution.md` §3.1).
 */
struct LOOTNPOP_API FLNPNavGraphNode
{
	/** slot local 지면점. */
	FVector3f LocalPoint = FVector3f::ZeroVector;
	/** 같은 Layer 6방향(ELNPNavNeighbor 순) 이웃의 asset-local 조밀 index. edge가 없으면 INDEX_NONE. */
	int32 Neighbors[6] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};
	uint16 I = 0;
	uint16 J = 0;
	uint16 LocalComponent = MAX_uint16;
	uint16 TileId = MAX_uint16;
	uint8 LocalCellIndex = 0;
	uint8 LayerOrdinal = 0;
	/** 열린 같은 Layer edge 수. 6이면 내부 node다(D-063 접근점 후보). */
	uint8 GridDegree = 0;
};

struct LOOTNPOP_API FLNPNavGraphLayer
{
	uint16 LocalNavLayerId = MAX_uint16;
	int32 Subdivisions = 0;
	/** 대응 Support Layer 기준 반지름. 투영 창 크기 계산에 쓴다. */
	double BaseRadius = 0.0;
	int32 NodeBegin = 0;
	int32 NodeEnd = 0;
	/** 한 행의 Tile 수(`Subdivisions / 16 + 1`). */
	int32 TilesPerRow = 0;
	/** `TileY * TilesPerRow + TileX` → TileId. 없으면 INDEX_NONE. */
	TArray<int32> TileIdByXY;
	/** `TileId * 256 + LocalCellIndex` → asset-local 조밀 index. 없으면 INDEX_NONE. */
	TArray<int32> NodeByTileCell;
	/** 이 Layer node가 속한 asset-local component. 오름차순, 중복 없음. */
	TArray<uint16> LocalComponents;
};

/** Layer 사이 정적 portal 방향 edge 하나(asset-local). */
struct LOOTNPOP_API FLNPNavGraphPortalLink
{
	int32 From = INDEX_NONE;
	int32 To = INDEX_NONE;
};

/** 한 SurfaceData asset의 조밀 graph. 같은 asset을 쓰는 slot이 한 벌을 공유한다. */
struct LOOTNPOP_API FLNPNavAssetGraph
{
	FLNPNavAgentProfile Agent;
	TArray<FLNPNavGraphLayer> Layers;
	TArray<FLNPNavGraphNode> Nodes;
	/** From 오름차순. */
	TArray<FLNPNavGraphPortalLink> PortalLinks;

	uint64 GetAllocatedBytes() const;
};

/** slot 사이 seam link와 portal의 전역 방향 edge. */
struct LOOTNPOP_API FLNPNavGraphLink
{
	int32 From = INDEX_NONE;
	int32 To = INDEX_NONE;
};

/**
 * 8-slot 전역 조밀 graph. 전역 index는 `SlotNodeBase[Slot] + asset-local index`다.
 * slot 사이 이동은 ExtraLinks(seam link·portal)로만 한다. D-060으로 막힌 이음매 node·edge는 여기서 제외한다.
 */
struct LOOTNPOP_API FLNPNavGraph
{
	TArray<TSharedPtr<const FLNPNavAssetGraph, ESPMode::ThreadSafe>> SlotGraphs;
	TArray<FQuat4d> SlotRotations;
	/** 원소 9개. 마지막 값이 전체 node 수다. */
	TArray<int32> SlotNodeBase;
	/** From 오름차순. HasExtraLinks가 선 node만 찾는다. */
	TArray<FLNPNavGraphLink> ExtraLinks;
	TBitArray<> HasExtraLinks;
	/** D-060 막힌 이음매 node 사본. */
	TBitArray<> BlockedNodes;
	/** `(From << 32) | To` 오름차순. HasBlockedEdge가 선 node만 찾는다. */
	TArray<uint64> BlockedEdges;
	TBitArray<> HasBlockedEdge;

	bool IsValid() const { return SlotNodeBase.Num() == 9 && SlotGraphs.Num() == 8; }
	int32 GetNodeCount() const { return SlotNodeBase.IsEmpty() ? 0 : SlotNodeBase.Last(); }
	uint64 GetAllocatedBytes() const;

	/** 전역 index의 slot. 범위 밖이면 INDEX_NONE. */
	int32 GetSlot(const int32 Node) const
	{
		if (Node < 0 || Node >= GetNodeCount())
		{
			return INDEX_NONE;
		}
		int32 Slot = 0;
		while (Node >= SlotNodeBase[Slot + 1])
		{
			++Slot;
		}
		return Slot;
	}

	const FLNPNavGraphNode& GetNode(const int32 Slot, const int32 Node) const
	{
		return SlotGraphs[Slot]->Nodes[Node - SlotNodeBase[Slot]];
	}

	FVector3d GetWorldPoint(const int32 Slot, const int32 Node) const
	{
		return SlotRotations[Slot].RotateVector(FVector3d(GetNode(Slot, Node).LocalPoint));
	}

	bool IsEdgeBlocked(const int32 From, const int32 To) const;

	/**
	 * From의 모든 통과 가능한 이웃을 Visitor(To)로 넘긴다. 같은 Layer grid edge 다음에 seam link·portal 순이다.
	 * 막힌 이음매 node·edge는 넘기지 않는다. 순서가 결정론적이어야 A* 결과가 같다.
	 */
	template <typename FVisitor>
	void ForEachNeighbor(const int32 Slot, const int32 From, FVisitor&& Visitor) const
	{
		const int32 Base = SlotNodeBase[Slot];
		const FLNPNavGraphNode& Node = SlotGraphs[Slot]->Nodes[From - Base];
		const bool bCheckEdges = HasBlockedEdge[From];
		for (const int32 LocalNeighbor : Node.Neighbors)
		{
			if (LocalNeighbor == INDEX_NONE)
			{
				continue;
			}
			const int32 To = Base + LocalNeighbor;
			if (!BlockedNodes[To] && !(bCheckEdges && IsEdgeBlocked(From, To)))
			{
				Visitor(To);
			}
		}
		if (HasExtraLinks[From])
		{
			int32 Index = Algo::LowerBoundBy(ExtraLinks, From, &FLNPNavGraphLink::From);
			for (; Index < ExtraLinks.Num() && ExtraLinks[Index].From == From; ++Index)
			{
				if (!BlockedNodes[ExtraLinks[Index].To])
				{
					Visitor(ExtraLinks[Index].To);
				}
			}
		}
	}
};

/** 투영 창 안에서 찾은 node 하나. */
struct LOOTNPOP_API FLNPNavGraphCandidate
{
	int32 Node = INDEX_NONE;
	uint32 Group = MAX_uint32;
	double Distance = 0.0;
};

enum class ELNPNavEndpointStatus : uint8
{
	/** 시작·목표가 같은 ReachabilityGroup이다. A*를 돌릴 수 있다. */
	Reachable,
	/** 스냅 반경 안에 공통 group 짝이 없다. ApproachNode가 있을 수 있다(D-063). */
	Unreachable,
	/** 시작 또는 목표 반경 안에 walkable node가 없다. 목표 쪽만 없으면 ApproachNode가 있을 수 있다. */
	NoNode,
	/** handle generation이 현재 snapshot과 다르다. */
	Stale,
};

struct LOOTNPOP_API FLNPNavEndpointQuery
{
	FVector3d StartPosition = FVector3d::ZeroVector;
	/** 시작 개체의 현재 Support handle(slot·Layer). */
	const FLNPSurfaceHandle* StartSurface = nullptr;
	FVector3d GoalPosition = FVector3d::ZeroVector;
	const FLNPSurfaceHandle* GoalSurface = nullptr;
	double SnapRadius = 300.0;
	/** D-063 접근점 탐색 반경. 0이면 접근점을 찾지 않는다. */
	double ApproachRadius = 3000.0;
};

struct LOOTNPOP_API FLNPNavEndpoints
{
	ELNPNavEndpointStatus Status = ELNPNavEndpointStatus::NoNode;
	int32 StartNode = INDEX_NONE;
	/** Reachable일 때의 목표 node. */
	int32 GoalNode = INDEX_NONE;
	/** Unreachable·NoNode일 때 시작 group 안에서 목표에 가장 가까운 내부 node(D-063). 경로 목표는 이 node다. */
	int32 ApproachNode = INDEX_NONE;
	double StartDistance = 0.0;
	double GoalDistance = 0.0;
	/** 가장 가까운 node끼리 group이 달라 주변 공통 group 짝으로 다시 골랐다(D-062). */
	bool bReselected = false;
};

namespace LNPNavGraph
{
	/** 경사 비용 배율의 최대값(walkable 한계 경사에서). 배율은 항상 1 이상이라 chord 휴리스틱이 하한이다(D-040). */
	constexpr double MaxSlopeCostMultiplier = 1.5;

	/** decoded payload와 Support Atlas에서 asset 단위 조밀 graph를 만든다. 베이커가 만든 node의 Support 조회가 실패하면 오류다. */
	LOOTNPOP_API bool BuildAssetGraph(
		const FLNPSupportAtlas& Support, const FLNPNavData& Navigation, const FLNPNavTraversalData& Traversal,
		FLNPNavAssetGraph& OutGraph, FString& OutError);

	/** 7a 조립이 끝난 Nav snapshot의 seam link·막힘 목록으로 전역 graph를 만든다. */
	LOOTNPOP_API bool BuildRuntimeGraph(
		TConstArrayView<TSharedPtr<const FLNPNavAssetGraph, ESPMode::ThreadSafe>> SlotGraphs,
		TConstArrayView<FRotator> SlotRotations, const FLNPNavSnapshot& Nav, FLNPNavGraph& OutGraph, FString& OutError);

	/** stale·범위 밖이면 INDEX_NONE. */
	LOOTNPOP_API int32 ToGraphNode(const FLNPNavSnapshot& Nav, const FLNPNavNodeRef& Node);
	LOOTNPOP_API bool ToNodeRef(const FLNPNavSnapshot& Nav, int32 GraphNode, FLNPNavNodeRef& OutNode);

	/** 전역 node의 ReachabilityGroup. 범위 밖이면 MAX_uint32. */
	LOOTNPOP_API uint32 GetGroup(const FLNPNavSnapshot& Nav, int32 GraphNode);

	/** 인접 두 node 사이 edge cost. 3D chord × 경사 배율(1~MaxSlopeCostMultiplier). 중력 원점은 월드 원점이다. */
	LOOTNPOP_API double ComputeEdgeCost(const FVector3d& From, const FVector3d& To, double WalkableMinDot);

	/**
	 * 한 slot·Layer에서 WorldPosition 3D 거리 Radius 안의 walkable node를 거리 오름차순(같으면 index 오름차순)으로 모은다.
	 * 막힌 이음매 node는 제외한다. 조밀 view의 격자 좌표 창 조회라 Tile 선형 탐색을 하지 않는다.
	 */
	LOOTNPOP_API void CollectNodesNear(
		const FLNPNavSnapshot& Nav, int32 Slot, uint16 LocalNavLayerId, const FVector3d& WorldPosition, double Radius,
		TArray<FLNPNavGraphCandidate>& OutCandidates);

	/**
	 * D-062 시작·목표 스냅과 D-063 접근점.
	 * 1) 각자 가장 가까운 node가 같은 group이면 그대로 쓴다.
	 * 2) 다르면 양쪽 SnapRadius 안에서 공통 group 짝을 스냅 거리 합 최소로 다시 고른다.
	 * 3) 없으면 A* 없이 Unreachable이고, 시작 후보 group들의 내부 node 중 목표에 가장 가까운 것을 ApproachRadius 안에서 찾는다.
	 */
	LOOTNPOP_API FLNPNavEndpoints ResolveEndpoints(const FLNPNavSnapshot& Nav, const FLNPNavEndpointQuery& Query);

	/**
	 * Nav 직선 보행 검사. 같은 slot·Layer의 두 node 사이 대원호를 격자 간격의 1/4로 샘플링해,
	 * 연속 샘플이 같은 node이거나 통과 가능한 grid edge로 이어진 이웃인지 본다. slot·Layer가 다르면 false다.
	 */
	LOOTNPOP_API bool IsDirectWalkable(const FLNPNavSnapshot& Nav, int32 From, int32 To);

	/** IsDirectWalkable과 같은 검사이며, 통과한 cell의 전역 index를 From부터 순서대로(중복 없이) 모은다. 경로 Tile fingerprint에 쓴다. */
	LOOTNPOP_API bool CollectDirectWalkNodes(const FLNPNavSnapshot& Nav, int32 From, int32 To, TArray<int32>& OutNodes);

	/** node가 속한 Tile의 전역 key `(RuntimeNavLayerId << 16) | TileId`. overlay Tile revision의 주소다. 범위 밖이면 MAX_uint32. */
	LOOTNPOP_API uint32 GetTileKey(const FLNPNavSnapshot& Nav, int32 GraphNode);
}
