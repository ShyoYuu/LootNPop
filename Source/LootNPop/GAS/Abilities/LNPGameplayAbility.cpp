// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPGameplayAbility.h"
#include "Character/LNPCharacterBase.h"
#include "Player/LNPPlayerState.h"

ALNPCharacterBase* ULNPGameplayAbility::GetOwningCharacter() const
{
	return Cast<ALNPCharacterBase>(GetAvatarActorFromActorInfo());
}

ALNPPlayerState* ULNPGameplayAbility::GetOwningLNPPlayerState() const
{
	return Cast<ALNPPlayerState>(GetOwningActorFromActorInfo());
}
