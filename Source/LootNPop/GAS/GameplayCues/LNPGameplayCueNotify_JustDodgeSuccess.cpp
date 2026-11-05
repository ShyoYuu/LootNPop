// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/GameplayCues/LNPGameplayCueNotify_JustDodgeSuccess.h"
#include "Character/LNPCharacterBase.h"
#include "HitDetection/LNPGuardParryTypes.h"
#include "GameFramework/Pawn.h"
#include "LootNPop.h"

bool ULNPGameplayCueNotify_JustDodgeSuccess::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// local=1인 줄이 소유 화면이다 — 서버 JustDodge success 줄과의 시각 차가 연출 지연이다.
	if (LNPGuardParry::IsDodgeDebugEnabled())
	{
		const APawn* Pawn = Cast<APawn>(MyTarget);
		UE_LOG(LogLootNPop, Log, TEXT("[Dodge] Success cue arrived player=%s local=%d role=%s"),
			*GetNameSafe(MyTarget), Pawn && Pawn->IsLocallyControlled() ? 1 : 0,
			MyTarget ? *UEnum::GetValueAsString(MyTarget->GetLocalRole()) : TEXT("None"));
	}

	if (ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(MyTarget))
		Character->ApplyJustDodgeBlink(DisappearVFX, ReappearVFX, Parameters.Normal, AfterimageMaterial);

	return Super::OnExecute_Implementation(MyTarget, Parameters);
}
