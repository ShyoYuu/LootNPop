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

	/** 기본 피해 공식: (AttackPower + WeaponDamage) * AttackMultiplier. Ability별로 Override 가능. */
	virtual float ComputeDamage() const;

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
