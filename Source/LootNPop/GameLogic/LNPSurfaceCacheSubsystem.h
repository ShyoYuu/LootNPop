// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "WorldCollision.h"
#include "Mass/ExternalSubsystemTraits.h"
#include <atomic>
#include "LNPSurfaceCacheSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLNPOnBakingComplete);

/**
 * 베이크된 표면 Cache의 Thread-Safe Snapshot.
 * 베이킹 완료 후 게임 Thread에서 생성되며, 어떤 Thread에서도 읽기 안전하다.
 */
struct LOOTNPOP_API FLNPSurfaceCacheSnapshot
{
	struct FPoint
	{
		FVector Loc = FVector::ZeroVector;
		bool bValid = false;
	};

	/** 베이크된 데이터의 Shared 참조. 복사 없음 — 여러 Snapshot이 하나의 할당을 공유. */
	TSharedPtr<const TArray<FPoint>> Points;
	int32 LatRes = 0;
	int32 LonRes = 0;

	bool IsValid() const { return Points.IsValid(); }
	bool GetPoint(const FVector& WorldDirection, FVector& OutPoint) const;
};

/**
 * 비동기 라인 트레이스로 구형 등장방형 격자의 표면 히트 지점을 사전 계산한다.
 * 트레이스는 Tick()이 프레임당 SurfaceCacheSamplesPerFrame개씩 나눠 발사하며, 결과는 매 Callback으로 수집된다.
 * 베이킹 완료 후 결과 Cache는 읽기 전용이 되어 Mass 워커 Thread에 안전하다.
 *
 * 베이킹은 머신당 1회다 — 서버는 ALNPGameMode가, 클라이언트는 ALNPGameState의 투-게이트가 각각 호출한다.
 * 완료 후 재호출은 BeginBaking()이 자체적으로 차단하므로, Mass 워커가 읽는 도중 CacheData가 교체되지 않는다.
 */
UCLASS()
class LOOTNPOP_API ULNPSurfaceCacheSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bIsBaking; }
	virtual TStatId GetStatId() const override;
	// End FTickableGameObject

	/**
	 * 표면 베이킹을 시작한다. World 생성 완료 후 GameMode가 호출.
	 * 실제 트레이스 발사는 Tick()이 프레임당 SurfaceCacheSamplesPerFrame개씩 나눠서 수행한다.
	 */
	void BeginBaking();

	/**
	 * Thread-Safe 표면 지점 조회.
	 * 주어진 World 방향과 가장 가까운 베이크된 표면 히트 지점을 반환한다.
	 * OnBakingComplete 발동 후에만 유효하다.
	 */
	bool GetSurfacePoint(const FVector& WorldDirection, FVector& OutPoint) const;

	/** 베이킹 진행률을 [0, 1]로 반환한다. OnBakingComplete 발동 시 1.0이 된다. */
	float GetBakingProgress() const;

	/** 백그라운드 Thread 사용을 위한 Thread-Safe Snapshot을 생성한다. OnBakingComplete 이후에만 유효. */
	FLNPSurfaceCacheSnapshot TakeSnapshot() const;

	UPROPERTY(BlueprintAssignable, Category = "LNP|Surface Cache")
	FLNPOnBakingComplete OnBakingComplete;

private:
	using FPoint = FLNPSurfaceCacheSnapshot::FPoint;

	/** 격자 셀 Index를 셀 중심의 단위 방향 벡터로 변환한다 (베이킹 시 트레이스 방향 계산용). */
	static FVector IndexToDirection(int32 LatIdx, int32 LonIdx, int32 LatRes, int32 LonRes);

	void OnAsyncTraceComplete(const FTraceHandle& Handle, FTraceDatum& Data);

	/** Tick에서 호출. 아직 발사하지 않은 샘플을 최대 Count개까지 발사한다. */
	void IssuePendingTraces(int32 Count);

	/** 베이크된 데이터 SharedPtr. BeginBaking()마다 교체되어 재베이킹이 라이브 Snapshot을 손상시키지 않는다. */
	TSharedPtr<TArray<FPoint>> CacheData;

	/** 단일 공유 델리게이트 — 모든 비동기 트레이스가 여기로 Callback, UserData에 Sample Index를 담아서 사용. */
	FTraceDelegate TraceDelegate;

	int32 LatResolution = 64;
	int32 LonResolution = 128;
	int32 TotalSamples = 0;
	int32 CompletedCount = 0;

	/** 다음에 발사할 샘플 Index. TotalSamples에 도달하면 발사가 끝난 것이고, 완료는 CompletedCount가 판단한다. */
	int32 NextSampleToIssue = 0;
	int32 SamplesPerFrame = 2000;

	float SphereRadius = 10000.0f;

	/** 게임 Thread 전용 — 발사 진행 상태. IsTickable()과 Tick()만 읽는다. */
	bool bIsBaking = false;

	/**
	 * 배열이 불변으로 확정됐음을 알리는 게시(publish) 플래그.
	 *
	 * 게임 Thread가 배열을 다 채운 뒤 release로 세우고, 워커 Thread가 acquire로 읽는다.
	 * 이 순서 덕에 플래그가 true로 보이는 Thread는 배열 쓰기도 전부 볼 수 있다 — 락 없이 성립하는 유일한 근거다.
	 * 평범한 bool로 두면 x86에서는 우연히 동작하지만 약한 메모리 모델에서는 보장이 없다.
	 */
	std::atomic<bool> bBakingComplete = false;
};

/**
 * Mass에 이 Subsystem의 Thread 모델을 알린다.
 *
 * 이 선언이 없으면 기본값(GameThreadOnly = true)이 적용되어, SurfaceCache를 요구하는 Processor
 * (ULNPProjectileMovementProcessor, ULNPEnemyMovementProcessor)가 통째로 게임 Thread로 승격된다.
 *
 * 워커 Thread가 호출하는 것은 읽기 전용 조회(GetSurfacePoint)뿐이고, 그 대상 배열은
 * bBakingComplete가 서고 난 뒤 불변이다. 베이킹 중에는 조회가 false를 반환해 진입 자체를 막는다.
 * 쓰기 경로(BeginBaking / OnAsyncTraceComplete)는 게임 Thread 전용이므로 ThreadSafeWrite는 false로 둔다.
 */
template<>
struct TMassExternalSubsystemTraits<ULNPSurfaceCacheSubsystem> final
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeWrite = false,
	};
};
