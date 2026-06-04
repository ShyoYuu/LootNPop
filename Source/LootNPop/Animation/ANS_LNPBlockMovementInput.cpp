// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/ANS_LNPBlockMovementInput.h"
#include "Character/LNPCharacterBase.h"
#include "LNPGameplayTags.h"

#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"

static UAbilitySystemComponent* GetASC(USkeletalMeshComponent* MeshComp)
{
	const ALNPCharacterBase* Character = MeshComp ? Cast<ALNPCharacterBase>(MeshComp->GetOwner()) : nullptr;
	return Character ? Character->GetAbilitySystemComponent() : nullptr;
}

void UANS_LNPBlockMovementInput::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UAbilitySystemComponent* ASC = GetASC(MeshComp))
		ASC->AddLooseGameplayTag(TAG_Block_MovementInput);
}

void UANS_LNPBlockMovementInput::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UAbilitySystemComponent* ASC = GetASC(MeshComp))
		ASC->RemoveLooseGameplayTag(TAG_Block_MovementInput);
}
