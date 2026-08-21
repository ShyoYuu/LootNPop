// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_BasicAttack.h"
#include "GAS/Effects/LNPGameplayEffect_Cooldown.h"
#include "Character/LNPCharacterBase.h"
#include "Item/LNPWeaponData.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "AbilitySystemComponent.h"

ULNPAbility_BasicAttack::ULNPAbility_BasicAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

UGameplayEffect* ULNPAbility_BasicAttack::GetCooldownGameplayEffect() const
{
	return ULNPGameplayEffect_Cooldown::StaticClass()->GetDefaultObject<UGameplayEffect>();
}

void ULNPAbility_BasicAttack::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const ULNPWeaponData* WeaponDef = GetEquippedWeaponDef();
	if (!WeaponDef || WeaponDef->FireCooldown <= 0.f)
		return;

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo,
		ULNPGameplayEffect_Cooldown::StaticClass(), GetAbilityLevel());

	if (!SpecHandle.IsValid())
		return;

	// AttackSpeed로 나눈다 — 원거리 공격은 몽타주 길이가 아니라 이 쿨다운이 발사 간격을 지배한다.
	SpecHandle.Data->SetDuration(WeaponDef->FireCooldown / GetAttackSpeed(), true);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}

const ULNPWeaponData* ULNPAbility_BasicAttack::GetEquippedWeaponDef() const
{
	const ALNPCharacterBase* Ch = GetOwningCharacter();
	return Ch ? Ch->GetActiveWeaponDef() : nullptr;
}

float ULNPAbility_BasicAttack::GetAttackSpeed() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC == nullptr)
		return 1.0f;

	// AttributeSet의 PreAttributeChange가 0.01 미만을 막지만, ASC 미초기화 시의 0을 한 번 더 방어한다
	// (쿨다운 계산의 제수라 0이면 무한대가 된다).
	return FMath::Max(0.01f, ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetAttackSpeedAttribute()));
}

float ULNPAbility_BasicAttack::GetKnockbackForCombo(int32 /*ComboIdx*/) const
{
	return KnockbackStrength;
}

float ULNPAbility_BasicAttack::ComputeDamage() const
{
	const ALNPCharacterBase* Ch = GetOwningCharacter();
	if (!Ch)
		return 0.f;

	const UAbilitySystemComponent* ASCLocal = Ch->GetAbilitySystemComponent();
	if (!ASCLocal)
		return 0.f;

	const ULNPBaseAttributeSet* Attrs = ASCLocal->GetSet<ULNPBaseAttributeSet>();
	if (!Attrs)
		return 0.f;

	// 무기 스텟은 장착 GE로 AttackPower에 합산되어 있고, 곱연산 버프도 어그리게이터가 이미 곱한 뒤다.
	// 여기에 어빌리티 계수(어빌리티 개성 × 무기 레벨)를 곱한 것이 최종 피해다.
	return Attrs->GetAttackPower() * GetDamageCoefficient();
}

float ULNPAbility_BasicAttack::GetDamageCoefficient() const
{
	const ULNPWeaponData* WeaponDef = GetEquippedWeaponDef();
	if (WeaponDef == nullptr)
		return BaseDamageCoefficient;

	// 스펙 레벨 = 무기 레벨 (GrantItemImpl). 활성화 밖에서 불리면 1로 떨어지므로 하한을 건다.
	const int32 WeaponLevel = FMath::Max(1, GetAbilityLevel());
	return BaseDamageCoefficient * WeaponDef->GetAbilityCoefScale(WeaponLevel);
}
