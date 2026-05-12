// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LNPVFXData.generated.h"

class UNiagaraSystem;

/**
 * Reusable VFX configuration DataAsset.
 * Referenced by weapons, skills, and any other system that needs timed effects.
 * Supports multiple Niagara systems per timing slot so effects can be layered.
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPVFXData : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(GetClass()->GetFName(), GetFName());
	}

	/** Played once at the spawn/cast location (muzzle flash, skill cast ring, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TArray<TObjectPtr<UNiagaraSystem>> SpawnEffects;

	/** Looping effects that follow the entity while alive (projectile trail, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TArray<TObjectPtr<UNiagaraSystem>> TrailEffects;

	/** Played once at the impact / despawn location (hit spark, explosion, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TArray<TObjectPtr<UNiagaraSystem>> ImpactEffects;
};
