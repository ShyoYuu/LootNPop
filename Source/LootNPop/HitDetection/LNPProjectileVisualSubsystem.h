// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityHandle.h"
#include "Containers/Queue.h"
#include "LNPProjectileVisualSubsystem.generated.h"

class UNiagaraComponent;
class ULNPVFXData;

/**
 * Mass Projectile Entity의 Niagara Component 수명을 관리한다.
 *
 * Thread 모델:
 *  - Enqueue* 메서드는 어떤 Thread에서도 안전하게 호출 가능 (Mass 워커 Thread).
 *  - Flush*, Allocate*, Update*, Spawn* 메서드는 게임 Thread 전용.
 *    살아있는 Entity 처리 전에 ULNPProjectileVisualizationProcessor에서
 *    FlushTrailReleases()와 FlushPendingImpacts()를 호출해야 한다.
 */
UCLASS()
class LOOTNPOP_API ULNPProjectileVisualSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	/** Entity에 트레일 Niagara Component를 스폰한다. 게임 Thread 전용. */
	void AllocateTrails(FMassEntityHandle Entity, const ULNPVFXData* VFXData, FVector Pos);

	/** Entity의 모든 트레일 Component를 Pos로 이동한다. 게임 Thread 전용. */
	void UpdateTrails(FMassEntityHandle Entity, FVector Pos);

	/** Thread-Safe: 다음 FlushTrailReleases 시 Entity 트레일을 Destroy 큐에 추가한다. */
	void EnqueueTrailRelease(FMassEntityHandle Entity);

	/** 트레일 해제 큐를 비우고 Component를 Destroy한다. 게임 Thread 전용. */
	void FlushTrailReleases();

	/** Pos에서 모든 SpawnEffects를 스폰한다 (fire-and-forget). 게임 Thread 전용. */
	void SpawnSpawnEffects(const ULNPVFXData* VFXData, FVector Pos);

	/** Thread-Safe: 다음 FlushPendingImpacts 시 스폰할 임팩트 이펙트를 큐에 추가한다. */
	void EnqueueImpact(const ULNPVFXData* VFXData, FVector Pos, FVector Normal);

	/** 임팩트 큐를 비우고 대기 중인 모든 이펙트를 스폰한다. 게임 Thread 전용. */
	void FlushPendingImpacts();

#if WITH_EDITOR
	/** Thread-Safe: 디버그 시각화를 위한 표면 임팩트 위치를 큐에 추가한다. */
	void EnqueueSurfaceImpactDebug(FVector Location, FColor Color, float SphereRadius);

	/** 표면 임팩트 디버그 큐를 비우고 지속 구체를 그린다. 게임 Thread 전용. */
	void FlushSurfaceImpactDebug(UWorld* World);
#endif

private:
	void ReleaseTrails(FMassEntityHandle Entity);
	void SpawnImpactEffects(const ULNPVFXData* VFXData, FVector Pos, FVector Normal);

	struct FPendingImpact
	{
		const ULNPVFXData* VFXData;
		FVector            Location;
		FVector            Normal;
	};

	TMap<FMassEntityHandle, TArray<TObjectPtr<UNiagaraComponent>>> ActiveTrails;
	TQueue<FMassEntityHandle, EQueueMode::Mpsc>                    TrailReleaseQueue;
	TQueue<FPendingImpact,    EQueueMode::Mpsc>                    ImpactQueue;

#if WITH_EDITOR
	struct FImpactDebug
	{
		FVector Location;
		FColor  Color;
		float   SphereRadius;
	};

	TQueue<FImpactDebug, EQueueMode::Mpsc> SurfaceImpactDebugQueue;
#endif
};
