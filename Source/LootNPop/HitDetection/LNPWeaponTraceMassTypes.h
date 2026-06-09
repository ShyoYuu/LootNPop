// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassEntityHandle.h"
#include "GameplayEffect.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "LNPWeaponTraceMassTypes.generated.h"

/**
 * 근거리 공격자 Mass 엔티티에 붙는 Fragment.
 * ANS_LNPMeleeHitWindow이 매 프레임 본(Bone) 위치를 기록하고,
 * ULNPWeaponTraceProcessor가 Swept Volume 판정에 사용한다.
 */
USTRUCT()
struct LOOTNPOP_API FLNPWeaponTraceFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector SwordTipPrev  = FVector::ZeroVector;
	FVector SwordTipCurr  = FVector::ZeroVector;
	FVector SwordRootPrev = FVector::ZeroVector;
	FVector SwordRootCurr = FVector::ZeroVector;

	float HitRadius   = 10.f;
	float ParryRadius = 12.f;    // 패링 판정 반경 — HitRadius보다 크게 설정
	float Damage      = 0.f;
	// NotifyEnd 미호출 시 엔티티를 자동 파괴하는 안전장치.
	float TimeToLive  = 0.f;

	UPROPERTY()
	UClass* DamageEffectClass = nullptr;

	UPROPERTY()
	FMassEntityHandle InstigatorEntity;

	ELNPInstigatorTeam InstigatorTeam = ELNPInstigatorTeam::Player;

	// 한 번의 공격에서 동일 타겟 중복 피격 방지.
	static constexpr int32 MaxAlreadyHit = 8;
	FMassEntityHandle AlreadyHit[MaxAlreadyHit] = {};
	int32             AlreadyHitCount = 0;
};
