// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_Stagger.h"
#include "Config/LNPSettings.h"
#include "LNPGameplayTags.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

ULNPAbility_Stagger::ULNPAbility_Stagger()
{
	// 경직도 상태 전이(FLNPStaggerCommand)가 보내는 이벤트로 발동한다.
	// 패링도 전용 경로가 아니라 경직도(LNPPoise::ApplyParryBreak)를 거쳐 여기로 들어온다.
	// FNativeGameplayTag는 복사 불가라 이니셜라이저 리스트에 그대로 넣을 수 없다 — FGameplayTag로 먼저 변환한다.
	const FGameplayTag TriggerTags[] = { TAG_GameplayEvent_Stagger_Light,
	                                     TAG_GameplayEvent_Stagger_Heavy };
	for (const FGameplayTag& TriggerTag : TriggerTags)
	{
		FAbilityTriggerData TriggerData;
		TriggerData.TriggerTag    = TriggerTag;
		TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(TriggerData);
	}

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 어빌리티가 살아 있는 동안이 곧 경직 구간이다. 몽타주 ANS로 태그를 세우면
	// Montage_Stop에 Begin/End 페어가 깨질 수 있지만, 어빌리티 수명에 묶으면 그럴 수 없다.
	ActivationOwnedTags.AddTag(TAG_State_Staggered);
	ActivationOwnedTags.AddTag(TAG_Block_AttackInput);
	ActivationOwnedTags.AddTag(TAG_Block_MovementInput);

	// 그로기가 유지되는 동안 같은 이벤트가 또 와도 구간이 처음부터 다시 시작되지 않게 막는다.
	// 다운은 이 태그에 자기가 막히므로 FLNPStaggerCommand가 그로기 GA를 먼저 취소한다.
	ActivationBlockedTags.AddTag(TAG_State_Staggered);
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

	const FGameplayTag TriggerTag = TriggerEventData ? TriggerEventData->EventTag : FGameplayTag();

	// 그로기는 **지속 시간이 없다** — 경직도가 T1 아래로 자연회복할 때까지 이어지는 상태이며,
	// 종료는 ULNPPoiseProcessor가 이탈 에지를 잡아 FLNPStaggerCommand로 취소해 준다.
	// 시간 제한을 여기에 두면 게이지가 아직 높은데 행동이 풀려 버린다.
	if (TriggerTag == TAG_GameplayEvent_Stagger_Light)
		return;

	// 다운만 고정 시간이다.
	const float LockSeconds = GetDefault<ULNPSettings>()->PoiseDownLockSeconds;

	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, LockSeconds);
	if (!WaitTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	WaitTask->OnFinish.AddDynamic(this, &ULNPAbility_Stagger::OnLockFinished);
	WaitTask->ReadyForActivation();
}

void ULNPAbility_Stagger::OnLockFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
