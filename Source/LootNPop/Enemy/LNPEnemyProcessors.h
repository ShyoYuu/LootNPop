// Copyright (c) 2026 LootNPop. All rights reserved.

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
 * 표현 상태(Actor vs. Entity)의 원천인 LOD 값을 넷 모드에 따라 Override한다.
 * 내장 MassRepresentationProcessor 이전에 실행된다.
 *
 * - 서버: 커스텀 로직(Targeting)이 전투 진입을 알리면 High로 올려 Actor 승격을 강제한다.
 * - 클라이언트: Mass가 스스로 Actor를 스폰하지 못하도록 메시 표현 단계로 눌러두고,
 *   서버가 복제해 준 Actor가 붙어 있을 때만 High로 올려 그 Actor를 표현으로 채택한다 (§7.10).
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
	FMassEntityQuery ClientRepresentationQuery;
};

/**
 * Low→High LOD 전환(PrevRepresentation → HighResSpawnedActor) 프레임마다 실행된다.
 * - InitializeOnce: Ability/무기 설정 (bInitializedOnce 가드로 1회만)
 * - SyncFromEntity: AnimSourceMesh 숨김 + HP 동기화 (매 활성화마다)
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

	FMassEntityQuery ActivationQuery;
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

	FMassEntityQuery EnemyQuery;
	FMassEntityQuery PlayerQuery;
};