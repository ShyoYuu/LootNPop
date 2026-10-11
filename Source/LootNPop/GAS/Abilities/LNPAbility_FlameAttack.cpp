// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_FlameAttack.h"
#include "GAS/Effects/LNPGameplayEffect_FlameFiring.h"
#include "Item/LNPWeaponData.h"
#include "AbilitySystemComponent.h"

void ULNPAbility_FlameAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (false == CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 분사 상태 갱신은 서버만 — 스택 갱신형 GE는 예측 적용이 막혀 있고, 이 태그를 읽는 회복도 서버에서만 돈다.
	const ULNPWeaponData* WeaponDef = GetEquippedWeaponDef();
	if (WeaponDef && HasAuthority(&ActivationInfo))
	{
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
			Handle, ActorInfo, ActivationInfo, ULNPGameplayEffect_FlameFiring::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
		if (SpecHandle.IsValid())
		{
			SpecHandle.Data->SetDuration(WeaponDef->MagazineRegenDelay, true);
			// 예측 키 없이 적용한다 — 키를 실으면 소유 클라가 복제본을 "자기가 예측한 것"으로 취급해 큐 이벤트를 건너뛴다.
			ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
