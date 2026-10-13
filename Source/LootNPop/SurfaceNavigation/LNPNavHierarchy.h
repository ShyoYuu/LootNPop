// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPNavPathfinding.h"

struct FLNPNavSnapshot;
struct FLNPNavOverlay;

/**
 * 계층형 A* 프로토타입(Phase 10 단위 4). processor·scheduler에 연결하지 않은 순수 함수이며 재생기와 자동화만 쓴다.
 * - Cluster는 slot·Layer별 16×16 Tile 하나다. key가 overlay Tile revision 주소(`LNPNavGraph::GetTileKey`)와 같다.
 * - Cluster 경계를 넘는 edge(grid·seam link·Layer portal 모두)를 Cluster 쌍별 연속 span으로 묶고, span마다 가운데 edge 하나를 Portal로 둔다.
 *   Portal 양 끝 node가 abstract node이고, 같은 Cluster abstract node 사이 비용은 Cluster 안으로 제한한 Dijkstra로 미리 계산한다.
 * - query는 고수준 A*(abstract graph) → 정제(기본 HPA* segment: Portal 사이 Cluster 안 A*, 또는 고수준 경로 Cluster corridor 안 A*) 순서다.
 * - overlay가 바뀐 Tile의 Cluster만 dirty로 다시 계산한다. Portal 위치는 바꾸지 않으므로 대표 node가 막힌 Portal은 닫힌다.
 * edge 비용은 보행 방향으로 재며 Nav edge가 양방향이라는 전제(`Nav.FlowField`가 검사)를 쓴다. 목표 link는 목표에서 역으로 편다.
 */
struct LOOTNPOP_API FLNPNavHierarchyParams
{
	/** span 하나가 Portal 하나로 압축되는 최대 경계 node 수(낮은 Cluster 쪽). 길면 잘라 Portal을 여럿 둔다. */
	int32 MaxSpanLength = 8;
	/** Cluster 행렬을 worker에서 병렬로 계산한다. 결과는 병렬 여부와 무관하다. */
	bool bParallel = true;
};

struct LOOTNPOP_API FLNPNavHierarchyCluster
{
	/** Abstract 배열에서 이 Cluster abstract node의 시작 index와 수. node index 오름차순이다. */
	int32 AbstractBegin = 0;
	int32 AbstractCount = 0;
	/** IntraCosts에서 이 Cluster K×K 행렬(행 = 출발, 열 = 도착)의 시작 index. */
	int32 CostBegin = 0;
	/** Cluster의 Nav node 수(≤256). 지역 Dijkstra 확정 수의 상한이다. */
	int32 NodeCount = 0;
	uint32 TileKey = MAX_uint32;
	/** 행렬을 계산할 때의 overlay Tile revision. */
	uint32 OverlayTileRevision = 0;
};

struct LOOTNPOP_API FLNPNavHierarchyEdge
{
	int32 To = INDEX_NONE;
	float Cost = 0.0f;
};

/** 빌드·dirty 재계산 통계. 확정 수는 A* 확장과 같은 예산 단위다. */
struct LOOTNPOP_API FLNPNavHierarchyBuildStats
{
	int32 Clusters = 0;
	int32 CrossingEdges = 0;
	int32 Spans = 0;
	int32 Portals = 0;
	int64 Settled = 0;
	double Seconds = 0.0;
};

struct LOOTNPOP_API FLNPNavHierarchyUpdateStats
{
	/** 다시 계산한 Cluster 수. */
	int32 DirtyClusters = 0;
	/** 그 Cluster들의 abstract node 수(= 지역 Dijkstra 수). */
	int32 DirtyAbstractNodes = 0;
	int64 Settled = 0;
	double Seconds = 0.0;
};

struct LOOTNPOP_API FLNPNavHierarchy
{
	uint64 SnapshotGeneration = 0;
	uint32 ConnectivityGraphVersion = 0;
	/** 마지막으로 반영한 overlay 전역 revision. query는 이 값이 현재 overlay와 같을 때만 시작한다. */
	uint32 OverlayRevision = 0;
	FLNPNavHierarchyParams Params;
	/** runtime Nav Layer ID → 그 Layer 첫 Cluster index(+ TileId). Layer가 없으면 INDEX_NONE. */
	TArray<int32> ClusterBaseByRuntimeLayer;
	TArray<FLNPNavHierarchyCluster> Clusters;
	/** abstract node의 전역 Nav node. Cluster별로 모여 있다. */
	TArray<int32> Abstract;
	/** abstract node의 Cluster index. */
	TArray<int32> AbstractCluster;
	/** abstract node별 Portal edge(CSR). InterBegin은 원소 Abstract.Num() + 1개다. */
	TArray<int32> InterBegin;
	TArray<FLNPNavHierarchyEdge> InterEdges;
	/** Cluster별 K×K 내부 비용. Cluster 안으로 갈 수 없으면 MAX_flt다. */
	TArray<float> IntraCosts;
	FLNPNavHierarchyBuildStats BuildStats;

