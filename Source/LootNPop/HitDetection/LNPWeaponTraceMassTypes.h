// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "GameplayEffect.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "LNPWeaponTraceMassTypes.generated.h"

class ALNPCharacterBase;

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

	float HitRadius         = 10.f;
	float ParryRadius       = 12.f;    // 패링 판정 반경 — HitRadius보다 크게 설정
	float Damage            = 0.f;
	float KnockbackStrength = 0.f;
	// NotifyEnd 미호출 시 엔티티를 자동 파괴하는 안전장치.
	float TimeToLive        = 0.f;

	UPROPERTY()
	UClass* DamageEffectClass = nullptr;

	UPROPERTY()
	FMassEntityHandle InstigatorEntity;

	/**
	 * 클라이언트 예측 판정 전용 — 공격자 액터 직접 참조.
	 * 서버 Pass 3(워커 스레드)는 InstigatorEntity(Mass 핸들)를 그대로 쓴다.
	 * 클라이언트 조기 분기는 NotifyBegin 시점에 이미 Character 포인터를 들고 있으므로,
	 * 굳이 Mass 핸들로 변환했다가(AgentComp->GetEntityHandle()) 다시 역조회하는 왕복을 거치지 않는다 —
	 * 그 중간 단계가 간헐적으로 불안정해(레이스) 예측 HitStop이 새어나가던 원인이었다.
	 */
	UPROPERTY()
	TWeakObjectPtr<ALNPCharacterBase> InstigatorActor;

	ELNPInstigatorTeam InstigatorTeam = ELNPInstigatorTeam::Player;

	/** 이 엔티티를 생성한 머신에서 공격자가 로컬 컨트롤 대상인지 여부 (IsLocallyControlled()).
	 *  true인 머신에서만 클라이언트 예측 HitStop 판정을 수행한다. 서버·리모트 클라에서는 false. */
	bool bIsLocalInstigator = false;

	/** 클라이언트 예측 판정 1회 발동 플래그. 스윙당 코스메틱 피드백은 한 번만 재생한다. */
	bool bLocalFeedbackFired = false;

	// 한 번의 공격에서 동일 타겟 중복 피격 방지.
	static constexpr int32 MaxAlreadyHit = 8;
	FMassEntityHandle AlreadyHit[MaxAlreadyHit] = {};
	int32             AlreadyHitCount = 0;
};
