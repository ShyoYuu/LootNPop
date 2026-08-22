// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPProjectileVisualSubsystem.h"
#include "VFX/LNPVFXData.h"
#include "Config/LNPSettings.h"

#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#if WITH_EDITOR
#include "MassDebugDrawHelpers.h"
#endif

const FName ULNPProjectileVisualSubsystem::TintColorParameterName(TEXT("TintColor"));

FLinearColor ULNPProjectileVisualSubsystem::GetTeamTintColor(ELNPInstigatorTeam Team)
{
	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	return (Team == ELNPInstigatorTeam::Player)
		? Settings->PlayerProjectileTintColor
		: Settings->EnemyProjectileTintColor;
}

void ULNPProjectileVisualSubsystem::AllocateTrails(FMassEntityHandle Entity, const ULNPVFXData* VFXData, FVector Pos,
	ELNPInstigatorTeam Team)
{
	if (nullptr == VFXData || VFXData->TrailEffects.IsEmpty())
		return;

	TArray<TObjectPtr<UNiagaraComponent>>& Components = ActiveTrails.FindOrAdd(Entity);
	Components.Reserve(VFXData->TrailEffects.Num());

	const FLinearColor TintColor = GetTeamTintColor(Team);

	UWorld* World = GetWorld();
	for (UNiagaraSystem* NS : VFXData->TrailEffects)
	{
		if (nullptr == NS)
			continue;

		UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, NS, Pos, FRotator::ZeroRotator, FVector::OneVector,
			/*bAutoDestroy=*/false, /*bAutoActivate=*/true, ENCPoolMethod::None);
		if (Comp != nullptr)
		{
			// TintColor를 노출하지 않는 시스템에서는 조용히 무시된다 — 진영 색을 안 쓰는 트레일도 그대로 동작.
			Comp->SetVariableLinearColor(TintColorParameterName, TintColor);
			Components.Add(Comp);
		}
	}
}

void ULNPProjectileVisualSubsystem::SetTrailTeam(FMassEntityHandle Entity, ELNPInstigatorTeam Team)
{
	TArray<TObjectPtr<UNiagaraComponent>>* Components = ActiveTrails.Find(Entity);
	if (nullptr == Components)
		return;

	const FLinearColor TintColor = GetTeamTintColor(Team);
	for (UNiagaraComponent* Comp : *Components)
	{
		if (Comp != nullptr)
			Comp->SetVariableLinearColor(TintColorParameterName, TintColor);
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
