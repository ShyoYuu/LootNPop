// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "Subsystems/WorldSubsystem.h"
#include "LNPLoadBaseline.generated.h"

class UMassEntityConfigAsset;
class ULNPEnemyConfig;
class ULNPMassWorldCollisionSubsystem;

/**
 * Phase 3 구현 단위 4 — 고정 부하 시나리오 harness(Phase03 문서 §4). Phase 3b·6도 같은 harness와 seed를 쓴다.
 *
 * 켜는 방법은 실행 인자 하나다. 스폰은 월드 개시 직후 한 번뿐이라 콘솔로는 늦다(LNP.Spawn.EnemyDensity와 같은 이유).
 *     -LNPLoadBaseline=300        지상 적 수. 비행 수와 함께 0이거나 없으면 harness 전체가 no-op이다
 *     -LNPLoadBaselineFlyers=100  비행 적 수(기본 0, Phase03c §3.8)
 *     -LNPLoadBaselineSeed=1      적 배치 seed(기본 1). Pod 배치 seed도 이 값으로 고정된다
 *     -LNPLoadBaselinePlayers=2   서버가 capture를 시작하기 전에 기다리는 플레이어 수(기본 2)
 *     -LNPLoadBaselineProjectiles=500  유지할 전체 발사체 수(기본 500). 요인 분리용 대조 실행에서만 바꾼다
 *     -LNPLoadBaselineQuit        보고 뒤 프로세스를 종료한다(무인 실행)
 *
 * - 적: DA_MassSpawnConfig의 적 수를 0으로 두고(Pod는 유지), slot 4 큰 섬 가장자리 아래를 중심으로 한 링에 정확히 N마리를 둔다.
 *   링 일부가 섬 아래 지각을 지나고, 20마리 중 2마리(근접·원거리 1마리씩)는 섬 윗면에 둔다(Phase03b §3.6).
 *   배치는 exact support probe로 층을 골라 찍어 섬 위와 섬 아래 지각을 명시적으로 구분한다.
 *   10마리 중 1마리가 ActorPromoted, 나머지는 PureEntity 근접·원거리 반반이다. 세력권 중심은 링 중심이다.
 * - 비행 적: 같은 방식의 두 번째 링(다른 seed)의 지면점에서 스폰 규약대로 배회 고도 하한까지 띄운다. Home은 링 중심(섬 가장자리 아래 지각)이라
 *   배회·교전 경로가 섬 밑면·측벽 사이를 지난다. 합성 넉백·분포·이동 이벤트는 지상 개념이라 비행 적을 세지 않는다.
 * - 플레이어: 준비가 끝나면 서버가 모든 플레이어를 링 중심에 다시 스폰한다(Mover 텔레포트는 게스트에 전달되지 않는다). 적이 추격·공격하고 Actor로 승격되는 부하를 유지하고,
 *   섬 위 적이 가장자리 아래 플레이어를 쫓다가 떨어지게 한다. 지상 적이 0이고 비행 적만 있으면 링 중심 쪽 큰 섬 가장자리에서 500cm 안쪽
 *   윗면에 스폰한다 — Home의 시야·세력권 안이라 비행 적이 교전하고, 섬 밑에서 가장자리를 돌아 올라가야 쏠 수 있다(Phase03c §3.8).
 *   혼합 실행은 지상 기준선과 비교하려고 링 중심을 유지한다.
 * - 투사체: 서버가 실제 원거리 적 DA의 무기 값으로 발사체를 채워 전체 수를 500발로 유지한다. PureEntity 원거리 적이 쏜
 *   발사체도 전체 수에 들어간다. 주입분은 Multicast를 하지 않아 게스트 Ghost 비용은 자연 발사분만 포함한다.
 * - 합성 넉백: 서버가 접지한 PureEntity 적마다 평균 10초에 한 번 고정 세기 넉백을 준다. 섬 위 적은 섬 중심 반대쪽으로 민다.
 *   플레이어가 무적이라 적을 때리지 않으므로 공중 경로 부하를 이것으로 만든다.
 * - 플레이어는 harness가 켜져 있는 동안 피해를 받지 않는다(사망·리스폰이 측정 구간을 끊지 않게). 경직·넉백은 그대로다.
 * - 계측: 준비 완료 뒤 warm-up 10초, capture 30초. capture 시작에 MassWorldCollision 통계를 비우고, 프레임마다 누적 counter의
 *   차분으로 프레임당 exact 합계·락 대기를 표본화한다. 서버는 적 이동 이벤트(낙하·착지·섬 이탈·층 순간이동)를 센다.
 *   P50/P95/최대와 성공 기준 판정을 한 번 로그로 남긴다. 락 probe CVar를 켠다.
 *   비행 적은 steering 결과 누계(LNPEnemyFlightStats)를 함께 남긴다. 관통 검출(LNP.SurfaceNav.LoadBaseline.FlightPenetrationCheck)은
 *   비행 적마다 프레임당 query 1회를 더해 프레임·exact 통계를 오염시키므로 켠 실행은 관통 판정에만 쓴다.
 */
