// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "WorldCollision.h"
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
 * 모든 트레이스는 BeginBaking()에서 한 번에 발사되며, 결과는 매 Callback으로 수집된다.
 * 베이킹 완료 후 결과 Cache는 읽기 전용이 되어 Mass 워커 Thread에 안전하다.
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

	/** 모든 표면 트레이스를 비동기로 발사한다. World 생성 완료 후 GameMode가 호출. */
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

	static FVector IndexToDirection(int32 LatIdx, int32 LonIdx, int32 LatRes, int32 LonRes);
	static void DirectionToIndex(const FVector& Dir, int32 LatRes, int32 LonRes, int32& OutLatIdx, int32& OutLonIdx);

	void OnAsyncTraceComplete(const FTraceHandle& Handle, FTraceDatum& Data);

	/** 베이크된 데이터 SharedPtr. BeginBaking()마다 교체되어 재베이킹이 라이브 Snapshot을 손상시키지 않는다. */
	TSharedPtr<TArray<FPoint>> CacheData;

	/** 단일 공유 델리게이트 — 모든 비동기 트레이스가 여기로 Callback, UserData에 Sample Index를 담아서 사용. */
	FTraceDelegate TraceDelegate;

	int32 LatResolution = 64;
	int32 LonResolution = 128;
	int32 TotalSamples = 0;
	int32 CompletedCount = 0;
	float SphereRadius = 10000.0f;

	bool bIsBaking = false;
	bool bBakingComplete = false;
};
