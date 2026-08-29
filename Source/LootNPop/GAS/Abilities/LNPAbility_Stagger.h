// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "LNPAbility_Stagger.generated.h"

/**
 * 경직 상태를 어빌리티 수명으로 소유하는 GA.
 *
 * 트리거는 3종이다 — 경직도 임계 돌파(Light/Heavy)와 패링당함(Parry.Stagger).
 *
 * 그로기(Light)는 **지속 시간이 없다** — 경직도가 T1 아래로 자연회복할 때까지 이어지는 상태이고,
 * 종료는 ULNPPoiseProcessor가 FLNPStaggerCommand로 취소해 준다. 다운(Heavy)과 패링 스태거만 고정 시간이다.
 *
 * **몽타주는 재생하지 않는다.** 연출은 GameplayCue.LNP.Character.Stagger가 전 머신에 나르고,
 * 이 어빌리티는 오직 `ActivationOwnedTags`로 입력 차단만 소유한다. 그렇게 나눈 이유:
 *   - 적 ASC는 Minimal 복제라 어빌리티 활성화가 시뮬 프록시에 도달하지 않는다 —
 *     GA가 몽타주를 들면 게스트 화면에서 적 경직이 보이지 않는다.
 *   - 반대로 차단은 권위 상태여야 하므로 코스메틱 큐에 실을 수 없다.
 *   - 양쪽 모두 재생하면 소유 클라에서 몽타주가 이중으로 돈다.
 *
 * ⚠️ 따라서 경직 길이와 몽타주 길이는 데이터상 분리돼 있다. 길이는 ULNPSettings의
 *    Poise*LockSeconds가, 몽타주는 Chooser(LNP.Montage.Situation.Stagger)가 각각 정한다.
 */
UCLASS()
class LOOTNPOP_API ULNPAbility_Stagger : public ULNPGameplayAbility
{
	GENERATED_BODY()

public:
	ULNPAbility_Stagger();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	UFUNCTION()
	void OnLockFinished();
};
