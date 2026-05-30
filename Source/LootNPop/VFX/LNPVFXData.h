// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LNPVFXData.generated.h"

class UNiagaraSystem;

/**
 * 재사용 가능한 VFX 설정 DataAsset.
 * 무기, 스킬, 타이밍 이펙트가 필요한 모든 시스템에서 참조된다.
 * 타이밍 슬롯당 여러 Niagara 시스템을 지원하여 이펙트를 레이어로 쌓을 수 있다.
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

	/** 스폰/시전 위치에서 한 번 재생 (총구 화염, 스킬 시전 링 등) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TArray<TObjectPtr<UNiagaraSystem>> SpawnEffects;

	/** 살아있는 동안 Entity를 따라다니는 루프 이펙트 (Projectile 트레일 등) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TArray<TObjectPtr<UNiagaraSystem>> TrailEffects;

	/** 임팩트/디스폰 위치에서 한 번 재생 (히트 스파크, 폭발 등) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TArray<TObjectPtr<UNiagaraSystem>> ImpactEffects;
};
