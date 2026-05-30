// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LNPEnemyProcessors.generated.h"

/**
 * Enemy의 우선순위 점수를 계산하고 Targeting Subsystem에 등록한다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyScoringProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyScoringProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery ScoringQuery;
	FMassEntityQuery PlayerQuery;
};

/**
 * Targeting Subsystem의 결과를 Enemy Fragment에 다시 동기화한다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyTargetingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyTargetingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery TargetingQuery;
	FMassEntityQuery PlayerQuery;
};

/**
 * 타겟 데이터를 MoveTarget Fragment에 동기화하여 이동 Intent를 처리한다.
 * 거리 기반 StateTree 시그널링도 담당한다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyTargetFollowProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyTargetFollowProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery FollowQuery;
};

/**
 * 순수 이동 실행 Processor.
 * MoveTarget Intent를 읽어 실제 이동/회전을 적용한다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyMovementProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery MovementQuery;
};

/**
 * Enemy NPC 시각적 디버깅. 에디터 빌드에서만 활성화된다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyDebugDrawProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyDebugDrawProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery DebugQuery;
};

/**
 * Enemy의 HP 업데이트와 사망을 처리한다.
 */
UCLASS()
class LOOTNPOP_API ULNPHealthProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPHealthProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery HealthQuery;
};

/**
 * 커스텀 로직(Targeting, StateTree)에 따라 표현 상태(Actor vs. Entity)를 Override한다.
 * 내장 MassRepresentationProcessor 이전에 실행된다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyLODOverrideProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyLODOverrideProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery LODOverrideQuery;
};

/**
 * 스폰된 Actor의 초기화와 Fragment에서의 데이터 동기화를 처리한다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyActorInitializerProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyActorInitializerProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery InitializerQuery;
};

/**
 * 게임 Thread에서 Representation 그룹 이후 실행된다.
 * - Actor 유효: ASC HP를 FLNPEnemyFragment에 다시 동기화 (SyncToEntity).
 * - Actor null:  FLNPEnemyActorInitializedTag를 제거하여 다음 스폰 시 ActorInitializer가 재실행됨.
 *
 * 별도 Processor 둘로 나누는 방식과의 트레이드오프: 같은 프레임에 Entity가 GE 피해를 입고
 * LOD 디스폰되면 그 프레임의 피해가 Fragment에 반영되지 않는다. 두 이벤트(치명타 + LOD 경계 이탈)가
 * 동일 프레임에 발생할 확률이 극히 낮으므로 일단은 허용.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyActorSyncProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyActorSyncProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery SyncQuery;
};

/**
 * Destroy가 예약된 Enemy의 DeathCountdown을 Tick하고 Timer가 0이 되면 Entity를 Destroy시킨다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyDeathTimerProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyDeathTimerProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery DeathTimerQuery;
};
