// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_Stagger.h"
#include "Character/LNPCharacterBase.h"
#include "LNPGameplayTags.h"

#include "Animation/AnimInstance.h"
#include "Abilities/GameplayAbility.h"

ULNPAbility_Stagger::ULNPAbility_Stagger()
{
	// TAG_GameplayEvent_Parry_Stagger 이벤트 수신 시 자동 발동
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag    = TAG_GameplayEvent_Parry_Stagger;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void ULNPAbility_Stagger::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
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

	if (StaggerMontage)
	{
		if (const ALNPCharacterBase* Character = GetOwningCharacter())
		{
			if (UAnimInstance* AnimInst = Character->GetAnimInstance())
				AnimInst->Montage_Play(StaggerMontage);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
