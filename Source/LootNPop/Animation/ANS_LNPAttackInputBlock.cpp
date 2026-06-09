// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/ANS_LNPAttackInputBlock.h"
#include "Character/LNPCharacterBase.h"
#include "LNPGameplayTags.h"

#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"

static ALNPCharacterBase* GetCharacter(USkeletalMeshComponent* MeshComp)
{
	return MeshComp ? Cast<ALNPCharacterBase>(MeshComp->GetOwner()) : nullptr;
}

void UANS_LNPAttackInputBlock::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (const ALNPCharacterBase* Character = GetCharacter(MeshComp))
	{
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
			ASC->AddLooseGameplayTag(TAG_Block_AttackInput);
	}
}

void UANS_LNPAttackInputBlock::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	ALNPCharacterBase* Character = GetCharacter(MeshComp);
	if (!Character)
		return;

	if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		ASC->RemoveLooseGameplayTag(TAG_Block_AttackInput);

	if (Character->ConsumeComboInput())
	{
		Character->IncrementComboIndex();
		Character->TryActivateAttack();
	}
	else
	{
		Character->ResetCombo();
	}
}
