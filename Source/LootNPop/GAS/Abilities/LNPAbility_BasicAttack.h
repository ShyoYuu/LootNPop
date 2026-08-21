// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "LNPAbility_BasicAttack.generated.h"

class ULNPWeaponData;

/**
 * 모든 무기 기본 공격 Ability의 추상 기반 클래스.
 * 서브클래스가 런타임에 무기 config를 읽을 수 있도록 GetEquippedWeaponDef()를 제공한다.
 */
UCLASS(Abstract)
class LOOTNPOP_API ULNPAbility_BasicAttack : public ULNPGameplayAbility
{
	GENERATED_BODY()
public:
	ULNPAbility_BasicAttack();

	/** 콤보 인덱스에 해당하는 넉백 강도를 반환한다. 기본 구현은 KnockbackStrength를 그대로 반환. */
	virtual float GetKnockbackForCombo(int32 ComboIdx) const;

	float GetParryRadius() const { return ParryRadius; }

	float GetAbilityDamage() const { return ComputeDamage(); }

protected:
	/** 현재 장착된 무기의 DataAsset을 반환한다. 없으면 null. */
	const ULNPWeaponData* GetEquippedWeaponDef() const;

	/**
	 * AttackSpeed 어트리뷰트(버프 합산 후 최종값). ASC가 없으면 1.0.
	 * 몽타주 재생 속도와 쿨다운에 모두 쓰인다 — 둘을 같은 계수로 스케일해야
	 * 실제 공격 빈도가 계수만큼 빨라진다 (한쪽만 줄이면 다른 쪽이 병목이 된다).
	 */
	float GetAttackSpeed() const;

	/**
	 * 기본 피해 = AttackPower 최종값(무기 스텟·합/곱 버프 반영) × 피해 계수. Ability별로 Override 가능.
	 */
	virtual float ComputeDamage() const;

	/**
	 * 이 어빌리티의 피해 계수 = BaseDamageCoefficient × 장착 무기 레벨 행의 AbilityCoefScale.
	 *
	 * 무기 레벨은 GAS 어빌리티 스펙 레벨로 들어온다 — ULNPEquipmentComponent::GrantItemImpl이
	 * `FGameplayAbilitySpec(Class, 아이템레벨)`로 부여하므로 GetAbilityLevel()이 곧 무기 레벨이다.
	 */
	float GetDamageCoefficient() const;

	/**
	 * 이 어빌리티 고유의 기본 피해 계수. 같은 무기의 강공격·특수공격에 개성을 주는 축이다.
	 * 레벨에 따른 증가는 무기 레벨 테이블(FLNPWeaponLevelRow::AbilityCoefScale)이 담당한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Combat", meta = (ClampMin = "0"))
	float BaseDamageCoefficient = 1.0f;

	/** 공용 Cooldown GE (Duration은 ApplyCooldown에서 무기별로 주입). */
	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

	/** 장착 무기의 FireCooldown을 Duration으로 주입해 Cooldown GE를 적용한다. FireCooldown <= 0이면 쿨다운 없음. */
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	/** 이 Ability가 가하는 넉백 강도 (cm/s 단위 임펄스). 0이면 넉백 없음. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Combat")
	float KnockbackStrength = 500.f;

	/** 패링 판정 반경 (cm). 피격 반경보다 크게 설정해 패링 창이 넓어 보이게 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Combat", meta = (ClampMin = "0"))
	float ParryRadius = 15.f;
};