	bool IsValid() const { return SnapshotGeneration != 0 && !Clusters.IsEmpty(); }
	uint64 GetAllocatedBytes() const;
};

/** link 방향. 비용은 개체가 걷는 방향으로 잰다. */
enum class ELNPNavHierarchyLinkSide : uint8
{
	/** 시작 node → abstract node. */
	Start,
	/** abstract node → 목표 node. 목표에서 역으로 편다. */
	Goal,
};

/** 시작 또는 목표 node와 그 Cluster abstract node 사이 비용. */
struct LOOTNPOP_API FLNPNavHierarchyLink
{
	int32 Node = INDEX_NONE;
	int32 Cluster = INDEX_NONE;
	/** Cluster abstract node 순서. 시작 link는 node → abstract, 목표 link는 abstract → node 비용이다. 못 가면 MAX_flt. */
	TArray<float> Costs;
	/** exact link를 만들 때 넘긴 같은 Cluster 대상 node까지의 비용. 없거나 못 가면 MAX_flt. */
	float TargetCost = MAX_flt;
	bool bExact = false;
};

enum class ELNPNavHierarchyFailure : uint8
{
	None,
	/** 입력이 없거나 막혔거나, 계층이 snapshot·overlay와 다르거나, 실행 중 바뀌었다. */
	Invalid,
	/** 고수준 A*의 open이 비었다. */
	HighLevel,
	/** 저수준 정제(corridor A*, 또는 segment 실패 뒤의 corridor 복구)의 open이 비었거나 확장 상한을 넘었다. */
	Corridor,
};

/** 고수준 경로를 저수준 node 열로 바꾸는 방법. */
enum class ELNPNavHierarchyRefine : uint8
{
	/** 고수준 경로가 지나는 Cluster(+ 고리) 안 저수준 A* 하나. Cluster 안 최적이다. */
	Corridor,
	/**
	 * HPA* 표준 정제. 고수준 경로의 abstract node를 차례로 잇는다. 같은 Cluster 구간은 그 Cluster 안 A*, Portal 구간은 edge 하나다.
	 * Portal 대표 node를 반드시 지나므로 Cluster 안 최적보다 길 수 있고, 구간 A*가 실패하면(chord 시작 link가 Cluster 안에서 못 가는 node를 골랐을 때) corridor로 다시 푼다.
	 */
	Segment,
};

struct LOOTNPOP_API FLNPNavHierarchyQueryParams
{
	ELNPNavHierarchyRefine Refine = ELNPNavHierarchyRefine::Segment;
	/** 고수준 확장 하나의 예산 단위. 이웃이 Cluster abstract node 수만큼이라 저수준 확장보다 조금 비싸다(`Nav.Hierarchy` 실측 최대 1.43배). */
	int32 HighLevelExpansionCost = 1;
	/** 0이면 고수준 경로가 지나는 Cluster만, 1이면 그 Cluster와 Portal로 이웃한 Cluster까지 corridor다. */
	int32 CorridorRing = 1;
	/** 저수준 정제(corridor 또는 구간 합) 확장 상한. */
	int32 MaxCorridorExpansions = 30000;
};

/** query 하나의 작업 메모리. 고수준 배열은 abstract node 수 + 1(가상 goal), corridor는 전역 node 수 크기다. */
struct LOOTNPOP_API FLNPNavHierarchyScratch
{
	TArray<float> HighG;
	TArray<int32> HighParent;
	TArray<uint32> HighStamp;
	TArray<FLNPNavOpenEntry> HighOpen;
	TArray<uint32> ClusterStamp;
	uint32 CurrentStamp = 0;
	FLNPNavSearchScratch Low;
	/** segment 정제: 시작 → abstract node들 → 목표의 저수준 node 열과, 지금까지 이은 경로. */
	TArray<int32> Waypoints;
	TArray<int32> RefinedPath;

	uint64 GetHighLevelBytes() const
	{
		return HighG.GetAllocatedSize() + HighParent.GetAllocatedSize() + HighStamp.GetAllocatedSize() + HighOpen.GetAllocatedSize()
			+ ClusterStamp.GetAllocatedSize() + Waypoints.GetAllocatedSize() + RefinedPath.GetAllocatedSize();
	}
};

enum class ELNPNavHierarchyPhase : uint8
{
	HighLevel,
	Corridor,
	Segment,
	Done,
};

