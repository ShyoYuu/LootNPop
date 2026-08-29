// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_MeleeAttack.h"
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

float ULNPAbility_MeleeAttack::GetPoiseDamageForCombo(int32 ComboIdx) const
{
	if (ComboPoiseDamages.IsValidIndex(ComboIdx))
		return ComboPoiseDamages[ComboIdx];
	return PoiseDamage;
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

	// AttackSpeed를 재생 속도로 — 몽타주에 붙은 ANS(히트 윈도우·입력 차단 구간)도 함께 압축되는 것이 의도된 동작이다.
	if (UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage, GetAttackSpeed(), SectionName))
	{
		Task->OnCompleted.AddDynamic(this, &ULNPAbility_MeleeAttack::OnMontageEnded);
		Task->OnBlendOut.AddDynamic(this, &ULNPAbility_MeleeAttack::OnMontageEnded);
		Task->OnInterrupted.AddDynamic(this, &ULNPAbility_MeleeAttack::OnMontageInterrupted);
		Task->OnCancelled.AddDynamic(this, &ULNPAbility_MeleeAttack::OnMontageInterrupted);
		Task->ReadyForActivation();
	}
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