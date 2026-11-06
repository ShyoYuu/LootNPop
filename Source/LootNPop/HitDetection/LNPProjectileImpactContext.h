// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "LNPProjectileImpactContext.generated.h"

class ULNPVFXData;

/**
 * GameplayCue.LNP.Projectile.Impact 전용 GameplayEffectContext.
 * Ghost Projectile 재조정(브랜치 A/B 판별)에 필요한 토큰(PredictionKeyID + SpawnIndex)과
 * 공격자 식별자(InstigatorPlayerID), 무기별 임팩트 VFX 소스(VFXData)를 함께 전달한다.
 */
USTRUCT()
struct LOOTNPOP_API FLNPProjectileImpactContext : public FGameplayEffectContext
{
	GENERATED_BODY()

	// Iris는 부모 전용 직렬화기 뒤에 파생 UPROPERTY만 수집한다. 아래 NetSerialize만으로는 키가 전달되지 않는다.
	UPROPERTY()
	int32 PredictionKeyID    = 0;
	UPROPERTY()
	uint8 SpawnIndex         = 0;
	UPROPERTY()
	int32 InstigatorPlayerID = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<ULNPVFXData> VFXData = nullptr;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FLNPProjectileImpactContext::StaticStruct();
	}

	virtual FGameplayEffectContext* Duplicate() const override
	{
		FLNPProjectileImpactContext* NewContext = new FLNPProjectileImpactContext(*this);
		if (GetHitResult())
			NewContext->AddHitResult(*GetHitResult(), true);
		return NewContext;
	}

	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;
};

template<>
struct TStructOpsTypeTraits<FLNPProjectileImpactContext> : public TStructOpsTypeTraitsBase2<FLNPProjectileImpactContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