namespace LNPLoadBaseline
{
	/** 모든 스레드. 실행 인자의 지상 적 수. */
	LOOTNPOP_API int32 GetEnemyCount();
	/** 모든 스레드. 실행 인자의 비행 적 수. */
	LOOTNPOP_API int32 GetFlyerCount();
	/** 모든 스레드. 지상·비행 적 수의 합이 0이면 harness가 꺼져 있다. */
	LOOTNPOP_API bool IsActive();
	LOOTNPOP_API int32 GetSeed();
	int32 GetTargetProjectiles();

	/** 합의한 고정값(Phase03 문서 §4). */
	inline constexpr int32 DefaultTargetProjectiles = 500;
	inline constexpr double WarmupSeconds = 10.0;
	inline constexpr double CaptureSeconds = 30.0;
	inline constexpr float FrameBudgetMs = 16.6f;
	inline constexpr float ExactP95BudgetMs = 2.f;
	inline constexpr float LockP95BudgetMs = 0.2f;

	/** 합성 넉백(Phase03b §3.6). 적마다 평균 간격과 속도(cm/s). 방향 0.7·Up 0.3 가중은 실제 넉백과 같다. */
	inline constexpr float KnockbackMeanIntervalSeconds = 10.f;
	inline constexpr float KnockbackStrength = 1200.f;

	/** 적 종류 인덱스. 스폰 계획과 로그가 같은 순서를 쓴다. */
	enum class EEnemyKind : uint8 { ActorPromotedMelee, PureEntityMelee, PureEntityRanged, Count };

	/** i번째 적의 종류. 10마리 중 1마리가 ActorPromoted이고 나머지는 PureEntity 근접·원거리를 번갈아 둔다. */
	EEnemyKind GetEnemyKind(int32 Index);

	/** 게임 스레드. 적 종류별 Mass entity config. 없으면 nullptr. */
	UMassEntityConfigAsset* LoadEnemyEntityConfig(EEnemyKind Kind);

	/** 게임 스레드. 비행 적 Mass entity config. 없으면 nullptr. */
	UMassEntityConfigAsset* LoadFlyerEntityConfig();

	/** 비행 적 링 배치 seed = 적 seed + 이 값. 지상 링과 자리가 겹치지 않게 한다. */
	inline constexpr int32 FlyerSeedOffset = 7919;

	/** 모든 스레드. 링 중심(큰 섬 가장자리 아래 지각)의 발밑 위치. exact probe가 지각을 못 찾으면 false. */
	bool FindRingCenter(const ULNPMassWorldCollisionSubsystem& Collision, FVector& OutCenter);

	/**
	 * 모든 스레드. 링과 섬 윗면에 Count개의 발밑 위치를 결정론적으로 고른다. OutCenter는 링 중심의 발밑 위치다.
	 * 링 넓이는 적 수에 비례해 키워 밀도를 일정하게 둔다. 자리를 못 찾은 개체는 빠지므로 결과 수를 확인한다.
	 * OutLocations[i]는 i번째 적(GetEnemyKind(i))의 자리다. 빠진 개체가 있으면 뒤 개체가 앞으로 당겨진다.
	 */
	void BuildEnemyRing(const ULNPMassWorldCollisionSubsystem& Collision, float SphereRadius, int32 Seed, int32 Count,
		TArray<FVector>& OutLocations, FVector& OutCenter);
}

