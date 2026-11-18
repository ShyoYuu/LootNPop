// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "Mass/EntityHandle.h"
#include "SurfaceNavigation/LNPNavGraph.h"
#include "SurfaceNavigation/LNPNavQuery.h"

class ULNPMassWorldCollisionSubsystem;
struct FLNPExactHitIdentity;
struct FLNPSurfaceDataSnapshot;

/** 기존 근접 슬롯의 도달 불가 유예. 후보 목록 초기화와 별도로 유지한다. */
struct FLNPEnemySlotReachability
{
	FMassEntityHandle Player;
	FLNPNavGroupRef GroundGroup;
	double UnreachableSince = -1.0;
};

/** 게임 스레드가 수집한 활성 Pod의 정적 Nav 정보. */
struct FLNPEnemyHomePod
{
	FMassEntityHandle Entity;
	FVector3d Position = FVector3d::ZeroVector;
	FLNPSurfaceHandle Surface;
	FLNPNavGroupRef Group;
};

namespace LNPEnemyNavigation
{
	/**
	 * Stale·공중·바닥 식별 실패는 새 슬롯을 주지 않고 기존 슬롯만 보존한다. 접지 도달 불가는 1.5초 뒤 반납한다.
	 * GroundSurface는 플레이어의 물리 바닥이다. 땅 위인데 Nav 소속(GroundGroup)이 없으면 도달 불가로 본다(D-091).
	 */
	LOOTNPOP_API bool UpdateMeleeSlot(const FLNPNavSnapshot& Nav, ELNPNavEndpointStatus Status,
		FMassEntityHandle Player, const FLNPNavGroupRef& GroundGroup, const FLNPSurfaceHandle& GroundSurface,
		bool bGrounded, bool bOccupied, double Now, FLNPEnemySlotReachability& State);

	/** D-062로 도달 가능한 활성 Pod 중 chord 거리가 가장 가까운 것을 고른다. Stale이면 판정을 미룬다. */
	LOOTNPOP_API ELNPNavEndpointStatus SelectHomePod(const FLNPNavSnapshot& Nav,
		const FVector3d& Feet, const FLNPSurfaceHandle& Surface, FMassEntityHandle Parent,
		TConstArrayView<FLNPEnemyHomePod> Pods, int32& OutPod);

	/** 보행 anchor 탐색 반경(cm). 기본 투영 반경(300cm) 밖에서만 쓴다(D-085). */
	constexpr double WalkAnchorSearchRadius = 600.0;
	/** 보행 검증을 시도할 가까운 node 수. */
	constexpr int32 WalkAnchorMaxCandidates = 4;
	/** 같은 개체의 보행 검증을 다시 하기까지의 간격(초). 결과(실패 포함)는 그동안 재사용한다. */
	constexpr double WalkAnchorRetrySeconds = 0.5;

	/**
	 * 플레이어가 Support 없는 정적 Blocker(바위) 위에 서면 바닥 층이 없다. 직전 접지가 같은 slot의 현재 generation 지지면이면
	 * 그 층을 빌려 Nav 투영을 계속한다(D-091). 트인 평지의 바위 위는 근처 node로 이어지고, 주머니 속 바위 위는 Nav 밖이 된다.
	 * Support·동적·slot 없는 바닥(Pod·이동 지지면·미등록 Actor)은 빌리지 않는다.
	 */
	LOOTNPOP_API bool BorrowBlockerFloorSurface(const FLNPExactHitIdentity& Identity, const FLNPSurfaceHandle& Previous,
		uint64 SnapshotGeneration, FLNPSurfaceHandle& OutSurface);

