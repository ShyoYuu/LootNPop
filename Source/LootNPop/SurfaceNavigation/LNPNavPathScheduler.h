// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Deque.h"
#include "Mass/EntityHandle.h"
#include "SurfaceNavigation/LNPNavPathfinding.h"
#include "SurfaceNavigation/LNPSurfaceTypes.h"

struct FLNPNavOverlay;
struct FLNPNavSnapshot;

/** 같은 우선순위는 먼저 온 순서다(`phases/Phase07b_PathExecution.md` §3.4). */
enum class ELNPNavPathPriority : uint8
{
	/** 새 추격 요청. */
	Chase,
	/** 경로 무효화에 따른 재계획. */
	Replan,
	/** 배회·재귀속·접근점 이동. */
	Background,

	Count
};

enum class ELNPNavPathStatus : uint8
{
	/** 이 owner·serial의 기록이 없다. */
	None,
	Queued,
	Running,
	Succeeded,
	/** 같은 group인데 경로가 없거나 요청당 확장 상한을 넘었다. */
	NoPath,
	/** 공통 group 짝이 없다(D-062). 접근점이 있으면 접근점까지의 경로가 함께 온다(D-063). */
	Unreachable,
	/** 스냅 반경 안에 walkable node가 없다. 목표 쪽만 없으면 접근점 경로가 올 수 있다. */
	NoNode,
	/** handle이 옛 snapshot이거나, 실행 중 generation·ConnectivityGraphVersion·overlay revision이 바뀌었다. 다시 요청한다. */
	Stale,
	/** owner가 더 새 요청을 냈거나, 취소했거나, 사라졌다. */
	Cancelled,
};

struct LOOTNPOP_API FLNPNavPathWaypoint
{
	FVector3d Location = FVector3d::ZeroVector;
	/** 전역 조밀 node index. */
	int32 Node = INDEX_NONE;
};

/** 단순화된 경로 하나. immutable이며 여러 개체가 공유한다. */
struct LOOTNPOP_API FLNPNavPath
{
	uint64 SnapshotGeneration = 0;
	uint32 ConnectivityGraphVersion = 0;
	TArray<FLNPNavPathWaypoint> Waypoints;
	/**
	 * TraversedRevisionFingerprint. waypoint 구간이 실제로 지나는 cell의 Tile key 오름차순과, 계산 당시 그 Tile의 overlay revision이다.
	 * A* node가 아니라 단순화된 직선 구간의 cell에서 모은다. 개체는 그 구간을 걷기 때문이다.
	 */
	TArray<uint32> TraversedTiles;
	TArray<uint32> TileRevisions;
	double Cost = 0.0;

	/** generation·version이 같고 지나는 Tile의 overlay revision이 모두 그대로인가. */
	bool IsCurrent(const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay) const;
};

using FLNPNavPathPtr = TSharedPtr<const FLNPNavPath, ESPMode::ThreadSafe>;

struct LOOTNPOP_API FLNPNavPathRequest
{
	FMassEntityHandle Owner;
	ELNPNavPathPriority Priority = ELNPNavPathPriority::Background;
	FVector3d StartPosition = FVector3d::ZeroVector;
	FLNPSurfaceHandle StartSurface;
	FVector3d GoalPosition = FVector3d::ZeroVector;
	FLNPSurfaceHandle GoalSurface;
	double SnapRadius = 300.0;
	/** D-063 접근점 반경. 0이면 도달 불가 결과에 접근점 경로를 붙이지 않는다. */
	double ApproachRadius = 0.0;
};

struct LOOTNPOP_API FLNPNavPathResult
{
	uint32 Serial = 0;
	ELNPNavPathStatus Status = ELNPNavPathStatus::None;
	/** Succeeded의 경로, 또는 Unreachable·NoNode의 접근점 경로(없을 수 있다). */
	FLNPNavPathPtr Path;
	/** 이 요청이 확장한 node 수. cache hit는 0이다. */
	int32 Expansions = 0;
	/** 요청이 실행을 시작한 뒤 끝날 때까지 걸친 tick 수. */
	int32 RunningTicks = 0;
	bool bFromCache = false;
};

struct LOOTNPOP_API FLNPNavPathSchedulerSettings
{
	/** 프레임 전체 확장 예산. 구현 단위 0 측정(확장당 0.38us)으로 경로 CPU 약 1.5ms에 맞춘 값이다. */
	int32 ExpansionsPerFrame = 4000;
	int32 MaxExpansionsPerRequest = 30000;
	/** 동시 실행 요청 수. scratch 하나가 전역 node 수 × 12B(Meadow 6.21MiB)다. */
	int32 ScratchCount = 4;
	int32 CacheCapacity = 256;
	/** 시작·목표 스냅(ResolveEndpoints, P95 약 8us)을 확장 수로 환산해 예산에서 뺀다. */
	int32 ResolveCostInExpansions = 16;
	/** 실행 중 요청들을 worker에서 병렬로 확장한다. 결과는 병렬 여부·예산 분할과 무관하다. */
	bool bParallel = true;
};

/** 누적 통계. NavReport·부하 측정이 프레임 차분으로 읽는다. */
struct LOOTNPOP_API FLNPNavPathSchedulerStats
{
	uint64 Submitted = 0;
	/** ELNPNavPathStatus별 종료 수. */
	uint64 Finished[static_cast<int32>(ELNPNavPathStatus::Cancelled) + 1] = {};
	uint64 CacheHits = 0;
	uint64 CacheMisses = 0;
	/** 같은 key의 cache 경로가 있었지만 revision이나 직선 보행 검사로 쓰지 못했다. */
	uint64 CacheRejects = 0;
	uint64 Expansions = 0;
	uint64 Ticks = 0;
	int32 LastTickExpansions = 0;
	int32 LastTickStarted = 0;
	int32 MaxConcurrentRunning = 0;
};

