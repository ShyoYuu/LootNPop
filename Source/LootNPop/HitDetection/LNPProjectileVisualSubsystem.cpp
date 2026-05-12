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
	if (!VFXData || VFXData->TrailEffects.IsEmpty()) return;

	TArray<TObjectPtr<UNiagaraComponent>>& Components = ActiveTrails.FindOrAdd(Entity);
	Components.Reserve(VFXData->TrailEffects.Num());

	UWorld* World = GetWorld();
	for (UNiagaraSystem* NS : VFXData->TrailEffects)
	{
		if (!NS) continue;
		UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, NS, Pos, FRotator::ZeroRotator, FVector::OneVector,
			/*bAutoDestroy=*/false, /*bAutoActivate=*/true, ENCPoolMethod::None);
		if (Comp)
			Components.Add(Comp);
	}
}

void ULNPProjectileVisualSubsystem::UpdateTrails(FMassEntityHandle Entity, FVector Pos)
{
	if (TArray<TObjectPtr<UNiagaraComponent>>* Components = ActiveTrails.Find(Entity))
	{
		for (UNiagaraComponent* Comp : *Components)
		{
			if (Comp)
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
			if (Comp)
				Comp->DestroyComponent();
		}
		ActiveTrails.Remove(Entity);
	}
}

void ULNPProjectileVisualSubsystem::SpawnSpawnEffects(const ULNPVFXData* VFXData, FVector Pos)
{
	if (!VFXData) return;
	UWorld* World = GetWorld();
	for (UNiagaraSystem* NS : VFXData->SpawnEffects)
	{
		if (NS)
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
		SpawnImpactEffects(Impact.VFXData, Impact.Location, Impact.Normal);
}

void ULNPProjectileVisualSubsystem::SpawnImpactEffects(const ULNPVFXData* VFXData, FVector Pos, FVector Normal)
{
	if (!VFXData) return;
	UWorld* World = GetWorld();
	const FRotator Rot = Normal.ToOrientationRotator();
	for (UNiagaraSystem* NS : VFXData->ImpactEffects)
	{
		if (NS)
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, NS, Pos, Rot);
	}
}

#if WITH_EDITOR
void ULNPProjectileVisualSubsystem::EnqueueSurfaceImpactDebug(FVector Location)
{
	SurfaceImpactDebugQueue.Enqueue(Location);
}

void ULNPProjectileVisualSubsystem::FlushSurfaceImpactDebug(UWorld* World, float SphereRadius)
{
	FVector Loc;
	while (SurfaceImpactDebugQueue.Dequeue(Loc))
		DrawDebugSphere(World, Loc, SphereRadius, 8, FColor::Orange, false, 1.f);
}
#endif
