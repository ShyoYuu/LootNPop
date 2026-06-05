// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_ParrySuccess.h"
#include "Character/LNPCharacterBase.h"
#include "LNPGameplayTags.h"

#include "Animation/AnimInstance.h"
#include "Abilities/GameplayAbility.h"

ULNPAbility_ParrySuccess::ULNPAbility_ParrySuccess()
{
	// TAG_GameplayEvent_Parry_Success 이벤트 수신 시 자동 발동
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag    = TAG_GameplayEvent_Parry_Success;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void ULNPAbility_ParrySuccess::ActivateAbility(
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

	if (ReactionMontage)
	{
		if (const ALNPCharacterBase* Character = GetOwningCharacter())
		{
			if (UAnimInstance* AnimInst = Character->GetAnimInstance())
				AnimInst->Montage_Play(ReactionMontage);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
