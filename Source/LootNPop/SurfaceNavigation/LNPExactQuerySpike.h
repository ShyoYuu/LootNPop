// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MassProcessor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LNPExactQuerySpike.generated.h"

class UPrimitiveComponent;
class UStaticMeshComponent;

/**
 * Phase 3 Gate 0 스파이크 — Mass worker에서 LNPWorldExact 동기 scene query를 실행하고 안전성·비용·결과 일치를 확인한다.
 * 프로덕션 경로가 아니다. CVar LNP.SurfaceNav.ExactSpike.QueriesPerFrame이 0이면 아무것도 하지 않는다.
 *
 * - 질의 집합: 월드 중심에서 나가는 고정 Fibonacci 방향 SetSize개. 방향 k의 질의 종류는 k % 3(line·sphere·capsule)이다.
 *   매 프레임 커서부터 QueriesPerFrame개를 질의하므로 한 바퀴가 돌면 결과 표가 결정론적으로 채워진다.
 * - 결과 표 해시: 정적 hit·miss·미해석만 해시한다. 서버·클라이언트, Editor·-game의 해시가 같아야 한다.
 * - 동적 body: ALNPExactSpikeMover가 게임 스레드 TG_PrePhysics에서 kinematic body를 움직여 쓰기 락 경합을 만든다.
 * - 락 대기: 쿼리 직전에 같은 scene read lock을 한 번 잡았다 놓는 시간으로 추정한다. 엔진 쿼리 내부의 대기와 같은 조건을
 *   표본으로 보는 근사다. 설치형 엔진이라 쿼리 내부 대기를 직접 분리할 수 없다.
 */
namespace LNPExactQuerySpike
{
	/** 질의 방향 수. 3의 배수라 종류별로 1024개씩이다. */
	inline constexpr int32 SetSize = 3072;
	inline constexpr int32 QueryTypeCount = 3;
}

/** 결과 표의 한 칸. */
struct FLNPExactSpikeResult
{
	enum class EKind : uint8 { Unset, Miss, Known, Dynamic, Unknown };

	EKind Kind = EKind::Unset;
	int8 Slot = INDEX_NONE;
	uint8 Lifetime = 0;
	int32 DistanceCm = 0;
	int32 FaceIndex = INDEX_NONE;
	int32 InstanceIndex = INDEX_NONE;
};

/** 한 프레임의 processor 실행 결과. */
struct FLNPExactSpikeFrame
{
	/** 질의 i의 종류는 (Start + i) % 3이다. */
	int32 Start = 0;
	TArray<uint32> QueryNs;
	TArray<uint32> LockNs;
	uint64 WallNs = 0;
	uint32 Hits[LNPExactQuerySpike::QueryTypeCount] = {};
	uint32 UnknownHits = 0;
	uint32 DynamicHits = 0;
	uint32 ClassificationErrors = 0;
	bool bOnGameThread = false;
};

/** 게임 스레드에서 움직이는 kinematic body 묶음. 머신마다 로컬로 스폰하며 복제하지 않는다. */
UCLASS(NotPlaceable, Transient)
class LOOTNPOP_API ALNPExactSpikeMover : public AActor
{
	GENERATED_BODY()

public:
	ALNPExactSpikeMover();

	/** 방향마다 지면을 찾아 그 위에 body를 하나씩 만든다. 지면이 없는 방향은 건너뛴다. */
	void Setup(const TArray<FVector>& InDirections);

	const TArray<TObjectPtr<UStaticMeshComponent>>& GetBodies() const { return Bodies; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Bodies;

	TArray<FVector> Directions;
	TArray<float> BaseRadii;
};

/** 스파이크 상태·통계와 동적 body 관리. */
UCLASS()
class LOOTNPOP_API ULNPExactQuerySpikeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** worker. 옥탄트 생성이 끝나 slot source가 게시될 수 있는 상태. */
	bool IsReady() const { return bReady; }

	/** worker. processor만 호출한다. 이번 프레임 질의 시작 커서를 받고 Count만큼 전진한다. */
	int32 AdvanceCursor(int32 Count);

	/** worker. 서로 다른 k에만 쓰므로 한 프레임 안에서 경합하지 않는다. */
	FLNPExactSpikeResult& GetResultSlot(int32 Index) { return Results[Index]; }

	/** worker. 스파이크 body인지. 게시는 Tick에서만 한다. */
	bool IsSpikeBody(const TWeakObjectPtr<UPrimitiveComponent>& Component) const;

	void RecordFrame(FLNPExactSpikeFrame&& Frame);

	/** 게임 스레드. */
	void ResetStats();
	void Report() const;

	static FVector GetDirection(int32 Index);
	float GetSphereRadius() const { return SphereRadius; }

	// UTickableWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void RebuildMovers(int32 Count);

	struct FWeakComponentKeyFuncs : BaseKeyFuncs<TWeakObjectPtr<UPrimitiveComponent>, TWeakObjectPtr<UPrimitiveComponent>, false>
	{
		static KeyInitType GetSetKey(ElementInitType Element) { return Element; }
		static bool Matches(KeyInitType A, KeyInitType B) { return A.HasSameIndexAndSerialNumber(B); }
		static uint32 GetKeyHash(KeyInitType Key) { return GetTypeHash(Key); }
	};
	using FBodySet = TSet<TWeakObjectPtr<UPrimitiveComponent>, FWeakComponentKeyFuncs>;

	UPROPERTY(Transient)
	TObjectPtr<ALNPExactSpikeMover> Mover;

	int32 MoverCount = 0;
	TSharedRef<const FBodySet, ESPMode::ThreadSafe> SpikeBodies = MakeShared<FBodySet, ESPMode::ThreadSafe>();

	bool bReady = false;
	float SphereRadius = 25000.f;
	int32 Cursor = 0;

	/** AutoCapture 진행: 0 warm-up, 1 capture, 2 완료. */
	int32 AutoCaptureStage = 0;
	double AutoCaptureReadyTime = -1.0;
	TArray<FLNPExactSpikeResult> Results;

	/** 통계. processor(worker)가 쓰고 콘솔 명령(게임 스레드)이 읽는다. */
	mutable FCriticalSection StatsLock;
	TArray<uint32> QueryNs[LNPExactQuerySpike::QueryTypeCount];
	TArray<uint32> LockNs;
	TArray<uint32> FrameQueryNs;
	TArray<uint32> FrameLockNs;
	TArray<uint32> FrameWallNs;
	uint64 QueryCount[LNPExactQuerySpike::QueryTypeCount] = {};
	uint64 HitCount[LNPExactQuerySpike::QueryTypeCount] = {};
	uint64 UnknownHits = 0;
	uint64 DynamicHits = 0;
	uint64 ClassificationErrors = 0;
	uint64 WorkerFrames = 0;
	uint64 GameThreadFrames = 0;
};

/** Mass worker에서 LNPWorldExact 동기 질의를 실행한다. entity를 순회하지 않으므로 query 기반 pruning을 끈다. */
UCLASS()
class LOOTNPOP_API ULNPExactQuerySpikeProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPExactQuerySpikeProcessor();

	virtual bool ShouldAllowQueryBasedPruning(const bool bRuntimeMode = true) const override { return false; }

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
};
