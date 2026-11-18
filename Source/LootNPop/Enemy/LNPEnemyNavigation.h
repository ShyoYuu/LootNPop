// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "Mass/EntityHandle.h"
#include "SurfaceNavigation/LNPNavGraph.h"
#include "SurfaceNavigation/LNPNavQuery.h"

class ULNPMassWorldCollisionSubsystem;
struct FLNPExactHitIdentity;
struct FLNPPlayerTrail;
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
	/** WalkReaches 선택 입출력(진단 프로브·보행 anchor 궤적). 보폭을 바꾸거나, 걸음이 끝난 사유·목표에 가장 가까웠던 지점·걸은 궤적을 받는다. */
	struct FWalkProbe
	{
		enum class EEnd : uint8 { Reached, NoSurface, NotGrounded, LayerChanged, Stuck, MaxSteps };

		/** 0보다 크면 한 걸음 길이(cm)와 걸음 시간(초)을 이 값으로 쓴다. 실제 이동 프로세서의 프레임 보폭을 흉내 낸다. */
		double StepLength = 0.0;
		float StepSeconds = 0.f;
		/** 있으면 걸음마다(출발 포함) 발 위치를 더한다. 보행 anchor 궤적(D-092)에 쓴다. */
		TArray<FVector>* FeetPath = nullptr;

		EEnd End = EEnd::NoSurface;
		LNPEnemyExactMovement::EGroundResult LastGround = LNPEnemyExactMovement::EGroundResult::Grounded;
		int32 Steps = 0;
		FVector EndFeet = FVector::ZeroVector;
		/** 목표 발까지 3D 거리가 가장 짧았던 걸음의 거리와 그때의 접평면 거리·높이차(목표가 위면 +). */
		double Best3D = DBL_MAX;
		double BestPlanar = 0.0;
		double BestHeight = 0.0;
	};

	LOOTNPOP_API bool WalkReaches(const FLNPSurfaceDataSnapshot& Snapshot, const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params, const FVector& StartCenter, const FLNPSurfaceHandle& Surface,
		const FVector& GoalFeet, double AcceptRadius, FWalkProbe* Probe = nullptr);

	/**
	 * 기본 투영 반경 안에 node가 없는 위치(또는 불완전 삼각형 안의 Nav 주머니, D-090)의 보행 anchor(D-085). 같은 slot·Layer에서 WalkAnchorSearchRadius 안의 가까운 node부터
	 * 실제 이동으로 걸어서 이어지는지 검증하고 처음 통과한 node를 돌려준다. 없으면 INDEX_NONE. 스냅·순간 이동은 하지 않는다.
	 * bWalkFromNode=false: Center의 개체가 node까지 걸어간다(Nav 밖 적의 복귀). AcceptRadius는 node 지면점 기준이다.
	 * bWalkFromNode=true: node에 선 적이 Center의 발까지 걸어간다(Nav 밖 플레이어에게 다가가기). AcceptRadius는 Center 발 기준이다.
	 */
	LOOTNPOP_API int32 FindWalkAnchor(const FLNPSurfaceDataSnapshot& Snapshot, const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params, const FVector& Center, const FLNPSurfaceHandle& Surface,
		const FLNPNavOverlay* Overlay, bool bWalkFromNode, double AcceptRadius, TArray<FVector>* OutFeetPath = nullptr);

	/** 보행 anchor 궤적(D-092)에서 남길 점 간격(cm). 걸음(캡슐 반지름)보다 넓게 솎는다. */
	constexpr double AnchorPathSpacing = 50.0;

	/**
	 * 보행 anchor 점열(D-092): [0]은 anchor node 지면점, 그 뒤는 검증 걸음 궤적(WalkedFeet)을 AnchorPathSpacing마다 솎은 발 위치다.
	 * OutPoints 크기에서 멈춘다. 점 수를 돌려준다.
	 */
	LOOTNPOP_API int32 BuildAnchorPath(const FVector& NodePoint, TConstArrayView<FVector> WalkedFeet, TArrayView<FVector> OutPoints);

	/** 발자취 점 간격(cm, D-092). 직전 점에서 이만큼 멀어지면 그 구간을 걸어 보고 점을 남긴다. */
	constexpr double TrailSpacing = 100.0;
	/** 발자취 구간 걸음 검증에서 새 점에 닿았다고 보는 거리(cm). 보행 anchor의 150cm보다 좁게 둔다. */
	constexpr double TrailAccept = 50.0;
	/** 발자취: 직전 확인 자리에서 이만큼 움직일 때마다 마지막 점 → 발 직선을 걸어 본다(cm). */
	constexpr double TrailCheckStep = 25.0;
	/** 적 자신의 발자취(되짚어 나오기) 점 간격(cm). 되짚는 한 구간을 짧게 둔다. */
	constexpr double CrumbSpacing = 50.0;
	/** 마지막으로 트인 자리에서 이보다 먼 곳에서 처음 Nav를 벗어났으면(순간 이동·낙하) 발자취를 시작하지 않는다. */
	constexpr double TrailStartMaxDistance = 600.0;
	/** 점열 추종: 점의 이 접평면 거리 안이면 다음 점으로 넘어간다. */
	constexpr double TailArrival = 50.0;
	/** 점열 머리·개정이 바뀌었을 때 이 접평면 거리 안의 점으로 다시 맞춘다. 없으면 경로로 머리 node부터 다시 간다. */
	constexpr double TailSyncRadius = 100.0;
	/** 현재 점까지 이 시간(초) 동안 TailStallProgress만큼 가까워지지 못하면 직전 점으로 한 칸 물러난다. */
	constexpr double TailStallSeconds = 1.0;
	constexpr double TailStallProgress = 10.0;

	/**
	 * 플레이어 발자취 한 접지 프레임 갱신(D-092). 공중 프레임은 부르지 않는다.
	 * bOnNav: 투영 node가 있고 Nav 주머니가 아니다. 이때 발자취를 지우고, OpenNode(발 삼각형이 트였을 때의 투영 node)가 있으면 시작점으로 기억한다.
	 * Nav 밖이면 마지막 트인 자리부터 발자취를 시작한다. TrailCheckStep마다(착지는 곧바로) 마지막 점 → 발 직선을 Params 캡슐로
	 * 걸어 보고, 통과하면 TrailSpacing마다 점을 남긴다. 막히면 마지막으로 통과한 자리를 점으로 남기고 거기서 다시 걸어 본다.
	 * 그래도 막히거나 상한 초과·다른 층·이전 snapshot이면 끊는다.
	 * 발이 앞선 점 가까이로 돌아오면 그 점 뒤를 지운다(고리 제거).
	 */
	LOOTNPOP_API void UpdatePlayerTrail(const FLNPSurfaceDataSnapshot& Snapshot, const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params, const FVector& Feet, const FLNPSurfaceHandle& Surface, bool bOnNav,
		int32 OpenNode, bool bLanded, FLNPPlayerTrail& Trail);

	/** 점열(Points 뒤 끝점 GoalFeet)에서 발의 TailSyncRadius 안에 있는 가장 뒤쪽 점의 번호. 없으면 INDEX_NONE. */
	LOOTNPOP_API int32 SyncTail(TConstArrayView<FVector> Points, const FVector& Feet, const FVector& Up);

	/**
	 * 점열 추종 한 프레임(D-092). Points 뒤의 끝점은 GoalFeet(플레이어 발)이다. 발이 TailArrival 안에 든 가장 뒤쪽 점 다음으로 넘어가고,
	 * TailStallSeconds 동안 가까워지지 못하면 한 칸 물러난다(밀리거나 미끄러져 검증된 선을 벗어난 적이 지나온 점으로 돌아간다).
	 * 조향할 점을 OutSteer로 준다. 첫 점(머리 node)에서도 정체하면 false다. 곧게 돌아갈 길이 없으니 점열 추종을 그만두고 경로로 다시 간다.
	 */
	LOOTNPOP_API bool FollowTail(TConstArrayView<FVector> Points, const FVector& GoalFeet, const FVector& Feet, const FVector& Up,
		double Now, int32& Index, double& BestDistance, double& ProgressTime, FVector& OutSteer);

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
