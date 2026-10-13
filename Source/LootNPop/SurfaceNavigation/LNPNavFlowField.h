// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPNavPathfinding.h"

struct FLNPNavSnapshot;
struct FLNPNavOverlay;

/**
 * 목표별 flow field 프로토타입(Phase 10 단위 3). processor·scheduler에 연결하지 않은 순수 함수이며 재생기와 자동화만 쓴다.
 * goal node에서 Dijkstra로 거리장을 펴고, 각 node에 목표 쪽 다음 node를 기록한다. 조회는 그 node 열을 따라간다.
 * 역방향으로 펴므로 Nav edge가 양방향으로 있어야 한다. edge 비용은 보행 방향(이웃 → 확정 node)으로 계산한다.
 */
enum class ELNPNavFlowFieldStatus : uint8
{
	/** 입력 node가 없거나 막혔거나, 짓는 중 snapshot·overlay가 바뀌었다. */
	Invalid,
	/** 다음 StepBuild에서 이어 짓는다. 이미 확정된 node는 조회할 수 있다. */
	Building,
	/** 반경·확정 상한에 닿았거나 open이 비었다. 확정되지 않은 node는 field 밖이다. */
	Complete,
};

/** Tile 하나(최대 256 cell)의 field 값. 조회 key는 overlay Tile revision과 같은 `LNPNavGraph::GetTileKey`다. */
struct LOOTNPOP_API FLNPNavFlowFieldBlock
{
	/** goal까지의 경로 cost. 아직 닿지 않은 cell은 MAX_flt다. */
	float Distance[256];
	/** 목표 쪽 다음 전역 node. goal과 미방문 cell은 INDEX_NONE이다. */
	int32 Next[256];
	/** 확정된 cell bit. 확정된 cell의 Distance·Next는 더 바뀌지 않는다. */
	uint64 Settled[4];
};

struct LOOTNPOP_API FLNPNavFlowFieldParams
{
	/** field 반경(경로 cost, cm). 이보다 먼 node는 확정하지 않고 A* 폴백에 맡긴다. */
	double MaxCost = 12000.0;
	/** 확정 node 수 안전 상한. */
	int32 MaxSettled = 100000;
};

struct LOOTNPOP_API FLNPNavFlowField
{
	ELNPNavFlowFieldStatus Status = ELNPNavFlowFieldStatus::Invalid;
	int32 Goal = INDEX_NONE;
	uint64 SnapshotGeneration = 0;
	uint32 ConnectivityGraphVersion = 0;
	uint32 OverlayRevision = 0;
	FLNPNavFlowFieldParams Params;
	/** 확정(갱신)한 node 수. 예산 단위로 A* 확장 수와 같게 센다. */
	int32 Settled = 0;
	TMap<uint32, int32> BlockByTile;
	TArray<FLNPNavFlowFieldBlock> Blocks;
	/** 짓는 동안의 Dijkstra open. F는 쓰지 않는다. Complete가 되면 비운다. */
	TArray<FLNPNavOpenEntry> Open;

	uint64 GetAllocatedBytes() const
	{
		return BlockByTile.GetAllocatedSize() + Blocks.GetAllocatedSize() + Open.GetAllocatedSize();
	}
};

namespace LNPNavFlowField
{
	/** goal node를 open에 넣는다. goal이 없거나 막혔으면 Invalid다. */
	LOOTNPOP_API ELNPNavFlowFieldStatus BeginBuild(const FLNPNavSnapshot& Nav, int32 Goal, const FLNPNavFlowFieldParams& Params,
		FLNPNavFlowField& OutField, const FLNPNavOverlay* Overlay = nullptr);

	/**
	 * 최대 Budget개 node를 확정한다. 확정 순서는 (거리, node index) 순이라 예산 분할과 무관하게 결과가 같다.
	 * snapshot generation·ConnectivityGraphVersion·overlay revision이 시작 때와 다르면 Invalid로 끝낸다.
	 */
	LOOTNPOP_API ELNPNavFlowFieldStatus StepBuild(const FLNPNavSnapshot& Nav, FLNPNavFlowField& Field, int32 Budget,
		const FLNPNavOverlay* Overlay = nullptr);

	/** Node가 확정됐으면 goal까지의 cost를 돌려준다. 짓는 중에도 확정된 node는 최종값이다. */
	LOOTNPOP_API bool GetSettledDistance(const FLNPNavSnapshot& Nav, const FLNPNavFlowField& Field, int32 Node, float& OutDistance);

	/** 확정된 Start에서 goal까지 다음 node를 따라간 전역 node 열(Start → goal). 확정되지 않았으면 false다. */
	LOOTNPOP_API bool ExtractNodePath(const FLNPNavSnapshot& Nav, const FLNPNavFlowField& Field, int32 Start, TArray<int32>& OutNodes);
}
