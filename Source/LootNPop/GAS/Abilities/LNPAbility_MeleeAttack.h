// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPAbility_BasicAttack.h"
#include "LNPAbility_MeleeAttack.generated.h"

/**
 * 근거리 기본 공격 Ability.
 * Chooser(EvaluateMontage)로 공격 몽타주를 선택해 PlayMontageAndWait 태스크로 재생하고,
 * 콤보 인덱스에 따라 몽타주 섹션(Section_N)을 골라 다단 콤보를 잇는다.
 * 몽타주가 끝나거나 인터럽트되면 잔여 태그를 정리하고 종료한다.
 * 피격 판정은 몽타주에 배치된 ANS_LNPMeleeHitWindow가 담당한다.
 */
UCLASS()
class LOOTNPOP_API ULNPAbility_MeleeAttack : public ULNPAbility_BasicAttack
{
	GENERATED_BODY()
public:
	/** ComboKnockbackStrengths에 값이 있으면 해당 인덱스 값을, 없으면 KnockbackStrength(기반 클래스)로 폴백. */
	virtual float GetKnockbackForCombo(int32 ComboIdx) const override;

	/** ComboPoiseDamages에 값이 있으면 해당 인덱스 값을, 없으면 PoiseDamage(기반 클래스)로 폴백. */
	virtual float GetPoiseDamageForCombo(int32 ComboIdx) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** 콤보 타수별 넉백 강도. 인덱스 0 = 첫 타.
	 *  비어있거나 인덱스 초과 시 KnockbackStrength(기반 클래스)로 폴백한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Combat")
	TArray<float> ComboKnockbackStrengths;

	/** 콤보 타수별 경직력. 인덱스 0 = 첫 타.
	 *  비어있거나 인덱스 초과 시 PoiseDamage(기반 클래스)로 폴백한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Combat")
	TArray<float> ComboPoiseDamages;

private:
	UFUNCTION()
	void OnMontageEnded();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void ClearRelativeTag();
};
