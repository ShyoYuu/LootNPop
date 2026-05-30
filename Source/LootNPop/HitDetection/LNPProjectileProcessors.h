// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "LNPProjectileProcessors.generated.h"

/**
 * Projectile 위치를 전진시키고, 수명 만료 및 지형 충돌을 처리한다.
 * 트레일 해제와 임팩트 이펙트를 큐에 추가하고, Entity Destroy을 지연한다.
 */
UCLASS()
class LOOTNPOP_API ULNPProjectileMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPProjectileMovementProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery ProjectileQuery;
};

/**
 * 매 프레임 Projectile 선분을 Enemy와 Player Capsule에 대해 검사한다.
 * 명중 시: 피해 적용, 트레일 해제와 임팩트 이펙트 큐 추가, Destroy 지연.
 * MovementProcessor 이후 실행된다.
 */
UCLASS()
class LOOTNPOP_API ULNPProjectileHitDetectionProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPProjectileHitDetectionProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery ProjectileQuery;
	FMassEntityQuery EnemyQuery;
	FMassEntityQuery PlayerQuery;
};

/**
 * 게임 Thread Processor: 큐에 쌓인 트레일 해제와 임팩트 이펙트를 처리하고,
 * 살아있는 Projectile의 Niagara 트레일 Component를 스폰/업데이트한다.
 * HitDetectionProcessor 이후 실행된다.
 */
UCLASS()
class LOOTNPOP_API ULNPProjectileVisualizationProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPProjectileVisualizationProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery ProjectileQuery;
};

/**
 * FLNPProjectileDeadTag를 가진 모든 Entity를 조회하고 Destroy을 지연한다.
 * PostPhysics에서 실행되며, StartPhysics 커맨드 버퍼 플러시가
 * MovementProcessor와 HitDetectionProcessor의 Dead Tag를 적용한 후 실행된다.
 */
UCLASS()
class LOOTNPOP_API ULNPProjectileDestructionProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPProjectileDestructionProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery DeadProjectileQuery;
};

/**
 * 살아있는 Projectile의 프레임별 디버그 드로우 (청록 구체 + 속도 화살표).
 * 표면 임팩트 마커(주황 구체, 1초)도 처리한다.
 * 게임 Thread에서 VisualizationProcessor 이후 실행된다. 에디터 빌드에서만 활성화.
 */
UCLASS()
class LOOTNPOP_API ULNPProjectileDebugDrawProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPProjectileDebugDrawProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery ProjectileQuery;
	FMassEntityQuery PlayerQuery;
};
