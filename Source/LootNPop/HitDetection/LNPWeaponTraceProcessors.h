// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "LNPWeaponTraceProcessors.generated.h"

/**
 * 근거리 공격 Swept Volume 피격 판정 Processor.
 * FLNPWeaponTraceFragment를 가진 공격자 엔티티의 칼날 Quad를 타겟 Capsule과 교차 검사한다.
 * 명중 시 GE 기반 피해를 큐에 추가하고, 동일 타겟 중복 피격을 막기 위해 AlreadyHit를 기록한다.
 */
UCLASS()
class LOOTNPOP_API ULNPWeaponTraceHitDetectionProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPWeaponTraceHitDetectionProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery AttackerQuery;
	FMassEntityQuery EnemyQuery;
	FMassEntityQuery PlayerQuery;
};

/**
 * FLNPWeaponTraceFragment의 TimeToLive를 감소시켜 만료된 엔티티를 파괴한다.
 * ANS_LNPMeleeHitWindow::NotifyEnd가 호출되지 않을 때를 대비한 안전장치.
 */
UCLASS()
class LOOTNPOP_API ULNPWeaponTraceLifetimeProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPWeaponTraceLifetimeProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery Query;
};

/**
 * 근거리 공격 피격 판정 디버그 드로우.
 * 칼날 Swept Quad (마젠타), 현재 칼날 위치 (노랑), 판정 반경 구체를 매 프레임 그린다.
 * 에디터 빌드에서만 활성화된다.
 */
UCLASS()
class LOOTNPOP_API ULNPWeaponTraceDebugDrawProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPWeaponTraceDebugDrawProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery AttackerQuery;
};