// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "Subsystems/WorldSubsystem.h"
#include "LNPLoadBaseline.generated.h"

class UMassEntityConfigAsset;
class ULNPEnemyConfig;
struct FLNPSurfaceCacheSnapshot;

/**
 * Phase 3 구현 단위 4 — 고정 부하 시나리오 harness(Phase03 문서 §4). Phase 3b·6도 같은 harness와 seed를 쓴다.
 *
 * 켜는 방법은 실행 인자 하나다. 스폰은 월드 개시 직후 한 번뿐이라 콘솔로는 늦다(LNP.Spawn.EnemyDensity와 같은 이유).
 *     -LNPLoadBaseline=300        적 수. 0이거나 없으면 harness 전체가 no-op이다
 *     -LNPLoadBaselineSeed=1      적 배치 seed(기본 1). Pod 배치 seed도 이 값으로 고정된다
 *     -LNPLoadBaselinePlayers=2   서버가 capture를 시작하기 전에 기다리는 플레이어 수(기본 2)
 *     -LNPLoadBaselineProjectiles=500  유지할 전체 발사체 수(기본 500). 요인 분리용 대조 실행에서만 바꾼다
 *     -LNPLoadBaselineQuit        보고 뒤 프로세스를 종료한다(무인 실행)
 *
 * - 적: DA_MassSpawnConfig의 적 수를 0으로 두고(Pod는 유지), PlayerStart 영역(-Z 극) 주변 링에 정확히 N마리를 둔다.
 *   10마리 중 1마리가 ActorPromoted, 나머지는 PureEntity 근접·원거리 반반이다. 세력권 중심은 링 중심이다.
 * - 투사체: 서버가 실제 원거리 적 DA의 무기 값으로 발사체를 채워 전체 수를 500발로 유지한다. PureEntity 원거리 적이 쏜
 *   발사체도 전체 수에 들어간다. 주입분은 Multicast를 하지 않아 게스트 Ghost 비용은 자연 발사분만 포함한다.
 * - 플레이어는 harness가 켜져 있는 동안 피해를 받지 않는다(사망·리스폰이 측정 구간을 끊지 않게). 경직·넉백은 그대로다.
 * - 계측: 준비 완료 뒤 warm-up 10초, capture 30초. 프레임마다 MassWorldCollision 누적 counter의 차분으로 프레임당
 *   exact 합계·락 대기를 표본화하고 P50/P95/최대와 성공 기준 판정을 한 번 로그로 남긴다. 락 probe CVar를 켠다.
 */
namespace LNPLoadBaseline
{
	/** 모든 스레드. 실행 인자의 적 수. 0이면 harness가 꺼져 있다. */
	LOOTNPOP_API int32 GetEnemyCount();
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

	/** 적 종류 인덱스. 스폰 계획과 로그가 같은 순서를 쓴다. */
	enum class EEnemyKind : uint8 { ActorPromotedMelee, PureEntityMelee, PureEntityRanged, Count };

	/** i번째 적의 종류. 10마리 중 1마리가 ActorPromoted이고 나머지는 PureEntity 근접·원거리를 번갈아 둔다. */
	EEnemyKind GetEnemyKind(int32 Index);

	/** 게임 스레드. 적 종류별 Mass entity config. 없으면 nullptr. */
	UMassEntityConfigAsset* LoadEnemyEntityConfig(EEnemyKind Kind);

	/**
	 * 모든 스레드. -Z 극 주변 링에 Count개의 발밑 위치를 결정론적으로 고른다. OutCenter는 링 중심의 표면점이다.
	 * 링 넓이는 적 수에 비례해 키워 밀도를 일정하게 둔다. 자리를 못 찾은 개체는 빠지므로 결과 수를 확인한다.
	 */
	void BuildEnemyRing(const FLNPSurfaceCacheSnapshot& Cache, float SphereRadius, int32 Seed, int32 Count,
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
	void TopUpProjectiles();
	void SampleFrame();
	void Report();

	/** 서버. 주입 발사체의 무기 상수와 발사 위치 범위. */
	UPROPERTY(Transient)
	TObjectPtr<const ULNPEnemyConfig> ProjectileSourceConfig;

	FMassEntityQuery ProjectileQuery;
	FRandomStream ProjectileStream;
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
};
