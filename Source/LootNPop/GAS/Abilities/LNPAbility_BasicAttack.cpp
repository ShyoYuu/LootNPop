// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_BasicAttack.h"
#include "GAS/Effects/LNPGameplayEffect_Cooldown.h"
#include "Character/LNPCharacterBase.h"
#include "Item/LNPWeaponData.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

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

	// 쿨다운을 무기별로 가르는 키. CheckCooldown()이 FGameplayEffectQuery::EffectSource로 되읽는다.
	// GetContext()는 핸들을 값으로 돌려주지만 내부 Data를 공유하므로 스탬프가 이 스펙에 남는다.
	SpecHandle.Data->GetContext().AddSourceObject(WeaponDef);

	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}

bool ULNPAbility_BasicAttack::CheckCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const ULNPWeaponData* WeaponDef = GetEquippedWeaponDefFor(ActorInfo);
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayTagContainer* CooldownTags = GetCooldownTags();

	// 무기를 못 찾으면 태그 하나로 막는 엔진 기본 동작으로 물러난다. 무기가 없으면 ApplyCooldown도
	// 아무것도 적용하지 않으므로 막을 GE 자체가 없고, 보수적인 쪽이기도 하다.
	if (WeaponDef == nullptr || ASC == nullptr || CooldownTags == nullptr || CooldownTags->IsEmpty())
		return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);

	// 태그로 1차 추린 뒤 EffectSource(= ApplyCooldown이 스탬프한 무기 정의)로 이 무기 것만 남긴다.
	FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*CooldownTags);
	Query.EffectSource = WeaponDef;

	if (ASC->GetActiveEffects(Query).Num() == 0)
		return true;

	// 실패 태그 통지는 엔진 기본 구현과 같게 맞춘다 (UGameplayAbility::CheckCooldown).
	if (OptionalRelevantTags)
	{
		const FGameplayTag& FailCooldownTag = UAbilitySystemGlobals::Get().ActivateFailCooldownTag;
		if (FailCooldownTag.IsValid())
			OptionalRelevantTags->AddTag(FailCooldownTag);

		OptionalRelevantTags->AppendMatchingTags(ASC->GetOwnedGameplayTags(), *CooldownTags);
	}

	return false;
}

const ULNPWeaponData* ULNPAbility_BasicAttack::GetEquippedWeaponDef() const
{
	return GetEquippedWeaponDefFor(CurrentActorInfo);
}

const ULNPWeaponData* ULNPAbility_BasicAttack::GetEquippedWeaponDefFor(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const ALNPCharacterBase* Ch = ActorInfo ? Cast<ALNPCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
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

float ULNPAbility_BasicAttack::GetPoiseDamageForCombo(int32 /*ComboIdx*/) const
{
	return PoiseDamage;
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
