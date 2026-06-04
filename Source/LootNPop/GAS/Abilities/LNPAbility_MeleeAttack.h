// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPAbility_BasicAttack.h"
#include "LNPAbility_MeleeAttack.generated.h"

/**
 * 근거리 기본 공격 Ability.
 * 무기 DataAsset의 AttackMontage를 재생하고 즉시 종료한다.
 * 피격 판정은 몽타주에 배치된 ANS_LNPMeleeHitWindow가 담당한다.
 */
UCLASS()
class LOOTNPOP_API ULNPAbility_MeleeAttack : public ULNPAbility_BasicAttack
{
	GENERATED_BODY()
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
};
