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
 * Manages Niagara component lifetime for Mass projectile entities.
 *
 * Thread model:
 *  - Enqueue* methods are safe to call from any thread (Mass worker threads).
 *  - Flush*, Allocate*, Update*, Spawn* methods are game-thread only.
 *    Call FlushTrailReleases() and FlushPendingImpacts() from
 *    ULNPProjectileVisualizationProcessor before processing live entities.
 */
UCLASS()
class LOOTNPOP_API ULNPProjectileVisualSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	/** Spawns trail Niagara components for Entity. Game thread only. */
	void AllocateTrails(FMassEntityHandle Entity, const ULNPVFXData* VFXData, FVector Pos);

	/** Moves all trail components for Entity to Pos. Game thread only. */
	void UpdateTrails(FMassEntityHandle Entity, FVector Pos);

	/** Thread-safe: queues Entity's trails for destruction on the next FlushTrailReleases. */
	void EnqueueTrailRelease(FMassEntityHandle Entity);

	/** Drains the trail-release queue and destroys components. Game thread only. */
	void FlushTrailReleases();

	/** Spawns all SpawnEffects at Pos (fire-and-forget). Game thread only. */
	void SpawnSpawnEffects(const ULNPVFXData* VFXData, FVector Pos);

	/** Thread-safe: queues an impact effect to be spawned on the next FlushPendingImpacts. */
	void EnqueueImpact(const ULNPVFXData* VFXData, FVector Pos, FVector Normal);

	/** Drains the impact queue and spawns all pending effects. Game thread only. */
	void FlushPendingImpacts();

#if WITH_EDITOR
	/** Thread-safe: queues a surface impact position for debug visualization. */
	void EnqueueSurfaceImpactDebug(FVector Location);

	/** Drains the surface impact debug queue and draws persistent spheres. Game thread only. */
	void FlushSurfaceImpactDebug(UWorld* World, float SphereRadius);
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
	TQueue<FVector, EQueueMode::Mpsc> SurfaceImpactDebugQueue;
#endif
};
