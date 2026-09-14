// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPGameplayAbility.h"
#include "Character/LNPCharacterBase.h"
#include "Player/LNPPlayerState.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "AbilitySystemComponent.h"

ALNPCharacterBase* ULNPGameplayAbility::GetOwningCharacter() const
{
	return Cast<ALNPCharacterBase>(GetAvatarActorFromActorInfo());
}

ALNPPlayerState* ULNPGameplayAbility::GetOwningLNPPlayerState() const
{
	return Cast<ALNPPlayerState>(GetOwningActorFromActorInfo());
}

float ULNPGameplayAbility::GetAttackSpeed() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC == nullptr)
		return 1.0f;

	// AttributeSet의 PreAttributeChange가 0.01 미만을 막지만, ASC 미초기화 시의 0을 한 번 더 방어한다
	// (쿨다운·재장전 계산의 제수라 0이면 무한대가 된다).
	return FMath::Max(0.01f, ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetAttackSpeedAttribute()));
}
