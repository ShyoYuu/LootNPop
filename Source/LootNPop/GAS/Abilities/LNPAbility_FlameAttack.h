// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPAbility_BasicAttack.h"
#include "LNPAbility_FlameAttack.generated.h"

/**
 * 화염방사기 기본 공격 — **발동 1회 = 분사 틱 1회**.
 *
 * 홀드 중 입력 핸들러가 매 프레임 발동을 시도하고 쿨다운 GE(FireCooldown)가 틱 간격을 정한다 — 라이플 연사와 같은 구조다.
 * 그래서 틱마다 조준 스냅샷이 발동 요청에 실리고(InputCmd 불필요), 충전 게이지 소모는 기존 탄약 비용(예측 적용)이 맡으며,
 * 입력을 떼면 발동이 멈추는 것으로 분사가 끝난다. 수명이 홀드 전체인 어빌리티를 두지 않는 이유다.
 *
 * 틱마다 분사 상태 GE(ULNPGameplayEffect_FlameFiring)를 갱신해 충전형 탄창 회복을 막는다.
 *
 * 피해 판정은 서버만 한다 — 틱마다 원뿔 판정 엔티티(FLNPFlameConeFragment)를 하나 만들고
 * ULNPFlameConeHitDetectionProcessor가 다음 StartPhysics에서 판정·파괴한다.
 * 형상 값은 산탄 퍼짐(Spread)처럼 무기 데이터가 아니라 어빌리티가 소유한다.
 */
UCLASS()
class LOOTNPOP_API ULNPAbility_FlameAttack : public ULNPAbility_BasicAttack
{
	GENERATED_BODY()
public:
	ULNPAbility_FlameAttack();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** 원뿔 사거리 (cm). 축 방향 월드 트레이스가 이보다 짧게 자를 수 있다.
	 *  비행형 교전 지점(고도 750 / sin 30° ≈ 1500cm)까지 닿게 잡았다 — 600이면 비행형을 아예 공격할 수 없었다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Flame", meta = (ClampMin = "0"))
	float ConeRange = 1600.f;

	/** 총구 단면 반경 (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Flame", meta = (ClampMin = "0"))
	float NearRadius = 30.f;

	/** 사거리 끝 단면 반경 (cm). 600cm에서 200cm였던 기울기를 유지한 값이다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Flame", meta = (ClampMin = "0"))
	float FarRadius = 480.f;

	/** 적중 1회(틱)당 화상 1중첩이 걸릴 확률. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Flame|Burn", meta = (ClampMin = "0", ClampMax = "1"))
	float BurnChance = 0.2f;

	/** 화상 중첩 상한. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Flame|Burn", meta = (ClampMin = "0", ClampMax = "255"))
	int32 MaxBurnStacks = 5;

	/** 화상 지속시간 (초). 모든 중첩이 공유하며 걸릴 때마다 처음으로 돌아간다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Flame|Burn", meta = (ClampMin = "0"))
	float BurnDuration = 3.f;

	/** 화상 틱 간격 (초). */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Flame|Burn", meta = (ClampMin = "0.05"))
	float BurnTickInterval = 0.5f;

	/** 화상 1중첩·1틱의 피해 계수. BaseDamageCoefficient와 같은 축(AttackPower × 계수 × 무기 레벨 배율)이다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Flame|Burn", meta = (ClampMin = "0"))
	float BurnDamageCoefficient = 0.3f;

private:
	/** 서버 전용 — 이번 틱의 원뿔 판정 엔티티를 만든다. */
	void SpawnConeJudge(const FGameplayAbilityActivationInfo& ActivationInfo, const FGameplayEventData* TriggerEventData) const;
};
