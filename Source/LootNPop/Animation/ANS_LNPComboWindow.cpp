// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/ANS_LNPComboWindow.h"
#include "Character/LNPCharacterBase.h"
#include "LNPGameplayTags.h"

#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UANS_LNPComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	const ALNPCharacterBase* Character = MeshComp ? Cast<ALNPCharacterBase>(MeshComp->GetOwner()) : nullptr;
	if (UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr)
	{
		if (!ASC->HasMatchingGameplayTag(TAG_State_ComboWindow))
			ASC->AddLooseGameplayTag(TAG_State_ComboWindow);
	}
}

void UANS_LNPComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	const ALNPCharacterBase* Character = MeshComp ? Cast<ALNPCharacterBase>(MeshComp->GetOwner()) : nullptr;
	if (UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr)
	{
		if (ASC->HasMatchingGameplayTag(TAG_State_ComboWindow))
			ASC->RemoveLooseGameplayTag(TAG_State_ComboWindow);
	}
}
