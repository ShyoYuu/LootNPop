// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/ANS_LNPCancelMontageOnMovement.h"
#include "Animation/UAFSpike/LNPUAFNotifyAudit.h"
#include "Character/LNPCharacterBase.h"
#include "Character/LNPInputHandlerComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

void UANS_LNPCancelMontageOnMovement::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	LNPUAFNotifyAudit::Trace(TEXT("Begin"), this, MeshComp, Animation, EventReference);
}

void UANS_LNPCancelMontageOnMovement::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	LNPUAFNotifyAudit::Trace(TEXT("Tick"), this, MeshComp, Animation, EventReference);

	const ALNPCharacterBase* Character = MeshComp ? Cast<ALNPCharacterBase>(MeshComp->GetOwner()) : nullptr;
	if (!Character)
		return;

	const ULNPInputHandlerComponent* InputHandler = Character->FindComponentByClass<ULNPInputHandlerComponent>();
	if (!InputHandler || !InputHandler->HasMovementInput())
		return;

	if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
	{
		LNPUAFNotifyAudit::Trace(TEXT("MovementStop"), this, MeshComp, Animation, EventReference);
		AnimInst->Montage_Stop(BlendOutTime);
	}
}

void UANS_LNPCancelMontageOnMovement::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	LNPUAFNotifyAudit::Trace(TEXT("End"), this, MeshComp, Animation, EventReference);
}
