// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_BasicAttack.h"
#include "Character/LNPCharacterBase.h"
#include "Item/LNPWeaponData.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "AbilitySystemComponent.h"

ULNPAbility_BasicAttack::ULNPAbility_BasicAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

const ULNPWeaponData* ULNPAbility_BasicAttack::GetEquippedWeaponDef() const
{
	const ALNPCharacterBase* Ch = GetOwningCharacter();
	return Ch ? Ch->GetActiveWeaponDef() : nullptr;
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
	const float WeaponBonus = WeaponDef ? WeaponDef->ProjectileDamage : 0.f;
	return (Attrs->GetAttackPower() + WeaponBonus) * Attrs->GetAttackMultiplier();
}
