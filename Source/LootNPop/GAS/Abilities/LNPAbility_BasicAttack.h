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

	/** 콤보 인덱스에 해당하는 경직력을 반환한다. 기본 구현은 PoiseDamage를 그대로 반환. */
	virtual float GetPoiseDamageForCombo(int32 ComboIdx) const;

	float GetParryRadius() const { return ParryRadius; }

	float GetAbilityDamage() const { return ComputeDamage(); }

protected:
	/** 현재 장착된 무기의 DataAsset을 반환한다. 없으면 null. */
	const ULNPWeaponData* GetEquippedWeaponDef() const;

	/**
	 * ActorInfo의 아바타에서 장착 무기 정의를 읽는다. 없으면 null.
	 *
	 * ⚠️ CurrentActorInfo에 의존하지 않는 것이 요점이다 — CheckCooldown()은 첫 활성화 전이면
	 * 인스턴스가 아니라 **CDO에서** 불리고(UAbilitySystemComponent::InternalTryActivateAbility),
	 * CDO의 CurrentActorInfo는 null이다.
	 */
	const ULNPWeaponData* GetEquippedWeaponDefFor(const FGameplayAbilityActorInfo* ActorInfo) const;

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

	/**
	 * 장착 무기의 FireCooldown을 Duration으로 주입해 Cooldown GE를 적용한다. FireCooldown <= 0이면 쿨다운 없음.
	 * 무기 정의를 스펙 컨텍스트의 SourceObject로 스탬프해 CheckCooldown()이 무기를 구분할 수 있게 한다.
	 */
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	/**
	 * 쿨다운을 **무기별로** 판정한다 — ApplyCooldown이 스탬프한 무기 정의와 일치하는 쿨다운 GE만 본다.
	 *
	 * 엔진 기본 구현은 GE가 부여한 태그를 ASC 태그와 대조하는 것이 전부라, 쿨다운 GE 클래스가
	 * 하나뿐인 이 프로젝트에서는 무기가 달라도 같은 태그 하나가 모두를 막았다. 태그 축을 무기별로
	 * 늘리는 대신 FGameplayEffectQuery::EffectSource(스펙 컨텍스트의 SourceObject)로 가른다 —
	 * 무기 추가가 여전히 DataAsset 편집만으로 끝난다.
	 *
	 * ⚠️ GetCooldownTimeRemaining()은 손대지 않았으므로 여전히 무기 무관("전 무기 중 최댓값")이다.
	 * 공격 쿨다운 UI를 붙일 때 같은 EffectSource 필터로 함께 오버라이드할 것.
	 */
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** 이 Ability가 가하는 넉백 강도 (cm/s 단위 임펄스). 0이면 넉백 없음. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Combat")
	float KnockbackStrength = 500.f;

	/**
	 * 이 Ability 한 방이 피격자에게 쌓는 경직력. 0이면 경직에 기여하지 않는다.
	 *
	 * 무기 레벨 스케일을 타지 않는다 — 피해와 달리 제곱으로 커지면 고레벨 무기 하나로
	 * 영구 경직락이 성립한다. 넉백(KnockbackStrength)도 같은 이유로 레벨과 무관하다.
	 * ⚠️ 산탄(ULNPAbility_RangedSpreadAttack)은 발마다 누적되므로 **발당 값**으로 잡을 것.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Combat", meta = (ClampMin = "0"))
	float PoiseDamage = 20.f;

	/** 패링 판정 반경 (cm). 피격 반경보다 크게 설정해 패링 창이 넓어 보이게 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Combat", meta = (ClampMin = "0"))
	float ParryRadius = 15.f;
};
