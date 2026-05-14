// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPProjectileVisualSubsystem.h"
#include "VFX/LNPVFXData.h"

#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#if WITH_EDITOR
#include "DrawDebugHelpers.h"
#endif

void ULNPProjectileVisualSubsystem::AllocateTrails(FMassEntityHandle Entity, const ULNPVFXData* VFXData, FVector Pos)
{
	if (nullptr == VFXData || VFXData->TrailEffects.IsEmpty())
		return;

	TArray<TObjectPtr<UNiagaraComponent>>& Components = ActiveTrails.FindOrAdd(Entity);
	Components.Reserve(VFXData->TrailEffects.Num());

	UWorld* World = GetWorld();
	for (UNiagaraSystem* NS : VFXData->TrailEffects)
	{
		if (nullptr == NS)
			continue;

		UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, NS, Pos, FRotator::ZeroRotator, FVector::OneVector,
			/*bAutoDestroy=*/false, /*bAutoActivate=*/true, ENCPoolMethod::None);
		if (Comp != nullptr)
			Components.Add(Comp);
	}
}

void ULNPProjectileVisualSubsystem::UpdateTrails(FMassEntityHandle Entity, FVector Pos)
{
	if (TArray<TObjectPtr<UNiagaraComponent>>* Components = ActiveTrails.Find(Entity))
	{
		for (UNiagaraComponent* Comp : *Components)
		{
			if (Comp != nullptr)
				Comp->SetWorldLocation(Pos);
		}
	}
}

void ULNPProjectileVisualSubsystem::EnqueueTrailRelease(FMassEntityHandle Entity)
{
	TrailReleaseQueue.Enqueue(Entity);
}

void ULNPProjectileVisualSubsystem::FlushTrailReleases()
{
	FMassEntityHandle Entity;
	while (TrailReleaseQueue.Dequeue(Entity))
		ReleaseTrails(Entity);
}

void ULNPProjectileVisualSubsystem::ReleaseTrails(FMassEntityHandle Entity)
{
	if (TArray<TObjectPtr<UNiagaraComponent>>* Components = ActiveTrails.Find(Entity))
	{
		for (UNiagaraComponent* Comp : *Components)
		{
			if (Comp != nullptr)
				Comp->DestroyComponent();
		}
		ActiveTrails.Remove(Entity);
	}
}

void ULNPProjectileVisualSubsystem::SpawnSpawnEffects(const ULNPVFXData* VFXData, FVector Pos)
{
	if (nullptr == VFXData)
		return;

	UWorld* World = GetWorld();
	for (UNiagaraSystem* NS : VFXData->SpawnEffects)
	{
		if (NS != nullptr)
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, NS, Pos);
	}
}

void ULNPProjectileVisualSubsystem::EnqueueImpact(const ULNPVFXData* VFXData, FVector Pos, FVector Normal)
{
	ImpactQueue.Enqueue({ VFXData, Pos, Normal });
}

void ULNPProjectileVisualSubsystem::FlushPendingImpacts()
{
	FPendingImpact Impact;
	while (ImpactQueue.Dequeue(Impact))
	{
		SpawnImpactEffects(Impact.VFXData, Impact.Location, Impact.Normal);
	}
}

void ULNPProjectileVisualSubsystem::SpawnImpactEffects(const ULNPVFXData* VFXData, FVector Pos, FVector Normal)
{
	if (nullptr == VFXData)
		return;

	UWorld* World = GetWorld();
	const FRotator Rot = Normal.ToOrientationRotator();
	for (UNiagaraSystem* NS : VFXData->ImpactEffects)
	{
		if (NS != nullptr)
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, NS, Pos, Rot);
	}
}

#if WITH_EDITOR
void ULNPProjectileVisualSubsystem::EnqueueSurfaceImpactDebug(FVector Location, FColor Color, float SphereRadius)
{
	SurfaceImpactDebugQueue.Enqueue({ Location, Color, SphereRadius });
}

void ULNPProjectileVisualSubsystem::FlushSurfaceImpactDebug(UWorld* World)
{
	FImpactDebug Debug;
	while (SurfaceImpactDebugQueue.Dequeue(Debug))
		DrawDebugSphere(World, Debug.Location, Debug.SphereRadius, 8, Debug.Color, false, 1.f);
}
#endif
