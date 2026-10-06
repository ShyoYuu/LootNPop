// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LNPEnemyCrawlMovementProcessor.generated.h"

/** 벽보행 이동 측정 누계(부하 harness, Phase 14 단위 6). 모든 스레드에서 읽고 비울 수 있다. */
namespace LNPSurfaceCrawlStats
{
	struct FCounts
	{
		/** 단순 우회(단위 2e)를 시작한 횟수. */
		uint64 DetourEntries = 0;
		/** 기어가다 붙을 표면을 잃고 공중으로 넘어간 횟수. */
		uint64 SurfaceLost = 0;
		/** 공중(넉백·표면 상실)에서 착지한 횟수. 시체 착지는 세지 않는다. */
		uint64 Landings = 0;
		/** 재귀속으로 부모 Pod가 바뀐 횟수. */
		uint64 Rehomes = 0;
		/** 움직이는 지형에 끼어 압사한 횟수. */
		uint64 Crushes = 0;
	};

	LOOTNPOP_API FCounts Get();
	LOOTNPOP_API void Reset();
}

/**
 * `ELNPNavigationDomain::SurfaceCrawl` 적(FLNPEnemyCrawlTag)의 이동 — 지상 ULNPEnemyMovementProcessor의 벽보행판이다(D-066).
 *
 * - 서버 전용, PureEntity 전용. Support·Nav를 쓰지 않고 LNPSurfaceCrawl의 exact 표면 이동만 쓴다.
 * - 목표점: 비교전은 StateTree가 준 배회점, 교전은 타겟 위치(원거리 정지 거리에서 선다). 길찾기는 없다 —
 *   목표 방향을 지금 표면의 접평면에 투영해 기어가고, 막히면 모서리를 갈아타며 넘는다.
 * - 붙어 있는지·공중인지는 지상과 같은 규약이다(FLNPEnemyVelocityFragment가 0이면 붙음). 넉백·표면 상실은
 *   지상과 같은 StepAirborne으로 구 중심 중력 낙하하고, 착지하면 그 자리 표면에 다시 붙는다. 스스로 점프하지 않는다.
 * - 엔티티 회전 Z는 매 프레임 표면 법선과 같다. 전방만 회전 속도로 보간한다.
 * - 피격 반응 시계·배회 타임아웃·도착 신호는 지상과 같은 규약이다(TechDesign_EnemyNPC.md §5.1).
 * - 재귀속: 비교전 중 Home 쪽으로 가까워지지 않으면 직선거리로 가장 가까운 활성 Pod로 바꾼다(D-066, Nav 도달성 없음).
 * - 벽보행끼리의 분리는 ULNPCrawlSpatialGridSubsystem의 3D 이웃으로 재고 접평면에 눕혀 이동 속도에 더한다.
 * - 움직이는 패널에 붙으면 함께 운반되고(D-013), 패널과 다른 면 사이에 끼어 겹침을 풀 수 없으면 압사한다(체력 0).
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyCrawlMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyCrawlMovementProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery CrawlQuery;

	/** 재귀속 후보(터지지 않은 Pod)의 위치. */
	FMassEntityQuery PodQuery;
};