struct LOOTNPOP_API FLNPNavHierarchySearch
{
	ELNPNavSearchStatus Status = ELNPNavSearchStatus::Invalid;
	ELNPNavHierarchyPhase Phase = ELNPNavHierarchyPhase::Done;
	ELNPNavHierarchyFailure Failure = ELNPNavHierarchyFailure::None;
	int32 Start = INDEX_NONE;
	int32 Goal = INDEX_NONE;
	int32 StartCluster = INDEX_NONE;
	int32 GoalCluster = INDEX_NONE;
	FVector3d GoalPoint = FVector3d::ZeroVector;
	uint64 SnapshotGeneration = 0;
	uint32 ConnectivityGraphVersion = 0;
	uint32 OverlayRevision = 0;
	uint32 Stamp = 0;
	uint32 LowStamp = 0;
	FLNPNavHierarchyQueryParams Params;
	/** 예산이 고수준 확장 하나의 단위보다 작게 들어왔을 때 모아 둔 단위. 다음 확장에 쓴다. */
	int32 HighLevelCredit = 0;
	/** 목표 link 비용(목표 Cluster abstract node 순서). BeginQuery가 복사한다. */
	TArray<float> GoalCosts;
	int32 HighLevelExpansions = 0;
	int32 CorridorExpansions = 0;
	/** 예산 단위 누적(고수준 × 가중치 + corridor). link 확정 수는 link를 만든 쪽이 따로 센다. */
	int32 Units = 0;
	/** 고수준 경로 cost(시작·목표 link 포함)와 corridor Cluster 수. */
	double HighLevelCost = 0.0;
	int32 CorridorClusters = 0;
	/** Found일 때 시작 → 목표 경로 cost. */
	double PathCost = 0.0;
	/** 지금 도는 저수준 A*의 목표 node·위치. corridor는 Goal이고 segment는 다음 abstract node다. */
	int32 LowTarget = INDEX_NONE;
	FVector3d LowTargetPoint = FVector3d::ZeroVector;
	/** segment 정제: 지금 구간의 Waypoints 시작 index와 제한 Cluster, 이은 구간 cost 합, 구간 A* 수. */
	int32 SegmentIndex = 0;
	int32 SegmentCluster = INDEX_NONE;
	double SegmentCost = 0.0;
	int32 SegmentSearches = 0;
	/** segment 구간 A*가 실패해 corridor로 다시 풀었다. */
	bool bRefineFallback = false;
};

namespace LNPNavHierarchy
{
	/** 계층 전체를 만든다. Overlay가 있으면 그 차단을 반영한 행렬로 시작한다. */
	LOOTNPOP_API bool Build(const FLNPNavSnapshot& Nav, const FLNPNavHierarchyParams& Params, FLNPNavHierarchy& OutHierarchy,
		const FLNPNavOverlay* Overlay = nullptr);

	/** node의 Cluster index. 범위 밖이면 INDEX_NONE. */
	LOOTNPOP_API int32 GetCluster(const FLNPNavSnapshot& Nav, const FLNPNavHierarchy& Hierarchy, int32 Node);

	/**
	 * overlay Tile revision이 행렬 계산 때와 다른 Cluster만 다시 계산한다(dirty). 나머지 Cluster와 Portal은 그대로다.
	 * 결과는 같은 overlay로 Build한 계층과 같다.
	 */
	LOOTNPOP_API FLNPNavHierarchyUpdateStats UpdateOverlay(const FLNPNavSnapshot& Nav, FLNPNavHierarchy& Hierarchy,
		const FLNPNavOverlay* Overlay);

	/**
	 * Node와 그 Cluster abstract node 사이 비용. bExact면 Cluster 안 Dijkstra(반환값 = 확정 수, ≤ Cluster NodeCount)이고,
	 * 아니면 같은 ReachabilityGroup abstract node까지의 3D chord(하한, 반환값 0)다. Target이 같은 Cluster면 그 비용도 잰다(exact만).
	 */
	LOOTNPOP_API int32 BuildLink(const FLNPNavSnapshot& Nav, const FLNPNavHierarchy& Hierarchy, int32 Node, ELNPNavHierarchyLinkSide Side,
		bool bExact, FLNPNavHierarchyLink& OutLink, const FLNPNavOverlay* Overlay = nullptr, int32 Target = INDEX_NONE);

	/** 시작 link의 abstract node(와 같은 Cluster면 목표)를 고수준 open에 넣는다. */
	LOOTNPOP_API ELNPNavSearchStatus BeginQuery(const FLNPNavSnapshot& Nav, const FLNPNavHierarchy& Hierarchy,
		const FLNPNavHierarchyLink& StartLink, const FLNPNavHierarchyLink& GoalLink, const FLNPNavHierarchyQueryParams& Params,
		FLNPNavHierarchyScratch& Scratch, FLNPNavHierarchySearch& OutSearch, const FLNPNavOverlay* Overlay = nullptr);

	/**
	 * 최대 Budget 단위만큼 진행한다. 확장 순서는 예산 분할과 무관하게 같다.
	 * snapshot generation·ConnectivityGraphVersion·overlay revision이 시작 때와 다르면 Invalid다.
	 */
	LOOTNPOP_API ELNPNavSearchStatus StepQuery(const FLNPNavSnapshot& Nav, const FLNPNavHierarchy& Hierarchy,
		FLNPNavHierarchyScratch& Scratch, FLNPNavHierarchySearch& Search, int32 Budget, const FLNPNavOverlay* Overlay = nullptr);

	/** Found 결과의 시작 → 목표 전역 node 열. segment 정제는 이은 구간 경로, corridor는 저수준 A* 경로다. */
	LOOTNPOP_API bool ExtractNodePath(const FLNPNavHierarchyScratch& Scratch, const FLNPNavHierarchySearch& Search,
		TArray<int32>& OutNodes);
}
