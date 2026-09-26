// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LNPEnemyFlightMovementProcessor.generated.h"

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
