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
 */
UCLASS()
class LOOTNPOP_API ULNPAbility_FlameAttack : public ULNPAbility_BasicAttack
{
	GENERATED_BODY()
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
