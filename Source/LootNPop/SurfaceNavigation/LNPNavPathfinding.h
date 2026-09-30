// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FLNPNavSnapshot;

struct FLNPNavOpenEntry
{
	float F = 0.0f;
	float G = 0.0f;
	int32 Node = INDEX_NONE;
};

/**
 * 요청 하나가 쓰는 A* 작업 메모리. 전역 node 수 크기의 g·parent·방문 stamp 배열이며, stamp를 쓰므로 요청마다 비우지 않는다.
 * 한 scratch는 동시에 요청 하나만 맡는다. 다중 프레임 요청은 끝날 때까지 scratch를 점유한다(`phases/Phase07b_PathExecution.md` §3.4).
 */
struct LOOTNPOP_API FLNPNavSearchScratch
{
	TArray<float> G;
	TArray<int32> Parent;
	TArray<uint32> Stamp;
	uint32 CurrentStamp = 0;
	TArray<FLNPNavOpenEntry> Open;

	uint64 GetAllocatedBytes() const
	{
		return G.GetAllocatedSize() + Parent.GetAllocatedSize() + Stamp.GetAllocatedSize() + Open.GetAllocatedSize();
	}
};

enum class ELNPNavSearchStatus : uint8
{
	Running,
	Found,
	/** 같은 group 안에서 open 목록이 비었거나 요청당 확장 상한을 넘었다. */
	NoPath,
	/** 시작·목표 ReachabilityGroup이 다르다. 탐색하지 않았다. */
	Unreachable,
	/** 입력 node가 없거나 막혔거나 snapshot generation이 다르다. */
	Invalid,
};

struct LOOTNPOP_API FLNPNavSearchParams
{
	/** 요청당 누적 확장 상한. 7a가 알려 둔 group 과대 추정(막힌 이음매가 유일한 다리)의 최악 탐색을 끊는다. */
	int32 MaxExpansions = 30000;
	/** false면 휴리스틱 0(Dijkstra)이다. 최적성 검증과 하한을 증명할 수 없는 link용이다(D-040). */
	bool bUseHeuristic = true;
};

/** 여러 프레임에 나눠 실행할 수 있는 A* 요청 상태. 확장 순서는 예산 분할과 무관하게 같다. */
struct LOOTNPOP_API FLNPNavSearch
{
	ELNPNavSearchStatus Status = ELNPNavSearchStatus::Invalid;
	int32 Start = INDEX_NONE;
	int32 Goal = INDEX_NONE;
	FVector3d GoalPoint = FVector3d::ZeroVector;
	uint64 SnapshotGeneration = 0;
	uint32 ConnectivityGraphVersion = 0;
	uint32 Stamp = 0;
	int32 Expansions = 0;
	FLNPNavSearchParams Params;
	/** Found일 때 시작→목표 경로 cost. */
	double PathCost = 0.0;
};

namespace LNPNavPathfinding
{
	/** scratch를 snapshot node 수에 맞추고 시작 node를 open에 넣는다. group이 다르면 Unreachable로 바로 끝난다. */
	LOOTNPOP_API ELNPNavSearchStatus BeginSearch(
		const FLNPNavSnapshot& Nav, int32 Start, int32 Goal, const FLNPNavSearchParams& Params,
		FLNPNavSearchScratch& Scratch, FLNPNavSearch& OutSearch);

	/**
	 * 최대 ExpansionBudget개 node를 확장한다. 재개할 때 snapshot generation·ConnectivityGraphVersion이 시작 때와 다르면
	 * Invalid로 끝내고 서로 다른 snapshot의 node를 섞지 않는다.
	 */
	LOOTNPOP_API ELNPNavSearchStatus StepSearch(
		const FLNPNavSnapshot& Nav, FLNPNavSearchScratch& Scratch, FLNPNavSearch& Search, int32 ExpansionBudget);

	/** Found 결과의 시작→목표 전역 node 열. */
	LOOTNPOP_API bool ExtractNodePath(
		const FLNPNavSearchScratch& Scratch, const FLNPNavSearch& Search, TArray<int32>& OutNodes);

	/**
	 * Nav 직선 보행 검사로 node 열을 waypoint 열로 줄인다. 같은 slot·Layer 구간 안에서만 건너뛰고,
	 * seam link·portal 전이의 양쪽 node는 항상 남긴다. 첫·마지막 node는 유지한다.
	 */
	LOOTNPOP_API void SimplifyPath(const FLNPNavSnapshot& Nav, TConstArrayView<int32> Nodes, TArray<int32>& OutWaypoints);
}
