// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "Mass/EntityHandle.h"
#include "SurfaceNavigation/LNPNavGraph.h"
#include "SurfaceNavigation/LNPNavQuery.h"

class ULNPMassWorldCollisionSubsystem;
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
	/** Stale·공중은 새 슬롯을 주지 않고 기존 슬롯만 보존한다. 접지 도달 불가는 1.5초 뒤 반납한다. */
	LOOTNPOP_API bool UpdateMeleeSlot(const FLNPNavSnapshot& Nav, ELNPNavEndpointStatus Status,
		FMassEntityHandle Player, const FLNPNavGroupRef& GroundGroup, bool bGrounded, bool bOccupied,
		double Now, FLNPEnemySlotReachability& State);

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
	 * StartCenter(캡슐 중심)에서 GoalFeet 쪽으로 실제 지상 이동(LNPEnemySurfaceMovement::StepGrounded, cache 우선·exact 폴백)을
	 * 반복해 발이 GoalFeet의 AcceptRadius 안에 닿는지 본다. 모든 걸음이 Grounded이고 같은 slot·Layer를 유지해야 한다.
	 * 벽·Support 없는 Blocker·지지면 끊김·다른 층은 실패한다. 위치를 옮기지 않는 검사일 뿐이다(접지 계측에는 걸음 수가 더해진다).
	 */
	LOOTNPOP_API bool WalkReaches(const FLNPSurfaceDataSnapshot& Snapshot, const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params, const FVector& StartCenter, const FLNPSurfaceHandle& Surface,
		const FVector& GoalFeet, double AcceptRadius);

	/**
	 * 기본 투영 반경 안에 node가 없는 위치의 보행 anchor(D-085). 같은 slot·Layer에서 WalkAnchorSearchRadius 안의 가까운 node부터
	 * 실제 이동으로 걸어서 이어지는지 검증하고 처음 통과한 node를 돌려준다. 없으면 INDEX_NONE. 스냅·순간 이동은 하지 않는다.
	 * bWalkFromNode=false: Center의 개체가 node까지 걸어간다(Nav 밖 적의 복귀). AcceptRadius는 node 지면점 기준이다.
	 * bWalkFromNode=true: node에 선 적이 Center의 발까지 걸어간다(Nav 밖 플레이어에게 다가가기). AcceptRadius는 Center 발 기준이다.
	 */
	LOOTNPOP_API int32 FindWalkAnchor(const FLNPSurfaceDataSnapshot& Snapshot, const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params, const FVector& Center, const FLNPSurfaceHandle& Surface,
		const FLNPNavOverlay* Overlay, bool bWalkFromNode, double AcceptRadius);
}