/**
 * 서버 전용 경로 요청 scheduler(`phases/Phase07b_PathExecution.md` §3.4·§3.5). UObject가 없는 순수 코어이며 게임 스레드에서만 호출한다.
 * owner마다 최신 요청 하나를 추적하고, 프레임 예산 안에서 scratch 수만큼의 요청을 병렬·다중 프레임으로 확장한다.
 */
class LOOTNPOP_API FLNPNavPathScheduler
{
public:
	FLNPNavPathSchedulerSettings Settings;

	/** owner의 이전 요청은 Cancelled가 된다. 반환값은 새 serial이다. */
	uint32 Submit(const FLNPNavPathRequest& Request);

	/** owner의 요청을 버린다. 실행 중이면 scratch를 반납한다. */
	void Cancel(FMassEntityHandle Owner);

	/** Serial이 owner의 최신 요청이 아니면 Cancelled, 기록이 없으면 None이다. */
	ELNPNavPathStatus GetResult(FMassEntityHandle Owner, uint32 Serial, FLNPNavPathResult& OutResult) const;

	/**
	 * 한 프레임을 처리한다. 사라진 owner를 정리하고, 실행 중 요청의 snapshot·overlay 일치를 검증하고,
	 * 예산이 남는 동안 대기열 시작과 병렬 확장을 반복한다.
	 */
	void Tick(const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay, TFunctionRef<bool(FMassEntityHandle)> IsOwnerValid);

	/** overlay가 바뀐 Tile을 지나던 완료 경로만 같은 serial로 재계획 대기열에 넣는다. */
	int32 RequeueInvalidatedPaths(const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay);

	bool HasWork() const { return !Running.IsEmpty() || QueuedCount > 0; }
	int32 GetRunningCount() const { return Running.Num(); }
	int32 GetQueuedCount() const { return QueuedCount; }
	int32 GetCacheCount() const { return Cache.Num(); }
	const FLNPNavPathSchedulerStats& GetStats() const { return Stats; }
	void VisitResults(TFunctionRef<void(FMassEntityHandle, const FLNPNavPathResult&)> Visitor) const;
	uint64 GetScratchBytes() const;

	/** 모든 요청·cache·통계를 버린다. scratch 메모리는 유지한다. */
	void Reset();

private:
	struct FRecord
	{
		FLNPNavPathRequest Request;
		FLNPNavPathResult Result;
	};

	struct FQueueEntry
	{
		FMassEntityHandle Owner;
		uint32 Serial = 0;
	};

	struct FRunningSearch
	{
		FMassEntityHandle Owner;
		uint32 Serial = 0;
		int32 ScratchIndex = INDEX_NONE;
		FLNPNavSearch Search;
		/** Reachable이 아니면 접근점 경로를 찾는 중이며, 끝나도 이 상태를 유지한다. */
		ELNPNavPathStatus EndpointStatus = ELNPNavPathStatus::Succeeded;
		uint32 OverlayRevision = 0;
		/** 이번 라운드 확장 전 누적 확장 수. */
		int32 ExpansionsBefore = 0;
		/** Succeeded 결과를 넣을 cache key의 Tile. */
		uint32 StartTile = MAX_uint32;
		uint32 GoalTile = MAX_uint32;
	};

	struct FCacheKey
	{
		uint32 StartTile = MAX_uint32;
		uint32 GoalTile = MAX_uint32;
		uint64 SnapshotGeneration = 0;
		uint32 ConnectivityGraphVersion = 0;

		bool operator==(const FCacheKey&) const = default;
		friend uint32 GetTypeHash(const FCacheKey& Key)
		{
			return HashCombine(HashCombine(Key.StartTile, Key.GoalTile),
				HashCombine(GetTypeHash(Key.SnapshotGeneration), Key.ConnectivityGraphVersion));
		}
	};

	struct FCacheEntry
	{
		FLNPNavPathPtr Path;
		uint64 LastUsedTick = 0;
	};

	bool PopNextQueued(FQueueEntry& OutEntry);
	int32 AcquireScratch();
	/** 스냅하고 cache를 보고, 필요하면 scratch를 잡아 탐색을 시작한다. 즉시 끝나면 결과를 기록한다. */
	void StartRequest(const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay, FRecord& Record);
	/** 끝난 탐색을 결과로 바꾸고 scratch를 반납한다. */
	void FinishSearch(const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay, FRunningSearch& Running);
	void Finish(FRecord& Record, ELNPNavPathStatus Status);
	FLNPNavPathPtr BuildPath(const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay, TConstArrayView<int32> Nodes, double Cost) const;
	FLNPNavPathPtr FindCachedPath(const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay, const FCacheKey& Key,
		int32 StartNode, int32 GoalNode);
	void AddToCache(const FCacheKey& Key, const FLNPNavPathPtr& Path);
	void ReleaseRunning(int32 RunningIndex);

	TMap<FMassEntityHandle, FRecord> Records;
	TDeque<FQueueEntry> Queues[static_cast<int32>(ELNPNavPathPriority::Count)];
	/** 대기 중인 최신 요청 수. 큐에는 교체·취소된 옛 항목이 남을 수 있어 따로 센다. */
	int32 QueuedCount = 0;
	TArray<FRunningSearch> Running;
	TArray<FLNPNavSearchScratch> Scratches;
	TBitArray<> ScratchInUse;
	TMap<FCacheKey, FCacheEntry> Cache;
	FLNPNavPathSchedulerStats Stats;
	uint32 NextSerial = 1;
	uint64 TickIndex = 0;
};
