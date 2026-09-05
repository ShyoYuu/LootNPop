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
 * 전투에 들어간 적의 표현 LOD를 강제로 High로 끌어올려 Actor 승격을 유도한다 (서버 전용).
 *
 * ⚠️ **PostPhysics인 것은 우연이 아니다** — 판단 근거인 `FLNPEnemyTargetingFragment`를
 * `ULNPEnemyScoringProcessor`(PostPhysics)가 채우므로 그보다 뒤여야 한다.
 * 그래서 **표현 체인(PrePhysics)에 끼어드는 일은 이 프로세서가 할 수 없다** —
 * 그쪽은 `ULNPEnemyClientRepresentationProcessor`가 맡는다.
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
 * 게스트 전용 — 적 엔티티의 표현을 **복제된 Actor로 일원화**한다.
 *
 * 게스트는 자기 Actor를 승격시켜서는 안 된다. 그렇게 스폰된 Actor는
 * `ULNPEnemyActorInitializerProcessor`가 클라이언트에서 조기 반환하는 탓에 무기도 HP 바도 없는
 * **빈 껍데기**이고, 뒤늦게 도착한 복제 퍼펫이 이미 점유된 `FMassActorFragment`에 붙어
 * 엔진 `checkf(!ActorInfo->IsValid())`로 **게스트를 죽인다.**
 *
 * ⚠️ **PrePhysics여야 한다. 이것이 이 프로세서가 따로 존재하는 유일한 이유다.**
 * 표현을 실제로 정하는 `UMassCrowdVisualizationLODProcessor`·`UMassCrowdVisualizationProcessor`가
 * 둘 다 `ProcessingPhase`를 설정하지 않아 **엔진 기본값 PrePhysics**로 돈다. 그런데 Mass는
 * 프로세서를 **페이즈별로 따로 버킷팅해 각 페이즈를 독립적으로 의존성 해소**하므로
 * (`MassEntitySettings.cpp`), `ExecuteAfter`/`ExecuteBefore`는 **페이즈를 건너지 못한다.**
 * 다른 페이즈에서 아무리 정확한 순서를 선언해도 조용히 무시되고, LOD는 매 프레임 PrePhysics에서
 * 거리 기준으로 다시 계산돼 우리가 쓴 값을 덮는다 — 2026-09-05에 실제로 그 상태였다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyClientRepresentationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyClientRepresentationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

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
 * 서버 행동 상태를 산출해 `FLNPEnemyActionFragment`에 기록한다 — 게스트 연출의 유일한 입력.
 *
 * 순수 엔티티의 공격은 서버 전용 Mass 로직이라, 이 프로세서가 없으면 게스트 화면에서 적은
 * 아무것도 하지 않는 것처럼 보인다. 값은 그대로 복제 페이로드에 실리고
 * (`ULNPMassReplicator::ProcessClientReplication`), 게스트에서는 버블 핸들러가 같은 프래그먼트를 채운다.
 *
 * ⚠️ **쿼리에 `FLNPEnemyDyingTag`를 `None`으로 걸지 않는다.** 이 파일의 다른 적 프로세서는 전부
 * 그렇게 하고 있어 그대로 베끼기 쉬운데, 그러면 죽는 순간 엔티티가 쿼리에서 빠져
 * **`Dying`을 아무도 싣지 못한다** — 게스트에서 적이 소리 없이 사라진다.
 *
 * `ULNPEntityAttackProcessor`보다 뒤에 돈다. 같은 프레임에 진행된 공격 위상을 읽어야
 * 전이가 한 프레임 늦지 않는다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyActionProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyActionProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery ActionQuery;
};

/**
 * 행동 상태 채널의 눈 — 엔티티 위에 현재 `Action`과 전이 카운터를 찍는다.
 *
 * ⚠️ **이 프로젝트에서 서버/클라 분기가 없는 유일한 Mass 프로세서이고, 그것이 의도다.**
 * 다른 프로세서가 전부 `LNPMass::IsClientWorld()` 가드로 시작하는 것과 대조된다 —
 * "양쪽이 같은 입력을 보고 같은 그림을 그린다"가 이 채널의 존재 이유이므로,
 * 호스트와 게스트가 같은 코드로 같은 값을 그려야 채널이 성립했다는 증거가 된다.
 * 트랙 C(ISKM)가 붙기 전까지 이것이 유일한 소비처다.
 *
 * 색으로 현재 행동을, 로그로 전이를 남긴다. **연속 공격 2회가 2회로 보이는지**는 눈보다 로그가
 * 확실하다 — 호스트 로그와 게스트 로그를 그대로 대조할 수 있기 때문이다.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyActionDebugDrawProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyActionDebugDrawProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery ActionQuery;

	/** 전이만 골라 로그로 남기기 위한 직전 관측값. 디버그 전용이라 프래그먼트를 늘리지 않는다. */
	TMap<FMassEntityHandle, uint8> LastLoggedSeq;
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