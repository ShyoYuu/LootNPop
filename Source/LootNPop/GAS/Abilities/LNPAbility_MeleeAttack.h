// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPAbility_BasicAttack.h"
#include "LNPAbility_MeleeAttack.generated.h"

/**
 * 근거리 기본 공격 Ability.
 * 무기 DataAsset의 AttackMontage를 PlayMontageAndWait 태스크로 재생하고,
 * 몽타주가 끝나거나 인터럽트될 때 종료한다.
 * 피격 판정은 몽타주에 배치된 ANS_LNPMeleeHitWindow가 담당한다.
 */
UCLASS()
class LOOTNPOP_API ULNPAbility_MeleeAttack : public ULNPAbility_BasicAttack
{
	GENERATED_BODY()
public:
	/** ComboKnockbackStrengths에 값이 있으면 해당 인덱스 값을, 없으면 KnockbackStrength(기반 클래스)로 폴백. */
	virtual float GetKnockbackForCombo(int32 ComboIdx) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	/** 콤보 타수별 넉백 강도. 인덱스 0 = 첫 타.
	 *  비어있거나 인덱스 초과 시 KnockbackStrength(기반 클래스)로 폴백한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Combat")
	TArray<float> ComboKnockbackStrengths;

private:
	UFUNCTION()
	void OnMontageEnded();

	UFUNCTION()
	void OnMontageInterrupted();
};