	/**
	 * StartCenter(캡슐 중심)에서 GoalFeet 쪽으로 실제 지상 이동(LNPEnemySurfaceMovement::StepGrounded, cache 우선·exact 폴백)을
	 * 반복해 발이 GoalFeet의 AcceptRadius 안에 닿는지 본다. 모든 걸음이 Grounded이고 같은 slot·Layer를 유지해야 한다.
	 * 실제 이동과 같은 옆걸음(D-087)으로 작은 Blocker는 돌아가며, 벽·돌아갈 수 없는 Support 없는 Blocker·지지면 끊김·다른 층은 실패한다. 위치를 옮기지 않는 검사일 뿐이다(접지 계측에는 걸음 수가 더해진다).
	 */
	LOOTNPOP_API bool WalkReaches(const FLNPSurfaceDataSnapshot& Snapshot, const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params, const FVector& StartCenter, const FLNPSurfaceHandle& Surface,
		const FVector& GoalFeet, double AcceptRadius);

	/**
	 * 기본 투영 반경 안에 node가 없는 위치(또는 불완전 삼각형 안의 Nav 주머니, D-090)의 보행 anchor(D-085). 같은 slot·Layer에서 WalkAnchorSearchRadius 안의 가까운 node부터
	 * 실제 이동으로 걸어서 이어지는지 검증하고 처음 통과한 node를 돌려준다. 없으면 INDEX_NONE. 스냅·순간 이동은 하지 않는다.
	 * bWalkFromNode=false: Center의 개체가 node까지 걸어간다(Nav 밖 적의 복귀). AcceptRadius는 node 지면점 기준이다.
	 * bWalkFromNode=true: node에 선 적이 Center의 발까지 걸어간다(Nav 밖 플레이어에게 다가가기). AcceptRadius는 Center 발 기준이다.
	 */
	LOOTNPOP_API int32 FindWalkAnchor(const FLNPSurfaceDataSnapshot& Snapshot, const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params, const FVector& Center, const FLNPSurfaceHandle& Surface,
		const FLNPNavOverlay* Overlay, bool bWalkFromNode, double AcceptRadius);

	/** 경로 지름길(D-088)을 볼 후보 waypoint의 최대 거리(cm, 현재 발 기준). 걸음 검증은 최대 약 37걸음이다. */
	constexpr double PathShortcutMaxDistance = 800.0;
	/** 현재 waypoint를 거쳐 가는 길보다 이만큼(cm) 이상 짧아질 때만 걸어 본다. 거의 일직선인 모서리는 보지 않는다. */
	constexpr double PathShortcutMinSaving = 50.0;
	/** 지름길 걸음 검증에서 후보 waypoint 지면점에 닿았다고 보는 거리(cm). */
	constexpr double PathShortcutAccept = 50.0;
	/** 한 번의 검사에서 격자 검사가 멈춘 뒤 이어서 건너뛸 수 있는 waypoint 수. */
	constexpr int32 PathShortcutMaxSkips = 2;

	enum class EPathShortcut : uint8
	{
		/** 실제로 걸어서 후보 waypoint에 닿는다. 건너뛴다. */
		Walkable,
		/** 후보가 PathShortcutMaxDistance 밖이다. 걷지 않는다. */
		TooFar,
		/** 절약 거리가 PathShortcutMinSaving보다 작다. 걷지 않는다. */
		LowGain,
		/** 걸어 봤지만 벽·돌아갈 수 없는 Support 없는 Blocker·지지면 끊김·다른 층으로 닿지 않는다. */
		Blocked,
	};

	/**
	 * 경로 추종의 지름길 판정(D-088). 격자 직선 검사(`LNPNavGraph::IsDirectWalkable`)는 Nav 구멍을 가로지르지 못하므로,
	 * 현재 waypoint를 건너뛰고 다음 waypoint(Candidate)로 바로 갈 수 있는지를 실제 보행(WalkReaches)으로 본다.
	 * 거리·절약 거리 조건을 먼저 보고, 통과한 경우에만 걷는다. Nav·overlay는 보지 않는다(Pod·마커는 실제 충돌이 막는다).
	 */
	LOOTNPOP_API EPathShortcut CheckPathShortcut(const FLNPSurfaceDataSnapshot& Snapshot, const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params, const FVector& StartCenter, const FLNPSurfaceHandle& Surface,
		const FVector& CurrentWaypoint, const FVector& Candidate);
}
