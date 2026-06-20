// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_MeleeAttack.h"
#include "GAS/Effects/LNPGameplayEffect_Cooldown.h"
#include "Item/LNPWeaponData.h"
#include "Character/LNPCharacterBase.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

float ULNPAbility_MeleeAttack::GetKnockbackForCombo(int32 ComboIdx) const
{
	if (ComboKnockbackStrengths.IsValidIndex(ComboIdx))
		return ComboKnockbackStrengths[ComboIdx];
	return KnockbackStrength;
}

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
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimMontage* AttackMontage = Character->EvaluateMontage(TAG_Montage_Situation_Attack);
	if (!AttackMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const int32 ComboIdx = Character->GetCurrentComboIndex();
	FName SectionName = NAME_None;
	if (ComboIdx > 0)
		SectionName = FName(FString::Printf(TEXT("Section_%d"), ComboIdx + 1));

	if (UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage, 1.f, SectionName))
	{
		Task->OnCompleted.AddDynamic(this, &ULNPAbility_MeleeAttack::OnMontageEnded);
		Task->OnBlendOut.AddDynamic(this, &ULNPAbility_MeleeAttack::OnMontageEnded);
		Task->OnInterrupted.AddDynamic(this, &ULNPAbility_MeleeAttack::OnMontageInterrupted);
		Task->OnCancelled.AddDynamic(this, &ULNPAbility_MeleeAttack::OnMontageInterrupted);
		Task->ReadyForActivation();
	}
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

void ULNPAbility_MeleeAttack::OnMontageEnded()
{
	ClearRelativeTag();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void ULNPAbility_MeleeAttack::OnMontageInterrupted()
{
	ClearRelativeTag();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void ULNPAbility_MeleeAttack::ClearRelativeTag()
{
	if (ALNPCharacterBase* Character = GetOwningCharacter())
	{
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		{
			if (ASC->HasMatchingGameplayTag(TAG_Block_AttackInput))
				ASC->RemoveLooseGameplayTag(TAG_Block_AttackInput);
			if (ASC->HasMatchingGameplayTag(TAG_State_ComboWindow))
				ASC->RemoveLooseGameplayTag(TAG_State_ComboWindow);
		}
	}
}