// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "SurfaceNavigation/LNPNavGraph.h"
#include "SurfaceNavigation/LNPNavQuery.h"

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
}