/** 부하 harness 실행·계측. harness가 꺼져 있으면 틱하지 않는다. */
UCLASS()
class LOOTNPOP_API ULNPLoadBaselineSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// UTickableWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool IsReadyToMeasure() const;
	void RespawnPlayersAtRing();
	void TopUpProjectiles();

	/** 서버. 합성 넉백을 주고 적 이동 이벤트·분포를 센다. bCount가 false면 상태만 갱신한다(warm-up). */
	void DriveEnemies(float DeltaTime, bool bCount);
	/** 서버. 관통 검출 CVar가 켜져 있으면 살아 있는 비행 적 캡슐이 exact 지형과 겹치는지 본다. */
	void CheckFlyerPenetration();
	void SampleFrame();
	void Report();

	/** 서버. 주입 발사체의 무기 상수와 발사 위치 범위. */
	UPROPERTY(Transient)
	TObjectPtr<const ULNPEnemyConfig> ProjectileSourceConfig;

	FMassEntityQuery ProjectileQuery;
	/** Actor 없는 지상 적(비행 제외). */
	FMassEntityQuery EnemyQuery;
	/** 살아 있는 비행 적. */
	FMassEntityQuery FlyerQuery;
	FRandomStream ProjectileStream;
	FRandomStream KnockbackStream;
	FVector RingCenter = FVector::ZeroVector;
	float RingOuterRadius = 0.f;

	enum class EStage : uint8 { Waiting, Warmup, Capture, Done };
	EStage Stage = EStage::Waiting;
	double StageStartTime = 0.0;

	/** capture 직전 누적 counter. 프레임 차분의 기준이다. */
	uint64 LastQueryCount = 0;
	uint64 LastQueryNs = 0;
	uint64 LastLockNs = 0;
	uint64 StartUnknownHits = 0;
	uint64 StartEnvelopeEscapes = 0;

	/** 서버. capture 구간 관통 검출 결과. */
	int32 PenetrationFrames = 0;
	int32 PenetrationEntityFrames = 0;
	int32 PenetrationCheckedFrames = 0;

	/** 서버. 비행 적 행동 상태(ELNPTargetingState: None·Alert·Confirmed)별 1초 표본 합과 표본 수. */
	int64 FlyerStateSums[3] = {};
	int32 FlyerStateSampleCount = 0;

	/** capture 표본(프레임당). 프레임 시간은 0.01ms 단위, exact·락은 ns, 나머지는 개수. */
	TArray<uint64> FrameMs100;
	TArray<uint64> FrameExactNs;
	TArray<uint64> FrameLockNs;
	TArray<uint64> FrameQueryCount;
	TArray<int32> ProjectileSamples;
	TArray<int32> PromotedActorSamples;
	int32 InjectedProjectiles = 0;
	double LastFrameWallTime = 0.0;
	double NextActorSampleTime = 0.0;

	/** 서버. Actor 없는 적의 직전 프레임 상태. 전이로 이벤트를 판정한다. */
	struct FEnemyTrack
	{
		float Radius = 0.f;
		bool bAirborne = false;
		/** 직전 공중 전이가 harness 넉백이었는지. 아니면 지지면 상실(낙하)이다. */
		bool bKnockedByHarness = false;
		/** 섬 윗면에서 공중으로 떠난 뒤 아직 착지하지 않았는지. */
		bool bLeftIslandTop = false;
	};
	TMap<FMassEntityHandle, FEnemyTrack> EnemyTracks;

	/** 서버. 적 분포(마지막 DriveEnemies 호출 기준). */
	struct FPopulation
	{
		int32 IslandTop = 0;
		int32 UnderIsland = 0;
		int32 OpenCrust = 0;
		int32 Airborne = 0;
	};
	FPopulation Population;
	FPopulation CaptureStartPopulation;

	/** 서버. capture 구간 이벤트 수. */
	struct FEvents
	{
		int32 Knockbacks = 0;
		/** harness 넉백 없이 접지→공중(지지면 상실). */
		int32 Falls = 0;
		int32 Landings = 0;
		/** 섬 윗면에서 공중으로 떠남. */
		int32 IslandLeaves = 0;
		/** 섬 윗면을 떠나 섬 아래(지각)에 착지함. */
		int32 IslandDrops = 0;
		/** 접지 상태로 한 프레임에 반지름이 LayerJumpThreshold 넘게 변함. legacy의 섬 윗면 순간이동을 잡는다. */
		int32 LayerJumps = 0;
	};
	FEvents Events;
};
