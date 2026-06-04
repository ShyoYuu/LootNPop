// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/ANS_LNPCancelMontageOnMovement.h"
#include "Character/LNPCharacterBase.h"
#include "Character/LNPInputHandlerComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

void UANS_LNPCancelMontageOnMovement::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	const ALNPCharacterBase* Character = MeshComp ? Cast<ALNPCharacterBase>(MeshComp->GetOwner()) : nullptr;
	if (!Character)
		return;

	const ULNPInputHandlerComponent* InputHandler = Character->FindComponentByClass<ULNPInputHandlerComponent>();
	if (!InputHandler || !InputHandler->HasMovementInput())
		return;

	if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
		AnimInst->Montage_Stop(BlendOutTime);
}
