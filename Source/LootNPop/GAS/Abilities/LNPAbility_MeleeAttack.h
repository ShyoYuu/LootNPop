// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPAbility_BasicAttack.h"
#include "RootMotionModifier_SkewWarp.h"
#include "LNPAbility_MeleeAttack.generated.h"

class UAnimMontage;

/**
 * 근접 공격 보정용 SkewWarp 모디파이어.
 *
 * 엔진의 URootMotionModifier_SkewWarp는 MaxSpeedClampRatio(보정 속력 상한)를 protected로 두고
 * setter를 제공하지 않아, 파생하지 않으면 C++에서 값을 넣을 수 없다. 이 클래스의 존재 이유는 그것뿐이다.
 */
UCLASS()
class LOOTNPOP_API ULNPMeleeAssistWarpModifier : public URootMotionModifier_SkewWarp
{
	GENERATED_BODY()
public:
	/** 애니메이션 원본 루트모션 속도 대비 배율. 0이면 무제한. */
	void SetMaxSpeedClampRatio(float InRatio) { MaxSpeedClampRatio = InRatio; }
};

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

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

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

	/**
	 * 근접 공격 타겟 보정을 건다 — 위치는 Motion Warping, 회전은 OrientationIntent가 담당한다.
	 * 보정할 타겟이 없거나 강도가 0이면 아무것도 하지 않는다.
	 */
	void ApplyMeleeAssist(ALNPCharacterBase* Character, UAnimMontage* Montage, FName SectionName);

	/** 워프 타겟과 회전 보정을 되돌린다. 어빌리티 종료 경로 전부에서 불린다. */
	void ClearMeleeAssist();
};
