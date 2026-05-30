// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "MassEntityHandle.h"
#include "Async/Future.h"
#include "LNPMassSpawnSubsystem.generated.h"

class ULNPMassSpawnConfig;
class UMassEntityConfigAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLNPOnSpawningComplete);

/** 스폰 요청된 Entity 타입 */
enum class ELNPSpawnRequestType : uint8
{
	LootPod,
	Enemy
};

/** Pod 스폰과 연관된 Enemy들 사이의 공유 상태 */
struct FLNPSpawnLink
{
	FMassEntityHandle PodHandle;
	FVector PodLocation = FVector::ZeroVector;
};

/** 스폰 대기 중인 Entity 배치를 추적하는 내부 구조체 */
struct FLNPMassSpawnRequest
{
	TObjectPtr<UMassEntityConfigAsset> ConfigAsset;
	TArray<FTransform> TargetTransforms;
	int32 ProcessedCount = 0;

	/** 이 요청의 Entity 타입 */
	ELNPSpawnRequestType RequestType = ELNPSpawnRequestType::Enemy;

	/** Pod Handle을 Enemy에게 전달하기 위한 공유 링크 */
	TSharedPtr<FLNPSpawnLink> SpawnLink;

	bool IsComplete() const { return ProcessedCount >= TargetTransforms.Num(); }
};

/**
 * 비동기 큐 빌드 태스크의 출력: 스폰 요청당 하나의 항목, UObject 참조 없음.
 * UObject 참조(ConfigAsset)는 CapturedAssets Index를 통해 게임 Thread에서 해결된다.
 */
struct FLNPAsyncSpawnEntry
{
	TArray<FTransform> Transforms;
	ELNPSpawnRequestType RequestType = ELNPSpawnRequestType::Enemy;
	TSharedPtr<FLNPSpawnLink> SpawnLink;
	int32 AssetIndex = -1;
};

/**
 * Mass Entity(LootPod 및 Enemy)의 Hierarchical 스폰을 담당하는 Subsystem.
 * 프레임 예산 관리, 표면 유효성 검사, 스폰 후 수동 Transform 적용을 처리한다.
 * ULNPSettings의 전역 설정을 사용한다.
 */
UCLASS()
class LOOTNPOP_API ULNPMassSpawnSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// End USubsystem

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return SpawnBuildFuture.IsValid() || SpawnQueueHead < SpawnQueue.Num(); }
	virtual TStatId GetStatId() const override;
	// End FTickableGameObject

	/** config를 기반으로 스폰 프로세스를 시작하는 진입점 */
	UFUNCTION(BlueprintCallable, Category = "LNP|Mass Spawning")
	void EnqueueSpawnProject(ULNPMassSpawnConfig* InConfig, const float SphereRadius);

	/** 표면 베이킹 완료 후 GameMode가 호출하여 Entity 스폰을 시작한다 */
	void BeginSpawning();

	/** 모든 큐의 Entity가 스폰되면 발동 */
	UPROPERTY(BlueprintAssignable, Category = "LNP|Mass Spawning")
	FLNPOnSpawningComplete OnSpawningComplete;

protected:

	/**
	 * 스폰된 Entity를 Transform과 선택적 메타데이터 (Leash 등)로 초기화한다.
	 */
	void SetupSpawnedEntities(TConstArrayView<FMassEntityHandle> Entities, TConstArrayView<FTransform> Transforms, FMassEntityHandle ParentLootPod = FMassEntityHandle(), const FVector& ParentPodLocation = FVector::ZeroVector);

private:
	/** 스폰 큐의 Chunk를 처리한다 */
	void ProcessQueue();

	/** 완료된 비동기 빌드 결과로 SpawnQueue를 조립한다. 게임 Thread에서 호출. */
	void AssembleSpawnQueueFromAsyncResult();

	/** 현재 처리 중인 활성 config */
	UPROPERTY(Transient)
	TObjectPtr<ULNPMassSpawnConfig> ActiveConfig;

	/** 비동기 태스크 실행 중 GC 안전을 위해 미리 캡처된 UObject 참조 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMassEntityConfigAsset>> CapturedAssets;

	TArray<FLNPMassSpawnRequest> SpawnQueue;
	int32 SpawnQueueHead = 0;

	/** 비동기 빌드 태스크가 작성한 결과; IsReady() 이후 게임 Thread에서 읽음 */
	TArray<FLNPAsyncSpawnEntry> AsyncBuildResult;

	/** 백그라운드 큐 빌드 태스크의 Future */
	TFuture<void> SpawnBuildFuture;

	FRandomStream RandomStream;
};
