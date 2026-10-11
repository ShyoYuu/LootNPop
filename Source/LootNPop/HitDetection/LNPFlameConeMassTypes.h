// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "LNPFlameConeMassTypes.generated.h"

/**
 * 화염방사기 분사 틱 1회의 원뿔(절두체) 판정 요청. **서버 전용, 수명 1프레임.**
 *
 * ULNPAbility_FlameAttack이 발동(=틱)마다 서버에서 하나 만들고,
 * ULNPFlameConeHitDetectionProcessor가 판정한 뒤 같은 실행에서 파괴한다 — 파괴 지점은 그 한 곳뿐이다.
 *
 * 형상: 원점에서 축 방향으로 거리 t만큼 떨어진 단면의 반경이 lerp(NearRadius, FarRadius, t / ConeRange)다.
 * Range는 축 방향 월드 트레이스로 잘린 실제 도달 거리이고, 반경 기울기는 잘리기 전 ConeRange로 잰다 —
 * 벽 앞에서 쏴도 원뿔이 뚱뚱해지지 않는다.
 */
USTRUCT()
struct LOOTNPOP_API FLNPFlameConeFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector Origin = FVector::ZeroVector;   // 총구 (월드)
	FVector Axis   = FVector::ForwardVector; // 단위 벡터

	float ConeRange  = 0.f;   // 원래 사거리 — 반경 보간의 분모
	float Range      = 0.f;   // 월드에 잘린 사거리 (≤ ConeRange)
	float NearRadius = 0.f;
	float FarRadius  = 0.f;

	float Damage      = 0.f;
	float PoiseDamage = 0.f;

	UPROPERTY()
	UClass* DamageEffectClass = nullptr;   // TObjectPtr는 프래그먼트의 trivially copyable 요건을 깬다

	UPROPERTY()
	FMassEntityHandle InstigatorEntity;

	/** 서버 되감기 입력 — 발동 시점 게임 스레드에서 저장한다 (근접 FLNPWeaponTraceFragment와 같은 규약). */
	float CachedRewindSeconds = 0.f;
};
