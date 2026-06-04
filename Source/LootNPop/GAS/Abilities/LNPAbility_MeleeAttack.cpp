// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_MeleeAttack.h"
#include "GAS/Effects/LNPGameplayEffect_Cooldown.h"
#include "Item/LNPWeaponData.h"
#include "Character/LNPCharacterBase.h"
#include "LootNPop.h"

#include "Animation/AnimInstance.h"

void ULNPAbility_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const ALNPCharacterBase* Character = GetOwningCharacter();
	if (nullptr == Character)
		return;

	const ULNPWeaponData* WeaponDef = GetEquippedWeaponDef();
	if (WeaponDef && WeaponDef->AttackMontage)
	{
		if (UAnimInstance* AnimInst = Character->GetAnimInstance())
		{
			AnimInst->Montage_Play(WeaponDef->AttackMontage);
		}
	}

	// 피격 판정은 몽타주의 ANS_LNPMeleeHitWindow가 처리하므로 Ability는 즉시 종료.
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UGameplayEffect* ULNPAbility_MeleeAttack::GetCooldownGameplayEffect() const
{
	return ULNPGameplayEffect_Cooldown::StaticClass()->GetDefaultObject<UGameplayEffect>();
}

void ULNPAbility_MeleeAttack::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
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
