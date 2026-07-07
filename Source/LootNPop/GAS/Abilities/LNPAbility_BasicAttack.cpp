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

	SpecHandle.Data->SetDuration(WeaponDef->FireCooldown, true);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}

const ULNPWeaponData* ULNPAbility_BasicAttack::GetEquippedWeaponDef() const
{
	const ALNPCharacterBase* Ch = GetOwningCharacter();
	return Ch ? Ch->GetActiveWeaponDef() : nullptr;
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

	const ULNPWeaponData* WeaponDef = GetEquippedWeaponDef();
	const float WeaponBonus = WeaponDef ? WeaponDef->Damage : 0.f;
	return (Attrs->GetAttackPower() + WeaponBonus) * Attrs->GetAttackMultiplier();
}
