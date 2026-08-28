// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "Containers/Queue.h"
#include "HitDetection/LNPProjectileMassTypes.h"
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
	/** Entity에 트레일 Niagara Component를 스폰하고 진영 색을 주입한다. 게임 Thread 전용. */
	void AllocateTrails(FMassEntityHandle Entity, const ULNPVFXData* VFXData, FVector Pos, ELNPInstigatorTeam Team);

	/** Entity의 모든 트레일 Component를 Pos로 이동한다. 게임 Thread 전용. */
	void UpdateTrails(FMassEntityHandle Entity, FVector Pos);

	/** Entity 트레일의 진영 색을 다시 주입한다 (패링으로 소유권이 넘어갔을 때). 게임 Thread 전용. */
	void SetTrailTeam(FMassEntityHandle Entity, ELNPInstigatorTeam Team);

	/** 진영에 대응하는 트레일 색 (LNPSettings). */
	static FLinearColor GetTeamTintColor(ELNPInstigatorTeam Team);

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

private:
	void ReleaseTrails(FMassEntityHandle Entity);
	void SpawnImpactEffects(const ULNPVFXData* VFXData, FVector Pos, FVector Normal);

	/** 트레일 Niagara 시스템이 노출하는 진영 색 User 파라미터 이름. */
	static const FName TintColorParameterName;

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

/**
 * Mass에 이 Subsystem의 Thread 모델을 알린다.
 *
 * 이 선언이 없으면 TMassExternalSubsystemTraits의 기본값(GameThreadOnly = true)이 적용되어,
 * AddSubsystemRequirement로 이 Subsystem을 요구하는 Processor가 통째로 게임 Thread로 승격된다
 * (UMassProcessor::CallsConfigureQueries의 bRequiresGameThreadExecution 계산).
 * Projectile 판정 Processor는 수백 발 규모를 전제로 설계했으므로 워커 Thread 실행이 필수다.
 *
 * Processor의 Execute()가 호출하는 것은 Enqueue* 뿐이고 그 경로는 MPSC TQueue라 동시 쓰기가 안전하다.
 * 게임 Thread 전용 메서드(Flush, Allocate, Update, Spawn 계열)는 ULNPProjectileVisualizationProcessor에만 있으며,
 * 그 Processor는 bRequiresGameThreadExecution = true로 명시되어 있다.
 */
template<>
struct TMassExternalSubsystemTraits<ULNPProjectileVisualSubsystem> final
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeWrite = true,
	};
};
