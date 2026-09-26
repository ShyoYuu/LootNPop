// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LNPEnemyFlightMovementProcessor.generated.h"

/**
 * 비행 steering 결과 누계 — 부하 harness 측정용(SurfaceSupportNavigation Phase03c §3.8).
 * 모든 스레드. 모든 월드가 한 누계를 공유하지만 steering은 서버만 돌리므로 측정 호스트의 값이다.
 */
namespace LNPEnemyFlightStats
{
	struct FCounts
	{
		uint64 Hover = 0;
		uint64 Clear = 0;
		uint64 Blocked = 0;
		uint64 Stuck = 0;
		/** 진행 없음이 교착 복구 첫 단계(StuckWidenTime)에 들어선 횟수. */
		uint64 RecoveryEntries = 0;
	};

	LOOTNPOP_API FCounts Get();
	LOOTNPOP_API void Reset();
}

/**
 * `ELNPNavigationDomain::FreeFlight` 적(FLNPEnemyFlyingTag)의 이동 — 지상 ULNPEnemyMovementProcessor의 비행판이다.
 *
 * - 서버 전용, PureEntity 전용(D-052). 위치는 Mass 복제(3D 위치 + 접평면 Yaw)로 게스트에 전달된다.
 * - 목표점: 비교전은 StateTree가 준 3D 배회점, 교전은 타겟 위 교전 고도(D-053). 이동은 LNPFlightSteering이 맡는다.
 * - 도착 판정은 **3D 거리**다. 지상은 접평면 거리지만 비행 개체는 고도 차이도 날아서 좁힐 수 있는 거리다.
 * - 피격 반응 시계·배회 타임아웃·도착 신호는 지상과 같은 규약이다(TechDesign_EnemyNPC.md §5.1).
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyFlightMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyFlightMovementProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery FlightQuery;
};
